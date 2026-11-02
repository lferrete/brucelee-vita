/*
 * Generic, deliberately small JNI surface for the ZSystem layer.
 *
 * libzplatform asks Java for a handful of infrastructure objects (a
 * ClassLoader, the external-storage state receiver, the AssetManager, ...).
 * We do not emulate Dalvik; we only need every call to return a non-null,
 * type-appropriate token so the native file-system/display setup can finish.
 *
 * Port code Copyright (C) 2026 Leandro Ferrete; see AUTHORS.md and LICENSE.
 */

#include "java_shim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <psp2/audioout.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/io/fcntl.h>

#include <falso_jni/FalsoJNI.h>
#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI_ImplBridge.h>

#include "utils/logger.h"

NameToMethodID nameToMethodId[] = {
    { 1, "getClassLoader", METHOD_TYPE_OBJECT },
    { 2, "loadClass", METHOD_TYPE_OBJECT },
    { 3, "onExternalStorageChanged", METHOD_TYPE_VOID },
    { 4, "getExternalStorageState", METHOD_TYPE_OBJECT },
    { 5, "getPackageName", METHOD_TYPE_OBJECT },
    { 6, "getFilesDir", METHOD_TYPE_OBJECT },
    { 7, "getExternalFilesDir", METHOD_TYPE_OBJECT },
    { 8, "getCacheDir", METHOD_TYPE_OBJECT },
    { 9, "getAbsolutePath", METHOD_TYPE_OBJECT },
    { 10, "registerReceiver", METHOD_TYPE_VOID },
    { 11, "unregisterReceiver", METHOD_TYPE_VOID },
    { 12, "getAssets", METHOD_TYPE_OBJECT },
};

#define SHIM_DYNAMIC_MAX 128
#define SHIM_ID_BASE 0x1000

typedef struct {
    jmethodID id;
    char name[160];
    char base[64];
    METHOD_TYPE type;
} ShimMethod;

static ShimMethod g_dyn[SHIM_DYNAMIC_MAX];
static int g_dyn_count;

/* ------------------------------------------------------------------------- */
/* android.media.AudioTrack on top of sceAudioOut                             */
/* ------------------------------------------------------------------------- */
/* The game's audio driver opens an AudioTrack at 44100 Hz/16-bit/stereo and
 * writes interleaved PCM.  The Vita's main output port only accepts 48000 Hz,
 * so we resample with a polyphase fractional-delay FIR (Kaiser-windowed sinc).
 * Linear/cubic interpolators fold the 15-22 kHz content back into the audible
 * band (measured -14 dB aliases); this FIR keeps the images below -67 dB. */
#define AT_OUT_RATE 48000
#define AT_GRAIN    512
/* The game's mix is very quiet (~-20 dBFS peaks).  Apply digital gain here;
 * the Vita output port itself stays at its maximum volume. */
#define AT_GAIN     6.0f

#define AT_PHASES   1024
#define AT_K        24                 /* taps on each side of the kernel */
#define AT_TAPS     (2 * AT_K)
#define AT_RING     8192               /* frames, power of two */

static int at_port = -1;
static int at_rate = 44100;
static int at_out_rate = 44100;
static int at_resample = 0;
static int at_channels = 2;
static int at_opened = 0;
static int at_paused = 0;
static int16_t at_grain[AT_GRAIN * 2];
static int at_grain_frames = 0;

static float at_coef[AT_PHASES][AT_TAPS];
static int at_coef_ready = 0;

static int16_t at_ring[AT_RING * 2];
static unsigned int at_ring_w = 0;
static long long at_total_in = 0;
static double at_out_t = 0.0;
static double at_step = 1.0;

static unsigned int at_write_calls = 0;
static unsigned int at_frames_written = 0;
static int at_output_err_logged = 0;
static int at_peak_since = 0;
static int at_out_peak = 0;
static int at_saw_audio = 0;

static unsigned int at_grains = 0;
static SceUInt64 at_last_us = 0;
static unsigned int at_last_frames = 0;
static unsigned int at_last_grains = 0;

static int16_t at_gain_clip(float v) {
    v *= AT_GAIN;
    if (v > 32767.0f) return 32767;
    if (v < -32768.0f) return -32768;
    return (int16_t)v;
}

static void at_output_grain(void);

static float at_i0(float x) {
    float sum = 1.0f, y = x * x * 0.25f, term = 1.0f;
    for (int k = 1; k < 32; k++) {
        term *= y / (float)(k * k);
        sum += term;
        if (term < sum * 1e-7f)
            break;
    }
    return sum;
}

static void at_build_coef(void) {
    if (at_coef_ready)
        return;
    const float fc = 0.42f;          /* cutoff ~18.5 kHz */
    const float beta = 10.0f;
    float denom = at_i0(beta);

    for (int p = 0; p < AT_PHASES; p++) {
        float frac = (float)p / (float)AT_PHASES;
        float sum = 0.0f;
        for (int k = 0; k < AT_TAPS; k++) {
            float t = (float)(k - (AT_K - 1)) - frac;
            float h;
            if (t > -1e-6f && t < 1e-6f)
                h = 2.0f * fc;
            else
                h = sinf(6.28318530718f * fc * t) /
                    (3.14159265359f * t);
            float w = 0.0f;
            if (t > -AT_K && t < AT_K) {
                float q = t / (float)AT_K;
                w = at_i0(beta * sqrtf(1.0f - q * q)) / denom;
            }
            h *= w;
            at_coef[p][k] = h;
            sum += h;
        }
        if (sum != 0.0f)
            for (int k = 0; k < AT_TAPS; k++)
                at_coef[p][k] /= sum;
    }
    at_coef_ready = 1;
    l_info("AudioTrack: FIR pronto (%d fases x %d taps, fc=%.2f)",
           AT_PHASES, AT_TAPS, fc);
}

static void at_ring_put(const int16_t *in, int frames) {
    const unsigned int mask = AT_RING - 1;
    unsigned int w = at_ring_w;

    if (at_channels == 2) {
        int first = (w + (unsigned int)frames > AT_RING)
                        ? (int)(AT_RING - w) : frames;
        memcpy(&at_ring[w * 2], in,
               (size_t)first * 2 * sizeof(int16_t));
        if (first < frames)
            memcpy(&at_ring[0], in + first * 2,
                   (size_t)(frames - first) * 2 * sizeof(int16_t));
    } else {
        for (int i = 0; i < frames; i++) {
            unsigned int r = (w + (unsigned int)i) & mask;
            at_ring[r * 2] = in[i];
            at_ring[r * 2 + 1] = in[i];
        }
    }
    at_ring_w = (w + (unsigned int)frames) & mask;
    at_total_in += frames;
}

static void at_process_fir(void) {
    const unsigned int mask = AT_RING - 1;
    while (at_out_t + (double)AT_K < (double)at_total_in) {
        int i = (int)at_out_t;
        float frac = (float)(at_out_t - (double)i);
        int p = (int)(frac * (float)AT_PHASES);
        if (p >= AT_PHASES)
            p = AT_PHASES - 1;
        const float *c = at_coef[p];
        int base = i - (AT_K - 1);
        float lv = 0.0f, rv = 0.0f;
        for (int k = 0; k < AT_TAPS; k++) {
            unsigned int idx = (unsigned int)(base + k) & mask;
            float g = c[k];
            lv += (float)at_ring[idx * 2] * g;
            rv += (float)at_ring[idx * 2 + 1] * g;
        }
        at_grain[at_grain_frames * 2] = at_gain_clip(lv);
        at_grain[at_grain_frames * 2 + 1] = at_gain_clip(rv);
        at_grain_frames++;
        if (at_grain_frames == AT_GRAIN)
            at_output_grain();
        at_out_t += at_step;
    }
}

static void at_output_grain(void) {
    at_grains++;
    for (int i = 0; i < AT_GRAIN * 2; i++) {
        int v = at_grain[i];
        if (v < 0) v = -v;
        if (v > at_out_peak) at_out_peak = v;
    }
    if (at_port >= 0) {
        int r = sceAudioOutOutput(at_port, at_grain);
        if (r < 0 && !at_output_err_logged) {
            at_output_err_logged = 1;
            l_error("AudioTrack: sceAudioOutOutput falhou 0x%08x", (unsigned)r);
        }
    }
    at_grain_frames = 0;
}

static void at_open(int sample_rate, int channels) {
    if (at_port >= 0) {
        sceAudioOutReleasePort(at_port);
        at_port = -1;
    }
    at_opened = 0;
    at_rate = (sample_rate > 0) ? sample_rate : 44100;
    at_channels = (channels == 1) ? 1 : 2;
    at_grain_frames = 0;
    at_paused = 0;

    /* Pre-pad the ring with AT_K zero frames so the FIR always has history,
     * even for the first output sample. */
    memset(at_ring, 0, sizeof(at_ring));
    at_ring_w = (unsigned int)AT_K & (AT_RING - 1);
    at_total_in = AT_K;
    at_out_t = (double)AT_K;

    /* The SDK header lists 44100, but SCE_AUDIO_OUT_PORT_TYPE_MAIN rejects it
     * at runtime with 0x80260008 (INVALID_SAMPLE_FREQ); the main port is
     * 48000-only.  Keep the requested rate when it is accepted (VOICE/BGM
     * ports may differ) and fall back to 48000 with the FIR otherwise. */
    int supported = (at_rate == 8000 || at_rate == 11025 || at_rate == 12000 ||
                     at_rate == 16000 || at_rate == 22050 || at_rate == 24000 ||
                     at_rate == 32000 || at_rate == 44100 || at_rate == 48000);
    at_out_rate = supported ? at_rate : AT_OUT_RATE;
    at_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, AT_GRAIN,
                                  at_out_rate, SCE_AUDIO_OUT_MODE_STEREO);
    if (at_port < 0 && at_out_rate != AT_OUT_RATE) {
        l_warn("AudioTrack: porta em %d Hz falhou 0x%08x, tentando %d Hz",
               at_out_rate, (unsigned)at_port, AT_OUT_RATE);
        at_out_rate = AT_OUT_RATE;
        at_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, AT_GRAIN,
                                      at_out_rate, SCE_AUDIO_OUT_MODE_STEREO);
    }
    if (at_port < 0) {
        l_error("AudioTrack: sceAudioOutOpenPort(%d) failed 0x%08x",
                at_out_rate, (unsigned)at_port);
        at_port = -1;
        return;
    }
    at_resample = (at_rate != at_out_rate);
    at_step = (double)at_rate / (double)at_out_rate;
    if (at_resample)
        at_build_coef();
    int vol[2] = { SCE_AUDIO_OUT_MAX_VOL, SCE_AUDIO_OUT_MAX_VOL };
    sceAudioOutSetVolume(at_port,
                         SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH,
                         vol);
    at_opened = 1;
    l_info("AudioTrack: sceAudioOut aberto (%d Hz stereo, %d frames/bloco; origem %d Hz, %d ch, resample=%d)",
           at_out_rate, AT_GRAIN, at_rate, at_channels, at_resample);
}

static void at_release(void) {
    if (at_port >= 0)
        sceAudioOutReleasePort(at_port);
    at_port = -1;
    at_opened = 0;
}

static void at_write_pcm(const int16_t *in, int frames) {
    if (!at_opened || at_port < 0 || frames <= 0)
        return;
    if (at_paused)
        return;

    {
        int peak = 0, n = frames * at_channels;
        for (int k = 0; k < n; k++) {
            int v = in[k];
            if (v < 0) v = -v;
            if (v > peak) peak = v;
        }
        if (peak > at_peak_since)
            at_peak_since = peak;
        if (!at_saw_audio && peak > 64) {
            at_saw_audio = 1;
            l_info("[at] PRIMEIRO PCM NAO-ZERO: %d frames, pico %d, "
                   "amostras %d,%d,%d,%d", frames, peak, in[0],
                   in[1 < n ? 1 : 0], in[2 < n ? 2 : 0], in[3 < n ? 3 : 0]);
        }
    }
    at_write_calls++;
    at_frames_written += (unsigned int)frames;
    if ((at_write_calls % 500) == 0) {
        SceUInt64 now = sceKernelGetProcessTimeWide();
        double in_rate = 0.0, out_rate = 0.0;
        if (at_last_us) {
            double secs = (double)(now - at_last_us) / 1000000.0;
            if (secs > 0.0) {
                in_rate = (double)(at_frames_written - at_last_frames) / secs;
                out_rate = (double)(at_grains - at_last_grains) *
                           (double)AT_GRAIN / secs;
            }
        }
        l_info("[at] writes=%u in=%.0f/s out=%.0f/s pico=%d out=%d",
               at_write_calls, in_rate, out_rate, at_peak_since, at_out_peak);
        at_last_us = now;
        at_last_frames = at_frames_written;
        at_last_grains = at_grains;
        at_peak_since = 0;
        at_out_peak = 0;
    }

    if (!at_resample) {
        int i = 0;
        while (i < frames) {
            int chunk = AT_GRAIN - at_grain_frames;
            if (chunk > frames - i)
                chunk = frames - i;
            for (int k = 0; k < chunk; k++) {
                float l = at_channels == 1 ? (float)in[i + k]
                                           : (float)in[(i + k) * 2];
                float r = at_channels == 1 ? (float)in[i + k]
                                           : (float)in[(i + k) * 2 + 1];
                at_grain[(at_grain_frames + k) * 2] = at_gain_clip(l);
                at_grain[(at_grain_frames + k) * 2 + 1] = at_gain_clip(r);
            }
            at_grain_frames += chunk;
            i += chunk;
            if (at_grain_frames == AT_GRAIN)
                at_output_grain();
        }
        return;
    }

    at_ring_put(in, frames);
    at_process_fir();
}

/* AudioTrack methods are registered by bare name (the FalsoJNI class prefix
 * is only kept for constructors), so identify them by name here. */
static int at_method_name(const char *n) {
    return n && (!strcmp(n, "write") || !strcmp(n, "getMinBufferSize") ||
                 !strcmp(n, "getPlayState") || !strcmp(n, "getState") ||
                 !strcmp(n, "setStereoVolume") || !strcmp(n, "flush") ||
                 !strcmp(n, "play") || !strcmp(n, "pause") ||
                 !strcmp(n, "stop") || !strcmp(n, "release") ||
                 !strcmp(n, "getMaxVolume") || !strcmp(n, "getMinVolume"));
}

static const char *base_name(const char *name, char *out, size_t out_size) {
    const char *slash = strrchr(name, '/');
    const char *b = slash ? slash + 1 : name;
    if (!strcmp(b, "<init>")) {
        snprintf(out, out_size, "%s", "<init>");
    } else {
        snprintf(out, out_size, "%s", b);
    }
    return out;
}

static const char *method_name_for_id(jmethodID id) {
    for (int i = 0; i < g_dyn_count; ++i) {
        if (g_dyn[i].id == id)
            return g_dyn[i].base;
    }
    for (int i = 0; i < (int)(sizeof(nameToMethodId) / sizeof(NameToMethodID)); ++i) {
        if (nameToMethodId[i].name && nameToMethodId[i].id == (int)id)
            return nameToMethodId[i].name;
    }
    return NULL;
}

static const char *method_full_name_for_id(jmethodID id) {
    for (int i = 0; i < g_dyn_count; ++i) {
        if (g_dyn[i].id == id)
            return g_dyn[i].name;
    }
    return NULL;
}

static jobject fake_object(jmethodID id) {
    (void)id;
    // A C string is a valid opaque jobject for our fake JVM, and it doubles as
    // a safe class-name token if the native code feeds it back into
    // GetMethodID() (which treats jclass as a C string for constructors).
    return (jobject)strdup("java/lang/Object");
}

static jobject string_result(const char *s) {
    return jni->NewStringUTF(&jni, s);
}

void java_shim_init(void) {
    l_info("java_shim: initialized (%d dynamic slots)", SHIM_DYNAMIC_MAX);
}

jmethodID jni_shim_register_method(const char *name, const char *sig) {
    if (!name || !sig) return NULL;

    for (int i = 0; i < g_dyn_count; ++i) {
        if (!strcmp(g_dyn[i].name, name))
            return g_dyn[i].id;
    }
    if (g_dyn_count >= SHIM_DYNAMIC_MAX) {
        l_error("java_shim: dynamic method table full (%s)", name);
        return NULL;
    }

    const char *ret = strchr(sig, ')');
    char c = ret ? ret[1] : 'V';
    METHOD_TYPE t = METHOD_TYPE_OBJECT;
    if (!strstr(name, "<init>")) {
        switch (c) {
            case 'V': t = METHOD_TYPE_VOID; break;
            case 'Z': t = METHOD_TYPE_BOOLEAN; break;
            case 'B': t = METHOD_TYPE_BYTE; break;
            case 'C': t = METHOD_TYPE_CHAR; break;
            case 'S': t = METHOD_TYPE_SHORT; break;
            case 'I': t = METHOD_TYPE_INT; break;
            case 'J': t = METHOD_TYPE_LONG; break;
            case 'F': t = METHOD_TYPE_FLOAT; break;
            case 'D': t = METHOD_TYPE_DOUBLE; break;
            default:  t = METHOD_TYPE_OBJECT; break;
        }
    }

    ShimMethod *m = &g_dyn[g_dyn_count];
    memset(m, 0, sizeof(*m));
    m->id = (jmethodID)(SHIM_ID_BASE + g_dyn_count);
    snprintf(m->name, sizeof(m->name), "%s", name);
    base_name(name, m->base, sizeof(m->base));
    m->type = t;
    g_dyn_count++;

    l_info("java_shim: registered '%s' as id 0x%x (type %d)", name, (int)m->id, t);
    return m->id;
}

jmethodID jni_shim_get_method_id(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_dyn_count; ++i) {
        if (!strcmp(g_dyn[i].name, name))
            return g_dyn[i].id;
    }
    return NULL;
}

int jni_shim_has_method(jmethodID id) {
    for (int i = 0; i < g_dyn_count; ++i) {
        if (g_dyn[i].id == id)
            return 1;
    }
    return 0;
}

jobject jni_shim_object_call(jmethodID id, va_list args) {
    const char *n = method_name_for_id(id);
    if (!n) return NULL;
    l_debug("jni: %s", n);

    const char *full = method_full_name_for_id(id);
    if (full && strstr(full, "AudioTrack") && !strcmp(n, "<init>")) {
        int stream_type = va_arg(args, int);
        int sample_rate = va_arg(args, int);
        int channel_config = va_arg(args, int);
        int audio_format = va_arg(args, int);
        int buffer_size = va_arg(args, int);
        int mode = va_arg(args, int);
        (void) stream_type;
        (void) audio_format;
        (void) buffer_size;
        (void) mode;
        at_open(sample_rate, channel_config == 4 ? 1 : 2);
        return fake_object(id);
    }

    if (!strcmp(n, "getSystemString")) {
        int which = va_arg(args, int);
        // 1 = platform name (matched against metadata.xml <platform type=...>),
        // 4 = language, 5 = region.  zpInitializeSystem() aborts the whole boot
        // if the platform name does not match the metadata.
        if (which == 1)
            return string_result("android");
        return string_result(which == 4 ? "en" : "US");
    }
    if (!strcmp(n, "getLanguage"))
        return string_result("en");
    if (!strcmp(n, "getCountry"))
        return string_result("US");
    if (!strcmp(n, "getVariant") || !strcmp(n, "getISO3Language") ||
        !strcmp(n, "getISO3Country") || !strcmp(n, "getDisplayLanguage") ||
        !strcmp(n, "getDisplayCountry"))
        return string_result("");
    if (!strcmp(n, "getExternalStorageState"))
        return string_result("mounted");
    if (!strcmp(n, "getPackageName"))
        return string_result("com.digitallegends.brucelee");
    if (!strcmp(n, "getFilesDir") || !strcmp(n, "getExternalFilesDir"))
        return string_result(DATA_PATH "files");
    if (!strcmp(n, "getCacheDir"))
        return string_result(DATA_PATH "cache");
    if (!strcmp(n, "getAbsolutePath"))
        return string_result(DATA_PATH "files");
    if (!strcmp(n, "getPath"))
        return string_result(DATA_PATH "files");
    if (!strcmp(n, "getText") || !strcmp(n, "toString") || !strcmp(n, "getString"))
        return string_result("");
    if (!strcmp(n, "list"))
        return jni->NewObjectArray(&jni, 0, NULL, NULL);
    if (!strcmp(n, "GetCACertsPEM"))
        return jni->NewByteArray(&jni, 0);

    /* ClassLoader, loaded Class, receivers, AssetManager, ...: opaque token. */
    return fake_object(id);
}

void jni_shim_void_call(jmethodID id, va_list args) {
    (void)args;
    const char *n = method_name_for_id(id);
    if (at_method_name(n)) {
        if (!strcmp(n, "play"))
            at_paused = 0;
        else if (!strcmp(n, "pause") || !strcmp(n, "stop"))
            at_paused = 1;
        else if (!strcmp(n, "release"))
            at_release();
        else if (!strcmp(n, "flush")) {
            at_grain_frames = 0;
            memset(at_ring, 0, sizeof(at_ring));
            at_ring_w = (unsigned int)AT_K & (AT_RING - 1);
            at_total_in = AT_K;
            at_out_t = (double)AT_K;
        }
        if (strcmp(n, "flush"))
            l_info("AudioTrack.%s()", n);
        return;
    }
    l_info("java_shim: void call %s", n ? n : "(unknown)");
}

jboolean jni_shim_boolean_call(jmethodID id, va_list args) {
    (void)args;
    l_debug("jni: %s -> false", method_name_for_id(id));
    return 0;
}
jbyte    jni_shim_byte_call(jmethodID id, va_list args)    { (void)args; l_debug("jni: %s -> 0", method_name_for_id(id)); return 0; }
jchar    jni_shim_char_call(jmethodID id, va_list args)    { (void)args; l_debug("jni: %s -> 0", method_name_for_id(id)); return 0; }
jshort   jni_shim_short_call(jmethodID id, va_list args)   { (void)args; l_debug("jni: %s -> 0", method_name_for_id(id)); return 0; }
jint     jni_shim_int_call(jmethodID id, va_list args)     {
    const char *n = method_name_for_id(id);
    if (at_method_name(n)) {
        if (!strcmp(n, "getMinBufferSize")) {
            int rate = va_arg(args, int);
            int ch = va_arg(args, int);
            int fmt = va_arg(args, int);
            l_info("AudioTrack.getMinBufferSize(%d, %d, %d) -> 4096",
                   rate, ch, fmt);
            return 4096;
        }
        if (!strcmp(n, "write")) {
            /* signature is ([BII)I: a byte array with a byte offset. */
            jbyteArray arr = va_arg(args, jbyteArray);
            int offset = va_arg(args, int);
            int size = va_arg(args, int);
            if (arr && size > 0) {
                JavaDynArray *jda = (JavaDynArray *) arr;
                const uint8_t *bytes = (const uint8_t *) jda->array + offset;
                int frames = size / (2 * at_channels);
                at_write_pcm((const int16_t *) bytes, frames);
            }
            return size;
        }
        if (!strcmp(n, "getPlayState"))
            return 3;   /* PLAYSTATE_PLAYING */
        if (!strcmp(n, "getState"))
            return 1;   /* STATE_INITIALIZED */
        if (!strcmp(n, "setStereoVolume"))
            return 0;   /* SUCCESS */
    }
    (void) args;
    l_debug("jni: %s -> 0", n ? n : "(unknown)");
    return 0;
}
jlong    jni_shim_long_call(jmethodID id, va_list args)    { (void)args; l_debug("jni: %s -> 0", method_name_for_id(id)); return 0; }
jfloat   jni_shim_float_call(jmethodID id, va_list args)   {
    (void)args;
    const char *n = method_name_for_id(id);
    if (n && !strcmp(n, "getMaxVolume"))
        return 1.0f;
    if (n && !strcmp(n, "getMinVolume"))
        return 0.0f;
    /* InputThreadProc polls query() tens of thousands of times per second;
     * logging each one buries every other line. */
    if (!n || strcmp(n, "query") != 0)
        l_debug("jni: %s -> 0.0", n ? n : "(unknown)");
    return 0.0f;
}
jdouble  jni_shim_double_call(jmethodID id, va_list args)  { (void)args; l_debug("jni: %s -> 0.0", method_name_for_id(id)); return 0.0; }

/* ------------------------------------------------------------------------- */
/* FalsoJNI tables.  Only the methods ZPlatform uses during init are listed  */
/* explicitly; every other method is registered dynamically by name/signature.*/
/* ------------------------------------------------------------------------- */

MethodsBoolean methodsBoolean[] = {};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsInt methodsInt[] = {};
MethodsLong methodsLong[] = {};

static jobject shim_object(jmethodID id, va_list args) { return jni_shim_object_call(id, args); }
static void shim_void(jmethodID id, va_list args) { jni_shim_void_call(id, args); }

MethodsObject methodsObject[] = {
    { 1, shim_object },
    { 2, shim_object },
    { 4, shim_object },
    { 5, shim_object },
    { 6, shim_object },
    { 7, shim_object },
    { 8, shim_object },
    { 9, shim_object },
    { 12, shim_object },
};

MethodsShort methodsShort[] = {};
MethodsVoid methodsVoid[] = {
    { 3, shim_void },
    { 10, shim_void },
    { 11, shim_void },
};

NameToFieldID nameToFieldId[] = {};
FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {};
FieldsLong fieldsLong[] = {};
FieldsObject fieldsObject[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES

/*
 * Bruce Lee: Dragon Warrior - PS Vita port
 * Port code Copyright (C) 2026 Leandro Ferrete.
 * Original game/assets and third-party libraries retain their own credits;
 * see AUTHORS.md and LICENSE.
 */

#include "loader.h"
#include "java_shim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>

#include <falso_jni/FalsoJNI.h>
#include <kubridge.h>

#include "utils/dialog.h"
#include "utils/init.h"
#include "utils/logger.h"
#include "utils/utils.h"

so_module zplatform_mod;
so_module default_mod;

void *zplatform_handle = (void *)0x5A000001;
void *default_handle   = (void *)0x5A000002;
void *android_handle   = (void *)0x5A000003;
void *opensles_handle  = (void *)0x5A000004;

/* ZPlatform keeps its current activity in a BSS pointer; _zpGetActivity()
 * simply returns it.  The offset below is from the shipped libzplatform.so
 * (ARM, version 1.13.17 / 1.15.26). */
#define ZP_ACTIVITY_GLOBAL_OFFSET 0x2bb7f8u

typedef struct {
    /* ZACTIVITY: the object libjava-activity.so builds on Android.  The game
     * reads it through the l_pActivity global; the exact offsets below come
     * from JNI_InitializeActivity() and ZAndroidEntry() in the shipped libs. */
    unsigned char activity[0x80];
    unsigned char callbacks[0x70];        /* ANativeActivityCallbacks */
    unsigned char native_activity[0x28];  /* ANativeActivity */
    unsigned char input_driver[0x260];    /* populated by ZAndroidEntry() */
    unsigned char surface_holder[0x40];   /* Java Surface holder (jobject at +0xc) */
} FakeActivity;

static FakeActivity g_activity;

#define ACT_FIELD(type, off) (*(type *)(g_activity.activity + (off)))

void brucelee_setup_activity(void) {
    memset(&g_activity, 0, sizeof(g_activity));

    /* ZACTIVITY fields used by ZPlatform. */
    ACT_FIELD(void *, 0x08) = g_activity.callbacks;
    ACT_FIELD(void *, 0x10) = g_activity.native_activity;
    ACT_FIELD(JavaVM *, 0x14) = &jvm;
    ACT_FIELD(jobject, 0x18) = (jobject)(uintptr_t)0x4A4A4A4A;
    ACT_FIELD(void *, 0x5c) = g_activity.input_driver;
    ACT_FIELD(void *, 0x60) = g_activity.surface_holder;
    ACT_FIELD(void *, 0x68) = (void *)(uintptr_t)0x60000001; /* ANativeWindow */
    ACT_FIELD(int, 0x6c) = 960;
    ACT_FIELD(int, 0x70) = 544;

    /* libjava-activity stores a size/version word at the head of both blocks;
     * ZAndroidEntry() refuses to run if it is smaller. */
    *(uint32_t *)g_activity.callbacks = 0x70;   /* must be > 0x6f */
    *(uint32_t *)g_activity.input_driver = 0x260;

    /* The input controller calls a method on the Java Surface object stored at
     * surface_holder+0xc. */
    *(jobject *)(g_activity.surface_holder + 0x0c) = (jobject)(uintptr_t)0x4A4A4A4A;

    /* ANativeActivity */
    unsigned char *na = g_activity.native_activity;
    *(void **)(na + 0x00) = g_activity.callbacks;
    *(JavaVM **)(na + 0x04) = &jvm;
    *(void **)(na + 0x08) = (void *)&jni;
    *(jobject *)(na + 0x0c) = (jobject)(uintptr_t)0x4A4A4A4A;
    *(const char **)(na + 0x10) = DATA_PATH;
    *(const char **)(na + 0x14) = DATA_PATH;
    *(int32_t *)(na + 0x18) = 15;   /* sdkVersion */
    *(void **)(na + 0x1c) = NULL;   /* instance */
    *(void **)(na + 0x20) = NULL;   /* AAssetManager */
    *(const char **)(na + 0x24) = NULL;

    void **slot = (void **)(zplatform_mod.text_base + ZP_ACTIVITY_GLOBAL_OFFSET);
    *slot = &g_activity;

    l_info("activity: fake ZACTIVITY at %p, global slot %p -> %p",
           &g_activity, slot, *slot);
    l_info("activity: JavaVM=%p JNIEnv=%p", &jvm, &jni);
}

void brucelee_poll_pad(void);

static int pad_poll_thread(SceSize args, void *argp) {
    /* Polling now happens on the main thread (from the display-valid hook),
     * which never stalls on the engine's input/event locks.  This thread is
     * kept only so the thread entry referenced by the loader still exists. */
    (void) args;
    (void) argp;
    return 0;
}

/* ZAndroidEntry() is what libjava-activity.so calls right after building the
 * ZACTIVITY: it dlopen()s libandroid.so and fills the input driver table at
 * activity+0x5c.  Without it zpTerminateSystem() dereferences a NULL pointer. */
void brucelee_android_entry(void) {
    uintptr_t fn = so_symbol(&zplatform_mod, "ZAndroidEntry");
    if (!fn) {
        l_warn("ZAndroidEntry not exported by libzplatform.so");
        return;
    }
    int (*entry)(void *) = (int (*)(void *))fn;
    int ret = entry(&g_activity);
    l_info("ZAndroidEntry -> %d", ret);

    /* zpTerminateSystem() calls input_driver[0x24c / 4] and then spins
     * forever waiting for a flag that only the Android input queue ever sets.
     * Dropping that one callback takes the safe teardown branch. */
    ((void **)g_activity.input_driver)[0x24c / 4] = NULL;

    /* Dedicated pad poller: the engine's input thread does not reliably call
     * usleep() after boot, so poll here and feed GameClass::OnInputEvent. */
    SceUID tid = sceKernelCreateThread("input_poll", pad_poll_thread, 0xA0,
                                       0x4000, 0, 0, NULL);
    if (tid >= 0)
        sceKernelStartThread(tid, 0, NULL);
}

static int stub_ret0(void) {
    return 0;
}

/* "no persistent storage" status used by the engine's zsys layer. */
static int stub_ret_no_storage(void) {
    return 0x30003;
}

/* CZdkDisplay reads the initial video mode from the Android window through
 * _zpWindowGetSize().  Our fake window has no real size, so without this the
 * engine starts at <0,0> and cannot find a compatible video mode. */
static int zp_window_get_size_hook(int *width, int *height) {
    if (width) *width = 960;
    if (height) *height = 544;
    return 0;
}

/* The ARM bionic atomics in libzplatform call the Linux kernel helper page at
 * 0xFFFF0FC0, which does not exist on the Vita (the first cmpxchg faults).
 * Replace them with the compiler's own ldrex/strex atomics. */
static int zc_atomic_dec_hook(volatile int *ptr) {
    return __sync_sub_and_fetch(ptr, 1);
}

static int zc_atomic_inc_hook(volatile int *ptr) {
    return __sync_add_and_fetch(ptr, 1);
}

static int zc_atomic_cmpxchg_hook(int old, int new, volatile int *ptr) {
    return __sync_val_compare_and_swap(ptr, old, new);
}

static int zc_atomic_xchgadd_hook(volatile int *ptr, int add) {
    return __sync_fetch_and_add(ptr, add);
}

/* CZdkDisplay overwrites its width/height fields from the (zeroed) display
 * description, so CRenderAPI starts at <0,0> and cannot find a video mode.
 * The display itself is still valid; report the engine's own default mode. */
static int czdk_get_width_hook(void *self) {
    (void) self;
    return 960;
}

static int czdk_get_height_hook(void *self) {
    (void) self;
    return 544;
}

/* The engine's built-in mode list only has two Android phone resolutions
 * (854x480 among them), so asking for the Vita panel size fails with "No
 * compatible video mode found" and the app aborts before the first frame.
 * Expose the real panel as the one and only mode. */
static int czdk_find_video_mode_hook(void *self, unsigned int w,
                                     unsigned int h, int fmt) {
    (void) self;
    if (w == 960 && h == 544) {
        l_info("[disp] FindVideoMode(%u,%u,fmt=%d) -> 0", w, h, fmt);
        return 0;
    }
    l_warn("[disp] FindVideoMode(%u,%u,fmt=%d) -> unavailable", w, h, fmt);
    return -1;
}

static int czdk_get_video_mode_info_hook(void *self, int index, int *w,
                                         int *h, int *fmt) {
    (void) self;
    (void) index;
    if (w) *w = 960;
    if (h) *h = 544;
    if (fmt) *fmt = 3;
    return 1;
}

static int czdk_get_video_mode_count_hook(void *self) {
    (void) self;
    return 1;
}

/* The Android display setup passes format/depth 0, so zplatform's internal
 * "display valid" flag never gets set even though EGL/GL initialized fine.
 * GameClass checks this every frame and otherwise refuses to render. */
static volatile int g_game_ready = 0;

static int zp_display_is_valid_hook(void) {
    g_game_ready = 1;
    /* Poll the pad on the game's own thread: the dedicated poll thread can
     * remain blocked inside sceCtrlPeekBufferPositive after a slow boot. */
    brucelee_poll_pad();
    return 1;
}

/* ------------------------------------------------------------------------- */
/* EULA bypass                                                               */
/* ------------------------------------------------------------------------- */
/* CheckEulaAcceptance() (libdefault.so+0x18d4c0) reads the EULA_ACCEPTED
 * TReal from CSettings and raises either "no_eula" or "eula_accepted".  The
 * disclaimer state has no usable input mapping for the Vita pad, so force the
 * comparison to take the accepted path (0x18d5e4).  Both conditional branches
 * are rewritten, because the setting item can be dropped when its refcount
 * reaches zero and that path skips the first comparison. */
#define EULA_VCMP_BNE_OFF  0x18d550u
#define EULA_VCMP_BEQ_OFF  0x18d5e0u
#define EULA_ACCEPTED_OFF  0x18d5e4u

static uint32_t arm_branch(uintptr_t from, uintptr_t to) {
    int32_t off = (int32_t)(to - (from + 8));
    return 0xEA000000u | (((uint32_t)(off >> 2)) & 0x00FFFFFFu);
}

static void patch_eula_check(void) {
    uintptr_t base = default_mod.text_base;
    volatile uint32_t *p_bne = (volatile uint32_t *)(base + EULA_VCMP_BNE_OFF);
    volatile uint32_t *p_beq = (volatile uint32_t *)(base + EULA_VCMP_BEQ_OFF);
    uint32_t w_bne = *p_bne;
    uint32_t w_beq = *p_beq;

    if ((w_bne & 0xFF000000u) != 0x1A000000u ||
        (w_beq & 0xFF000000u) != 0x0A000000u) {
        l_warn("[eula] unexpected opcodes %08x %08x (not patched)",
               (unsigned)w_bne, (unsigned)w_beq);
        return;
    }

    /* The text segment is mapped RX, so plain stores fault; kubridge is the
     * only way to write code pages from here. */
    uintptr_t target = base + EULA_ACCEPTED_OFF;
    uint32_t insn_bne = arm_branch(base + EULA_VCMP_BNE_OFF, target);
    uint32_t insn_beq = arm_branch(base + EULA_VCMP_BEQ_OFF, target);
    kuKernelCpuUnrestrictedMemcpy((void *)p_bne, &insn_bne, sizeof(insn_bne));
    kuKernelCpuUnrestrictedMemcpy((void *)p_beq, &insn_beq, sizeof(insn_beq));
    kuKernelFlushCaches((void *)base, default_mod.text_size);
    l_success("[eula] CheckEulaAcceptance forced to the accepted path (%08x %08x)",
              (unsigned)insn_bne, (unsigned)insn_beq);
}

/* The engine's Android input thread polls usleep() every 33 ms.  Feed it the
 * Vita pad there so the ZSystem input dispatcher receives real key/touch
 * events (the game's intro UI otherwise waits forever). */
typedef struct { uint32_t w[6]; } zevent_words;

/* GameClass::OnInputEvent() turns ZEVENT touch events into pixel coordinates
 * with
 *     x_px = (w4 + 0.5) * display_width
 *     y_px = (w5 + 0.5) * display_height
 * so the event carries *centered* normalized coordinates (-0.5 .. 0.5).
 * The Vita front panel reports 1920x1088 for the 960x544 screen. */
/* GameClass::OnInputEvent maps ZEVENT type 7 to TInputEvent press, 8 to
 * release and 6 to move (see the disassembly at libdefault.so+0x12ed48). */
#define ZEV_TOUCH_MOVE 6
#define ZEV_TOUCH_DOWN 7
#define ZEV_TOUCH_UP   8

static int g_key_down[16];
static volatile int g_touch_wait = 0;
static volatile int g_in_fight = 0;
/* GamePause/settings/confirm-back are ZSystem menus: they confirm with
 * ZP_BTN_SELECT, unlike the main menu which uses ZP_BTN_A.  g_pause_main is
 * true only on the root pause screen (RESUME/COMMAND LIST/SETTINGS/MAIN MENU);
 * there Circle has no key action, so it taps the RESUME button instead. */
static volatile int g_paused = 0;
static volatile int g_pause_main = 0;

/* ZEVENT key events: 18 = button down, 17 = up; w[1] is the ZP gamepad button
 * index (the engine's Android controller maps Android key codes to these
 * indices in _ZPAndroidNativeInputController::HandleKeyEvent). */
#define ZEV_KEY_UP   17
#define ZEV_KEY_DOWN 18

/* ZP gamepad button indices, i.e. the Xperia Play layout.  The engine's
 * TranslateKeyCode() converts them into EKeyCode values (0=up, 1=down,
 * 2=left, 3=right, 4=A, 5=B, 6=X, 7=Y, 8=start, 9=select, 10=L, 11=R). */
#define ZP_BTN_UP     0
#define ZP_BTN_DOWN   1
#define ZP_BTN_LEFT   2
#define ZP_BTN_RIGHT  3
#define ZP_BTN_A      4
#define ZP_BTN_B      5
#define ZP_BTN_X      6
#define ZP_BTN_Y      7
#define ZP_BTN_START  8
#define ZP_BTN_SELECT 9
#define ZP_BTN_L      10
#define ZP_BTN_R      11

typedef struct { unsigned int vita; int zp; } pad_key_map_t;

static const pad_key_map_t k_pad_map[] = {
    { SCE_CTRL_UP,       ZP_BTN_UP },
    { SCE_CTRL_DOWN,     ZP_BTN_DOWN },
    { SCE_CTRL_LEFT,     ZP_BTN_LEFT },
    { SCE_CTRL_RIGHT,    ZP_BTN_RIGHT },
    { SCE_CTRL_SELECT,   ZP_BTN_SELECT },
    { SCE_CTRL_LTRIGGER, ZP_BTN_L },
    { SCE_CTRL_RTRIGGER, ZP_BTN_R },
};

/* ZEVENT touch events: 7 = press, 8 = release, 6 = move.  Coordinates are
 * centered normalized values: x_px = (w4 + 0.5) * display_width. */
#define ZEV_TOUCH_MOVE 6
#define ZEV_TOUCH_DOWN 7
#define ZEV_TOUCH_UP   8

/* Copy the instructions displaced by hook_addr() into the module code cave
 * and append a jump back to addr+8, giving us a callable "original". */
static void *make_trampoline(so_module *mod, uintptr_t addr, so_hook h) {
    uintptr_t p = (mod->cave_head + 15u) & ~15u;
    if (p + 16 > mod->cave_base + mod->cave_size) {
        l_warn("[hook] no code cave left for trampoline @ %p", (void *) addr);
        return NULL;
    }
    uint32_t code[4];
    code[0] = h.orig_instr[0];
    code[1] = h.orig_instr[1];
    code[2] = 0xE51FF004u;              /* ldr pc, [pc, #-4] */
    code[3] = (uint32_t) (addr + 8u);
    kuKernelCpuUnrestrictedMemcpy((void *) p, code, sizeof(code));
    kuKernelFlushCaches((void *) p, sizeof(code));
    mod->cave_head = p + sizeof(code);
    return (void *) p;
}

/* Diagnostic: dump the input bindings the engine loads (keyboard / analog /
 * touch tables of CInputManager) and every event that reaches the game. */
static void (*g_orig_loadcfg)(void *, unsigned int, void *, float);
static void (*g_orig_krminput)(void *, const void *);
static void *(*g_orig_getitem)(void *, const char *, int);
static volatile int g_cfg_loading = 0;

static void *getitem_hook(void *self, const char *name, int create) {
    if (g_cfg_loading && name)
        l_info("[str] %s", name);
    return g_orig_getitem ? g_orig_getitem(self, name, create) : NULL;
}

static void loadcfg_hook(void *self, unsigned int idx, void *gfx, float scale) {
    g_cfg_loading = 1;
    if (g_orig_loadcfg)
        g_orig_loadcfg(self, idx, gfx, scale);
    g_cfg_loading = 0;
    const int *p = (const int *) self;
    l_info("[cfg] LoadInputConfig(%u): kb=%d ana=%d touch=%d",
           idx, p[0x2c / 4], p[0x54 / 4], p[0x68 / 4]);

    int count = *(int *) ((char *) self + 0x40);
    int stride = *(int *) ((char *) self + 0x48);
    char *arr = *(char **) ((char *) self + 0x44);
    l_info("[btn] count=%d stride=%d arr=%p", count, stride, (void *) arr);
    for (int i = 0; i < count && i < 16 && arr; i++) {
        const int *e = (const int *) (arr + (size_t) i * stride);
        l_info("[btn] %d rect=%d,%d %dx%d h18=%08x h1c=%08x h20=%08x",
               i, e[0], e[1], e[2], e[3], (unsigned) e[6],
               (unsigned) e[7], (unsigned) e[8]);
    }
    l_info("[pad] fixed1 c=%d,%d r=%d  fixed2 c=%d,%d r=%d",
           *(int *) ((char *) self + 0xe8), *(int *) ((char *) self + 0xec),
           *(int *) ((char *) self + 0xf0),
           *(int *) ((char *) self + 0x1a4), *(int *) ((char *) self + 0x1a8),
           *(int *) ((char *) self + 0x1b4));
}

static void (*g_orig_mousedown)(void *, unsigned int, unsigned int,
                                unsigned int, unsigned int);

static void mousedown_hook(void *self, unsigned int x, unsigned int y,
                           unsigned int id, unsigned int ts) {
    l_info("[mouse] DOWN x=%u y=%u id=%u ts=%u nbtn=%d", x, y, id, ts,
           *(int *) ((char *) self + 0x40));
    if (g_orig_mousedown)
        g_orig_mousedown(self, x, y, id, ts);
}

static void (*g_orig_vke)(void *, unsigned int, const void *, const void *,
                          unsigned int);

static void vke_hook(void *self, unsigned int player, const void *action,
                     const void *key, unsigned int value) {
    const char *an = action ? *(const char *const *) action : NULL;
    const char *kn = key ? *(const char *const *) key : NULL;
    l_info("[vke] p=%u action=%s key=%s value=%u", player,
           an ? an : "?", kn ? kn : "?", value);
    if (g_orig_vke)
        g_orig_vke(self, player, action, key, value);
}

static void krminput_hook(void *self, const void *ev) {
    const uint32_t *w = (const uint32_t *) ev;
    l_info("[krm] ev type=%u sub=%u a=%d b=%d c=%d",
           (unsigned) w[0], (unsigned) w[2], (int) w[3], (int) w[4],
           (int) w[5]);
    if (g_orig_krminput)
        g_orig_krminput(self, ev);
}

/* The HUD/fight controls are CTouchScreen virtual buttons registered through
 * AddButton(id, x, y, w, h, key).  Each area is a 48-byte record; pressing it
 * generates a keyboard event with `key`, which is what the fighter listens
 * to.  Mirror the original implementation (it is a handful of stores) so we
 * can log the layout and later reuse `key` from the physical pad. */
static void *g_touch_screen = NULL;

static void ctouch_add_button_hook(void *self, unsigned int id, int x, int y,
                                   int w, int h, int key) {
    l_info("[area] id=%u rect=%d,%d %dx%d key=%d", id, x, y, w, h, key);
    char *b = (char *) self + (size_t) id * 48;
    b[0] = 1;
    *(int *) (b + 0x04) = (int) id;
    *(int *) (b + 0x08) = x;
    *(int *) (b + 0x0C) = y;
    *(int *) (b + 0x10) = w;
    *(int *) (b + 0x14) = x + w;
    *(int *) (b + 0x18) = h;
    *(int *) (b + 0x1C) = y + h;
    *(int *) (b + 0x20) = 0;
    *(int *) (b + 0x2C) = key;
    g_touch_screen = self;
}

static void zp_send_key(void (*on_input)(void *, unsigned int, zevent_words),
                        void *gc, int button, int down) {
    /* Use the engine's gamepad layer (the same one the Xperia Play build
     * uses) instead of calling GameClass::OnInputEvent from this thread:
     * it posts the event to ZSystem's queue, so the state manager processes
     * it on the game thread and we cannot deadlock inside the UI. */
    static int (*gamepad_button)(int, int, int) = NULL;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        gamepad_button = (int (*)(int, int, int))
            so_symbol(&default_mod, "zinpGamepadPushButtonState");
        l_info("[pad] zinpGamepadPushButtonState=%p", (void *) gamepad_button);
    }
    if (gamepad_button) {
        gamepad_button(0, button, down ? 1 : 0);
        return;
    }
    zevent_words ev;
    memset(&ev, 0, sizeof(ev));
    ev.w[1] = (uint32_t) button;
    on_input(gc, down ? ZEV_KEY_DOWN : ZEV_KEY_UP, ev);
}

typedef struct {
    int device;                 /* +0 */
    int flags;                  /* +4: 1=down, 4=move, 2=up */
    unsigned short id;          /* +8 */
    unsigned short pad;         /* +10 */
    float unused_x;             /* +12 */
    float unused_y;             /* +16 */
    /* The engine reads the pointer position from the two floats below
     * (verified through the MouseDown probe: sending x/y in +12/+16 made it
     * receive the pressure/size values as coordinates). */
    float pos_x;                /* +20, centered normalized */
    float pos_y;                /* +24, centered normalized */
} zp_touch_state_t;

static void zp_send_touch(void (*on_input)(void *, unsigned int, zevent_words),
                          void *gc, unsigned int type, int id,
                          float nx, float ny) {
    /* Same engine-level touch queue the Android driver feeds (zaptouch.cpp),
     * so the UI is updated on the game thread. */
    static int (*touch_push)(int, zp_touch_state_t *) = NULL;
    static int tried = 0;
    if (!tried) {
        tried = 1;
        touch_push = (int (*)(int, zp_touch_state_t *))
            so_symbol(&default_mod, "zinpTouchPushState");
        l_info("[touch] zinpTouchPushState=%p", (void *) touch_push);
    }
    if (touch_push) {
        zp_touch_state_t st;
        memset(&st, 0, sizeof(st));
        st.flags = (type == ZEV_TOUCH_DOWN) ? 1
                 : (type == ZEV_TOUCH_MOVE ? 4 : 2);
        st.id = (unsigned short) id;
        st.pos_x = nx;
        st.pos_y = ny;
        touch_push(0, &st);
        return;
    }
    zevent_words ev;
    memset(&ev, 0, sizeof(ev));
    ev.w[1] = (uint32_t) id;
    memcpy(&ev.w[4], &nx, sizeof(nx));
    memcpy(&ev.w[5], &ny, sizeof(ny));
    on_input(gc, type, ev);
}

void brucelee_poll_pad(void) {
    static void *(*get_instance)(void) = NULL;
    static void (*on_input)(void *, unsigned int, zevent_words) = NULL;
    static int touch_down = 0;
    static float last_nx = 0.0f, last_ny = 0.0f;
    static int cross_phase = 0;

    /* Do not touch GameClass before its first Update(): the engine's input
     * thread starts during zpInitializeSystem(), before the game object and
     * its state manager exist. */
    if (!g_game_ready)
        return;

    if (!get_instance) {
        get_instance = (void *) so_symbol(&default_mod, "_ZN9GameClass11GetInstanceEv");
        on_input = (void *) so_symbol(&default_mod, "_ZN9GameClass12OnInputEventEj6ZEVENT");
    }
    static int s_pad_log = 0;
    if (s_pad_log++ < 3)
        l_info("[pad] poll get=%p send=%p", get_instance, on_input);
    if (!get_instance || !on_input)
        return;

    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    if (sceCtrlPeekBufferPositive(0, &pad, 1) < 0)
        return;

    static int s_last_buttons = -1;
    if ((int) pad.buttons != s_last_buttons) {
        l_info("[pad] buttons=0x%08X", (unsigned) pad.buttons);
        s_last_buttons = (int) pad.buttons;
    }

    void *gc = get_instance();
    if (!gc)
        return;

    /* ---- real front touchscreen ---- */
    SceTouchData td;
    memset(&td, 0, sizeof(td));
    int touching = 0;
    if (sceTouchPeek(SCE_TOUCH_PORT_FRONT, &td, 1) > 0 && td.reportNum > 0) {
        touching = 1;
        last_nx = (float) td.report[0].x / 1920.0f - 0.5f;
        last_ny = (float) td.report[0].y / 1088.0f - 0.5f;
        int id = td.report[0].id & 0x7F;
        if (!touch_down) {
            zp_send_touch(on_input, gc, ZEV_TOUCH_DOWN, id, last_nx, last_ny);
            l_info("[touch] down id=%d nx=%.3f ny=%.3f", id, last_nx, last_ny);
            touch_down = 1;
        } else {
            zp_send_touch(on_input, gc, ZEV_TOUCH_MOVE, id, last_nx, last_ny);
        }
    } else if (touch_down) {
        zp_send_touch(on_input, gc, ZEV_TOUCH_UP, 0, last_nx, last_ny);
        l_info("[touch] up");
        touch_down = 0;
    }

    /* ---- the VS screen waits for a touch; tap it automatically so the
     * fight starts with any pad/button workflow (and validates the touch
     * queue we feed through zinpTouchPushState). ---- */
    static int tap_phase = 0, tap_tick = 0;
    if (g_touch_wait) {
        tap_tick++;
        if (tap_phase == 0 && tap_tick > 40) {
            zp_send_touch(on_input, gc, ZEV_TOUCH_DOWN, 0, 0.0f, 0.0f);
            l_info("[autotap] down");
            tap_phase = 1;
            tap_tick = 0;
        } else if (tap_phase == 1 && tap_tick > 6) {
            zp_send_touch(on_input, gc, ZEV_TOUCH_UP, 0, 0.0f, 0.0f);
            l_info("[autotap] up");
            tap_phase = 2;
            tap_tick = 0;
        }
    } else {
        tap_phase = 0;
        tap_tick = 0;
    }

    /* ---- fight buttons -> HUD touch taps ----
     * The fight HUD is touch-only.  Top-right is the punch button and
     * bottom-right the kick button (confirmed on hardware).  Square and
     * Triangle punch; Cross and Circle kick. */
#define PUNCH_NX (830.0f / 960.0f - 0.5f)
#define PUNCH_NY (180.0f / 544.0f - 0.5f)
#define KICK_NX  (830.0f / 960.0f - 0.5f)
#define KICK_NY  (430.0f / 544.0f - 0.5f)
    static int punch_down = 0, kick_down = 0;
    int punch = g_in_fight && !g_paused &&
        (pad.buttons & (SCE_CTRL_SQUARE | SCE_CTRL_TRIANGLE)) != 0;
    int kick = g_in_fight && !g_paused &&
        (pad.buttons & (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE)) != 0;
    if (punch != punch_down) {
        punch_down = punch;
        zp_send_touch(on_input, gc, punch ? ZEV_TOUCH_DOWN : ZEV_TOUCH_UP, 1,
                      PUNCH_NX, PUNCH_NY);
        l_info("[hud] punch %s", punch ? "down" : "up");
    }
    if (kick != kick_down) {
        kick_down = kick;
        zp_send_touch(on_input, gc, kick ? ZEV_TOUCH_DOWN : ZEV_TOUCH_UP, 2,
                      KICK_NX, KICK_NY);
        l_info("[hud] kick %s", kick ? "down" : "up");
    }

    /* ---- left stick / D-pad -> virtual pad touch (fight only) ----
     * The fight's movement is the on-screen fixed pad (centre 160,408 in the
     * 960x544 frame, radius 183).  Drag a touch inside it with either the
     * left analog stick or the D-pad; both feed the same vector so they can
     * even be combined. */
    static int move_down = 0, move_tick = 0;
    static float move_nx = 0.0f, move_ny = 0.0f;
    float dx = ((float) pad.lx - 128.0f) / 127.0f;
    float dy = ((float) pad.ly - 128.0f) / 127.0f;

    /* Ignore the stick's resting drift when the D-pad is being used. */
    if (dx * dx + dy * dy < 0.1225f) {
        dx = 0.0f;
        dy = 0.0f;
    }

    if (pad.buttons & SCE_CTRL_LEFT)  dx -= 1.0f;
    if (pad.buttons & SCE_CTRL_RIGHT) dx += 1.0f;
    if (pad.buttons & SCE_CTRL_UP)    dy -= 1.0f;
    if (pad.buttons & SCE_CTRL_DOWN)  dy += 1.0f;

    /* Keep the combined vector inside the stick's unit circle so diagonal
     * D-pad movement has the same speed as a cardinal direction. */
    float mag2 = dx * dx + dy * dy;
    if (mag2 > 1.0f) {
        float inv = 1.0f / sqrtf(mag2);
        dx *= inv;
        dy *= inv;
        mag2 = 1.0f;
    }

    if (g_in_fight && !g_paused && mag2 > 0.1225f) {   /* deadzone 0.35 */
        float tx = 160.0f + dx * 120.0f;
        float ty = 408.0f + dy * 120.0f;
        move_nx = tx / 960.0f - 0.5f;
        move_ny = ty / 544.0f - 0.5f;
        if (!move_down) {
            zp_send_touch(on_input, gc, ZEV_TOUCH_DOWN, 3, move_nx, move_ny);
            move_down = 1;
            move_tick = 0;
            l_info("[pad] move down %.2f,%.2f", dx, dy);
        } else if (++move_tick >= 3) {
            zp_send_touch(on_input, gc, ZEV_TOUCH_MOVE, 3, move_nx, move_ny);
            move_tick = 0;
        }
    } else if (move_down) {
        zp_send_touch(on_input, gc, ZEV_TOUCH_UP, 3, move_nx, move_ny);
        move_down = 0;
        l_info("[pad] move up");
    }

    /* ---- physical pad -> engine gamepad keys (menus) ---- */
    for (unsigned int i = 0; i < sizeof(k_pad_map) / sizeof(k_pad_map[0]); i++) {
        int pressed = (pad.buttons & k_pad_map[i].vita) != 0;
        if (pressed != g_key_down[i]) {
            g_key_down[i] = pressed;
            zp_send_key(on_input, gc, k_pad_map[i].zp, pressed);
            l_info("[pad] zp-button %d %s", k_pad_map[i].zp,
                   pressed ? "down" : "up");
        }
    }

    /* Menu face buttons: Cross=A, Circle=B, Square=X, Triangle=Y.  In a
     * fight they are attack buttons instead (handled above). */
    static int menu_down[4] = { 0, 0, 0, 0 };
    static const struct { unsigned int vita; int zp; } menu_map[4] = {
        { SCE_CTRL_CROSS, ZP_BTN_A },
        { SCE_CTRL_CIRCLE, ZP_BTN_B },
        { SCE_CTRL_SQUARE, ZP_BTN_X },
        { SCE_CTRL_TRIANGLE, ZP_BTN_Y },
    };
    for (int i = 0; i < 4; i++) {
        int pressed = !g_in_fight && (pad.buttons & menu_map[i].vita) != 0;
        if (pressed != menu_down[i]) {
            menu_down[i] = pressed;
            zp_send_key(on_input, gc, menu_map[i].zp, pressed);
            l_info("[pad] menu-button %d %s", menu_map[i].zp,
                   pressed ? "down" : "up");
        }
    }
    /* ---- pause menu: Cross = confirm, Circle = back ----
     * gui/pause.ui is a ZSystem menu and confirms with ZP_BTN_SELECT (9),
     * while the main menu confirms with A (4).  Translate Cross to Select
     * only while paused; also translate Circle to B for "back".
     *
     * Fight HUD touches/attacks are disabled above while g_paused, so a held
     * stick/button cannot click pause-menu items.  Initialize the edge state
     * from the current buttons so pausing while holding Cross does not
     * instantly confirm "Resume". */
    static int prev_paused = 0;
    static int pause_cross_down = 0, pause_circle_down = 0;
    static int pause_cross_sent = 0, pause_circle_sent = 0;
    static int pause_resume_tap = 0;

    if (g_paused != prev_paused) {
        if (g_paused) {
            pause_cross_down = (pad.buttons & SCE_CTRL_CROSS) != 0;
            pause_circle_down = (pad.buttons & SCE_CTRL_CIRCLE) != 0;
            pause_cross_sent = pause_circle_sent = 0;
            pause_resume_tap = 0;
            l_info("[pad] pause menu: Cross->Select, Circle->%s",
                   g_pause_main ? "RESUME tap" : "B");
        } else {
            if (pause_cross_sent) {
                zp_send_key(on_input, gc, ZP_BTN_SELECT, 0);
                pause_cross_sent = 0;
            }
            if (pause_circle_sent) {
                zp_send_key(on_input, gc, ZP_BTN_B, 0);
                pause_circle_sent = 0;
            }
            if (pause_resume_tap > 0) {
                zp_send_touch(on_input, gc, ZEV_TOUCH_UP, 1,
                              PUNCH_NX, PUNCH_NY);
                pause_resume_tap = 0;
            }
            pause_cross_down = pause_circle_down = 0;
        }
        prev_paused = g_paused;
    }

    if (g_paused) {
        int cx = (pad.buttons & SCE_CTRL_CROSS) != 0;
        int ci = (pad.buttons & SCE_CTRL_CIRCLE) != 0;

        if (cx != pause_cross_down) {
            pause_cross_down = cx;
            if (cx) {
                zp_send_key(on_input, gc, ZP_BTN_SELECT, 1);
                pause_cross_sent = 1;
            } else if (pause_cross_sent) {
                zp_send_key(on_input, gc, ZP_BTN_SELECT, 0);
                pause_cross_sent = 0;
            }
            l_info("[pad] pause cross->select %s", cx ? "down" : "up");
        }
        if (ci != pause_circle_down) {
            pause_circle_down = ci;
            if (g_pause_main) {
                /* No back key on the root pause screen: send a one-poll tap
                 * on RESUME, which closes the pause.  Down+up together avoid
                 * leaving a synthetic touch held if the UI resumes on down. */
                if (ci) {
                    zp_send_touch(on_input, gc, ZEV_TOUCH_DOWN, 1,
                                  PUNCH_NX, PUNCH_NY);
                    pause_resume_tap = 6;   /* ~100 ms at 60 fps */
                    l_info("[pad] pause circle->RESUME tap down");
                }
            } else {
                if (ci) {
                    zp_send_key(on_input, gc, ZP_BTN_B, 1);
                    pause_circle_sent = 1;
                } else if (pause_circle_sent) {
                    zp_send_key(on_input, gc, ZP_BTN_B, 0);
                    pause_circle_sent = 0;
                }
                l_info("[pad] pause circle->B %s", ci ? "down" : "up");
            }
        }
    }

    if (pause_resume_tap > 0) {
        if (--pause_resume_tap == 0) {
            zp_send_touch(on_input, gc, ZEV_TOUCH_UP, 1,
                          PUNCH_NX, PUNCH_NY);
            l_info("[pad] pause circle->RESUME tap up");
        }
    }

    static int start_down = 0;
    int stbtn = (pad.buttons & SCE_CTRL_START) != 0;
    if (stbtn != start_down) {
        start_down = stbtn;
        zp_send_key(on_input, gc, ZP_BTN_B, stbtn);
        l_info("[pad] start->pause %s", stbtn ? "down" : "up");
    }
}

/* CZdkDisplay zeroes its ZPDISPLAYSETUP before calling zpgfxDisplaySetup(),
 * so the first two ints (width, height) are 0 and the whole renderer starts
 * at <0,0>.  Fill them in before the original function copies the struct. */
static void zpgfx_setup_fix(void *setup) {
    int *p = (int *) setup;
    p[0] = 960;
    p[1] = 544;
}

__attribute__((naked, target("arm")))
static void zpgfx_display_setup_hook(void) {
    __asm__ volatile(
        "push {r0, lr}\n"
        "bl zpgfx_setup_fix\n"
        "pop {r0, lr}\n"
        /* emulate the two instructions overwritten by hook_addr:
         *   push {r4, r5, r6, r7, r8, lr}
         *   ldr  r4, [pc, #124]   ; -> zplatform's l_bDisplaySetup
         * and continue at +0xC with the final r4 already computed. */
        "push {r4, r5, r6, r7, r8, lr}\n"
        "ldr r4, =0x982b6004\n"
        "ldr pc, =0x9806b26c\n"
    );
}

/* zpOutputDebugString() is ZPlatform's own log sink.  It is gated behind a
 * debug mask and writes through fwrite(3), so the engine's messages never
 * reach our log.  Hooking it gives us the engine's own diagnostics. */
static void zp_debug_hook(const char *s) {
    if (!s || !*s)
        return;
    l_info("[zsys] %s", s);
    if (strstr(s, "Entering state: gameloadtouchwait")) {
        g_touch_wait = 1;
        l_info("[touch] gameloadtouchwait: auto-tapping to start the fight");
    } else if (strstr(s, "Entering state:")) {
        g_touch_wait = 0;
    }
    if (strstr(s, "[STATE] Entering state:")) {
        g_in_fight = strstr(s, "Entering state: Game") != NULL;
        g_pause_main = strstr(s, "Entering state: GamePause") != NULL;
        g_paused = g_pause_main ||
                   strstr(s, "Entering state: ingamesettingsmain") != NULL ||
                   strstr(s, "Entering state: confirmbacktomainmenu") != NULL;
        l_info("[pad] state: fight=%d paused=%d main=%d",
               g_in_fight, g_paused, g_pause_main);
    }
}

static void install_hook(const char *name, void *dst) {
    uintptr_t addr = so_symbol(&zplatform_mod, name);
    if (!addr)
        addr = so_symbol(&default_mod, name);
    if (!addr) {
        l_warn("hook: symbol not found: %s", name);
        return;
    }
    hook_addr(addr, (uintptr_t)dst);
    l_info("hook: %s @ %p -> %p", name, (void *)addr, dst);
}

void brucelee_install_hooks(void) {
    /* Avoid the Android Java input/audio driver bodies during the first bring-up.
     * They are not needed to reach the engine's first frame and can block on
     * JNI objects we do not emulate yet. */
    install_hook("zinpControllerConnect", stub_ret0);
    install_hook("zinpControllerDeviceEnable", stub_ret0);
    install_hook("zinpControllerDeviceDisable", stub_ret0);
    install_hook("zinpControllerRegisterCallbacks", stub_ret0);
    install_hook("zinpControllerUnregisterCallbacks", stub_ret0);

    /* ZSystem's "service" layer talks to the Android Java ZService object for
     * licensing/entitlement.  That service does not exist outside Android and
     * zpInitializeSystem() aborts the whole boot when it fails, so report it as
     * initialised (the game data is provided by the user anyway). */
    install_hook("_zpServiceInitialize", stub_ret0);

    /* Engine diagnostics (zsysOutputDebugString -> zcPrintf -> here). */
    install_hook("zpOutputDebugString", (void *)zp_debug_hook);

    /* The Android clipboard driver spawns threads and creates handles that
     * abort inside zcHandleCreate() on this platform.  Clipboard text is not
     * needed to boot the game, so skip the whole driver. */
    install_hook("_zpClipboardInitialize", stub_ret0);

    /* Reading the persistent-storage blob opens a file through the handle
     * manager and aborts on this platform; the game boots fine with an empty
     * profile (it recreates the file on the first save). */
    install_hook("_zsysPersistentStorageReadData", stub_ret_no_storage);

    /* The engine's CZdkDisplay gets its initial <width,height> from the fake
     * Android window; force the Vita panel size. */
    install_hook("_zpWindowGetSize", (void *)zp_window_get_size_hook);
    install_hook("_ZNK3krm3sal11CZdkDisplay8GetWidthEv", (void *)czdk_get_width_hook);
    install_hook("_ZNK3krm3sal11CZdkDisplay9GetHeightEv", (void *)czdk_get_height_hook);
    install_hook("_ZN3krm3sal11CZdkDisplay13FindVideoModeEjjNS0_8IDisplay12EPixelFormatE",
                 (void *)czdk_find_video_mode_hook);
    install_hook("_ZN3krm3sal11CZdkDisplay16GetVideoModeInfoEiRiS2_RNS0_8IDisplay12EPixelFormatE",
                 (void *)czdk_get_video_mode_info_hook);
    install_hook("_ZN3krm3sal11CZdkDisplay17GetVideoModeCountEv",
                 (void *)czdk_get_video_mode_count_hook);
    install_hook("zpgfxDisplayIsValid", (void *)zp_display_is_valid_hook);

    /* ARM bionic's kernel-helper atomics are unusable on Vita. */
    install_hook("__zcatomic_dec", (void *)zc_atomic_dec_hook);
    install_hook("__zcatomic_inc", (void *)zc_atomic_inc_hook);
    install_hook("__zcatomic_cmpxchg", (void *)zc_atomic_cmpxchg_hook);
    install_hook("__zcatomic_xchgadd", (void *)zc_atomic_xchgadd_hook);
}

static int load_one(so_module *mod, const char *path, uint32_t addr) {
    if (!file_exists(path)) {
        l_fatal("missing shared object: %s", path);
        fatal_error("Arquivo nao encontrado:\n%s\n\nCopie os dados do jogo para ux0:data/brucelee/.", path);
        return -1;
    }

    int res = so_file_load(mod, path, addr);
    if (res < 0) {
        l_fatal("so_file_load failed for %s: %d", path, res);
        fatal_error("Falha ao carregar %s", path);
        return -1;
    }
    l_success("loaded %s @ %p", path, (void *)mod->text_base);
    return 0;
}

int brucelee_load_modules(void) {
    if (load_one(&zplatform_mod, ZPLATFORM_SO_PATH, ZPLATFORM_LOAD_ADDR) < 0)
        return -1;
    if (load_one(&default_mod, DEFAULT_SO_PATH, DEFAULT_LOAD_ADDR) < 0)
        return -1;

    if (so_relocate(&zplatform_mod) < 0 || so_relocate(&default_mod) < 0) {
        l_fatal("SO relocation failed");
        return -1;
    }
    l_success("SOs relocated");

    /* Resolve zplatform first, then the game.  so_resolve() also looks up
     * symbols exported by already-loaded modules, which is how libdefault's
     * zp, zc, zpgfx, zpinp and zpadr imports are connected to libzplatform. */
    resolve_imports(&zplatform_mod);
    l_success("zplatform imports resolved");
    resolve_imports(&default_mod);
    l_success("default imports resolved");

    so_flush_caches(&zplatform_mod);
    so_flush_caches(&default_mod);
    l_success("SO caches flushed");

    patch_eula_check();

    // Install the fake Android activity and input shims before any static
    // constructor runs: ZPlatform constructors may query the activity.
    brucelee_setup_activity();
    brucelee_install_hooks();
    so_flush_caches(&zplatform_mod);

    so_initialize(&zplatform_mod);
    l_success("zplatform constructors done");

    /* zcHandleCreate() dereferences the handle-node table unconditionally.
     * That table is built by "global constructors keyed to zchandle.cpp"; if
     * the constructor bailed out (e.g. its global-memory allocation failed at
     * boot time) every handle creation crashes with r1 = NULL.  Re-run it and
     * report the state so the log tells us what happened. */
    {
        uintptr_t base = zplatform_mod.text_base;
        volatile uint32_t *h = (volatile uint32_t *)(base + 0x2bbcc4);
        uintptr_t ctor = base + 0xbeea0;   /* global constructors keyed to zchandle.cpp */
        l_info("zchandle: pre  len=%08x array=%08x nextFree=%08x",
               (unsigned)h[0], (unsigned)h[3], (unsigned)h[10]);
        if (h[10] == 0) {
            void (*fn)(void) = (void (*)(void))ctor;
            fn();
            l_info("zchandle: re-ran constructor -> nextFree=%08x array=%08x",
                   (unsigned)h[10], (unsigned)h[3]);
        }
    }

    so_initialize(&default_mod);
    l_success("default constructors done");

    return 0;
}

/* ------------------------------------------------------------------------- */
/* dlopen()/dlsym() replacements used by libjava-activity and libzplatform.  */
/* ------------------------------------------------------------------------- */

static void *ANativeWindow_fromSurface(void *env, void *surface) {
    (void)env; (void)surface;
    return (void *)(uintptr_t)0x60000001;
}
static int ANativeWindow_getWidth(void *window) {
    (void)window; return 960;
}
static int ANativeWindow_getHeight(void *window) {
    (void)window; return 544;
}
static int ANativeWindow_getFormat(void *window) {
    (void)window; return 1; /* WINDOW_FORMAT_RGBA_8888 */
}
static int ANativeWindow_setBuffersGeometry(void *window, int w, int h, int f) {
    (void)window; (void)w; (void)h; (void)f; return 0;
}

/* The Android looper blocks for the requested timeout.  Returning immediately
 * turns the engine's input thread into a busy loop that pegs a core. */
static int looper_poll_stub(int timeoutMillis, int *outFd, int *outEvents,
                            void **outData) {
    if (outFd) *outFd = -1;
    if (outEvents) *outEvents = 0;
    if (outData) *outData = NULL;
    int ms = timeoutMillis;
    if (ms < 0) ms = 16;          /* "block forever": poll at ~60 Hz */
    if (ms > 250) ms = 250;       /* never block the worker for long */
    sceKernelDelayThread((SceUInt)(ms * 1000));
    return 0;                     /* no file descriptor ready */
}

void *brucelee_dlopen(const char *path, int flags) {
    (void)flags;
    if (!path) return NULL;

    l_info("dlopen(%s)", path);

    if (strstr(path, "zplatform"))
        return zplatform_handle;
    if (strstr(path, "libdefault") || strstr(path, "libgame"))
        return default_handle;
    if (strstr(path, "libandroid"))
        return android_handle;
    if (strstr(path, "OpenSLES") || strstr(path, "libOpenSLES"))
        return opensles_handle;
    if (strstr(path, "crypto") || strstr(path, "zcryptoplugin"))
        return (void *)(uintptr_t)0x5A000005;

    l_warn("dlopen: unknown library %s", path);
    return NULL;
}

void *brucelee_dlsym(void *handle, const char *symbol) {
    if (!symbol) return NULL;

    if (handle == zplatform_handle) {
        uintptr_t p = so_symbol(&zplatform_mod, symbol);
        if (!p) l_warn("dlsym(zplatform, %s): not found", symbol);
        return (void *)p;
    }
    if (handle == default_handle) {
        uintptr_t p = so_symbol(&default_mod, symbol);
        if (!p) l_warn("dlsym(default, %s): not found", symbol);
        return (void *)p;
    }

    if (handle == android_handle) {
        if (!strcmp(symbol, "ANativeWindow_fromSurface")) return (void *)&ANativeWindow_fromSurface;
        if (!strcmp(symbol, "ANativeWindow_getWidth")) return (void *)&ANativeWindow_getWidth;
        if (!strcmp(symbol, "ANativeWindow_getHeight")) return (void *)&ANativeWindow_getHeight;
        if (!strcmp(symbol, "ANativeWindow_getFormat")) return (void *)&ANativeWindow_getFormat;
        if (!strcmp(symbol, "ANativeWindow_setBuffersGeometry")) return (void *)&ANativeWindow_setBuffersGeometry;
        if (!strcmp(symbol, "ALooper_pollAll") || !strcmp(symbol, "ALooper_pollOnce")) {
            l_info("dlsym(android, %s): blocking stub", symbol);
            return (void *)&looper_poll_stub;
        }
        l_warn("dlsym(android, %s): stubbed", symbol);
        return (void *)&stub_ret0;
    }

    if (handle == opensles_handle || handle == (void *)(uintptr_t)0x5A000005) {
        l_warn("dlsym(opensles/crypto, %s): stubbed", symbol);
        return (void *)&stub_ret0;
    }

    /* RTLD_DEFAULT: prefer the game, then the platform, then Android glue. */
    uintptr_t p = so_symbol(&default_mod, symbol);
    if (p) return (void *)p;
    p = so_symbol(&zplatform_mod, symbol);
    if (p) return (void *)p;

    l_warn("dlsym(%p, %s): not found", handle, symbol);
    return NULL;
}

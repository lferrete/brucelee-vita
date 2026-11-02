/*
 * Bruce Lee: Dragon Warrior - PS Vita port
 * Port code Copyright (C) 2026 Leandro Ferrete.
 * The original game and its assets remain property of their owners; see
 * AUTHORS.md and LICENSE for credits and third-party licenses.
 */

#include "loader.h"
#include "java_shim.h"
#include "port_author.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <psp2/appmgr.h>
#include <psp2/apputil.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/touch.h>
#include <psp2/ctrl.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include "utils/breadcrumb.h"
#include "utils/dialog.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/perf.h"
#include "utils/watchdog.h"

int _newlib_heap_size_user = 256 * 1024 * 1024;

static void init_platform(void) {
    sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});

    /* Keep the console awake while the game runs: the default suspend timer
     * otherwise parks the process mid-load and looks like a freeze. */
    sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
    sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_OLED_OFF);

    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    sceKernelChangeThreadPriority(0, 127);
    sceKernelChangeThreadCpuAffinityMask(0, SCE_KERNEL_CPU_MASK_USER_2);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    _log_print(LT_INFO, "==============================================");
    _log_print(LT_INFO, " Bruce Lee: Dragon Warrior - PS Vita loader");
    _log_print(LT_INFO, " Port by %s (v%s)", PORT_AUTHOR, PORT_VERSION);
    _log_print(LT_INFO, " %s", PORT_AUTHOR_COPYRIGHT);
    _log_print(LT_INFO, " Build: " __DATE__ " " __TIME__);
    _log_print(LT_INFO, " ZSystem/KRM bring-up build");
    _log_print(LT_INFO, "==============================================");

    init_platform();

    sceIoMkdir(DATA_PATH "logs", 0777);
    sceIoMkdir(DATA_PATH "shader_cache", 0777);

    if (chdir(DATA_PATH "content") != 0)
        _log_print(LT_WARN, "chdir(%scontent) failed", DATA_PATH);
    else
        _log_print(LT_INFO, "cwd: %scontent", DATA_PATH);

    /* The game is touch-driven; forward the Vita front panel to it, and read
     * the analog sticks (used as the fight's virtual pad). */
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    jni_init();
    java_shim_init();

    gl_preload();
    gl_init();
    l_success("vitaGL initialized");

    if (brucelee_load_modules() < 0) {
        l_fatal("module loading failed");
        fatal_error("Falha ao carregar as bibliotecas do jogo.\n"
                    "Verifique ux0:data/brucelee/logs/.");
        sceKernelExitProcess(0);
    }

    // Re-install in case a static constructor reset the ZPlatform activity slot.
    brucelee_setup_activity();

    // Fill the ANativeActivity callbacks / input driver tables exactly like
    // libjava-activity.so does on Android.
    brucelee_android_entry();

    bc_set_main_tid(sceKernelGetThreadId());
    perf_init();
    watchdog_start();

    void *entry = (void *)so_symbol(&default_mod, "ZInternalMain");
    if (!entry) {
        l_fatal("ZInternalMain not exported by libdefault.so");
        fatal_error("Simbolo ZInternalMain nao encontrado.");
        sceKernelExitProcess(0);
    }

    l_success("calling ZInternalMain @ %p", entry);
    watchdog_mark("ZInternalMain", 0);

    int (*ZInternalMain)(int, char **) = entry;
    /* ZSystem expects the Android command line: "<exe> -c <content root>".
     * zpInitializeSystem() parses "-c" and mounts the directory; without it
     * the engine aborts the boot and tears everything down. */
    char *game_argv[] = { "brucelee", "-c", "ux0:data/brucelee", NULL };
    int ret = ZInternalMain(3, game_argv);

    l_error("ZInternalMain returned %d", ret);
    watchdog_mark("ZInternalMain returned", ret);

    /* If the engine returns, keep the process alive long enough for the log to
     * be flushed and for the user to read the on-screen dialog. */
    sceKernelDelayThread(3 * 1000 * 1000);
    sceKernelExitProcess(0);
    return 0;
}

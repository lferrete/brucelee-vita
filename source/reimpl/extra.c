/*
 * Small Android libc/libm/libEGL gaps that libzplatform imports but newlib
 * does not provide (or that we deliberately stub for the Vita environment).
 */

#include <setjmp.h>
#include <stdint.h>
#include <string.h>

int alarm_stub(unsigned seconds) { (void)seconds; return 0; }
void *getpwuid_stub(unsigned uid) { (void)uid; return NULL; }

int sched_getparam_stub(int pid, void *param) { (void)pid; (void)param; return -1; }
int sched_getscheduler_stub(int pid) { (void)pid; return 0; }
int sched_rr_get_interval_stub(int pid, void *interval) { (void)pid; (void)interval; return -1; }
int sched_setparam_stub(int pid, const void *param) { (void)pid; (void)param; return 0; }
int sched_setscheduler_stub(int pid, int policy, const void *param) {
    (void)pid; (void)policy; (void)param; return 0;
}

/* ZPlatform uses sigsetjmp for signal guards, but newlib only exposes
 * setjmp/longjmp.  The buffers it passes are at least jmp_buf-sized. */
int sigsetjmp_stub(jmp_buf env, int savesigs) {
    (void)savesigs;
    return setjmp(env);
}
void siglongjmp_stub(jmp_buf env, int val) {
    longjmp(env, val);
}

int statfs_stub(const char *path, void *buf) {
    (void)path; (void)buf;
    return -1;
}

/* vitaGL does not implement the GLES2 query; the KRM engine only uses it for
 * diagnostics. */
void glGetAttachedShaders_stub(unsigned program, int maxCount, int *count, unsigned *shaders) {
    (void)program; (void)maxCount; (void)shaders;
    if (count) *count = 0;
}

/* Minimal EGL surface/display queries. */
void *eglGetCurrentDisplay_stub(void) { return NULL; }
void *eglGetCurrentSurface_stub(int readdraw) { (void)readdraw; return NULL; }
unsigned eglSwapInterval_stub(void *dpy, int interval) { (void)dpy; (void)interval; return 1; }

#ifndef BRUCELEE_EXTRA_H
#define BRUCELEE_EXTRA_H

#include <setjmp.h>

int alarm_stub(unsigned seconds);
void *getpwuid_stub(unsigned uid);

int sched_getparam_stub(int pid, void *param);
int sched_getscheduler_stub(int pid);
int sched_rr_get_interval_stub(int pid, void *interval);
int sched_setparam_stub(int pid, const void *param);
int sched_setscheduler_stub(int pid, int policy, const void *param);

int sigsetjmp_stub(jmp_buf env, int savesigs);
void siglongjmp_stub(jmp_buf env, int val);
int statfs_stub(const char *path, void *buf);

void glGetAttachedShaders_stub(unsigned program, int maxCount, int *count, unsigned *shaders);

void *eglGetCurrentDisplay_stub(void);
void *eglGetCurrentSurface_stub(int readdraw);
unsigned eglSwapInterval_stub(void *dpy, int interval);

#endif

/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022      GrapheneCt
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/pthr.h"

#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h>
#include <stdatomic.h>

#include "utils/breadcrumb.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "utils/watchdog.h"


#define BIONIC_PTHREAD_COND_INITIALIZER              0
#define PTHR_MUTEX_ALLOC 64
#define PTHR_COND_ALLOC  64

#define BIONIC_PTHREAD_MUTEX_INITIALIZER             0
#define BIONIC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER   0x4000
#define BIONIC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER  0x8000

enum {
    BIONIC_PTHREAD_MUTEX_NORMAL = 0,
    BIONIC_PTHREAD_MUTEX_RECURSIVE = 1,
    BIONIC_PTHREAD_MUTEX_ERRORCHECK = 2,

    BIONIC_PTHREAD_MUTEX_ERRORCHECK_NP = BIONIC_PTHREAD_MUTEX_ERRORCHECK,
    BIONIC_PTHREAD_MUTEX_RECURSIVE_NP  = BIONIC_PTHREAD_MUTEX_RECURSIVE,

    BIONIC_PTHREAD_MUTEX_DEFAULT = BIONIC_PTHREAD_MUTEX_NORMAL
};

#define PTHR_INLINE static inline __attribute__((always_inline))

static SceKernelLwMutexWork pthr_mutex;
static volatile short int pthr_mutex_inited = 0;

#define PTHR_LOCK \
    if (!pthr_mutex_inited) { \
        int ret = sceKernelCreateLwMutex(&pthr_mutex, "log_lock", 0, 0, NULL); \
        if (ret < 0) { \
            sceClibPrintf("Error: failed to create pthr mutex: 0x%x\n", ret); \
            return 0; \
        } \
        pthr_mutex_inited = 1; \
    } \
    sceKernelLockLwMutex(&pthr_mutex, 1, NULL);

#define PTHR_UNLOCK \
    if (pthr_mutex_inited) { \
        sceKernelUnlockLwMutex(&pthr_mutex, 1); \
    }

/*
 * Registro de mutex/cond ya inicializados (los de Bionic son 4 bytes que estas
 * funciones reemplazan por un puntero al objeto real; los estaticos llegan en 0 y se
 * inicializan en el primer uso).
 *
 * Antes era un array lineal de 1024 punteros recorrido ENTERO bajo un LwMutex global
 * en CADA pthread_mutex_lock/cond_wait del motor: miles de veces por frame, cada una
 * con hasta 1024 comparaciones y dos llamadas al kernel. Ahora es una tabla hash con
 * sondeo lineal cuya LECTURA no toma lock (palabras alineadas, atomicas en ARM); solo
 * alta/baja van bajo el LwMutex. Mismas semanticas que el array: "inicializado" =
 * puntero presente en la tabla. Las bajas dejan una lapida; cuando las lapidas se
 * acumulan se reconstruye en la OTRA tabla y se publica con un solo store, asi un
 * lector que todavia recorre la vieja ve un estado valido.
 */
#define PTHR_HASH_SIZE 4096u              // potencia de 2; carga maxima 50%
#define PTHR_HASH_MASK (PTHR_HASH_SIZE - 1u)
#define PTHR_TOMBSTONE ((void *) 1)

static void *pthr_table_a[PTHR_HASH_SIZE];
static void *pthr_table_b[PTHR_HASH_SIZE];
static void ** volatile pthr_table = pthr_table_a;
static unsigned pthr_used = 0;            // vivos + lapidas en la tabla actual
static unsigned pthr_live = 0;

static inline unsigned pthr_hash(const void *p) {
    return ((uint32_t)(uintptr_t) p * 2654435761u) >> 20; // 12 bits altos
}

int isObjectInitialized(const void * mut) {
    void **t = pthr_table;
    for (unsigned n = 0, i = pthr_hash(mut); n < PTHR_HASH_SIZE; ++n, i = (i + 1) & PTHR_HASH_MASK) {
        void *v = __atomic_load_n(&t[i], __ATOMIC_ACQUIRE);
        if (v == mut) return 1;
        if (v == NULL) return 0;
    }
    return 0;
}

// Bajo PTHR_LOCK. Reconstruye sin lapidas en la tabla que no esta en uso.
static void pthr_rehash_locked(void) {
    void **old = pthr_table;
    void **nt = (old == pthr_table_a) ? pthr_table_b : pthr_table_a;
    memset(nt, 0, sizeof(pthr_table_a));
    unsigned live = 0;
    for (unsigned j = 0; j < PTHR_HASH_SIZE; ++j) {
        void *v = old[j];
        if (v == NULL || v == PTHR_TOMBSTONE) continue;
        unsigned i = pthr_hash(v);
        while (nt[i]) i = (i + 1) & PTHR_HASH_MASK;
        nt[i] = v;
        live++;
    }
    __atomic_store_n(&pthr_table, nt, __ATOMIC_RELEASE);
    pthr_used = pthr_live = live;
}

int rememberObject(void * mut) {
    PTHR_LOCK
    if (pthr_live >= PTHR_HASH_SIZE / 2) {
        PTHR_UNLOCK
        return 0;
    }
    if (pthr_used >= PTHR_HASH_SIZE / 2)
        pthr_rehash_locked();
    void **t = pthr_table;
    unsigned i = pthr_hash(mut);
    while (t[i] != NULL && t[i] != PTHR_TOMBSTONE && t[i] != mut)
        i = (i + 1) & PTHR_HASH_MASK;
    if (t[i] != mut) {
        if (t[i] == NULL) pthr_used++;
        __atomic_store_n(&t[i], mut, __ATOMIC_RELEASE);
        pthr_live++;
    }
    PTHR_UNLOCK
    return 1;
}

int forgetObject(const void * mut) {
    PTHR_LOCK
    void **t = pthr_table;
    for (unsigned n = 0, i = pthr_hash(mut); n < PTHR_HASH_SIZE; ++n, i = (i + 1) & PTHR_HASH_MASK) {
        if (t[i] == mut) {
            __atomic_store_n(&t[i], PTHR_TOMBSTONE, __ATOMIC_RELEASE);
            pthr_live--;
            PTHR_UNLOCK
            return 1;
        }
        if (t[i] == NULL) break;
    }
    PTHR_UNLOCK
    return 0;
}

// null check for `attr` must be performed before this
PTHR_INLINE int _attr_t_static_init(pthread_attr_t_bionic * attr) {
    if (attr->magic != 0x42424242) {
        attr->magic = 0x42424242;
        attr->real_ptr = malloc(sizeof(pthread_attr_t));
        return pthread_attr_init(attr->real_ptr);
    }
    return 0;
}

// null check for `mutex` param must be performed before this, `attr` is fine as null
PTHR_INLINE int _mutex_t_static_init(pthread_mutex_t_bionic * mutex, const pthread_mutexattr_t * attr) {
    int ret = 0, kind = PTHREAD_MUTEX_NORMAL;

    if (isObjectInitialized(mutex)) {
        //logv_debug("mutex already initialized: %p", mutex);
        return ret;
    }

    if (attr) {
        pthread_mutexattr_gettype((pthread_mutexattr_t *) attr, &kind);
    } else {
        if (* (int *) mutex == BIONIC_PTHREAD_MUTEX_INITIALIZER) kind = PTHREAD_MUTEX_NORMAL;
        else if (* (int *) mutex == BIONIC_PTHREAD_RECURSIVE_MUTEX_INITIALIZER) kind = PTHREAD_MUTEX_RECURSIVE;
        else if (* (int *) mutex == BIONIC_PTHREAD_ERRORCHECK_MUTEX_INITIALIZER) kind = PTHREAD_MUTEX_ERRORCHECK;
    }

    /* sizeof(pthread_mutex_t) is only 4 in this newlib, but the object the
     * implementation actually fills in is larger, so hand it a padded block:
     * writing past a 4-byte allocation corrupted the heap and aborted later
     * inside malloc()/free(). */
    pthread_mutex_t * mut = malloc(PTHR_MUTEX_ALLOC);
    if (!mut) {
        l_error("out of memory initializing mutex %p", mutex);
        return ENOMEM;
    }
    memset(mut, 0, PTHR_MUTEX_ALLOC);
    mutex->real_ptr = mut;

    l_info("[mtx] init %p kind=%d", mutex, kind);
    if (kind == PTHREAD_MUTEX_NORMAL) {
        /* No attribute object at all: newlib allocates/frees one internally
         * and that path has been aborting on this platform. */
        ret = pthread_mutex_init(mut, NULL);
    } else {
        pthread_mutexattr_t mutattr;
        pthread_mutexattr_init(&mutattr);
        pthread_mutexattr_settype(&mutattr, kind);
        ret = pthread_mutex_init(mut, &mutattr);
        pthread_mutexattr_destroy(&mutattr);
    }
    l_info("[mtx] init %p -> %d", mutex, ret);

    if (ret == 0) {
        rememberObject(mutex);
    } else {
        l_error("mutex initialization for %p has failed", mutex);
    }

    return ret;
}

// null check for `cond` param must be performed before this, `attr` is fine as null
PTHR_INLINE int _cond_t_static_init(pthread_cond_t_bionic * cond, const pthread_condattr_t * attr) {
    int ret = 0;

    if (isObjectInitialized(cond)) {
        //logv_debug("cond already initialized: %p", cond);
        return ret;
    }

    pthread_cond_t * c = malloc(PTHR_COND_ALLOC);
    if (!c) {
        l_error("out of memory initializing cond %p", cond);
        return ENOMEM;
    }
    memset(c, 0, PTHR_COND_ALLOC);
    cond->real_ptr = c;

    ret = pthread_cond_init(cond->real_ptr, attr);

    if (ret == 0) {
        rememberObject(cond);
    } else {
        l_error("cond initialization for %p has failed", cond);
    }

    return ret;
}

/* ------------------------------------------------------------------------- */
/* Real Vita threads, without newlib pthreads.                                */
/*                                                                            */
/* newlib's pthread_create()/pthread_mutex_init() end up in                   */
/* pte_osSemaphoreCreate() -> sceKernelCreateSema("", ...) which is where the */
/* boot dies inside SceLibKernel.  The engine creates its workers through     */
/* pthread_create(), so hand those straight to sceKernelCreateThread() and    */
/* keep pthread_t == SceUID (which is what VitaSDK's own pthreads do too).    */
/* ------------------------------------------------------------------------- */
#define PTHR_ATTR_MAGIC 0x42424242

typedef struct {
    void *(*start)(void *);
    void *param;
} soloader_thread_param;

static int soloader_thread_entry(SceSize args, void *argp)
{
    soloader_thread_param p = { NULL, NULL };
    if (argp && args >= sizeof(p))
        memcpy(&p, argp, sizeof(p));
    l_info("[pthr] entry tid=0x%08X start=%p", (unsigned) sceKernelGetThreadId(), p.start);
    if (p.start)
        p.start(p.param);
    l_info("[pthr] entry done tid=0x%08X", (unsigned) sceKernelGetThreadId());
    return 0;
}

int pthread_create_soloader(pthread_t *thread, const pthread_attr_t_bionic *attr,
                            void *(*start)(void *), void *param) {
    if (!thread || !start)
        return EINVAL;
    *thread = 0;

    SceSize stack_size = 512 * 1024;
    int priority = 0xA0; /* Vita default (160) */

    if (attr && attr->magic == PTHR_ATTR_MAGIC) {
        if (attr->stack_size >= 0x4000)
            stack_size = (SceSize) attr->stack_size;
        if (attr->sched_priority)
            priority = 0xA0 - attr->sched_priority;
    }
    if (priority < 1)   priority = 1;
    if (priority > 0xBF) priority = 0xBF;

    l_info("[pthr] create start=%p attr=%p stack=%u prio=%d",
           start, attr, (unsigned) stack_size, priority);

    soloader_thread_param p = { start, param };
    SceUID thid = sceKernelCreateThread("soloader", soloader_thread_entry,
                                        priority, stack_size, 0, 0, NULL);
    l_info("[pthr] create uid=0x%08X", (unsigned) thid);
    if (thid < 0)
        return (int) thid;

    int ret = sceKernelStartThread(thid, sizeof(p), &p);
    l_info("[pthr] start uid=0x%08X ret=%d", (unsigned) thid, ret);
    if (ret < 0) {
        sceKernelDeleteThread(thid);
        return ret;
    }

    *thread = (pthread_t) thid;

    bc_event("pthread_create", BC_RA);
    watchdog_register_thread((int) *thread, "hilo-del-.so");
    return 0;
}

int pthread_kill_soloader(pthread_t thread, int sig)
{
    (void) thread; (void) sig;
    return 0;
}

int pthread_mutexattr_init_soloader(pthread_mutexattr_t *attr)
{
    return pthread_mutexattr_init(attr);
}

int pthread_mutexattr_settype_soloader(pthread_mutexattr_t *attr, int type)
{
    return pthread_mutexattr_settype(attr, type);
}

int pthread_mutexattr_destroy_soloader(pthread_mutexattr_t *attr)
{
    return pthread_mutexattr_destroy(attr);
}

/* ------------------------------------------------------------------------- */
/* Bring-up: no real mutex/cond objects.                                      */
/* newlib's pthread_mutex_init() does calloc(1,20) + pte_osSemaphoreCreate()  */
/* and it is aborting mid-boot on this platform (heap/semaphore related).      */
/* The engine only uses these for handle/node guards, and with our single      */
/* threaded main path they are not needed, so make them inert.                */
/* ------------------------------------------------------------------------- */
int pthread_mutex_init_soloader(pthread_mutex_t_bionic *mutex,
                                const pthread_mutexattr_t *attr)
{
    (void)attr;
    if (!mutex) return EINVAL;
    mutex->real_ptr = (pthread_mutex_t *)(uintptr_t)0x1;
    return 0;
}

int pthread_mutex_destroy_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return 0;
    mutex->real_ptr = 0x0;
    return 0;
}

int pthread_mutex_lock_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return EINVAL;
    mutex->real_ptr = (pthread_mutex_t *)(uintptr_t)0x1;
    return 0;
}

int pthread_mutex_trylock_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return EINVAL;
    mutex->real_ptr = (pthread_mutex_t *)(uintptr_t)0x1;
    return 0;
}

int pthread_mutex_unlock_soloader(pthread_mutex_t_bionic *mutex)
{
    if (!mutex) return EINVAL;
    return 0;
}

int pthread_condattr_init_soloader(pthread_condattr_t *attr)
{
    if (!attr) return EINVAL;
    return pthread_condattr_init(attr);
}

int pthread_condattr_destroy_soloader(pthread_condattr_t *attr)
{
    if (!attr) return EINVAL;
    return pthread_condattr_destroy(attr);
}

/* Real bionic-condition implementation on top of a Vita semaphore.
 * The UI/package loader waits on these conditions, so making them completely
 * inert leaves the loading thread asleep forever. */
typedef struct {
    SceUID sem;
    atomic_int waiters;
} pthr_cond_impl_t;

static pthr_cond_impl_t *pthr_cond_get(pthread_cond_t_bionic *cond) {
    uintptr_t raw = (uintptr_t) cond->real_ptr;
    if (raw >= 0x81800000u && raw < 0x90000000u)
        return (pthr_cond_impl_t *) raw;

    pthr_cond_impl_t *impl = calloc(1, sizeof(*impl));
    if (!impl)
        return NULL;
    impl->sem = sceKernelCreateSema("cond", 0, 0, 0x7fffffff, NULL);
    if (impl->sem < 0) {
        free(impl);
        return NULL;
    }
    atomic_init(&impl->waiters, 0);
    cond->real_ptr = (pthread_cond_t *) impl;
    return impl;
}

int pthread_cond_init_soloader(pthread_cond_t_bionic *cond,
                               const pthread_condattr_t *attr)
{
    (void) attr;
    if (!cond) return EINVAL;
    return pthr_cond_get(cond) ? 0 : ENOMEM;
}

int pthread_cond_destroy_soloader(pthread_cond_t_bionic *cond)
{
    if (!cond) return 0;
    uintptr_t raw = (uintptr_t) cond->real_ptr;
    if (raw >= 0x81800000u && raw < 0x90000000u) {
        pthr_cond_impl_t *impl = (pthr_cond_impl_t *) raw;
        sceKernelDeleteSema(impl->sem);
        /* no free(): the same zip-system condition is destroyed twice and
         * the second free() landed on newlib's allocator bookkeeping. */
    }
    cond->real_ptr = NULL;
    return 0;
}

int pthread_cond_signal_soloader(pthread_cond_t_bionic *cond)
{
    if (!cond) return 0;
    pthr_cond_impl_t *impl = pthr_cond_get(cond);
    if (!impl) return 0;
    if (atomic_load(&impl->waiters) > 0)
        sceKernelSignalSema(impl->sem, 1);
    return 0;
}

int pthread_cond_broadcast_soloader(pthread_cond_t_bionic *cond)
{
    if (!cond) return 0;
    pthr_cond_impl_t *impl = pthr_cond_get(cond);
    if (!impl) return 0;
    int n = atomic_load(&impl->waiters);
    if (n > 0)
        sceKernelSignalSema(impl->sem, n);
    return 0;
}

int pthread_cond_timedwait_soloader(pthread_cond_t_bionic *cond,
                                    pthread_mutex_t_bionic *mutex,
                                    struct timespec *abstime)
{
    (void) abstime;
    if (!cond || !mutex) return EINVAL;
    pthr_cond_impl_t *impl = pthr_cond_get(cond);
    if (!impl) return ENOMEM;

    atomic_fetch_add(&impl->waiters, 1);
    pthread_mutex_unlock_soloader(mutex);
    SceUInt timeout = 200 * 1000; /* 200 ms fallback */
    int r = sceKernelWaitSema(impl->sem, 1, &timeout);
    atomic_fetch_sub(&impl->waiters, 1);
    pthread_mutex_lock_soloader(mutex);
    return (r < 0) ? ETIMEDOUT : 0;
}

int pthread_cond_wait_soloader(pthread_cond_t_bionic *cond,
                               pthread_mutex_t_bionic *mutex)
{
    if (!cond || !mutex) return EINVAL;
    pthr_cond_impl_t *impl = pthr_cond_get(cond);
    if (!impl) return ENOMEM;

    atomic_fetch_add(&impl->waiters, 1);
    pthread_mutex_unlock_soloader(mutex);
    /* Bounded wait: our semaphore-backed condition does not remember a
     * signal that arrives between the predicate check and the wait, and the
     * engine's loaders then sleep forever.  POSIX allows spurious wakeups, so
     * time out and let the caller re-check its predicate. */
    SceUInt timeout = 100 * 1000;
    sceKernelWaitSema(impl->sem, 1, &timeout);
    atomic_fetch_sub(&impl->waiters, 1);
    pthread_mutex_lock_soloader(mutex);
    return 0;
}

int pthread_join_soloader(pthread_t thread, void **value_ptr)
{
    /* Non-blocking for bring-up: the engine only uses join while shutting
     * worker threads down, and blocking here on a thread that never exits
     * would freeze the whole app. */
    (void) thread;
    if (value_ptr) *value_ptr = NULL;
    return 0;
}

int pthread_detach_soloader(pthread_t thread)
{
    (void) thread;
    return 0;
}

int pthread_equal_soloader(const pthread_t t1, const pthread_t t2)
{
    return t1 == t2;
}

pthread_t pthread_self_soloader()
{
    return (pthread_t) sceKernelGetThreadId();
}

void pthread_exit_soloader(void *value_ptr)
{
    (void) value_ptr;
    sceKernelExitDeleteThread(0);
    for (;;) { }
}

int pthread_attr_init_soloader(pthread_attr_t_bionic *attr)
{
    if (!attr) return EINVAL;
    memset(attr, 0, sizeof(*attr));
    attr->magic = PTHR_ATTR_MAGIC;
    return 0;
}

int pthread_attr_destroy_soloader(pthread_attr_t_bionic *attr)
{
    if (attr) attr->magic = 0;
    return 0;
}

int pthread_attr_setdetachstate_soloader(pthread_attr_t_bionic *attr, int state)
{
    if (!attr) return EINVAL;
    if (attr->magic != PTHR_ATTR_MAGIC)
        pthread_attr_init_soloader(attr);
    attr->real_ptr = (pthread_attr_t *)(uintptr_t)(state == 1 ? 1 : 0);
    return 0;
}

int pthread_attr_getdetachstate_soloader(const pthread_attr_t_bionic *attr)
{
    if (attr && attr->magic == PTHR_ATTR_MAGIC && (uintptr_t)attr->real_ptr == 1)
        return 1;
    return 0;
}

int pthread_attr_setstacksize_soloader(pthread_attr_t_bionic *attr, size_t stacksize)
{
    if (!attr) return EINVAL;
    if (attr->magic != PTHR_ATTR_MAGIC)
        pthread_attr_init_soloader(attr);
    attr->stack_size = stacksize;
    return 0;
}

int pthread_attr_getstacksize_soloader(const pthread_attr_t_bionic *attr, size_t *stacksize)
{
    if (!stacksize) return EINVAL;
    *stacksize = (attr && attr->magic == PTHR_ATTR_MAGIC && attr->stack_size)
                     ? attr->stack_size : (512 * 1024);
    return 0;
}

int pthread_attr_getschedpolicy_soloader(const pthread_attr_t_bionic *attr, int *policy)
{
    (void) attr;
    if (policy) *policy = 0;
    return 0;
}

int pthread_attr_getscope_soloader(const pthread_attr_t_bionic *attr, int *scope)
{
    (void) attr;
    if (scope) *scope = 0;
    return 0;
}

int pthread_attr_getstack_soloader(const pthread_attr_t_bionic *attr,
                                   void **stackaddr, size_t *stacksize)
{
    (void) attr;
    if (stackaddr) *stackaddr = NULL;
    return pthread_attr_getstacksize_soloader(attr, stacksize);
}

int pthread_attr_getstackaddr_soloader(const pthread_attr_t_bionic *attr, void **stackaddr)
{
    (void) attr;
    if (stackaddr) *stackaddr = NULL;
    return 0;
}

int pthread_setschedparam_soloader(pthread_t thread, int policy,
                                   const struct sched_param *param)
{
    (void) policy;
    if (!thread || !param) return EINVAL;
    int prio = 0xA0 - param->sched_priority;
    if (prio < 1)   prio = 1;
    if (prio > 0xBF) prio = 0xBF;
    sceKernelChangeThreadPriority((SceUID) thread, prio);
    return 0;
}

int pthread_getschedparam_soloader(pthread_t thread, int *policy,
                                   struct sched_param *param)
{
    (void) thread;
    if (policy) *policy = 0;
    if (param) param->sched_priority = 0;
    return 0;
}

int pthread_once_soloader(volatile int *once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine)
        return -1;
    if (__sync_lock_test_and_set(once_control, 1) == 0)
        (*init_routine)();
    return 0;
}

#ifndef MAX_TASK_COMM_LEN
#define MAX_TASK_COMM_LEN 16
#endif

int pthread_setname_np_soloader(pthread_t thread, const char* thread_name) {
    if (thread == 0 || thread_name == NULL) {
        return EINVAL;
    }
    size_t thread_name_len = strlen(thread_name);
    if (thread_name_len >= MAX_TASK_COMM_LEN) {
        return ERANGE;
    }

    sceClibPrintf("PTHREAD: pthread_setname_np with name %s for thread:0x%x\n", thread_name, pthread_self());

    return 0;
}

int sem_destroy_soloader(int * uid) {
    if (sceKernelDeleteSema(*uid) < 0)
        return -1;
    return 0;
}

int sem_getvalue_soloader (int * uid, int * sval) {
    SceKernelSemaInfo info;
    info.size = sizeof(SceKernelSemaInfo);

    if (sceKernelGetSemaInfo(*uid, &info) < 0) return -1;
    if (!sval) sval = malloc(sizeof(int32_t));
    *sval = info.currentCount;
    return 0;
}

int sem_init_soloader (int * uid, int pshared, unsigned int value) {
    *uid = sceKernelCreateSema("sema", 0, (int) value, 0x7fffffff, NULL);
    if (*uid < 0)
        return -1;
    static int s_init_log = 0;
    if (s_init_log++ < 24)
        l_info("[sem] init %p uid=0x%08X value=%u ra=%p",
               uid, (unsigned)*uid, value, __builtin_return_address(0));
    return 0;
}

int sem_post_soloader (int * uid) {
    int v = uid ? *uid : 0;
    if ((unsigned)v == 0x40010179u || (unsigned)v == 0x40010175u || (unsigned)v == 0x40010177u)
        l_info("[sem] post uid=0x%08X ra=%p", (unsigned)v, __builtin_return_address(0));
    if (sceKernelSignalSema(v, 1) < 0)
        return -1;
    return 0;
}

int sem_timedwait_soloader (int * uid, const struct timespec * abstime) {
    uint timeout = 1000;
    if (sceKernelWaitSema(*uid, 1, &timeout) >= 0)
        return 0;
    if (!abstime) return -1;
    long long now = (long long) current_timestamp_ms() * 1000; // us
    long long _timeout = abstime->tv_sec * 1000 * 1000 + abstime->tv_nsec / 1000; // us
    if (_timeout-now >= 0) return -1;
    uint timeout_real = _timeout - now;
    if (sceKernelWaitSema(*uid, 1, &timeout_real) < 0)
        return -1;
    return 0;
}

int sem_trywait_soloader (int * uid) {
    uint timeout = 1000;
    if (sceKernelWaitSema(*uid, 1, &timeout) < 0)
        return -1;
    return 0;
}

int sem_wait_soloader (int * uid) {
    int v = uid ? *uid : 0;
    if ((unsigned)v == 0x40010179u || (unsigned)v == 0x40010175u || (unsigned)v == 0x40010177u)
        l_info("[sem] wait uid=0x%08X ra=%p", (unsigned)v, __builtin_return_address(0));
    /* Never block forever: the Android/Java side that would post some of these
     * semaphores (notably the _zpgfxInitializeSystem one waited on by
     * _zpWindowSetFormat) does not exist in this loader.  Wait a bounded amount
     * of time and then continue. */
    uint32_t timeout = 2 * 1000 * 1000;
    if (sceKernelWaitSema(v, 1, &timeout) < 0)
        return 0;
    return 0;
}

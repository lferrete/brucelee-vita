/*
 * Bruce Lee: Dragon Warrior - PS Vita loader
 *
 * The game is a closed-source Android title built on Digital Legends' KRM engine
 * and Sony Ericsson's ZSystem/ZPlatform layer.  We load the original ARM shared
 * objects and provide the Android/ZSystem environment they expect.
 *
 * Port code Copyright (C) 2026 Leandro Ferrete; see AUTHORS.md and LICENSE.
 */

#ifndef BRUCELEE_LOADER_H
#define BRUCELEE_LOADER_H

#include <stdint.h>
#include <so_util/so_util.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZPLATFORM_LOAD_ADDR 0x98000000u
#define DEFAULT_LOAD_ADDR   0x99000000u

extern so_module zplatform_mod;
extern so_module default_mod;

/* Opaque handles returned by our dlopen() replacement. */
extern void *zplatform_handle;
extern void *default_handle;
extern void *android_handle;
extern void *opensles_handle;

int  brucelee_load_modules(void);
void brucelee_setup_activity(void);
void brucelee_android_entry(void);
void brucelee_install_hooks(void);

void *brucelee_dlopen(const char *path, int flags);
void *brucelee_dlsym(void *handle, const char *symbol);

#ifdef __cplusplus
}
#endif

#endif // BRUCELEE_LOADER_H

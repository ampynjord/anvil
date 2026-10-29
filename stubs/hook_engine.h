/*
 * hook_engine.h — x64 inline hook API.
 */
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define ANVIL_HOOK_TRAMPOLINE_SIZE 12

typedef struct {
    void *target;
    BYTE  saved[ANVIL_HOOK_TRAMPOLINE_SIZE];
    int   installed;
} AnvilHook;

/* Hook *target → redirect to *replacement.  Saves original bytes in hook. */
int  anvil_hook_install(AnvilHook *hook, void *target, void *replacement);

/* Restore original bytes. */
void anvil_hook_remove(AnvilHook *hook);

/* Look up proc by name in a loaded module, then hook it. */
int  anvil_hook_proc(AnvilHook *hook, const char *module, const char *proc,
                     void *replacement);

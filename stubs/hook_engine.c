/*
 * hook_engine.c — Minimal x64 inline hook engine for Anvil.
 *
 * Patches a target function's first 12 bytes with an absolute jump:
 *   48 B8 [imm64]   mov rax, <replacement>
 *   FF E0           jmp rax
 *
 * The original bytes are saved so the hook can be removed.
 * VirtualProtect makes the code page writable before patching.
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_hook);
# define HLOG(fmt,...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# define HLOG(fmt,...) fprintf(stderr, "[anvil/hook] " fmt "\n", ##__VA_ARGS__)
#endif

#include <string.h>
#include "hook_engine.h"

#define TRAMPOLINE_SIZE 12   /* mov rax, imm64 (10) + jmp rax (2) */

/*
 * Build the 12-byte trampoline into buf[]:
 *   48 B8 <lo32> <hi32>   mov rax, replacement
 *   FF E0                  jmp rax
 */
static void build_trampoline(BYTE *buf, void *replacement)
{
    UINT64 addr = (UINT64)(uintptr_t)replacement;
    buf[0]  = 0x48; buf[1] = 0xB8;          /* REX.W mov rax, imm64 */
    buf[2]  = (BYTE)(addr);
    buf[3]  = (BYTE)(addr >>  8);
    buf[4]  = (BYTE)(addr >> 16);
    buf[5]  = (BYTE)(addr >> 24);
    buf[6]  = (BYTE)(addr >> 32);
    buf[7]  = (BYTE)(addr >> 40);
    buf[8]  = (BYTE)(addr >> 48);
    buf[9]  = (BYTE)(addr >> 56);
    buf[10] = 0xFF; buf[11] = 0xE0;         /* jmp rax */
}

/*
 * anvil_hook_install — overwrite *target with a jump to *replacement.
 * Saves the original TRAMPOLINE_SIZE bytes into hook->saved[].
 * Returns non-zero on success.
 */
int anvil_hook_install(AnvilHook *hook, void *target, void *replacement)
{
    DWORD old_prot, dummy;
    BYTE tramp[TRAMPOLINE_SIZE];
    SIZE_T written = 0;

    hook->target = target;
    hook->installed = 0;
    memcpy(hook->saved, target, TRAMPOLINE_SIZE);
    build_trampoline(tramp, replacement);

    if (VirtualProtect(target, TRAMPOLINE_SIZE, PAGE_EXECUTE_READWRITE, &old_prot)) {
        memcpy(target, tramp, TRAMPOLINE_SIZE);
        VirtualProtect(target, TRAMPOLINE_SIZE, old_prot, &dummy);
        hook->installed = 1;
    } else {
        /* Fallback: WriteProcessMemory bypasses page-protection on Wine */
        if (WriteProcessMemory(GetCurrentProcess(), target,
                               tramp, TRAMPOLINE_SIZE, &written)
            && written == TRAMPOLINE_SIZE)
            hook->installed = 1;
    }

    if (!hook->installed) {
        HLOG("hook_install FAILED on %p (VProt+WPM both failed, err=%lu)",
             target, (unsigned long)GetLastError());
        return 0;
    }

    FlushInstructionCache(GetCurrentProcess(), target, TRAMPOLINE_SIZE);
    HLOG("hook installed: %p → %p", target, replacement);
    return 1;
}

/*
 * anvil_hook_remove — restore original bytes.
 */
void anvil_hook_remove(AnvilHook *hook)
{
    DWORD old_prot;
    if (!hook->installed) return;
    VirtualProtect(hook->target, TRAMPOLINE_SIZE, PAGE_EXECUTE_READWRITE, &old_prot);
    memcpy(hook->target, hook->saved, TRAMPOLINE_SIZE);
    VirtualProtect(hook->target, TRAMPOLINE_SIZE, old_prot, &old_prot);
    hook->installed = 0;
    HLOG("hook removed: %p", hook->target);
}

/*
 * anvil_hook_proc — find a named export in a loaded module and hook it.
 * Returns non-zero on success.
 */
int anvil_hook_proc(AnvilHook *hook, const char *module, const char *proc, void *replacement)
{
    HMODULE hmod = GetModuleHandleA(module);
    void *target;

    if (!hmod) {
        HLOG("module '%s' not loaded", module);
        return 0;
    }
    target = (void *)GetProcAddress(hmod, proc);
    if (!target) {
        HLOG("proc '%s' not found in '%s'", proc, module);
        return 0;
    }
    return anvil_hook_install(hook, target, replacement);
}

/*
 * dxgi_stubs.c — Anvil: patch IDXGIFactory2::CreateSwapChainForCoreWindow
 *
 * UWP apps call CreateSwapChainForCoreWindow(factory, device, coreWindow, desc,
 * restrictOutput, &swapchain).  Wine's DXGI doesn't know about ICoreWindow, so
 * we intercept by patching vtable slot 16 of IDXGIFactory2 to extract the
 * HWND from our CoreWindow stub and forward to CreateSwapChainForHwnd (slot 15).
 *
 * IDXGIFactory2 vtable layout:
 *   0-2   IUnknown
 *   3-6   IDXGIObject
 *   7-11  IDXGIFactory
 *   12-13 IDXGIFactory1
 *   14    IsWindowedStereoEnabled
 *   15    CreateSwapChainForHwnd
 *   16    CreateSwapChainForCoreWindow  ← we patch this
 *   17    CreateSwapChainForComposition
 *   ...
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_dxgi);
# define DLOG(fmt,...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# define DLOG(fmt,...) fprintf(stderr, "[anvil/dxgi] " fmt "\n", ##__VA_ARGS__)
#endif

extern HWND anvil_get_corewindow_hwnd(void);  /* core_objects.c */

/* IDXGIFactory2 IID — used to obtain the right factory type */
static const IID IID_IDXGIFactory2 = {
    0x50C83A1C, 0xE072, 0x4C48,
    { 0x87, 0xB0, 0x36, 0x30, 0xFA, 0x36, 0xA6, 0xD0 }
};

/* Saved vtable pointer — valid for the DLL's lifetime */
static void **g_dxgi_vtbl = NULL;

/*
 * CreateSwapChainForHwnd signature (slot 15):
 *   HRESULT (This, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictOut, ppSwapChain)
 */
typedef HRESULT (WINAPI *PFN_CreateSCFHwnd)(
    void *This, void *pDevice, HWND hWnd,
    void *pDesc, void *pFullscreenDesc,
    void *pRestrictToOutput, void **ppSwapChain);

/*
 * Our replacement for slot 16 — CreateSwapChainForCoreWindow:
 *   HRESULT (This, pDevice, pWindow, pDesc, pRestrictOut, ppSwapChain)
 * We ignore pWindow (the ICoreWindow*) and use our known HWND instead.
 */
static HRESULT WINAPI dxgi_CreateSCFCW_hook(
    void *This, void *pDevice, void *pWindow,
    void *pDesc, void *pRestrictToOutput, void **ppSwapChain)
{
    (void)pWindow;

    HWND hwnd = anvil_get_corewindow_hwnd();
    DLOG("CreateSwapChainForCoreWindow hook: HWND=%p", (void *)hwnd);

    if (!hwnd || !g_dxgi_vtbl) {
        DLOG("  no CoreWindow HWND yet — failing");
        return E_FAIL;
    }

    PFN_CreateSCFHwnd fn = (PFN_CreateSCFHwnd)g_dxgi_vtbl[15];
    return fn(This, pDevice, hwnd, pDesc,
              NULL /* pFullscreenDesc */, pRestrictToOutput, ppSwapChain);
}

/* ---- Public API ---------------------------------------------------------- */

void anvil_install_dxgi_hooks(void)
{
    HMODULE hdxgi = LoadLibraryA("dxgi.dll");
    if (!hdxgi) {
        DLOG("dxgi.dll not found — DXGI hook skipped");
        return;
    }

    /* Try CreateDXGIFactory2 first, fall back to CreateDXGIFactory1 */
    void *factory = NULL;
    HRESULT hr = E_FAIL;

    typedef HRESULT (WINAPI *PFN_CDF2)(UINT flags, REFIID riid, void **ppFactory);
    PFN_CDF2 pCDF2 = (PFN_CDF2)GetProcAddress(hdxgi, "CreateDXGIFactory2");
    if (pCDF2) {
        hr = pCDF2(0, &IID_IDXGIFactory2, &factory);
        DLOG("CreateDXGIFactory2 hr=0x%08lx factory=%p",
             (unsigned long)hr, factory);
    }

    if (FAILED(hr) || !factory) {
        typedef HRESULT (WINAPI *PFN_CDF1)(REFIID riid, void **ppFactory);
        PFN_CDF1 pCDF1 = (PFN_CDF1)GetProcAddress(hdxgi, "CreateDXGIFactory1");
        if (!pCDF1) { DLOG("CreateDXGIFactory1 not found"); return; }
        hr = pCDF1(&IID_IDXGIFactory2, &factory);
        DLOG("CreateDXGIFactory1 hr=0x%08lx factory=%p",
             (unsigned long)hr, factory);
    }

    if (FAILED(hr) || !factory) {
        DLOG("Cannot create IDXGIFactory2 — DXGI hook skipped");
        return;
    }

    /* Grab the vtable pointer (stable: points into dxgi.dll's .rdata) */
    g_dxgi_vtbl = *(void ***)factory;

    /* Release the temporary factory — vtable pointer remains valid */
    typedef ULONG (WINAPI *PFN_Release)(void *);
    ((PFN_Release)g_dxgi_vtbl[2])(factory);

    /* Patch slot 16: CreateSwapChainForCoreWindow → our hook */
    void  **slot    = g_dxgi_vtbl + 16;
    void   *hook    = (void *)dxgi_CreateSCFCW_hook;
    DWORD   old_prot, dummy;

    if (!VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &old_prot)) {
        /* Fallback: WriteProcessMemory can bypass page protection */
        SIZE_T written = 0;
        if (!WriteProcessMemory(GetCurrentProcess(), slot,
                                &hook, sizeof(void *), &written)) {
            DLOG("Failed to patch DXGI vtable slot 16 (err=%lu)",
                 (unsigned long)GetLastError());
            return;
        }
    } else {
        *slot = hook;
        VirtualProtect(slot, sizeof(void *), old_prot, &dummy);
    }

    DLOG("Patched IDXGIFactory2 vtable slot 16 → CreateSwapChainForCoreWindow hook");
}

void anvil_remove_dxgi_hooks(void)
{
    /* Not restored on exit — process teardown handles cleanup */
}

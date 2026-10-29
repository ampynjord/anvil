/*
 * xaml_stubs.c — Anvil Layer 3: minimal WinRT factory stubs (P1 + P2)
 *
 * Provides IActivationFactory implementations for the WinRT classes that
 * Calculator's XAML-generated InitializeComponent() calls first.
 *
 * Each factory:
 *   - Responds to RoGetActivationFactory with S_OK (factory lookup succeeds)
 *   - Logs every method call to stderr for empirical call-graph collection
 *   - Returns E_NOTIMPL from ActivateInstance (real objects come later)
 *
 * Registered via anvil_register_all_factories() which combase_anvil.c
 * calls from DllMain(DLL_PROCESS_ATTACH).
 *
 * COMPILATION:
 *   DLL  (MinGW):  compiled together with combase_anvil.c, -D_WIN32 implicit
 *   Test (Linux):  gcc -fshort-wchar -I. xaml_stubs.c combase_anvil.c ...
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_xaml);
# define XLOG(fmt, ...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# include <stdio.h>
# define XLOG(fmt, ...) fprintf(stderr, "[anvil/xaml] " fmt "\n", ##__VA_ARGS__)
#endif

/* Functions provided by combase_anvil.c */
extern HRESULT WINAPI WindowsCreateString(LPCWSTR src, UINT32 len, HSTRING *out);
extern int anvil_register_factory(const WCHAR *class_id, void *factory);

/* Real CoreApplication object from core_objects.c (two-vtable COM) */
extern IActivationFactory *anvil_get_core_app_factory(void);

/* P4 statics-enabled factories from p4_stubs.c */
extern void anvil_register_p4_factories(void);
/* P5 statics-enabled factories from p5_stubs.c */
extern void anvil_register_p5_factories(void);
/* Storage stubs from storage_stubs.c (overwrites P2 ApplicationData) */
extern void anvil_register_storage_factories(void);
/* System UI / input stubs from sysui_stubs.c */
extern void anvil_register_sysui_factories(void);
/* Package WinRT stubs from pkg_winrt.c */
extern void anvil_register_pkg_winrt_factories(void);
/* XAML controls from xaml_controls_stubs.c */
extern void anvil_register_xaml_controls(void);
/* Composition API from composition_stubs.c */
extern void anvil_register_composition_factories(void);

/* =========================================================================
 * Shared no-op vtable helpers (all stubs reuse these)
 * ========================================================================= */

static HRESULT WINAPI stub_QI(void *T, REFIID iid, void **out)
{
    (void)iid; *out = T; return S_OK;
}
static ULONG WINAPI stub_AddRef (void *T) { (void)T; return 1; }
static ULONG WINAPI stub_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI stub_GetIids(void *T, ULONG *n, IID **ids)
{
    (void)T; *n = 0; *ids = NULL; return S_OK;
}
static HRESULT WINAPI stub_GetTrustLevel(void *T, TrustLevel *tl)
{
    (void)T; *tl = BaseTrust; return S_OK;
}

/* =========================================================================
 * Macro: declare a named factory with a fixed class name string.
 *
 * Expands to:
 *   static HRESULT WINAPI <tag>_GetRuntimeClassName(...)
 *   static HRESULT WINAPI <tag>_ActivateInstance(...)
 *   static const IActivationFactoryVtbl <tag>_vtbl = { ... };
 *   static IActivationFactory <tag>_factory = { &<tag>_vtbl };
 * ========================================================================= */

#define DEFINE_FACTORY(tag, wname, wlen, msg)                                    \
static HRESULT WINAPI tag##_GetRCN(void *T, HSTRING *n)                         \
{                                                                                 \
    (void)T;                                                                      \
    return WindowsCreateString(wname, wlen, n);                                   \
}                                                                                 \
static HRESULT WINAPI tag##_Activate(void *T, IInspectable **inst)               \
{                                                                                 \
    (void)T; *inst = NULL;                                                        \
    XLOG(msg " — ActivateInstance called (E_NOTIMPL, not yet implemented)");      \
    return E_NOTIMPL;                                                             \
}                                                                                 \
static const IActivationFactoryVtbl tag##_vtbl = {                               \
    stub_QI, stub_AddRef, stub_Release,                                           \
    stub_GetIids, tag##_GetRCN, stub_GetTrustLevel, tag##_Activate,              \
};                                                                                \
static IActivationFactory tag##_factory = { &tag##_vtbl }

/* =========================================================================
 * P1 — process cannot start without these
 * ========================================================================= */

DEFINE_FACTORY(application,
    L"Windows.UI.Xaml.Application", 27,
    "Windows.UI.Xaml.Application");

/* CoreApplication: uses real two-vtable object from core_objects.c (no DEFINE_FACTORY) */

DEFINE_FACTORY(corewin,
    L"Windows.UI.Core.CoreWindow", 26,
    "Windows.UI.Core.CoreWindow");

DEFINE_FACTORY(window,
    L"Windows.UI.Xaml.Window", 22,
    "Windows.UI.Xaml.Window");

DEFINE_FACTORY(coredispatcher,
    L"Windows.UI.Core.CoreDispatcher", 30,
    "Windows.UI.Core.CoreDispatcher");

DEFINE_FACTORY(frame,
    L"Windows.UI.Xaml.Controls.Frame", 30,
    "Windows.UI.Xaml.Controls.Frame");

DEFINE_FACTORY(resdictionary,
    L"Windows.UI.Xaml.ResourceDictionary", 34,
    "Windows.UI.Xaml.ResourceDictionary");

DEFINE_FACTORY(dependencyprop,
    L"Windows.UI.Xaml.DependencyProperty", 34,
    "Windows.UI.Xaml.DependencyProperty");

/* =========================================================================
 * P2 — stub with fake/empty data
 * ========================================================================= */

DEFINE_FACTORY(appview,
    L"Windows.UI.ViewManagement.ApplicationView", 41,
    "Windows.UI.ViewManagement.ApplicationView");

DEFINE_FACTORY(appdata,
    L"Windows.Storage.ApplicationData", 31,
    "Windows.Storage.ApplicationData");

DEFINE_FACTORY(sizehelper,
    L"Windows.Foundation.Size", 23,
    "Windows.Foundation.Size");

/* =========================================================================
 * P3 — no-op stubs (don't crash, don't do anything)
 * ========================================================================= */

DEFINE_FACTORY(jumplist,
    L"Windows.UI.StartScreen.JumpList", 31,
    "Windows.UI.StartScreen.JumpList");

DEFINE_FACTORY(sysuser,
    L"Windows.System.User", 19,
    "Windows.System.User");

/* =========================================================================
 * Registration — called by combase_anvil.c DllMain(DLL_PROCESS_ATTACH)
 * ========================================================================= */

void anvil_register_all_factories(void)
{
    /* P1 */
    anvil_register_factory(L"Windows.UI.Xaml.Application",           &application_factory);
    /* CoreApplication: real two-vtable object with working Run() */
    anvil_register_factory(L"Windows.ApplicationModel.Core.CoreApplication",
                           anvil_get_core_app_factory());
    anvil_register_factory(L"Windows.UI.Core.CoreWindow",            &corewin_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Window",                &window_factory);
    anvil_register_factory(L"Windows.UI.Core.CoreDispatcher",        &coredispatcher_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.Frame",        &frame_factory);
    anvil_register_factory(L"Windows.UI.Xaml.ResourceDictionary",    &resdictionary_factory);
    anvil_register_factory(L"Windows.UI.Xaml.DependencyProperty",    &dependencyprop_factory);
    /* P2 */
    anvil_register_factory(L"Windows.UI.ViewManagement.ApplicationView", &appview_factory);
    anvil_register_factory(L"Windows.Storage.ApplicationData",       &appdata_factory);
    anvil_register_factory(L"Windows.Foundation.Size",               &sizehelper_factory);
    /* P3 */
    anvil_register_factory(L"Windows.UI.StartScreen.JumpList",       &jumplist_factory);
    anvil_register_factory(L"Windows.System.User",                   &sysuser_factory);

    /* P4 */
    anvil_register_p4_factories();

    /* P5 — overwrites P1 Window/Application/DependencyProperty with statics-enabled versions */
    anvil_register_p5_factories();

    /* P6 — overwrites P2 ApplicationData with real IApplicationData + StorageFolder stubs */
    anvil_register_storage_factories();

    /* P7 — DisplayInformation, ApplicationView, PropertyValue, ThreadPool, Gamepad */
    anvil_register_sysui_factories();

    /* P8 — Package WinRT class */
    anvil_register_pkg_winrt_factories();

    /* P9 — XAML controls */
    anvil_register_xaml_controls();

    /* P10 — Composition */
    anvil_register_composition_factories();

    XLOG("anvil_register_all_factories: 83 stubs registered (P1-P10)");
}

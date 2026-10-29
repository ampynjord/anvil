/*
 * test_system_stubs.c — Integration test for dispatcher, sysui, and pkg_winrt stubs.
 *
 * Run under Wine with:
 *   ANVIL_PKG_FULLNAME=Microsoft.WindowsCalculator_10.2103.8.0_x64__8wekyb3d8bbwe
 *   ANVIL_PKG_FAMILYNAME=Microsoft.WindowsCalculator_8wekyb3d8bbwe
 *   ANVIL_PKG_PATH="C:\Program Files\WindowsApps\..."
 *   ANVIL_PKG_PUBLISHER="CN=Microsoft Corporation, ..."
 *   WINEDLLOVERRIDES=combase=n,b
 *
 * Expected: "ALL SYSTEM CHECKS PASS"
 */

#define DEFINE_ANVIL_IID  /* suppress IID_IActivationFactory from winrt_types.h; we define our own */
#include "winrt_types.h"  /* HSTRING and WinRT baseline, includes <windows.h> */
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stddef.h>

/* --------------------------------------------------------------------------
 * Runtime function pointers resolved from combase.dll (our override)
 * -------------------------------------------------------------------------- */
typedef HRESULT (WINAPI *PFN_RoGetActivationFactory)(HSTRING, REFIID, void **);
typedef HRESULT (WINAPI *PFN_WindowsCreateString)(LPCWSTR, UINT32, HSTRING *);
typedef LPCWSTR (WINAPI *PFN_WindowsGetStringRawBuffer)(HSTRING, UINT32 *);
typedef HRESULT (WINAPI *PFN_WindowsDeleteString)(HSTRING);

static PFN_RoGetActivationFactory    pRoGet;
static PFN_WindowsCreateString       pWCS;
static PFN_WindowsGetStringRawBuffer pWGSRB;
static PFN_WindowsDeleteString       pWDS;

/* --------------------------------------------------------------------------
 * IIDs
 * -------------------------------------------------------------------------- */
static const IID IID_IActivationFactory = {
    0x00000035, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};
static const IID IID_IDisplayInformationStatics = {
    0xC6A02A6C, 0xD452, 0x44DC,
    { 0x8A, 0x18, 0xC9, 0xE8, 0xDC, 0xF2, 0x5A, 0xD0 }
};
static const IID IID_IDisplayInformation = {
    0xBED112AE, 0xADC3, 0x4DC9,
    { 0xAE, 0x65, 0x85, 0x1F, 0x4D, 0x7D, 0x47, 0x99 }
};
static const IID IID_IApplicationViewStatics = {
    0xA10801F5, 0xE422, 0x4E3B,
    { 0x8E, 0x8C, 0xC4, 0xB0, 0xF9, 0xAC, 0x5C, 0xF6 }
};
static const IID IID_IPackageStatics = {
    0x7B978F69, 0x81E6, 0x4B58,
    { 0x8A, 0x1C, 0x5A, 0xA9, 0xF0, 0xB7, 0xFD, 0x68 }
};
static const IID IID_IPackage = {
    0x163C792F, 0xBD75, 0x413C,
    { 0xBF, 0x23, 0xB1, 0xFE, 0x7B, 0x95, 0xD8, 0x25 }
};
static const IID IID_IPackageId = {
    0x1ADB665E, 0x37C7, 0x4790,
    { 0x99, 0x80, 0xDD, 0x7A, 0xE7, 0x4E, 0x8B, 0xB2 }
};
static const IID IID_IThreadPoolStatics = {
    0xB6BF67DD, 0x84EF, 0x4CD4,
    { 0xA2, 0xAD, 0x75, 0x7C, 0x5D, 0x7F, 0x79, 0x20 }
};
static const IID IID_IAsyncAction = {
    0x5A648006, 0x843A, 0x4DA9,
    { 0x86, 0x5B, 0x9D, 0x26, 0xE5, 0xDF, 0xAD, 0x7B }
};
static const IID IID_IAsyncInfo = {
    0x00000036, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

/* --------------------------------------------------------------------------
 * Vtable call helpers
 * -------------------------------------------------------------------------- */
typedef HRESULT (WINAPI *PFN_QI)(void *, REFIID, void **);
typedef HRESULT (WINAPI *PFN_get_UI32)(void *, UINT32 *);
typedef HRESULT (WINAPI *PFN_get_Float)(void *, float *);
typedef HRESULT (WINAPI *PFN_get_Double)(void *, double *);
typedef HRESULT (WINAPI *PFN_get_Ptr)(void *, void **);
typedef HRESULT (WINAPI *PFN_get_HStr)(void *, HSTRING *);
typedef HRESULT (WINAPI *PFN_RunAsync)(void *, void *, void **);

#define VTBL(p)                (*(void ***)(p))
#define CALL_QI(p,iid,out)     ((PFN_QI)VTBL(p)[0])(p, iid, out)
#define CALL_GET_CURRENT(p,out)  ((PFN_get_Ptr)VTBL(p)[6])(p, out)  /* IDisplayInfoStatics::GetForCurrentView */
#define CALL_DI_LOGICALDPI(p,out) ((PFN_get_Float)VTBL(p)[13])(p, out)
#define CALL_DI_RESOLUTION(p,out) ((PFN_get_UI32)VTBL(p)[12])(p, out)
#define CALL_DI_SCREENW(p,out)    ((PFN_get_UI32)VTBL(p)[23])(p, out)
#define CALL_DI_SCREENH(p,out)    ((PFN_get_UI32)VTBL(p)[24])(p, out)
#define CALL_PKG_CURRENT(p,out)   ((PFN_get_Ptr)VTBL(p)[7])(p, out) /* IPackageStatics */
#define CALL_PKG_GET_ID(p,out)    ((PFN_get_Ptr)VTBL(p)[6])(p, out) /* IPackage */
#define CALL_PKG_INSTALLED(p,out) ((PFN_get_Ptr)VTBL(p)[7])(p, out) /* IPackage */
#define CALL_PKGID_FULLNAME(p,out) ((PFN_get_HStr)VTBL(p)[12])(p, out)
#define CALL_PKGID_FAMILY(p,out)   ((PFN_get_HStr)VTBL(p)[13])(p, out)
#define CALL_ASYNC_STATUS(p,out)  ((PFN_get_UI32)VTBL(p)[7])(p, out)  /* IAsyncInfo slot 7 */

/* --------------------------------------------------------------------------
 * Test bookkeeping
 * -------------------------------------------------------------------------- */
static int g_pass = 0, g_fail = 0;

static void check(const char *label, int cond)
{
    if (cond) { printf("  PASS  %s\n", label); g_pass++; }
    else       { printf("  FAIL  %s\n", label); g_fail++; }
}

static void *do_QI(void *iface, const IID *iid, const char *label)
{
    void *out = NULL;
    HRESULT hr = CALL_QI(iface, iid, &out);
    check(label, SUCCEEDED(hr) && out != NULL);
    return out;
}

static void *get_factory(LPCWSTR class_name, UINT32 len)
{
    HSTRING hs = NULL;
    pWCS(class_name, len, &hs);
    void *factory = NULL;
    HRESULT hr = pRoGet(hs, &IID_IActivationFactory, &factory);
    pWDS(hs);
    if (FAILED(hr) || !factory) return NULL;
    return factory;
}

/* ==========================================================================
 * [1] DisplayInformation
 * ========================================================================== */
static void test_display_info(void)
{
    printf("\n[1] DisplayInformation\n");

    void *factory = get_factory(L"Windows.Graphics.Display.DisplayInformation", 43);
    check("RoGetActivationFactory(DisplayInformation)", factory != NULL);
    if (!factory) return;

    void *statics = do_QI(factory, &IID_IDisplayInformationStatics,
                          "QI → IDisplayInformationStatics");
    if (!statics) return;

    void *di = NULL;
    HRESULT hr = CALL_GET_CURRENT(statics, &di);
    check("GetForCurrentView returns S_OK", SUCCEEDED(hr));
    check("GetForCurrentView returns non-NULL", di != NULL);
    if (!di) return;

    UINT32 res = 0;
    hr = CALL_DI_RESOLUTION(di, &res);
    check("get_ResolutionScale returns S_OK", SUCCEEDED(hr));
    check("get_ResolutionScale == 100", res == 100);

    float dpi = 0.0f;
    hr = CALL_DI_LOGICALDPI(di, &dpi);
    check("get_LogicalDpi returns S_OK", SUCCEEDED(hr));
    check("get_LogicalDpi == 96.0f", dpi >= 95.0f && dpi <= 97.0f);

    UINT32 sw = 0, sh = 0;
    CALL_DI_SCREENW(di, &sw);
    CALL_DI_SCREENH(di, &sh);
    check("ScreenWidthInRawPixels == 1920", sw == 1920);
    check("ScreenHeightInRawPixels == 1080", sh == 1080);
}

/* ==========================================================================
 * [2] ApplicationView
 * ========================================================================== */
static void test_app_view(void)
{
    printf("\n[2] ApplicationView\n");

    void *factory = get_factory(L"Windows.UI.ViewManagement.ApplicationView", 41);
    check("RoGetActivationFactory(ApplicationView)", factory != NULL);
    if (!factory) return;

    void *statics = do_QI(factory, &IID_IApplicationViewStatics,
                          "QI → IApplicationViewStatics");
    if (!statics) return;

    /* GetForCurrentView is at slot 9 on IApplicationViewStatics */
    void *av = NULL;
    HRESULT hr = ((HRESULT (WINAPI *)(void *, void **))VTBL(statics)[9])(statics, &av);
    check("GetForCurrentView returns S_OK", SUCCEEDED(hr));
    check("GetForCurrentView returns non-NULL", av != NULL);
    if (!av) return;

    /* put_Title (slot 10) */
    HSTRING hs_title = NULL;
    pWCS(L"Anvil Test", 10, &hs_title);
    hr = ((HRESULT (WINAPI *)(void *, HSTRING))VTBL(av)[10])(av, hs_title);
    check("put_Title returns S_OK", SUCCEEDED(hr));
    pWDS(hs_title);
}

/* ==========================================================================
 * [3] Package
 * ========================================================================== */
static void test_package(void)
{
    printf("\n[3] Windows.ApplicationModel.Package\n");

    void *factory = get_factory(L"Windows.ApplicationModel.Package", 32);
    check("RoGetActivationFactory(Package)", factory != NULL);
    if (!factory) return;

    void *statics = do_QI(factory, &IID_IPackageStatics,
                          "QI → IPackageStatics");
    if (!statics) return;

    /* get_Current at slot 7 */
    void *pkg = NULL;
    HRESULT hr = CALL_PKG_CURRENT(statics, &pkg);
    check("get_Current returns S_OK", SUCCEEDED(hr));
    check("get_Current returns non-NULL IPackage", pkg != NULL);
    if (!pkg) return;

    /* get_Id (slot 6) */
    void *pkgid = NULL;
    hr = CALL_PKG_GET_ID(pkg, &pkgid);
    check("get_Id returns S_OK", SUCCEEDED(hr));
    check("get_Id returns non-NULL IPackageId", pkgid != NULL);
    if (!pkgid) return;

    HSTRING hs_full = NULL, hs_family = NULL;
    CALL_PKGID_FULLNAME(pkgid, &hs_full);
    CALL_PKGID_FAMILY(pkgid, &hs_family);

    const WCHAR *full   = hs_full   ? pWGSRB(hs_full,   NULL) : L"";
    const WCHAR *family = hs_family ? pWGSRB(hs_family, NULL) : L"";

    check("PackageId.FullName is non-empty", full && full[0] != 0);
    check("PackageId.FamilyName contains 'WindowsCalculator'",
          family && wcsstr(family, L"WindowsCalculator") != NULL);

    printf("    FullName:   %ls\n", full);
    printf("    FamilyName: %ls\n", family);

    if (hs_full)   pWDS(hs_full);
    if (hs_family) pWDS(hs_family);

    /* get_InstalledLocation (slot 7 on IPackage) */
    void *folder = NULL;
    hr = CALL_PKG_INSTALLED(pkg, &folder);
    check("get_InstalledLocation returns S_OK", SUCCEEDED(hr));
    check("get_InstalledLocation returns non-NULL", folder != NULL);
    if (folder) {
        HSTRING hs_path = NULL;
        ((HRESULT (WINAPI *)(void *, HSTRING *))VTBL(folder)[12])(folder, &hs_path);
        const WCHAR *path = hs_path ? pWGSRB(hs_path, NULL) : L"";
        check("InstalledLocation.Path is non-empty", path && path[0] != 0);
        printf("    InstalledLocation: %ls\n", path);
        if (hs_path) pWDS(hs_path);
    }
}

/* ==========================================================================
 * [4] ThreadPool RunAsync (synchronous)
 * ========================================================================== */

static int g_work_ran = 0;

/* Minimal IWorkItemHandler: 0=QI, 1=AddRef, 2=Release, 3=Invoke(IAsyncAction*) */
static HRESULT WINAPI wih_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static ULONG   WINAPI wih_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI wih_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI wih_Invoke(void *T, void *action)
    { (void)T; (void)action; g_work_ran = 1; return S_OK; }

static void *s_wih_vtbl[4] = {
    (void *)wih_QI, (void *)wih_AddRef, (void *)wih_Release, (void *)wih_Invoke
};
static void *g_work_handler_vtbl = NULL;  /* points to s_wih_vtbl */
static struct { void **lpVtbl; } g_work_handler;

static void test_threadpool(void)
{
    printf("\n[4] ThreadPool.RunAsync\n");

    g_work_handler.lpVtbl = s_wih_vtbl;

    void *factory = get_factory(L"Windows.System.Threading.ThreadPool", 35);
    check("RoGetActivationFactory(ThreadPool)", factory != NULL);
    if (!factory) return;

    void *statics = do_QI(factory, &IID_IThreadPoolStatics,
                          "QI → IThreadPoolStatics");
    if (!statics) return;

    g_work_ran = 0;
    void *action = NULL;
    /* RunAsync at slot 7 */
    HRESULT hr = ((HRESULT (WINAPI *)(void *, void *, void **))VTBL(statics)[7])(
        statics, &g_work_handler, &action);
    check("RunAsync returns S_OK", SUCCEEDED(hr));
    check("Work item ran synchronously", g_work_ran == 1);
    check("RunAsync returns non-NULL IAsyncAction", action != NULL);

    if (action) {
        /* IAsyncAction QI for IAsyncInfo */
        void *info = NULL;
        hr = CALL_QI(action, &IID_IAsyncInfo, &info);
        check("IAsyncAction QI → IAsyncInfo succeeds", SUCCEEDED(hr) && info != NULL);

        if (info) {
            UINT32 status = 99;
            hr = CALL_ASYNC_STATUS(info, &status);
            check("IAsyncInfo.Status == 1 (Completed)", SUCCEEDED(hr) && status == 1);
        }
    }
}

/* ==========================================================================
 * [5] CoreDispatcher RunAsync
 * ========================================================================== */

static int g_dispatched = 0;

/* IDispatchedHandler: 0=QI, 1=AddRef, 2=Release, 3=Invoke() */
static HRESULT WINAPI dh_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static ULONG   WINAPI dh_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI dh_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI dh_Invoke(void *T) { (void)T; g_dispatched = 1; return S_OK; }

static void *s_dh_vtbl[4] = {
    (void *)dh_QI, (void *)dh_AddRef, (void *)dh_Release, (void *)dh_Invoke
};
static struct { void **lpVtbl; } g_dispatch_handler;

typedef void *(WINAPI *PFN_anvil_get_dispatcher)(void);

static void test_dispatcher(void)
{
    printf("\n[5] CoreDispatcher.RunAsync\n");

    HMODULE hCom = GetModuleHandleA("combase.dll");
    if (!hCom) hCom = LoadLibraryA("combase.dll");
    if (!hCom) { check("combase.dll loaded", 0); return; }

    PFN_anvil_get_dispatcher pGetDisp =
        (PFN_anvil_get_dispatcher)GetProcAddress(hCom, "anvil_get_dispatcher");
    check("anvil_get_dispatcher exported", pGetDisp != NULL);
    if (!pGetDisp) return;

    void *disp = pGetDisp();
    check("anvil_get_dispatcher returns non-NULL", disp != NULL);
    if (!disp) return;

    /* HasThreadAccess (slot 6) */
    BOOL has_access = FALSE;
    HRESULT hr = ((HRESULT (WINAPI *)(void *, BOOL *))VTBL(disp)[6])(disp, &has_access);
    check("get_HasThreadAccess returns S_OK", SUCCEEDED(hr));
    check("get_HasThreadAccess returns TRUE", has_access == TRUE);

    /* RunAsync (slot 8) */
    g_dispatch_handler.lpVtbl = s_dh_vtbl;
    g_dispatched = 0;
    void *action = NULL;
    hr = ((HRESULT (WINAPI *)(void *, UINT32, void *, void **))VTBL(disp)[8])(
        disp, 0 /*Normal*/, &g_dispatch_handler, &action);
    check("RunAsync returns S_OK", SUCCEEDED(hr));
    check("Dispatched handler ran synchronously", g_dispatched == 1);
    check("RunAsync returns non-NULL IAsyncAction", action != NULL);
}

/* --------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------- */
int main(void)
{
    printf("=== Anvil System Stubs Test ===\n");
    printf("ANVIL_PKG_FULLNAME = %s\n",
           getenv("ANVIL_PKG_FULLNAME") ?: "(not set)");

    HMODULE hCom = LoadLibraryA("combase.dll");
    if (!hCom) { fprintf(stderr, "ERROR: combase.dll not loaded\n"); return 1; }

    pRoGet = (PFN_RoGetActivationFactory) GetProcAddress(hCom, "RoGetActivationFactory");
    pWCS   = (PFN_WindowsCreateString)    GetProcAddress(hCom, "WindowsCreateString");
    pWGSRB = (PFN_WindowsGetStringRawBuffer) GetProcAddress(hCom, "WindowsGetStringRawBuffer");
    pWDS   = (PFN_WindowsDeleteString)    GetProcAddress(hCom, "WindowsDeleteString");

    if (!pRoGet || !pWCS || !pWGSRB || !pWDS) {
        fprintf(stderr, "ERROR: missing combase exports\n"); return 1;
    }

    test_display_info();
    test_app_view();
    test_package();
    test_threadpool();
    test_dispatcher();

    printf("\n=== RESULTS: %d pass, %d fail ===\n", g_pass, g_fail);
    if (g_fail == 0) printf("ALL SYSTEM CHECKS PASS\n");
    else             printf("SOME CHECKS FAILED\n");
    return g_fail ? 1 : 0;
}

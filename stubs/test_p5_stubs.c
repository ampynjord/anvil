/*
 * test_p5_stubs.c — Anvil Milestone 5: validate Window, Application, DependencyProperty statics.
 *
 * Tests:
 *   1. IWindowStatics QI + get_Current → non-NULL IWindow object
 *   2. IWindow::get_Bounds → {0,0,1024,768}
 *   3. IWindow::get_Visible → TRUE
 *   4. IWindow::Activate → S_OK
 *   5. IApplicationStatics QI → non-NULL statics pointer
 *   6. Application::Start with stub callback → callback invoked, S_OK
 *   7. IDependencyPropertyStatics QI → non-NULL statics pointer
 *   8. DependencyProperty::Register → non-NULL DP object
 *   9. DependencyProperty::get_UnsetValue → non-NULL sentinel
 *   10. RegisterAttached → non-NULL DP object (distinct from first)
 *
 * Run with:
 *   WINEDLLOVERRIDES="combase=n,b" wine test_p5_stubs.exe
 *
 * Exit codes:
 *   0 — all P5 checks pass
 *   non-zero — failure
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

typedef struct HSTRING__ *HSTRING;
typedef enum { RO_INIT_MULTITHREADED = 1 } RO_INIT_TYPE;
typedef struct { HSTRING Name; unsigned Kind; } WinRTTypeName;

typedef HRESULT (WINAPI *PFN_RoInit)(RO_INIT_TYPE);
typedef void    (WINAPI *PFN_RoUninit)(void);
typedef HRESULT (WINAPI *PFN_WCS)(LPCWSTR, unsigned, HSTRING *);
typedef void    (WINAPI *PFN_WDS)(HSTRING);
typedef HRESULT (WINAPI *PFN_RGAF)(HSTRING, const void *, void **);

static PFN_RoInit   g_RoInit;
static PFN_RoUninit g_RoUninit;
static PFN_WCS      g_WCS;
static PFN_WDS      g_WDS;
static PFN_RGAF     g_RGAF;

/* IIDs */
static const struct { unsigned d1; unsigned short d2, d3; unsigned char d4[8]; }
IID_IActivationFactory = {
    0x00000035, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};
static const struct { unsigned d1; unsigned short d2, d3; unsigned char d4[8]; }
IID_IWindowStatics = {
    0x93328409, 0x4EA1, 0x4AFA,
    { 0x83, 0xDC, 0x0C, 0x4E, 0x73, 0xE8, 0x8B, 0xB1 }
};
static const struct { unsigned d1; unsigned short d2, d3; unsigned char d4[8]; }
IID_IApplicationStatics = {
    0x06499997, 0xF7B4, 0x45FE,
    { 0xB7, 0x63, 0x75, 0x77, 0xD1, 0xD3, 0xCB, 0x4A }
};
static const struct { unsigned d1; unsigned short d2, d3; unsigned char d4[8]; }
IID_IDependencyPropertyStatics = {
    0x49E5F28F, 0x8259, 0x4D5C,
    { 0xAA, 0xE0, 0x83, 0xD5, 0x6D, 0xBB, 0x68, 0xD9 }
};

static int g_pass = 0, g_fail = 0;

#define CHECK(expr, desc) do { \
    if (expr) { printf("  PASS  %s\n", desc); g_pass++; } \
    else { printf("  FAIL  %s\n", desc); g_fail++; } \
} while(0)
#define CHECK_HR(hr, desc) CHECK((hr)==0, desc)

typedef HRESULT (WINAPI *PFNQI)(void *, const void *, void **);
static HRESULT qi(void *obj, const void *iid, void **out)
{
    typedef struct { const void **lpVtbl; } IUnk;
    IUnk *u = (IUnk *)obj;
    return ((PFNQI)u->lpVtbl[0])(obj, iid, out);
}

static void *get_factory(const wchar_t *name, unsigned len)
{
    HSTRING hs = NULL;
    void *factory = NULL;
    g_WCS(name, len, &hs);
    HRESULT hr = g_RGAF(hs, &IID_IActivationFactory, &factory);
    g_WDS(hs);
    if (hr || !factory) {
        printf("  FAIL  RoGetActivationFactory('%ls') hr=0x%08lx\n",
               name, (unsigned long)hr);
        g_fail++;
        return NULL;
    }
    printf("  OK    factory for '%ls' at %p\n", name, factory);
    g_pass++;
    return factory;
}

/* =========================================================================
 * Test 1: IWindowStatics — get_Current + basic IWindow methods
 * ========================================================================= */

/* Stub callback for Application::Start */
static int g_callback_invoked = 0;
static HRESULT WINAPI stub_AppInit_Invoke(void *T, void *args)
{
    (void)T; (void)args;
    g_callback_invoked = 1;
    printf("    [callback] ApplicationInitialized invoked\n");
    return S_OK;
}
static const void *s_stub_cb_vtbl[] = {
    NULL, NULL, NULL, NULL, NULL, NULL,
    (void *)stub_AppInit_Invoke,  /* slot 6 */
};
static struct { const void **lpVtbl; } s_stub_callback = { s_stub_cb_vtbl };

static void test_window_statics(void)
{
    puts("\n[P5] ── IWindowStatics — get_Current ───────────────────────────────");
    void *factory = get_factory(L"Windows.UI.Xaml.Window", 22);
    if (!factory) return;

    void *statics = NULL;
    HRESULT hr = qi(factory, &IID_IWindowStatics, &statics);
    CHECK_HR(hr, "QI(IWindowStatics) → S_OK");
    CHECK(statics != NULL, "IWindowStatics pointer non-NULL");
    if (!statics) return;

    /* get_Current (slot 6) */
    typedef struct { const void **lpVtbl; } IS;
    IS *s = (IS *)statics;
    void *win = NULL;
    typedef HRESULT (WINAPI *PFN_GetCurrent)(void *, void **);
    hr = ((PFN_GetCurrent)s->lpVtbl[6])(statics, &win);
    CHECK_HR(hr, "IWindowStatics::get_Current → S_OK");
    CHECK(win != NULL, "IWindow* non-NULL");
    if (!win) return;

    /* get_Bounds (slot 6 of IWindow) */
    typedef struct { float X, Y, Width, Height; } Rect;
    typedef struct { const void **lpVtbl; } IWin;
    IWin *w = (IWin *)win;
    Rect r = {0};
    typedef HRESULT (WINAPI *PFN_GetBounds)(void *, Rect *);
    hr = ((PFN_GetBounds)w->lpVtbl[6])(win, &r);
    CHECK_HR(hr, "IWindow::get_Bounds → S_OK");
    CHECK(r.Width == 1024.0f && r.Height == 768.0f, "Bounds = 1024×768");

    /* get_Visible (slot 7 of IWindow) */
    BOOL vis = FALSE;
    typedef HRESULT (WINAPI *PFN_GetVis)(void *, BOOL *);
    hr = ((PFN_GetVis)w->lpVtbl[7])(win, &vis);
    CHECK_HR(hr, "IWindow::get_Visible → S_OK");
    CHECK(vis == TRUE, "Visible == TRUE");

    /* Activate (slot 18 of IWindow) */
    typedef HRESULT (WINAPI *PFN_Activate)(void *);
    hr = ((PFN_Activate)w->lpVtbl[18])(win);
    CHECK_HR(hr, "IWindow::Activate → S_OK");
}

/* =========================================================================
 * Test 2: IApplicationStatics — QI + Start with stub callback
 * ========================================================================= */
static void test_application_statics(void)
{
    puts("\n[P5] ── IApplicationStatics — QI + Start ───────────────────────────");
    void *factory = get_factory(L"Windows.UI.Xaml.Application", 27);
    if (!factory) return;

    void *statics = NULL;
    HRESULT hr = qi(factory, &IID_IApplicationStatics, &statics);
    CHECK_HR(hr, "QI(IApplicationStatics) → S_OK");
    CHECK(statics != NULL, "IApplicationStatics pointer non-NULL");
    if (!statics) return;

    /* Start (slot 6) with stub callback */
    typedef struct { const void **lpVtbl; } IS;
    IS *s = (IS *)statics;
    typedef HRESULT (WINAPI *PFN_Start)(void *, void *);
    g_callback_invoked = 0;
    hr = ((PFN_Start)s->lpVtbl[6])(statics, &s_stub_callback);
    CHECK_HR(hr, "Application::Start → S_OK");
    CHECK(g_callback_invoked == 1, "callback was invoked");
}

/* =========================================================================
 * Test 3: IDependencyPropertyStatics — Register + get_UnsetValue + RegisterAttached
 * ========================================================================= */
static void test_dp_statics(void)
{
    puts("\n[P5] ── IDependencyPropertyStatics — Register / UnsetValue ─────────");
    void *factory = get_factory(L"Windows.UI.Xaml.DependencyProperty", 34);
    if (!factory) return;

    void *statics = NULL;
    HRESULT hr = qi(factory, &IID_IDependencyPropertyStatics, &statics);
    CHECK_HR(hr, "QI(IDependencyPropertyStatics) → S_OK");
    CHECK(statics != NULL, "IDependencyPropertyStatics pointer non-NULL");
    if (!statics) return;

    typedef struct { const void **lpVtbl; } IS;
    IS *s = (IS *)statics;

    /* Register (slot 6): pass empty TypeNames and NULL metadata */
    HSTRING hname = NULL;
    g_WCS(L"TestProperty", 12, &hname);
    WinRTTypeName emptyType = { NULL, 0 };
    void *dp1 = NULL;
    typedef HRESULT (WINAPI *PFN_Register)(void *, HSTRING,
                                            WinRTTypeName, WinRTTypeName,
                                            void *, void **);
    hr = ((PFN_Register)s->lpVtbl[6])(statics, hname, emptyType, emptyType, NULL, &dp1);
    g_WDS(hname);
    CHECK_HR(hr, "DependencyProperty::Register → S_OK");
    CHECK(dp1 != NULL, "dp1 non-NULL");

    /* get_UnsetValue (slot 7) */
    void *unset = NULL;
    typedef HRESULT (WINAPI *PFN_UnsetValue)(void *, void **);
    hr = ((PFN_UnsetValue)s->lpVtbl[7])(statics, &unset);
    CHECK_HR(hr, "DependencyProperty::get_UnsetValue → S_OK");
    CHECK(unset != NULL, "UnsetValue non-NULL");

    /* RegisterAttached (slot 8) */
    HSTRING hname2 = NULL;
    g_WCS(L"TestAttachedProperty", 20, &hname2);
    void *dp2 = NULL;
    typedef HRESULT (WINAPI *PFN_RegAttached)(void *, HSTRING,
                                               WinRTTypeName, WinRTTypeName,
                                               void *, void **);
    hr = ((PFN_RegAttached)s->lpVtbl[8])(statics, hname2, emptyType, emptyType, NULL, &dp2);
    g_WDS(hname2);
    CHECK_HR(hr, "DependencyProperty::RegisterAttached → S_OK");
    CHECK(dp2 != NULL, "dp2 non-NULL");
    CHECK(dp1 != dp2, "dp1 and dp2 are distinct objects");
}

/* ── main ── */
int main(void)
{
    HMODULE hCom;
    HRESULT hr;

    puts("[P5] === Milestone 5: Window + Application + DependencyProperty statics ===\n");

    hCom = LoadLibraryW(L"combase.dll");
    if (!hCom) { fprintf(stderr, "FATAL: cannot load combase.dll\n"); return 1; }

#define LOAD(v,n) v=(void*)GetProcAddress(hCom,#n); if(!v){fprintf(stderr,"missing: " #n "\n");return 2;}
    LOAD(g_RoInit,   RoInitialize)
    LOAD(g_RoUninit, RoUninitialize)
    LOAD(g_WCS,      WindowsCreateString)
    LOAD(g_WDS,      WindowsDeleteString)
    LOAD(g_RGAF,     RoGetActivationFactory)
#undef LOAD

    hr = g_RoInit(RO_INIT_MULTITHREADED);
    if (hr) { fprintf(stderr, "RoInitialize failed 0x%08lx\n", (unsigned long)hr); return 3; }

    test_window_statics();
    test_application_statics();
    test_dp_statics();

    g_RoUninit();
    FreeLibrary(hCom);

    puts("");
    printf("[P5] Results: %d pass, %d fail\n", g_pass, g_fail);
    if (g_fail == 0) { puts("[P5] ALL P5 CHECKS PASS — EXIT 0"); return 0; }
    printf("[P5] FAIL — %d checks failed\n", g_fail);
    return 4;
}

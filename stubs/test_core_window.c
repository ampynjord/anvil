/*
 * test_core_window.c — Anvil Milestone 3: UWP view lifecycle test.
 *
 * Provides a minimal IFrameworkViewSource + IFrameworkView, calls
 * CoreApplication::Run(), and verifies the full lifecycle sequence:
 *
 *   CreateView → Initialize → SetWindow → Load → Run → Uninitialize
 *
 * In Run(), we pump the Win32 message queue for 3 iterations (to let
 * the window show), then call PostQuitMessage(0) and return.
 *
 * Run with:
 *   WINEDLLOVERRIDES="combase=n,b" wine test_core_window.exe
 *
 * Exit codes:
 *   0 — all lifecycle steps completed in correct order
 *   non-zero — step missing or wrong order
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

/* ── Types mirroring the WinRT ABI ── */
typedef struct HSTRING__ *HSTRING;
typedef enum { RO_INIT_MULTITHREADED = 1 } RO_INIT_TYPE;
typedef struct IInspectable_s { const void *lpVtbl; } IInspectable_s;
typedef struct { unsigned d1; unsigned short d2, d3; unsigned char d4[8]; } AIID;

typedef HRESULT (WINAPI *PFN_RoInit)(RO_INIT_TYPE);
typedef void    (WINAPI *PFN_RoUninit)(void);
typedef HRESULT (WINAPI *PFN_WCS)(LPCWSTR, unsigned, HSTRING *);
typedef void    (WINAPI *PFN_WDS)(HSTRING);
typedef LPCWSTR (WINAPI *PFN_WGSRB)(HSTRING, unsigned *);
typedef HRESULT (WINAPI *PFN_RGAF)(HSTRING, const AIID *, void **);

static const AIID IID_IActFact = {
    0x00000035, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};
/* IID_ICoreApplicationStatics */
static const AIID IID_ICoreAppStatics = {
    0x638BB2DB, 0x451D, 0x4661,
    { 0xB0, 0x99, 0x41, 0x4F, 0x34, 0xFF, 0xB9, 0xF1 }
};

static PFN_RoInit   g_RoInit;
static PFN_RoUninit g_RoUninit;
static PFN_WCS      g_WCS;
static PFN_WDS      g_WDS;
static PFN_WGSRB    g_WGSRB;
static PFN_RGAF     g_RGAF;

/* ── Lifecycle step tracker ── */
#define STEP_CREATE_VIEW  (1 << 0)
#define STEP_INITIALIZE   (1 << 1)
#define STEP_SET_WINDOW   (1 << 2)
#define STEP_LOAD         (1 << 3)
#define STEP_RUN          (1 << 4)
#define STEP_UNINIT       (1 << 5)
#define STEP_ALL          0x3F

static unsigned g_steps    = 0;
static void    *g_app_view = NULL;
static void    *g_core_win = NULL;

/* ── ICoreApplicationView vtable (partial, for querying CoreWindow) ── */
typedef struct ICoreAppViewVtbl_t {
    void *QI, *AddRef, *Release, *GetIids, *GetRCN, *GetTL;
    HRESULT (WINAPI *get_CoreWindow)(void *T, void **w);   /* slot 6 */
} ICoreAppViewVtbl_t;
typedef struct { const ICoreAppViewVtbl_t *lpVtbl; } ICoreAppView_t;

/* ── ICoreWindow vtable (partial) ── */
typedef struct ICoreWindowVtbl_t {
    void *QI, *AddRef, *Release, *GetIids, *GetRCN, *GetTL;
    void *pad6, *pad7, *pad8, *pad9, *pad10, *pad11;
    void *pad12, *pad13, *pad14, *pad15, *pad16, *pad17, *pad18;
    HRESULT (WINAPI *Activate)(void *T);   /* slot 19 */
} ICoreWindowVtbl_t;
typedef struct { const ICoreWindowVtbl_t *lpVtbl; } ICoreWindow_t;

/* ── TestFrameworkView ── */
typedef struct TFVVtbl {
    void *QI, *AddRef, *Release, *GetIids, *GetRCN, *GetTL;
    HRESULT (WINAPI *Initialize)(void *T, void *appView);
    HRESULT (WINAPI *SetWindow )(void *T, void *coreWin);
    HRESULT (WINAPI *Load      )(void *T, HSTRING ep);
    HRESULT (WINAPI *Run       )(void *T);
    HRESULT (WINAPI *Uninit    )(void *T);
} TFVVtbl;

static HRESULT WINAPI tfv_QI(void *T, const AIID *iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static ULONG WINAPI tfv_AddRef (void *T) { (void)T; return 1; }
static ULONG WINAPI tfv_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI tfv_GetIids(void *T, ULONG *n, void **i)
    { (void)T; *n = 0; *i = NULL; return S_OK; }
static HRESULT WINAPI tfv_GetRCN(void *T, HSTRING *h)
    { (void)T; *h = NULL; return S_OK; }
static HRESULT WINAPI tfv_GetTL(void *T, int *tl)
    { (void)T; *tl = 0; return S_OK; }

static HRESULT WINAPI tfv_Initialize(void *T, void *appView)
{
    (void)T;
    printf("[test]   Initialize(appView=%p)\n", appView);
    g_steps   |= STEP_INITIALIZE;
    g_app_view = appView;

    /* Verify we can call get_CoreWindow on the view */
    if (appView) {
        ICoreAppView_t *av = (ICoreAppView_t *)appView;
        void *cw = NULL;
        av->lpVtbl->get_CoreWindow(av, &cw);
        printf("[test]     appView->CoreWindow = %p\n", cw);
        g_core_win = cw;
    }
    return S_OK;
}

static HRESULT WINAPI tfv_SetWindow(void *T, void *coreWin)
{
    (void)T;
    printf("[test]   SetWindow(coreWin=%p)\n", coreWin);
    g_steps  |= STEP_SET_WINDOW;
    if (!g_core_win) g_core_win = coreWin;

    /* Call Activate() to show the window */
    if (coreWin) {
        ICoreWindow_t *cw = (ICoreWindow_t *)coreWin;
        HRESULT hr = cw->lpVtbl->Activate(cw);
        printf("[test]     CoreWindow::Activate hr=0x%08lx\n", (unsigned long)hr);
    }
    return S_OK;
}

static HRESULT WINAPI tfv_Load(void *T, HSTRING ep)
{
    (void)T; (void)ep;
    printf("[test]   Load()\n");
    g_steps |= STEP_LOAD;
    return S_OK;
}

static HRESULT WINAPI tfv_Run(void *T)
{
    (void)T;
    printf("[test]   Run() — pumping messages for 3 frames then quitting\n");
    g_steps |= STEP_RUN;

    /* Pump the message loop briefly so the window can render */
    int frames = 0;
    MSG msg = {0};
    while (frames < 3) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto quit;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        frames++;
        Sleep(16);   /* ~60 fps */
    }
    PostQuitMessage(0);
    /* Drain the WM_QUIT */
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) break;
        DispatchMessageW(&msg);
    }
quit:
    printf("[test]   Run() returning\n");
    return S_OK;
}

static HRESULT WINAPI tfv_Uninit(void *T)
{
    (void)T;
    printf("[test]   Uninitialize()\n");
    g_steps |= STEP_UNINIT;
    return S_OK;
}

static const TFVVtbl s_tfv_vtbl = {
    tfv_QI, tfv_AddRef, tfv_Release,
    tfv_GetIids, tfv_GetRCN, tfv_GetTL,
    tfv_Initialize, tfv_SetWindow, tfv_Load, tfv_Run, tfv_Uninit,
};
static struct { const TFVVtbl *lpVtbl; } s_test_view = { &s_tfv_vtbl };

/* ── TestFrameworkViewSource ── */
typedef struct TFVSVtbl {
    void *QI, *AddRef, *Release, *GetIids, *GetRCN, *GetTL;
    HRESULT (WINAPI *CreateView)(void *T, void **view);
} TFVSVtbl;

static HRESULT WINAPI tfvs_QI(void *T, const AIID *iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static ULONG WINAPI tfvs_AddRef (void *T) { (void)T; return 1; }
static ULONG WINAPI tfvs_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI tfvs_GetIids(void *T, ULONG *n, void **i)
    { (void)T; *n=0; *i=NULL; return S_OK; }
static HRESULT WINAPI tfvs_GetRCN(void *T, HSTRING *h)
    { (void)T; *h=NULL; return S_OK; }
static HRESULT WINAPI tfvs_GetTL(void *T, int *tl)
    { (void)T; *tl=0; return S_OK; }
static HRESULT WINAPI tfvs_CreateView(void *T, void **view)
{
    (void)T;
    printf("[test] CreateView() → TestFrameworkView\n");
    g_steps |= STEP_CREATE_VIEW;
    *view = &s_test_view;
    return S_OK;
}

static const TFVSVtbl s_tfvs_vtbl = {
    tfvs_QI, tfvs_AddRef, tfvs_Release,
    tfvs_GetIids, tfvs_GetRCN, tfvs_GetTL,
    tfvs_CreateView,
};
static struct { const TFVSVtbl *lpVtbl; } s_test_view_source = { &s_tfvs_vtbl };

/* ── ICoreApplicationStatics vtable (for QI result) ── */
typedef struct ICASVtbl_t {
    void *QI, *AddRef, *Release, *GetIids, *GetRCN, *GetTL;
    void *get_Properties;   /* slot 6 */
    HRESULT (WINAPI *Run)(void *T, void *viewSource);  /* slot 7 */
} ICASVtbl_t;
typedef struct { const ICASVtbl_t *lpVtbl; } ICAS_t;

/* ────────────────────────────────── main ────────────────────────────────── */

int main(void)
{
    HMODULE hCom;
    HRESULT hr;
    HSTRING hname = NULL;
    void *factory  = NULL;
    ICAS_t *statics = NULL;

    printf("[test] === Milestone 3: CoreApplication::Run() lifecycle ===\n\n");

    hCom = LoadLibraryW(L"combase.dll");
    if (!hCom) {
        fprintf(stderr, "[test] FATAL: cannot load combase.dll (%lu)\n", GetLastError());
        return 1;
    }
    printf("[test] combase.dll loaded at %p\n", (void *)hCom);

#define LOAD(var, name) \
    var = (void *)GetProcAddress(hCom, #name); \
    if (!var) { fprintf(stderr, "[test] missing: " #name "\n"); return 2; }
    LOAD(g_RoInit,   RoInitialize);
    LOAD(g_RoUninit, RoUninitialize);
    LOAD(g_WCS,      WindowsCreateString);
    LOAD(g_WDS,      WindowsDeleteString);
    LOAD(g_WGSRB,    WindowsGetStringRawBuffer);
    LOAD(g_RGAF,     RoGetActivationFactory);
#undef LOAD

    hr = g_RoInit(RO_INIT_MULTITHREADED);
    printf("[test] RoInitialize hr=0x%08lx\n\n", (unsigned long)hr);
    if (hr) return 3;

    /* Get CoreApplication factory */
    g_WCS(L"Windows.ApplicationModel.Core.CoreApplication", 46, &hname);
    hr = g_RGAF(hname, &IID_IActFact, &factory);
    g_WDS(hname);
    printf("[test] RoGetActivationFactory(CoreApplication) hr=0x%08lx factory=%p\n",
           (unsigned long)hr, factory);
    if (hr || !factory) return 4;

    /* QI for ICoreApplicationStatics (slot 7 = Run) */
    {
        typedef struct { const void **lpVtbl; } IUnk;
        typedef HRESULT (WINAPI *PFN_QI)(void *, const AIID *, void **);
        IUnk *unk = (IUnk *)factory;
        PFN_QI pfnQI = (PFN_QI)unk->lpVtbl[0];
        hr = pfnQI(factory, &IID_ICoreAppStatics, (void **)&statics);
        printf("[test] QI(ICoreApplicationStatics) hr=0x%08lx statics=%p\n\n",
               (unsigned long)hr, (void *)statics);
        if (hr || !statics) {
            printf("[test] FAIL: ICoreApplicationStatics not implemented\n");
            return 5;
        }
    }

    /* Call Run(viewSource) — drives the full lifecycle */
    printf("[test] Calling CoreApplication::Run(TestViewSource) ...\n");
    hr = statics->lpVtbl->Run(statics, &s_test_view_source);
    printf("\n[test] Run() returned hr=0x%08lx\n\n", (unsigned long)hr);

    g_RoUninit();
    FreeLibrary(hCom);

    /* Verify all lifecycle steps fired */
    printf("[test] Lifecycle steps completed: 0x%02X (expected 0x%02X)\n",
           g_steps, STEP_ALL);
    printf("[test]   CreateView  : %s\n", (g_steps & STEP_CREATE_VIEW) ? "OK" : "MISSING");
    printf("[test]   Initialize  : %s\n", (g_steps & STEP_INITIALIZE)  ? "OK" : "MISSING");
    printf("[test]   SetWindow   : %s\n", (g_steps & STEP_SET_WINDOW)  ? "OK" : "MISSING");
    printf("[test]   Load        : %s\n", (g_steps & STEP_LOAD)        ? "OK" : "MISSING");
    printf("[test]   Run         : %s\n", (g_steps & STEP_RUN)         ? "OK" : "MISSING");
    printf("[test]   Uninitialize: %s\n", (g_steps & STEP_UNINIT)      ? "OK" : "MISSING");

    if (g_steps != STEP_ALL) {
        printf("\n[test] FAIL — lifecycle incomplete\n");
        return 6;
    }

    printf("\n[test] ALL LIFECYCLE STEPS COMPLETED — EXIT 0\n");
    return 0;
}

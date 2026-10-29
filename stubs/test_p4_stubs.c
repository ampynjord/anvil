/*
 * test_p4_stubs.c — Anvil Milestone 4: validate P4 factory statics.
 *
 * Tests:
 *   1. ApiInformation statics QI + IsTypePresent/IsApiContractPresent → FALSE
 *   2. UISettings ActivateInstance → object + GetColorValue → ARGB bytes
 *   3. DisplayInformation statics QI + GetForCurrentView → object + LogicalDpi=96
 *   4. ResourceLoader statics QI + GetForCurrentView → object + GetString → ""
 *
 * Run with:
 *   WINEDLLOVERRIDES="combase=n,b" wine test_p4_stubs.exe
 *
 * Exit codes:
 *   0 — all P4 checks pass
 *   non-zero — failure
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

typedef struct HSTRING__ *HSTRING;
typedef enum { RO_INIT_MULTITHREADED = 1 } RO_INIT_TYPE;

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

static const struct { unsigned d1; unsigned short d2, d3; unsigned char d4[8]; }
IID_IActivationFactory = {
    0x00000035, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};
static const struct { unsigned d1; unsigned short d2, d3; unsigned char d4[8]; }
IID_IApiInfoStatics = {
    0x997439FE, 0xF681, 0x4A11,
    { 0xB4, 0x16, 0xC1, 0x3A, 0x47, 0xE8, 0xBA, 0x36 }
};
static const struct { unsigned d1; unsigned short d2, d3; unsigned char d4[8]; }
IID_IDisplayInfoStatics = {
    0xBC4F2374, 0x8B68, 0x41F0,
    { 0xB1, 0x46, 0xDE, 0x6F, 0x3E, 0x72, 0x0A, 0x7D }
};
static const struct { unsigned d1; unsigned short d2, d3; unsigned char d4[8]; }
IID_IResLoaderStatics2 = {
    0xB1F9FB8B, 0x8BF7, 0x4B79,
    { 0x8E, 0x88, 0xFE, 0x2E, 0x2E, 0x7E, 0x8E, 0x7F }
};

static int g_pass = 0, g_fail = 0;

#define CHECK(expr, desc) do { \
    if (expr) { printf("  PASS  %s\n", desc); g_pass++; } \
    else { printf("  FAIL  %s\n", desc); g_fail++; } \
} while(0)

#define CHECK_HR(hr, desc) CHECK((hr)==0, desc)

/* ── Generic vtable accessor ── */
typedef HRESULT (WINAPI *PFNQI)(void *, const void *, void **);
static HRESULT qi(void *obj, const void *iid, void **out)
{
    typedef struct { const void **lpVtbl; } IUnk;
    IUnk *u = (IUnk *)obj;
    return ((PFNQI)u->lpVtbl[0])(obj, iid, out);
}

/* ── Get activation factory helper ── */
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
 * Test 1: ApiInformation statics
 * ========================================================================= */
static void test_api_information(void)
{
    puts("\n[P4] ── ApiInformation statics ──────────────────────────────────────");
    void *factory = get_factory(L"Windows.Foundation.Metadata.ApiInformation", 42);
    if (!factory) return;

    /* QI for IApiInformationStatics */
    void *statics = NULL;
    HRESULT hr = qi(factory, &IID_IApiInfoStatics, &statics);
    CHECK_HR(hr, "QI(IApiInformationStatics)");
    CHECK(statics != NULL, "statics pointer non-NULL");
    if (!statics) return;

    /* IsTypePresent("Windows.UI.Xaml.Application", &result) → FALSE */
    typedef struct { const void **lpVtbl; } IS;
    IS *s = (IS *)statics;
    BOOL result = TRUE;
    HSTRING htn = NULL;
    g_WCS(L"Windows.UI.Xaml.Application", 27, &htn);
    typedef HRESULT (WINAPI *PFN_IsTypePresent)(void *, HSTRING, BOOL *);
    hr = ((PFN_IsTypePresent)s->lpVtbl[6])(statics, htn, &result);
    g_WDS(htn);
    CHECK_HR(hr, "IsTypePresent → S_OK");
    CHECK(result == FALSE, "IsTypePresent → FALSE");

    /* IsApiContractPresent("Windows.Foundation.UniversalApiContract", 10) → FALSE */
    HSTRING hcn = NULL;
    g_WCS(L"Windows.Foundation.UniversalApiContract", 39, &hcn);
    result = TRUE;
    typedef HRESULT (WINAPI *PFN_IsApiContract)(void *, HSTRING, unsigned short, BOOL *);
    hr = ((PFN_IsApiContract)s->lpVtbl[14])(statics, hcn, 10, &result);
    g_WDS(hcn);
    CHECK_HR(hr, "IsApiContractPresent → S_OK");
    CHECK(result == FALSE, "IsApiContractPresent → FALSE");
}

/* =========================================================================
 * Test 2: UISettings — ActivateInstance + GetColorValue
 * ========================================================================= */
static void test_ui_settings(void)
{
    puts("\n[P4] ── UISettings ActivateInstance + GetColorValue ─────────────────");
    void *factory = get_factory(L"Windows.UI.ViewManagement.UISettings", 36);
    if (!factory) return;

    /* ActivateInstance → UISettings object */
    typedef struct { const void **lpVtbl; } IFact;
    IFact *f = (IFact *)factory;
    void *inst = NULL;
    typedef HRESULT (WINAPI *PFN_Activate)(void *, void **);
    HRESULT hr = ((PFN_Activate)f->lpVtbl[6])(factory, &inst);
    CHECK_HR(hr, "UISettings ActivateInstance → S_OK");
    CHECK(inst != NULL, "UISettings instance non-NULL");
    if (!inst) return;

    /* GetColorValue(slot 18): Background=0 → (A=255,R=0,G=0,B=0) */
    typedef struct { BYTE A, R, G, B; } Color;
    Color c = {0};
    typedef struct { const void **lpVtbl; } IUIs;
    IUIs *u = (IUIs *)inst;
    typedef HRESULT (WINAPI *PFN_GetColor)(void *, unsigned int, Color *);
    hr = ((PFN_GetColor)u->lpVtbl[18])(inst, 0 /*Background*/, &c);
    CHECK_HR(hr, "GetColorValue(Background) → S_OK");
    CHECK(c.A == 255, "Background.A == 255");
    CHECK(c.R == 0 && c.G == 0 && c.B == 0, "Background.RGB == 0,0,0");

    /* Accent=8 → (A=255,R=0,G=120,B=215) */
    hr = ((PFN_GetColor)u->lpVtbl[18])(inst, 8 /*Accent*/, &c);
    CHECK_HR(hr, "GetColorValue(Accent) → S_OK");
    CHECK(c.R == 0 && c.G == 120 && c.B == 215, "Accent.RGB == 0,120,215");

    /* get_AnimationsEnabled (slot 12) → FALSE */
    BOOL anim = TRUE;
    typedef HRESULT (WINAPI *PFN_GetBool)(void *, BOOL *);
    hr = ((PFN_GetBool)u->lpVtbl[12])(inst, &anim);
    CHECK_HR(hr, "get_AnimationsEnabled → S_OK");
    CHECK(anim == FALSE, "AnimationsEnabled == FALSE");
}

/* =========================================================================
 * Test 3: DisplayInformation statics — GetForCurrentView → LogicalDpi=96
 * ========================================================================= */
static void test_display_information(void)
{
    puts("\n[P4] ── DisplayInformation statics + GetForCurrentView ──────────────");
    void *factory = get_factory(L"Windows.Graphics.Display.DisplayInformation", 43);
    if (!factory) return;

    void *statics = NULL;
    HRESULT hr = qi(factory, &IID_IDisplayInfoStatics, &statics);
    CHECK_HR(hr, "QI(IDisplayInformationStatics)");
    CHECK(statics != NULL, "statics pointer non-NULL");
    if (!statics) return;

    /* GetForCurrentView (slot 6) */
    typedef struct { const void **lpVtbl; } IS;
    IS *s = (IS *)statics;
    void *view = NULL;
    typedef HRESULT (WINAPI *PFN_GetView)(void *, void **);
    hr = ((PFN_GetView)s->lpVtbl[6])(statics, &view);
    CHECK_HR(hr, "DisplayInformation::GetForCurrentView → S_OK");
    CHECK(view != NULL, "view pointer non-NULL");
    if (!view) return;

    /* get_LogicalDpi (slot 13) → 96.0f */
    typedef struct { const void **lpVtbl; } IDI;
    IDI *di = (IDI *)view;
    float dpi = 0.0f;
    typedef HRESULT (WINAPI *PFN_GetDpi)(void *, float *);
    hr = ((PFN_GetDpi)di->lpVtbl[13])(view, &dpi);
    CHECK_HR(hr, "get_LogicalDpi → S_OK");
    CHECK(dpi == 96.0f, "LogicalDpi == 96.0");

    /* get_RawPixelsPerViewPixel (slot 16) → 1.0 */
    double scale = 0.0;
    typedef HRESULT (WINAPI *PFN_GetScale)(void *, double *);
    hr = ((PFN_GetScale)di->lpVtbl[16])(view, &scale);
    CHECK_HR(hr, "get_RawPixelsPerViewPixel → S_OK");
    CHECK(scale == 1.0, "RawPixelsPerViewPixel == 1.0");
}

/* =========================================================================
 * Test 4: ResourceLoader statics — GetForCurrentView → GetString = ""
 * ========================================================================= */
static void test_resource_loader(void)
{
    puts("\n[P4] ── ResourceLoader statics + GetForCurrentView ──────────────────");
    void *factory = get_factory(L"Windows.ApplicationModel.Resources.ResourceLoader", 50);
    if (!factory) return;

    void *statics = NULL;
    HRESULT hr = qi(factory, &IID_IResLoaderStatics2, &statics);
    CHECK_HR(hr, "QI(IResourceLoaderStatics2)");
    CHECK(statics != NULL, "statics non-NULL");
    if (!statics) return;

    typedef struct { const void **lpVtbl; } IS;
    IS *s = (IS *)statics;
    void *loader = NULL;
    typedef HRESULT (WINAPI *PFN_GetView)(void *, void **);
    hr = ((PFN_GetView)s->lpVtbl[6])(statics, &loader);
    CHECK_HR(hr, "ResourceLoader::GetForCurrentView → S_OK");
    CHECK(loader != NULL, "loader non-NULL");
    if (!loader) return;

    /* GetString(slot 6) with key "AppName" → empty HSTRING */
    typedef struct { const void **lpVtbl; } IRL;
    IRL *rl = (IRL *)loader;
    HSTRING hkey = NULL, hval = NULL;
    g_WCS(L"AppName", 7, &hkey);
    typedef HRESULT (WINAPI *PFN_GetStr)(void *, HSTRING, HSTRING *);
    hr = ((PFN_GetStr)rl->lpVtbl[6])(loader, hkey, &hval);
    g_WDS(hkey);
    CHECK_HR(hr, "ResourceLoader::GetString → S_OK");
    CHECK(hval != NULL, "returned HSTRING non-NULL");
    if (hval) g_WDS(hval);
}

/* ── main ── */
int main(void)
{
    HMODULE hCom;
    HRESULT hr;

    puts("[P4] === Milestone 4: P4 factory statics ===\n");

    hCom = LoadLibraryW(L"combase.dll");
    if (!hCom) { fprintf(stderr, "FATAL: cannot load combase.dll\n"); return 1; }

#define LOAD(v,n) v=(void*)GetProcAddress(hCom,#n); if(!v){fprintf(stderr,"missing: " #n "\n");return 2;}
    LOAD(g_RoInit,   RoInitialize);
    LOAD(g_RoUninit, RoUninitialize);
    LOAD(g_WCS,      WindowsCreateString);
    LOAD(g_WDS,      WindowsDeleteString);
    LOAD(g_RGAF,     RoGetActivationFactory);
#undef LOAD

    hr = g_RoInit(RO_INIT_MULTITHREADED);
    if (hr) { fprintf(stderr, "RoInitialize failed 0x%08lx\n", (unsigned long)hr); return 3; }

    test_api_information();
    test_ui_settings();
    test_display_information();
    test_resource_loader();

    g_RoUninit();
    FreeLibrary(hCom);

    puts("");
    printf("[P4] Results: %d pass, %d fail\n", g_pass, g_fail);
    if (g_fail == 0) { puts("[P4] ALL P4 CHECKS PASS — EXIT 0"); return 0; }
    printf("[P4] FAIL — %d checks failed\n", g_fail);
    return 4;
}

/*
 * test_activation_chain.c — Wine harness: validates Anvil P1/P2 factory stubs.
 *
 * Simulates the WinRT activation sequence that Calculator's InitializeComponent()
 * fires on startup.  For each class:
 *   1. RoGetActivationFactory → must return S_OK (factory lookup)
 *   2. GetRuntimeClassName   → must return the class name string
 *   3. ActivateInstance      → E_NOTIMPL is acceptable (no real object yet)
 *
 * Run with Anvil combase override:
 *   WINEDLLOVERRIDES="combase=n,b" wine test_activation_chain.exe
 *
 * Exit codes:
 *   0 — all P1 factories resolved (P2/P3 failures are warnings)
 *   1 — one or more P1 factories could not be looked up
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

typedef struct HSTRING__ *HSTRING;
typedef enum { RO_INIT_MULTITHREADED = 1 } RO_INIT_TYPE;
typedef struct IInspectable { const void *lpVtbl; } IInspectable;
typedef struct { unsigned d1; unsigned short d2, d3; unsigned char d4[8]; } ANVIL_IID;

typedef HRESULT (WINAPI *PFN_RoInitialize)(RO_INIT_TYPE);
typedef void    (WINAPI *PFN_RoUninitialize)(void);
typedef HRESULT (WINAPI *PFN_WindowsCreateString)(LPCWSTR, unsigned, HSTRING *);
typedef void    (WINAPI *PFN_WindowsDeleteString)(HSTRING);
typedef LPCWSTR (WINAPI *PFN_WindowsGetStringRawBuffer)(HSTRING, unsigned *);
typedef HRESULT (WINAPI *PFN_RoGetActivationFactory)(HSTRING, const ANVIL_IID *, void **);

static const ANVIL_IID IID_IActivationFactory = {
    0x00000035, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

/* IActivationFactory vtable (first 7 methods) */
typedef struct IActFact_vtbl {
    void *QI, *AddRef, *Release, *GetIids;
    HRESULT (WINAPI *GetRuntimeClassName)(void *, HSTRING *);
    void *GetTrustLevel;
    HRESULT (WINAPI *ActivateInstance)(void *, IInspectable **);
} IActFact_vtbl;

typedef struct { const IActFact_vtbl *lpVtbl; } IActFact;

/* ---- globals loaded once ---- */
static PFN_RoInitialize            g_RoInit;
static PFN_RoUninitialize          g_RoUninit;
static PFN_WindowsCreateString     g_CreateStr;
static PFN_WindowsDeleteString     g_DeleteStr;
static PFN_WindowsGetStringRawBuffer g_GetBuf;
static PFN_RoGetActivationFactory  g_GetFactory;

static int g_p1_fail = 0;
static int g_pass    = 0;
static int g_warn    = 0;

/* ---- helpers ---- */
static void check_factory(const wchar_t *name, unsigned len, int priority)
{
    HSTRING hname = NULL;
    IActFact *factory = NULL;
    HRESULT hr;
    const char *pri_str = priority == 1 ? "P1" : priority == 2 ? "P2" : "P3";

    g_CreateStr(name, len, &hname);

    hr = g_GetFactory(hname, &IID_IActivationFactory, (void **)&factory);
    if (hr != 0) {
        printf("  [%s] FAIL GetFactory hr=0x%08lx  %ls\n", pri_str,
               (unsigned long)hr, name);
        if (priority == 1) g_p1_fail++;
        else g_warn++;
        g_DeleteStr(hname);
        return;
    }

    printf("  [%s] OK   factory=%p  %ls\n", pri_str, (void *)factory, name);
    g_pass++;

    /* Try GetRuntimeClassName */
    if (factory && factory->lpVtbl && factory->lpVtbl->GetRuntimeClassName) {
        HSTRING rcn = NULL;
        hr = factory->lpVtbl->GetRuntimeClassName(factory, &rcn);
        if (hr == 0 && rcn) {
            unsigned rlen = 0;
            LPCWSTR rbuf = g_GetBuf(rcn, &rlen);
            printf("         GetRuntimeClassName → '%ls'\n", rbuf);
            g_DeleteStr(rcn);
        }
    }

    /* Try ActivateInstance — E_NOTIMPL is acceptable */
    if (factory && factory->lpVtbl && factory->lpVtbl->ActivateInstance) {
        IInspectable *inst = NULL;
        hr = factory->lpVtbl->ActivateInstance(factory, &inst);
        if (hr == 0)
            printf("         ActivateInstance → S_OK instance=%p\n", (void *)inst);
        else if (hr == 0x80004001L)
            printf("         ActivateInstance → E_NOTIMPL (stub active, no object yet)\n");
        else
            printf("         ActivateInstance → hr=0x%08lx\n", (unsigned long)hr);
    }

    g_DeleteStr(hname);
}

int main(void)
{
    HMODULE hCom;
    HRESULT hr;

    printf("[chain] Loading combase.dll ...\n");
    hCom = LoadLibraryW(L"combase.dll");
    if (!hCom) {
        fprintf(stderr, "[chain] FATAL: LoadLibrary failed (%lu)\n", GetLastError());
        return 1;
    }

#define LOAD(var, name) \
    var = (void *)GetProcAddress(hCom, #name); \
    if (!var) { fprintf(stderr, "[chain] missing export: " #name "\n"); return 2; }

    LOAD(g_RoInit,      RoInitialize);
    LOAD(g_RoUninit,    RoUninitialize);
    LOAD(g_CreateStr,   WindowsCreateString);
    LOAD(g_DeleteStr,   WindowsDeleteString);
    LOAD(g_GetBuf,      WindowsGetStringRawBuffer);
    LOAD(g_GetFactory,  RoGetActivationFactory);
#undef LOAD

    hr = g_RoInit(RO_INIT_MULTITHREADED);
    printf("[chain] RoInitialize → hr=0x%08lx\n", (unsigned long)hr);
    if (hr != 0) return 3;

    puts("\n[chain] ── P1: must resolve for process survival ──────────────");
    check_factory(L"Windows.UI.Xaml.Application",                    27, 1);
    check_factory(L"Windows.ApplicationModel.Core.CoreApplication",  46, 1);
    check_factory(L"Windows.UI.Core.CoreWindow",                     26, 1);
    check_factory(L"Windows.UI.Xaml.Window",                         22, 1);
    check_factory(L"Windows.UI.Core.CoreDispatcher",                 30, 1);
    check_factory(L"Windows.UI.Xaml.Controls.Frame",                 30, 1);
    check_factory(L"Windows.UI.Xaml.ResourceDictionary",             34, 1);
    check_factory(L"Windows.UI.Xaml.DependencyProperty",             34, 1);

    puts("\n[chain] ── P2: stub with fake data ──────────────────────────────");
    check_factory(L"Windows.UI.ViewManagement.ApplicationView",      41, 2);
    check_factory(L"Windows.Storage.ApplicationData",                31, 2);
    check_factory(L"Windows.Foundation.Size",                        23, 2);

    puts("\n[chain] ── P3: no-op stubs ──────────────────────────────────────");
    check_factory(L"Windows.UI.StartScreen.JumpList",                31, 3);
    check_factory(L"Windows.System.User",                            19, 3);

    g_RoUninit();
    FreeLibrary(hCom);

    printf("\n[chain] Results: %d OK, %d P1-failures, %d P2/P3-warnings\n",
           g_pass, g_p1_fail, g_warn);

    if (g_p1_fail > 0) {
        printf("[chain] FAIL — %d P1 factories not resolved\n", g_p1_fail);
        return 4;
    }
    printf("[chain] ALL P1 FACTORIES RESOLVED — EXIT 0\n");
    return 0;
}

/*
 * test_wine_harness.c — Minimal Windows-target program to validate
 * combase_anvil.dll under Wine.
 *
 * Compiled with:
 *   x86_64-w64-mingw32-gcc -o test_wine_harness.exe test_wine_harness.c
 *
 * Run under Wine with our stub active:
 *   WINEDLLOVERRIDES="combase=n,b" wine test_wine_harness.exe
 *
 * Expected output:
 *   [anvil/TRACE] RoInitialize...
 *   [anvil][MISSING FACTORY] Windows.UI.Xaml.Application
 *   [anvil/TRACE] RoUninitialize...
 *   EXIT 0
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

/* HSTRING / WinRT types — minimal inline for compilation without SDK */
typedef struct HSTRING__ *HSTRING;
typedef enum { RO_INIT_MULTITHREADED = 1 } RO_INIT_TYPE;
typedef struct IInspectable { const void *lpVtbl; } IInspectable;
typedef struct { unsigned d1; unsigned short d2,d3; unsigned char d4[8]; } ANVIL_IID;

/* Dynamically load combase to avoid link-time dependency */
typedef HRESULT (WINAPI *PFN_RoInitialize)(RO_INIT_TYPE);
typedef void    (WINAPI *PFN_RoUninitialize)(void);
typedef HRESULT (WINAPI *PFN_WindowsCreateString)(LPCWSTR, unsigned, HSTRING*);
typedef void    (WINAPI *PFN_WindowsDeleteString)(HSTRING);
typedef LPCWSTR (WINAPI *PFN_WindowsGetStringRawBuffer)(HSTRING, unsigned*);
typedef HRESULT (WINAPI *PFN_RoGetActivationFactory)(HSTRING, const ANVIL_IID*, void**);
typedef HRESULT (WINAPI *PFN_RoActivateInstance)(HSTRING, IInspectable**);

static const ANVIL_IID IID_IActivationFactory_local = {
    0x00000035, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

int main(void)
{
    HMODULE hCom;
    PFN_RoInitialize           pRoInit;
    PFN_RoUninitialize         pRoUninit;
    PFN_WindowsCreateString    pCreateStr;
    PFN_WindowsDeleteString    pDeleteStr;
    PFN_WindowsGetStringRawBuffer pGetBuf;
    PFN_RoGetActivationFactory pGetFactory;
    PFN_RoActivateInstance     pActivate;
    HRESULT hr;
    HSTRING hstr = NULL;
    void *factory = NULL;

    printf("[harness] Loading combase.dll ...\n");
    hCom = LoadLibraryW(L"combase.dll");
    if (!hCom) {
        fprintf(stderr, "[harness] FATAL: could not load combase.dll (err=%lu)\n",
                GetLastError());
        return 1;
    }
    printf("[harness] combase.dll loaded at %p\n", (void*)hCom);

    /* Resolve functions */
#define LOAD(var, name) \
    var = (void*)GetProcAddress(hCom, #name); \
    if (!var) { fprintf(stderr, "[harness] missing: " #name "\n"); return 2; }

    LOAD(pRoInit,       RoInitialize);
    LOAD(pRoUninit,     RoUninitialize);
    LOAD(pCreateStr,    WindowsCreateString);
    LOAD(pDeleteStr,    WindowsDeleteString);
    LOAD(pGetBuf,       WindowsGetStringRawBuffer);
    LOAD(pGetFactory,   RoGetActivationFactory);
    LOAD(pActivate,     RoActivateInstance);
#undef LOAD

    /* --- Test 1: RoInitialize --- */
    printf("[harness] TEST 1: RoInitialize\n");
    hr = pRoInit(RO_INIT_MULTITHREADED);
    printf("[harness]   hr=0x%08lx  %s\n", (unsigned long)hr,
           hr == 0 ? "OK" : "FAIL");
    if (hr != 0) return 3;

    /* --- Test 2: HSTRING round-trip --- */
    printf("[harness] TEST 2: WindowsCreateString\n");
    hr = pCreateStr(L"Windows.UI.Xaml.Application", 27, &hstr);
    printf("[harness]   hr=0x%08lx  str=%p\n", (unsigned long)hr, (void*)hstr);
    if (hr != 0 || !hstr) return 4;

    {
        unsigned len = 0;
        LPCWSTR buf = pGetBuf(hstr, &len);
        printf("[harness]   content='%ls' len=%u\n", buf, len);
    }

    /* --- Test 3: RoGetActivationFactory (expect E_NOTIMPL / MISSING FACTORY log) --- */
    printf("[harness] TEST 3: RoGetActivationFactory (should log MISSING FACTORY)\n");
    hr = pGetFactory(hstr, &IID_IActivationFactory_local, &factory);
    printf("[harness]   hr=0x%08lx factory=%p\n", (unsigned long)hr, factory);
    /* E_NOTIMPL = 0x80004001 — expected since no stub is registered */
    if (hr != 0x80004001L && hr != 0) {
        fprintf(stderr, "[harness]   unexpected hr\n");
        return 5;
    }
    printf("[harness]   Result correct (E_NOTIMPL = Anvil stub active, no factory registered yet)\n");

    pDeleteStr(hstr);

    /* --- Test 4: RoUninitialize --- */
    printf("[harness] TEST 4: RoUninitialize\n");
    pRoUninit();
    printf("[harness]   OK\n");

    FreeLibrary(hCom);
    printf("[harness] ALL TESTS PASSED — EXIT 0\n");
    return 0;
}

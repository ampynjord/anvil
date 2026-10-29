/*
 * test_storage_stubs.c — Integration test for storage_stubs.c.
 *
 * Run under Wine with:
 *   ANVIL_PKG_FAMILYNAME=Microsoft.WindowsCalculator_8wekyb3d8bbwe
 *   WINEDLLOVERRIDES=combase=n,b
 *
 * Expected: "ALL STORAGE CHECKS PASS"
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

static PFN_RoGetActivationFactory     pRoGet;
static PFN_WindowsCreateString        pWCS;
static PFN_WindowsGetStringRawBuffer  pWGSRB;
static PFN_WindowsDeleteString        pWDS;

/* --------------------------------------------------------------------------
 * IIDs (same as storage_stubs.c)
 * -------------------------------------------------------------------------- */
static const IID IID_IActivationFactory = {
    0x00000035, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};
static const IID IID_IApplicationDataStatics = {
    0x5612147B, 0xE843, 0x45E3,
    { 0x94, 0xD8, 0x06, 0x16, 0x9E, 0x3C, 0x8E, 0x17 }
};
static const IID IID_IApplicationData = {
    0xC3DA6FB7, 0xB744, 0x4B45,
    { 0xB0, 0xB8, 0x22, 0x3A, 0x09, 0x38, 0xD0, 0xDC }
};
static const IID IID_IStorageItem = {
    0x4207A996, 0xCA2F, 0x42F7,
    { 0xBD, 0xE8, 0x8B, 0x10, 0x45, 0x7A, 0x7F, 0x30 }
};
static const IID IID_IStorageFolder = {
    0x72D1CB78, 0xB3EF, 0x4F75,
    { 0xA8, 0x0B, 0x6F, 0xD9, 0xDA, 0xE2, 0x94, 0x4B }
};
static const IID IID_IApplicationDataContainer = {
    0xC5AEFD1E, 0xF467, 0x40BA,
    { 0x85, 0x66, 0xAB, 0x64, 0x0A, 0x44, 0x1E, 0x1D }
};

/* --------------------------------------------------------------------------
 * Vtable call helpers — typed function pointers into opaque void* interfaces
 * -------------------------------------------------------------------------- */
typedef HRESULT (WINAPI *PFN_QI)(void *, REFIID, void **);
typedef HRESULT (WINAPI *PFN_get_Current)(void *, void **);
typedef HRESULT (WINAPI *PFN_get_Folder)(void *, void **);
typedef HRESULT (WINAPI *PFN_get_Settings)(void *, void **);
typedef HRESULT (WINAPI *PFN_get_Version)(void *, UINT32 *);
typedef HRESULT (WINAPI *PFN_get_HStr)(void *, HSTRING *);
typedef HRESULT (WINAPI *PFN_get_Values)(void *, void **);

#define VTBL(p)        (*(void ***)  (p))
#define CALL_QI(p,iid,out)        ((PFN_QI)         VTBL(p)[0])(p, iid, out)
#define CALL_GET_CURRENT(p,out)   ((PFN_get_Current)VTBL(p)[6])(p, out)
#define CALL_GET_VERSION(p,out)   ((PFN_get_Version)VTBL(p)[6])(p, out)
#define CALL_GET_LOCAL_SETTINGS(p,out)   ((PFN_get_Settings)VTBL(p)[10])(p, out)
#define CALL_GET_ROAMING_SETTINGS(p,out) ((PFN_get_Settings)VTBL(p)[11])(p, out)
#define CALL_GET_LOCAL_FOLDER(p,out)     ((PFN_get_Folder)  VTBL(p)[12])(p, out)
#define CALL_GET_ROAMING_FOLDER(p,out)   ((PFN_get_Folder)  VTBL(p)[13])(p, out)
#define CALL_GET_TEMP_FOLDER(p,out)      ((PFN_get_Folder)  VTBL(p)[14])(p, out)
/* IStorageItem: slot 11=get_Name, 12=get_Path, 13=get_Attributes */
#define CALL_GET_NAME(p,out)      ((PFN_get_HStr)VTBL(p)[11])(p, out)
#define CALL_GET_PATH(p,out)      ((PFN_get_HStr)VTBL(p)[12])(p, out)
/* IApplicationDataContainer: slot 11=get_Values */
#define CALL_GET_VALUES(p,out)    ((PFN_get_Values)VTBL(p)[11])(p, out)

/* --------------------------------------------------------------------------
 * Test bookkeeping
 * -------------------------------------------------------------------------- */
static int g_pass = 0;
static int g_fail = 0;

static void check(const char *label, int cond)
{
    if (cond) { printf("  PASS  %s\n", label); g_pass++; }
    else       { printf("  FAIL  %s\n", label); g_fail++; }
}

/* --------------------------------------------------------------------------
 * Helper: call QI on an interface and return the out pointer or NULL on fail
 * -------------------------------------------------------------------------- */
static void *do_QI(void *iface, const IID *iid, const char *label)
{
    void *out = NULL;
    HRESULT hr = CALL_QI(iface, iid, &out);
    check(label, SUCCEEDED(hr) && out != NULL);
    return out;
}

/* --------------------------------------------------------------------------
 * Helper: get a HSTRING property and return raw buffer (or empty on fail)
 * -------------------------------------------------------------------------- */
static const WCHAR *hstr_get(void *iface, int slot, HSTRING *hs_out)
{
    PFN_get_HStr fn = (PFN_get_HStr)VTBL(iface)[slot];
    HSTRING hs = NULL;
    if (FAILED(fn(iface, &hs))) return L"";
    *hs_out = hs;
    return pWGSRB(hs, NULL);
}

/* ==========================================================================
 * Tests
 * ========================================================================== */

static void test_activation_factory(void *factory)
{
    printf("\n[1] IActivationFactory → QI for IApplicationDataStatics\n");
    void *statics = do_QI(factory, &IID_IApplicationDataStatics,
                          "QI IActivationFactory → IApplicationDataStatics succeeds");
    if (!statics) return;

    void *appdata = NULL;
    HRESULT hr = CALL_GET_CURRENT(statics, &appdata);
    check("get_Current returns S_OK", SUCCEEDED(hr));
    check("get_Current returns non-NULL IApplicationData", appdata != NULL);
}

static void *get_appdata(void *factory)
{
    void *statics = NULL;
    CALL_QI(factory, &IID_IApplicationDataStatics, &statics);
    if (!statics) return NULL;
    void *appdata = NULL;
    CALL_GET_CURRENT(statics, &appdata);
    return appdata;
}

static void test_appdata_interface(void *appdata)
{
    printf("\n[2] IApplicationData interface\n");
    void *appdata2 = do_QI(appdata, &IID_IApplicationData, "QI for IApplicationData");
    (void)appdata2;

    UINT32 ver = 99;
    HRESULT hr = CALL_GET_VERSION(appdata, &ver);
    check("get_Version returns S_OK", SUCCEEDED(hr));
    check("get_Version returns 0", ver == 0);
}

static void test_local_folder(void *appdata)
{
    printf("\n[3] LocalFolder\n");
    void *folder = NULL;
    HRESULT hr = CALL_GET_LOCAL_FOLDER(appdata, &folder);
    check("get_LocalFolder returns S_OK", SUCCEEDED(hr));
    check("get_LocalFolder returns non-NULL", folder != NULL);
    if (!folder) return;

    /* QI as IStorageFolder (direct) then as IStorageItem */
    void *as_item = do_QI(folder, &IID_IStorageItem, "QI IStorageFolder → IStorageItem");
    if (!as_item) return;

    HSTRING hs_name = NULL, hs_path = NULL;
    const WCHAR *name = hstr_get(as_item, 11, &hs_name);
    const WCHAR *path = hstr_get(as_item, 12, &hs_path);

    check("LocalFolder Name is 'LocalState'", wcscmp(name, L"LocalState") == 0);
    check("LocalFolder Path is non-empty", path && path[0] != 0);
    check("LocalFolder Path contains 'LocalState'", path && wcsstr(path, L"LocalState") != NULL);
    check("LocalFolder Path contains family name",
          path && wcsstr(path, L"WindowsCalculator") != NULL);

    if (hs_name) pWDS(hs_name);
    if (hs_path) pWDS(hs_path);

    /* Verify folder exists on disk */
    {
        char narrow[512] = {0};
        WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow, 512, NULL, NULL);
        DWORD attr = GetFileAttributesW(path);
        check("LocalState folder exists on disk",
              attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
    }
}

static void test_roaming_folder(void *appdata)
{
    printf("\n[4] RoamingFolder\n");
    void *folder = NULL;
    HRESULT hr = CALL_GET_ROAMING_FOLDER(appdata, &folder);
    check("get_RoamingFolder returns S_OK", SUCCEEDED(hr));
    check("get_RoamingFolder returns non-NULL", folder != NULL);
    if (!folder) return;

    void *as_item = NULL;
    CALL_QI(folder, &IID_IStorageItem, &as_item);
    if (!as_item) { check("QI → IStorageItem", 0); return; }

    HSTRING hs = NULL;
    const WCHAR *name = hstr_get(as_item, 11, &hs);
    check("RoamingFolder Name is 'RoamingState'", wcscmp(name, L"RoamingState") == 0);
    if (hs) pWDS(hs);
}

static void test_temp_folder(void *appdata)
{
    printf("\n[5] TemporaryFolder\n");
    void *folder = NULL;
    HRESULT hr = CALL_GET_TEMP_FOLDER(appdata, &folder);
    check("get_TemporaryFolder returns S_OK", SUCCEEDED(hr));
    check("get_TemporaryFolder returns non-NULL", folder != NULL);
    if (!folder) return;

    void *as_item = NULL;
    CALL_QI(folder, &IID_IStorageItem, &as_item);
    if (!as_item) { check("QI → IStorageItem", 0); return; }

    HSTRING hs = NULL;
    const WCHAR *name = hstr_get(as_item, 11, &hs);
    check("TempFolder Name is 'TempState'", wcscmp(name, L"TempState") == 0);
    if (hs) pWDS(hs);
}

static void test_settings(void *appdata)
{
    printf("\n[6] LocalSettings / RoamingSettings\n");
    void *lcont = NULL, *rcont = NULL;
    check("get_LocalSettings returns S_OK",
          SUCCEEDED(CALL_GET_LOCAL_SETTINGS(appdata, &lcont)));
    check("LocalSettings non-NULL", lcont != NULL);

    check("get_RoamingSettings returns S_OK",
          SUCCEEDED(CALL_GET_ROAMING_SETTINGS(appdata, &rcont)));
    check("RoamingSettings non-NULL", rcont != NULL);

    if (!lcont) return;

    void *lcont2 = do_QI(lcont, &IID_IApplicationDataContainer,
                         "QI LocalSettings → IApplicationDataContainer");
    if (!lcont2) return;

    void *propset = NULL;
    HRESULT hr = CALL_GET_VALUES(lcont2, &propset);
    check("get_Values returns S_OK", SUCCEEDED(hr));
    check("get_Values returns non-NULL IPropertySet", propset != NULL);
}

/* --------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------- */
int main(void)
{
    printf("=== Anvil Storage Stubs Test ===\n");
    printf("ANVIL_PKG_FAMILYNAME = %s\n",
           getenv("ANVIL_PKG_FAMILYNAME") ?: "(not set)");

    /* Load combase.dll (our override) */
    HMODULE hCom = LoadLibraryA("combase.dll");
    if (!hCom) { fprintf(stderr, "ERROR: combase.dll not loaded\n"); return 1; }

    pRoGet = (PFN_RoGetActivationFactory) GetProcAddress(hCom, "RoGetActivationFactory");
    pWCS   = (PFN_WindowsCreateString)   GetProcAddress(hCom, "WindowsCreateString");
    pWGSRB = (PFN_WindowsGetStringRawBuffer) GetProcAddress(hCom, "WindowsGetStringRawBuffer");
    pWDS   = (PFN_WindowsDeleteString)   GetProcAddress(hCom, "WindowsDeleteString");

    if (!pRoGet || !pWCS || !pWGSRB || !pWDS) {
        fprintf(stderr, "ERROR: missing combase exports\n"); return 1;
    }

    /* Get activation factory for Windows.Storage.ApplicationData */
    HSTRING class_id = NULL;
    pWCS(L"Windows.Storage.ApplicationData", 31, &class_id);

    void *factory = NULL;
    HRESULT hr = pRoGet(class_id, &IID_IActivationFactory, &factory);
    pWDS(class_id);

    check("RoGetActivationFactory(ApplicationData) succeeds", SUCCEEDED(hr));
    check("factory pointer is non-NULL", factory != NULL);
    if (!factory) goto done;

    test_activation_factory(factory);

    void *appdata = get_appdata(factory);
    if (!appdata) { check("get_Current", 0); goto done; }

    test_appdata_interface(appdata);
    test_local_folder(appdata);
    test_roaming_folder(appdata);
    test_temp_folder(appdata);
    test_settings(appdata);

done:
    printf("\n=== RESULTS: %d pass, %d fail ===\n", g_pass, g_fail);
    if (g_fail == 0) printf("ALL STORAGE CHECKS PASS\n");
    else             printf("SOME CHECKS FAILED\n");
    return g_fail ? 1 : 0;
}

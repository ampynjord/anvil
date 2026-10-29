/*
 * pkg_winrt.c — Anvil: Windows.ApplicationModel.Package WinRT stubs
 *
 * Provides IPackageStatics::get_Current() → IPackage → IPackageId.
 * All data is read from ANVIL_PKG_* environment variables at init time.
 *
 * ANVIL_PKG_FULLNAME   e.g. "Microsoft.WindowsCalculator_10.2103.8.0_x64__8wekyb3d8bbwe"
 * ANVIL_PKG_FAMILYNAME e.g. "Microsoft.WindowsCalculator_8wekyb3d8bbwe"
 * ANVIL_PKG_PATH       e.g. "C:\Program Files\WindowsApps\..."
 * ANVIL_PKG_PUBLISHER  e.g. "CN=Microsoft Corporation, ..."
 *
 * IPackageStatics vtable: 0-5 IInspectable, 6 ActivateInstance, 7 get_Current
 * IPackage vtable:        0-5 IInspectable, 6 get_Id, 7 get_InstalledLocation,
 *                         8 get_IsFramework, 9 get_Dependencies, 10 get_Publisher
 * IPackageId vtable:      0-5 IInspectable, 6 get_Name, 7 get_Version,
 *                         8 get_Architecture, 9 get_ResourceId, 10 get_Publisher,
 *                         11 get_PublisherId, 12 get_FullName, 13 get_FamilyName
 * IStorageItem vtable:    0-5 IInspectable, 6-10 NOTIMPL, 11 get_Name,
 *                         12 get_Path, 13 get_Attributes, 14-15 NOTIMPL
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_pkg);
# define PLOG(fmt,...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# define PLOG(fmt,...) fprintf(stderr, "[anvil/pkg_wrt] " fmt "\n", ##__VA_ARGS__)
#endif

extern HRESULT WINAPI WindowsCreateString(LPCWSTR src, UINT32 len, HSTRING *out);
extern LPCWSTR WINAPI WindowsGetStringRawBuffer(HSTRING str, UINT32 *len);
extern int     anvil_register_factory(const WCHAR *class_id, void *factory);

static HRESULT WINAPI pw_notimpl(void *T, ...) { (void)T; return E_NOTIMPL; }
#define NOTIMPL ((void *)pw_notimpl)

static ULONG   WINAPI pw_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI pw_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI pw_GetIids(void *T, ULONG *n, IID **ids)
    { (void)T; *n = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI pw_GetTL(void *T, TrustLevel *tl)
    { (void)T; *tl = BaseTrust; return S_OK; }
static HRESULT WINAPI pw_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }

/* =========================================================================
 * Parsed package state (populated at first access)
 * ========================================================================= */

typedef struct {
    UINT16 Major, Minor, Build, Revision;
} PackageVersion;

static WCHAR  g_name[128]      = {0};  /* e.g. "Microsoft.WindowsCalculator"         */
static WCHAR  g_full[256]      = {0};  /* ANVIL_PKG_FULLNAME                         */
static WCHAR  g_family[128]    = {0};  /* ANVIL_PKG_FAMILYNAME                       */
static WCHAR  g_path[512]      = {0};  /* ANVIL_PKG_PATH                             */
static WCHAR  g_publisher[256] = {0};  /* ANVIL_PKG_PUBLISHER                        */
static WCHAR  g_pubid[32]      = {0};  /* publisher ID (tail of FAMILYNAME)          */
static PackageVersion g_version = {0};
static BOOL   g_pkg_ready = FALSE;

static void pkg_init(void)
{
    if (g_pkg_ready) return;
    g_pkg_ready = TRUE;

    /* ANVIL_PKG_FULLNAME — wide via GetEnvironmentVariableW */
    GetEnvironmentVariableW(L"ANVIL_PKG_FULLNAME",   g_full,      256);
    GetEnvironmentVariableW(L"ANVIL_PKG_FAMILYNAME", g_family,    128);
    GetEnvironmentVariableW(L"ANVIL_PKG_PATH",       g_path,      512);
    GetEnvironmentVariableW(L"ANVIL_PKG_PUBLISHER",  g_publisher, 256);

    if (!g_full[0]) {
        /* Defaults if env vars not set */
        wcscpy(g_full,      L"UnknownPackage_1.0.0.0_x64__0000000000000");
        wcscpy(g_family,    L"UnknownPackage_0000000000000");
        wcscpy(g_path,      L"C:\\Program Files\\WindowsApps\\UnknownPackage");
        wcscpy(g_publisher, L"CN=Unknown");
    }

    /*
     * Parse FULLNAME: "Name_Major.Minor.Build.Revision_Arch__PubId"
     * Extract name (up to first '_'), then version (up to next '_').
     */
    WCHAR tmp[256];
    wcscpy(tmp, g_full);

    WCHAR *p = wcschr(tmp, L'_');
    if (p) {
        *p = L'\0';
        wcsncpy(g_name, tmp, 127);
        *p = L'_';

        /* version segment after first '_' */
        WCHAR *ver = p + 1;
        unsigned ma = 0, mi = 0, bu = 0, re = 0;
        swscanf(ver, L"%u.%u.%u.%u", &ma, &mi, &bu, &re);
        g_version.Major    = (UINT16)ma;
        g_version.Minor    = (UINT16)mi;
        g_version.Build    = (UINT16)bu;
        g_version.Revision = (UINT16)re;
    } else {
        wcsncpy(g_name, g_full, 127);
    }

    /* Publisher ID = part after '_' in FAMILYNAME */
    WCHAR *pid = wcsrchr(g_family, L'_');
    if (pid) wcsncpy(g_pubid, pid + 1, 31);
    else      wcsncpy(g_pubid, L"000000000000", 31);

    PLOG("pkg_init: name=%ls ver=%u.%u.%u.%u",
         g_name,
         (unsigned)g_version.Major, (unsigned)g_version.Minor,
         (unsigned)g_version.Build, (unsigned)g_version.Revision);
}

/* =========================================================================
 * AnvilInstallFolder — minimal IStorageItem/IStorageFolder for InstalledLocation
 *
 * Flat vtable, QI returns self for all IIDs (good enough for PoC).
 * IStorageItem get_Name=11, get_Path=12.
 * ========================================================================= */

typedef struct { void **lpVtbl; } AnvilInstallFolder;

static HRESULT WINAPI ifld_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.Storage.StorageFolder", 29, s); }

static HRESULT WINAPI ifld_get_Name(void *T, HSTRING *out)
{
    (void)T; pkg_init();
    /* last path component */
    WCHAR *p = wcsrchr(g_path, L'\\');
    LPCWSTR name = p ? p + 1 : g_path;
    return WindowsCreateString(name, (UINT32)wcslen(name), out);
}

static HRESULT WINAPI ifld_get_Path(void *T, HSTRING *out)
{
    (void)T; pkg_init();
    return WindowsCreateString(g_path, (UINT32)wcslen(g_path), out);
}

static HRESULT WINAPI ifld_get_Attributes(void *T, UINT32 *out)
    { (void)T; *out = 0x10; /* FILE_ATTRIBUTE_DIRECTORY */ return S_OK; }

static void *s_ifld_vtbl[16] = {
    (void *)pw_QI, (void *)pw_AddRef, (void *)pw_Release,
    (void *)pw_GetIids, (void *)ifld_GetRCN, (void *)pw_GetTL,
    NOTIMPL, NOTIMPL, NOTIMPL, NOTIMPL, NOTIMPL, /* 6-10 */
    (void *)ifld_get_Name,        /* 11 */
    (void *)ifld_get_Path,        /* 12 */
    (void *)ifld_get_Attributes,  /* 13 */
    NOTIMPL,                      /* 14 get_DateCreated */
    NOTIMPL,                      /* 15 IsOfType        */
};

static AnvilInstallFolder g_install_folder = { s_ifld_vtbl };

/* =========================================================================
 * AnvilPackageId — IPackageId
 *
 * vtable:
 *   0-5  IInspectable
 *   6    get_Name (HSTRING*)
 *   7    get_Version (PackageVersion*)
 *   8    get_Architecture (ProcessorArchitecture* / UINT32*) — X64=9
 *   9    get_ResourceId (HSTRING*)
 *   10   get_Publisher (HSTRING*)
 *   11   get_PublisherId (HSTRING*)
 *   12   get_FullName (HSTRING*)
 *   13   get_FamilyName (HSTRING*)
 * ========================================================================= */

typedef struct { void **lpVtbl; } AnvilPackageId;

static HRESULT WINAPI pid_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.ApplicationModel.PackageId", 34, s); }

static HRESULT WINAPI pid_get_Name(void *T, HSTRING *out)
    { (void)T; pkg_init(); return WindowsCreateString(g_name, (UINT32)wcslen(g_name), out); }

static HRESULT WINAPI pid_get_Version(void *T, PackageVersion *out)
    { (void)T; pkg_init(); *out = g_version; return S_OK; }

static HRESULT WINAPI pid_get_Architecture(void *T, UINT32 *out)
    { (void)T; *out = 9; /* X64 */ return S_OK; }

static HRESULT WINAPI pid_get_ResourceId(void *T, HSTRING *out)
    { (void)T; return WindowsCreateString(L"", 0, out); }

static HRESULT WINAPI pid_get_Publisher(void *T, HSTRING *out)
{
    (void)T; pkg_init();
    return WindowsCreateString(g_publisher, (UINT32)wcslen(g_publisher), out);
}

static HRESULT WINAPI pid_get_PublisherId(void *T, HSTRING *out)
{
    (void)T; pkg_init();
    return WindowsCreateString(g_pubid, (UINT32)wcslen(g_pubid), out);
}

static HRESULT WINAPI pid_get_FullName(void *T, HSTRING *out)
    { (void)T; pkg_init(); return WindowsCreateString(g_full, (UINT32)wcslen(g_full), out); }

static HRESULT WINAPI pid_get_FamilyName(void *T, HSTRING *out)
    { (void)T; pkg_init(); return WindowsCreateString(g_family, (UINT32)wcslen(g_family), out); }

static void *s_pid_vtbl[14] = {
    (void *)pw_QI, (void *)pw_AddRef, (void *)pw_Release,
    (void *)pw_GetIids, (void *)pid_GetRCN, (void *)pw_GetTL,
    (void *)pid_get_Name,         /* 6  */
    (void *)pid_get_Version,      /* 7  */
    (void *)pid_get_Architecture, /* 8  */
    (void *)pid_get_ResourceId,   /* 9  */
    (void *)pid_get_Publisher,    /* 10 */
    (void *)pid_get_PublisherId,  /* 11 */
    (void *)pid_get_FullName,     /* 12 */
    (void *)pid_get_FamilyName,   /* 13 */
};

static AnvilPackageId g_pkg_id = { s_pid_vtbl };

/* =========================================================================
 * AnvilPackage — IPackage
 *
 * vtable:
 *   0-5  IInspectable
 *   6    get_Id (IPackageId**)
 *   7    get_InstalledLocation (IStorageFolder**)
 *   8    get_IsFramework (BOOL*)
 *   9    get_Dependencies (IVectorView**)
 *   10   get_Publisher (HSTRING*) [IPackage2]
 * ========================================================================= */

typedef struct { void **lpVtbl; } AnvilPackage;

static HRESULT WINAPI pkg_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.ApplicationModel.Package", 32, s); }

static HRESULT WINAPI pkg_get_Id(void *T, void **out)
    { (void)T; *out = &g_pkg_id; return S_OK; }

static HRESULT WINAPI pkg_get_InstalledLocation(void *T, void **out)
    { (void)T; *out = &g_install_folder; return S_OK; }

static HRESULT WINAPI pkg_get_IsFramework(void *T, BOOL *out)
    { (void)T; *out = FALSE; return S_OK; }

static HRESULT WINAPI pkg_get_Publisher(void *T, HSTRING *out)
{
    (void)T; pkg_init();
    return WindowsCreateString(g_publisher, (UINT32)wcslen(g_publisher), out);
}

static void *s_pkg_vtbl[11] = {
    (void *)pw_QI, (void *)pw_AddRef, (void *)pw_Release,
    (void *)pw_GetIids, (void *)pkg_GetRCN, (void *)pw_GetTL,
    (void *)pkg_get_Id,               /* 6  */
    (void *)pkg_get_InstalledLocation,/* 7  */
    (void *)pkg_get_IsFramework,      /* 8  */
    NOTIMPL,                          /* 9  get_Dependencies */
    (void *)pkg_get_Publisher,        /* 10 get_Publisher    */
};

static AnvilPackage g_package = { s_pkg_vtbl };

/* =========================================================================
 * IPackageStatics factory
 *
 * vtable:
 *   0-5  IInspectable
 *   6    ActivateInstance
 *   7    get_Current (IPackage**)
 * ========================================================================= */

typedef struct { void **lpVtbl; } PkgStaticsObj;

static HRESULT WINAPI pkgs_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.ApplicationModel.Package", 32, s); }

static HRESULT WINAPI pkgs_get_Current(void *T, void **out)
    { (void)T; pkg_init(); *out = &g_package; return S_OK; }

static void *s_pkgs_fact_vtbl[8] = {
    (void *)pw_QI, (void *)pw_AddRef, (void *)pw_Release,
    (void *)pw_GetIids, (void *)pkgs_GetRCN, (void *)pw_GetTL,
    NOTIMPL,                      /* 6 ActivateInstance */
    (void *)pkgs_get_Current,     /* 7 get_Current      */
};

static PkgStaticsObj g_pkg_factory = { s_pkgs_fact_vtbl };

/* =========================================================================
 * Registration
 * ========================================================================= */

void anvil_register_pkg_winrt_factories(void)
{
    pkg_init();
    anvil_register_factory(L"Windows.ApplicationModel.Package", &g_pkg_factory);
    PLOG("anvil_register_pkg_winrt_factories: Package factory registered");
}

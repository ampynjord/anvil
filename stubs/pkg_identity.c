/*
 * pkg_identity.c — Anvil package identity stubs.
 *
 * Implements the Windows AppModel package identity APIs by reading from
 * environment variables set by the anvil_msix.py launcher:
 *
 *   ANVIL_PKG_FULLNAME    e.g. "Microsoft.WindowsCalculator_10.2103.8.0_x64__8wekyb3d8bbwe"
 *   ANVIL_PKG_FAMILYNAME  e.g. "Microsoft.WindowsCalculator_8wekyb3d8bbwe"
 *   ANVIL_PKG_PATH        e.g. "C:\Program Files\WindowsApps\Microsoft.WindowsCalculator_..."
 *   ANVIL_APP_AUMID       e.g. "Microsoft.WindowsCalculator_8wekyb3d8bbwe!App"
 *
 * These functions are hooked into kernelbase.dll at DLL_PROCESS_ATTACH
 * (see combase_anvil.c) via the inline hook engine.
 *
 * APIs implemented:
 *   GetCurrentPackageId           — fills PACKAGE_ID in caller buffer
 *   GetCurrentPackageFullName     — returns full package name wide string
 *   GetCurrentPackageFamilyName   — returns family name wide string
 *   GetCurrentApplicationUserModelId — returns AUMID wide string
 *   GetCurrentPackagePath         — returns install path wide string
 *   GetPackagePathByFullName       — same, by explicit name
 *   OpenPackageInfoByFullName      — returns opaque handle (stub)
 *   ClosePackageInfo               — releases handle (stub)
 *
 * Error codes:
 *   0 (ERROR_SUCCESS)          — ok
 *   122 (ERROR_INSUFFICIENT_BUFFER) — buffer too small, *len updated
 *   15700 (APPMODEL_ERROR_NO_PACKAGE) — env vars not set
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_pkg);
# define PKGLOG(fmt,...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# define PKGLOG(fmt,...) fprintf(stderr, "[anvil/pkg] " fmt "\n", ##__VA_ARGS__)
#endif

#include <string.h>
#include <stdlib.h>
#include <wchar.h>

#define APPMODEL_ERROR_NO_PACKAGE  15700L
#define APPMODEL_ERROR_NO_APPLICATION 15703L

/* =========================================================================
 * Package info cache — parsed once from env vars at first call.
 * ========================================================================= */

typedef struct {
    WCHAR fullname[256];     /* Name_Version_Arch_Resource_PublisherId */
    WCHAR familyname[128];   /* Name_PublisherId */
    WCHAR path[512];         /* C:\Program Files\WindowsApps\<fullname> */
    WCHAR aumid[256];        /* FamilyName!AppId */
    WCHAR name[128];         /* just the package name */
    WCHAR publisher[128];    /* CN=... */
    WCHAR publisherid[16];   /* 8wekyb3d8bbwe */
    WCHAR version[32];       /* "10.2103.8.0" */
    WCHAR arch[16];          /* "x64" / "x86" / "neutral" */
    int   loaded;
} AnvilPkgInfo;

static AnvilPkgInfo g_pkg;

static WCHAR *getenv_wide(const char *name)
{
    static WCHAR buf[512];
    const char *v = getenv(name);
    if (!v) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, v, -1, buf, 512);
    return buf;
}

/*
 * Parse "Name_Version_Arch_Resource_PublisherId" into components.
 * Format: PackageName_Major.Minor.Build.Revision_Architecture_ResourceId_PublisherId
 * The ResourceId part may be empty, giving Name_Version_Arch__PublisherId.
 */
static void parse_fullname(const WCHAR *full)
{
    /* Split on '_' — fields: name, version, arch, resourceid, publisherid */
    WCHAR tmp[256];
    wcscpy(tmp, full);

    WCHAR *parts[5] = {0};
    int n = 0;
    WCHAR *p = tmp, *start = tmp;
    for (; *p && n < 4; p++) {
        if (*p == L'_') { *p = 0; parts[n++] = start; start = p + 1; }
    }
    parts[n++] = start;

    if (n >= 1) wcscpy(g_pkg.name,        parts[0]);
    if (n >= 2) wcscpy(g_pkg.version,     parts[1]);
    if (n >= 3) wcscpy(g_pkg.arch,        parts[2]);
    if (n >= 5) wcscpy(g_pkg.publisherid, parts[4]);

    /* Family name = name + "_" + publisherid */
    if (n >= 5) {
        wcscpy(g_pkg.familyname, g_pkg.name);
        wcscat(g_pkg.familyname, L"_");
        wcscat(g_pkg.familyname, g_pkg.publisherid);
    }
}

static void pkg_load(void)
{
    WCHAR *v;
    if (g_pkg.loaded) return;
    g_pkg.loaded = 1;

    v = getenv_wide("ANVIL_PKG_FULLNAME");
    if (v) { wcscpy(g_pkg.fullname, v); parse_fullname(v); }

    v = getenv_wide("ANVIL_PKG_FAMILYNAME");
    if (v) wcscpy(g_pkg.familyname, v);

    v = getenv_wide("ANVIL_PKG_PATH");
    if (v) wcscpy(g_pkg.path, v);

    v = getenv_wide("ANVIL_APP_AUMID");
    if (v) wcscpy(g_pkg.aumid, v);

    v = getenv_wide("ANVIL_PKG_PUBLISHER");
    if (v) wcscpy(g_pkg.publisher, v);

    if (g_pkg.fullname[0])
        PKGLOG("package identity loaded: %ls", g_pkg.fullname);
    else
        PKGLOG("ANVIL_PKG_FULLNAME not set — returning APPMODEL_ERROR_NO_PACKAGE");
}

static int pkg_available(void)
{
    pkg_load();
    return g_pkg.fullname[0] != 0;
}

/* =========================================================================
 * PACKAGE_ID structure layout (Windows SDK)
 * ========================================================================= */

typedef struct {
    UINT16 Revision, Build, Minor, Major;
} ANVIL_PKG_VERSION;

typedef struct {
    UINT32 reserved;
    UINT32 processorArchitecture;   /* 0=neutral, 5=x86, 9=x64, 12=arm64 */
    ANVIL_PKG_VERSION version;
    PWSTR  name;
    PWSTR  publisher;
    PWSTR  resourceId;
    PWSTR  publisherId;
} ANVIL_PACKAGE_ID;

static UINT32 arch_code(const WCHAR *arch)
{
    if (!wcsicmp(arch, L"x64"))     return 9;
    if (!wcsicmp(arch, L"x86"))     return 5;
    if (!wcsicmp(arch, L"arm64"))   return 12;
    if (!wcsicmp(arch, L"arm"))     return 5;
    return 0;  /* neutral */
}

static void parse_version(const WCHAR *s, ANVIL_PKG_VERSION *v)
{
    int a=0,b=0,c=0,d=0;
    swscanf(s, L"%d.%d.%d.%d", &a, &b, &c, &d);
    v->Major = (UINT16)a; v->Minor = (UINT16)b;
    v->Build = (UINT16)c; v->Revision = (UINT16)d;
}

/* =========================================================================
 * GetCurrentPackageId
 *
 * Buffer layout: [ANVIL_PACKAGE_ID struct] [name\0] [publisher\0]
 *                [resourceId\0] [publisherId\0]
 * All string pointers in the struct point into the same buffer.
 * ========================================================================= */
LONG WINAPI anvil_GetCurrentPackageId(UINT32 *bufferLength, BYTE *buffer)
{
    UINT32 needed, off;
    ANVIL_PACKAGE_ID *pid;

    pkg_load();
    if (!pkg_available()) return APPMODEL_ERROR_NO_PACKAGE;

    /* Compute required size */
    needed = sizeof(ANVIL_PACKAGE_ID)
           + (UINT32)((wcslen(g_pkg.name)        + 1) * sizeof(WCHAR))
           + (UINT32)((wcslen(g_pkg.publisher)   + 1) * sizeof(WCHAR))
           + (UINT32)((                           1) * sizeof(WCHAR))  /* empty resourceId */
           + (UINT32)((wcslen(g_pkg.publisherid) + 1) * sizeof(WCHAR));

    if (!buffer || *bufferLength < needed) {
        *bufferLength = needed;
        return ERROR_INSUFFICIENT_BUFFER;
    }

    memset(buffer, 0, needed);
    pid = (ANVIL_PACKAGE_ID *)buffer;
    pid->reserved = 0;
    pid->processorArchitecture = arch_code(g_pkg.arch);
    parse_version(g_pkg.version, &pid->version);

    off = sizeof(ANVIL_PACKAGE_ID);

    pid->name = (PWSTR)(buffer + off);
    wcscpy(pid->name, g_pkg.name);
    off += (UINT32)((wcslen(g_pkg.name) + 1) * sizeof(WCHAR));

    pid->publisher = (PWSTR)(buffer + off);
    wcscpy(pid->publisher, g_pkg.publisher);
    off += (UINT32)((wcslen(g_pkg.publisher) + 1) * sizeof(WCHAR));

    pid->resourceId = (PWSTR)(buffer + off);
    pid->resourceId[0] = 0;
    off += sizeof(WCHAR);

    pid->publisherId = (PWSTR)(buffer + off);
    wcscpy(pid->publisherId, g_pkg.publisherid);

    *bufferLength = needed;
    PKGLOG("GetCurrentPackageId → %ls", g_pkg.fullname);
    return ERROR_SUCCESS;
}

/* =========================================================================
 * Wide-string helper: copy g_pkg.xxx into caller buffer, update *len.
 * ========================================================================= */
static LONG wstr_out(UINT32 *len, WCHAR *buf, const WCHAR *src)
{
    UINT32 need = (UINT32)wcslen(src) + 1;
    if (!buf || *len < need) { *len = need; return ERROR_INSUFFICIENT_BUFFER; }
    wcscpy(buf, src);
    *len = need;
    return ERROR_SUCCESS;
}

LONG WINAPI anvil_GetCurrentPackageFullName(UINT32 *len, WCHAR *fullName)
{
    pkg_load();
    if (!pkg_available()) return APPMODEL_ERROR_NO_PACKAGE;
    PKGLOG("GetCurrentPackageFullName → %ls", g_pkg.fullname);
    return wstr_out(len, fullName, g_pkg.fullname);
}

LONG WINAPI anvil_GetCurrentPackageFamilyName(UINT32 *len, WCHAR *familyName)
{
    pkg_load();
    if (!pkg_available()) return APPMODEL_ERROR_NO_PACKAGE;
    PKGLOG("GetCurrentPackageFamilyName → %ls", g_pkg.familyname);
    return wstr_out(len, familyName, g_pkg.familyname);
}

LONG WINAPI anvil_GetCurrentApplicationUserModelId(UINT32 *len, WCHAR *id)
{
    pkg_load();
    if (!g_pkg.aumid[0]) return APPMODEL_ERROR_NO_APPLICATION;
    PKGLOG("GetCurrentApplicationUserModelId → %ls", g_pkg.aumid);
    return wstr_out(len, id, g_pkg.aumid);
}

LONG WINAPI anvil_GetCurrentPackagePath(UINT32 *len, WCHAR *path)
{
    pkg_load();
    if (!pkg_available()) return APPMODEL_ERROR_NO_PACKAGE;
    if (!g_pkg.path[0]) {
        /* Synthesise from Wine prefix + fullname if not set */
        const char *pfx = getenv("WINEPREFIX");
        if (!pfx) pfx = getenv("HOME");
        WCHAR base[512];
        MultiByteToWideChar(CP_UTF8, 0, pfx, -1, base, 512);
        wcscat(base, L"\\drive_c\\Program Files\\WindowsApps\\");
        wcscat(base, g_pkg.fullname);
        wcscpy(g_pkg.path, base);
    }
    PKGLOG("GetCurrentPackagePath → %ls", g_pkg.path);
    return wstr_out(len, path, g_pkg.path);
}

LONG WINAPI anvil_GetPackagePathByFullName(LPCWSTR fullName, UINT32 *len, WCHAR *path)
{
    (void)fullName;
    return anvil_GetCurrentPackagePath(len, path);
}

/* Opaque handle stub — just return a non-NULL token */
static int g_pkg_info_handle = 0xA991;

LONG WINAPI anvil_OpenPackageInfoByFullName(LPCWSTR fullName, UINT32 reserved,
                                            void **packageInfoReference)
{
    (void)fullName; (void)reserved;
    pkg_load();
    if (!pkg_available()) return APPMODEL_ERROR_NO_PACKAGE;
    *packageInfoReference = &g_pkg_info_handle;
    PKGLOG("OpenPackageInfoByFullName → handle %p", *packageInfoReference);
    return ERROR_SUCCESS;
}

LONG WINAPI anvil_ClosePackageInfo(void *packageInfoReference)
{
    (void)packageInfoReference;
    return ERROR_SUCCESS;
}

/* =========================================================================
 * Public exports under Windows API names.
 * On Wine versions that lack these in kernelbase/kernel32, callers can
 * resolve them directly from combase.dll.
 * ========================================================================= */
LONG WINAPI GetCurrentPackageId(UINT32 *bufLen, BYTE *buf)
    { return anvil_GetCurrentPackageId(bufLen, buf); }
LONG WINAPI GetCurrentPackageFullName(UINT32 *len, WCHAR *name)
    { return anvil_GetCurrentPackageFullName(len, name); }
LONG WINAPI GetCurrentPackageFamilyName(UINT32 *len, WCHAR *name)
    { return anvil_GetCurrentPackageFamilyName(len, name); }
LONG WINAPI GetCurrentApplicationUserModelId(UINT32 *len, WCHAR *id)
    { return anvil_GetCurrentApplicationUserModelId(len, id); }
LONG WINAPI GetCurrentPackagePath(UINT32 *len, WCHAR *path)
    { return anvil_GetCurrentPackagePath(len, path); }
LONG WINAPI GetPackagePathByFullName(LPCWSTR full, UINT32 *len, WCHAR *path)
    { return anvil_GetPackagePathByFullName(full, len, path); }
LONG WINAPI OpenPackageInfoByFullName(LPCWSTR full, UINT32 reserved, void **ref)
    { return anvil_OpenPackageInfoByFullName(full, reserved, ref); }
LONG WINAPI ClosePackageInfo(void *ref)
    { return anvil_ClosePackageInfo(ref); }

/* =========================================================================
 * Registration — called from combase_anvil.c DllMain via hook engine.
 * ========================================================================= */
#include "hook_engine.h"

#define MAX_PKG_HOOKS 24   /* up to 3 modules × 8 procs */
static AnvilHook g_pkg_hooks[MAX_PKG_HOOKS];
static int       g_pkg_hook_count = 0;

static const char *s_pkg_modules[] = {
    "kernelbase", "kernel32", "api-ms-win-appmodel-runtime-l1-1-0", NULL
};

/* Hook a proc in every loaded module that exports it. */
static void hook_one(const char *proc, void *replacement)
{
    int i;
    for (i = 0; s_pkg_modules[i]; i++) {
        if (g_pkg_hook_count >= MAX_PKG_HOOKS) break;
        if (anvil_hook_proc(&g_pkg_hooks[g_pkg_hook_count],
                            s_pkg_modules[i], proc, replacement))
            g_pkg_hook_count++;
    }
}

static void install_hooks_now(void)
{
    hook_one("GetCurrentPackageId",               anvil_GetCurrentPackageId);
    hook_one("GetCurrentPackageFullName",         anvil_GetCurrentPackageFullName);
    hook_one("GetCurrentPackageFamilyName",       anvil_GetCurrentPackageFamilyName);
    hook_one("GetCurrentApplicationUserModelId",  anvil_GetCurrentApplicationUserModelId);
    hook_one("GetCurrentPackagePath",             anvil_GetCurrentPackagePath);
    hook_one("GetPackagePathByFullName",          anvil_GetPackagePathByFullName);
    hook_one("OpenPackageInfoByFullName",         anvil_OpenPackageInfoByFullName);
    hook_one("ClosePackageInfo",                  anvil_ClosePackageInfo);
    PKGLOG("pkg hooks installed: %d total", g_pkg_hook_count);
}

static DWORD WINAPI deferred_hook_thread(LPVOID unused)
{
    (void)unused;
    Sleep(200);   /* let Wine finish resolving all module exports */
    install_hooks_now();
    return 0;
}

void anvil_install_pkg_hooks(void)
{
    /* Immediate attempt (works if modules already mapped) */
    install_hooks_now();
    /* Deferred attempt covers cases where Wine resolves exports late */
    CloseHandle(CreateThread(NULL, 0, deferred_hook_thread, NULL, 0, NULL));
}


void anvil_remove_pkg_hooks(void)
{
    int i;
    for (i = 0; i < g_pkg_hook_count; i++)
        anvil_hook_remove(&g_pkg_hooks[i]);
    g_pkg_hook_count = 0;
}

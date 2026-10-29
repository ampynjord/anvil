/*
 * winrt_types.h — Portable WinRT ABI types for Anvil stubs.
 *
 * On Windows / MinGW, all the standard COM types (GUID, IUnknown, BOOL, …)
 * are already provided by <windows.h>.  This header only adds the types that
 * MinGW's windows.h does NOT include out of the box (WinRT-specific:
 * HSTRING, RO_INIT_TYPE, IInspectable, IActivationFactory).
 *
 * On Linux (non-Windows cross-compile / unit-test with GCC):
 * <windows.h> is not available, so we define everything ourselves.
 */

#ifndef ANVIL_WINRT_TYPES_H
#define ANVIL_WINRT_TYPES_H

/* =========================================================================
 * On Windows / MinGW: lean <windows.h> provides the COM baseline.
 * ========================================================================= */
#ifdef _WIN32
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <windows.h>
# include <stdint.h>

/* INT32 / UINT32 — windows.h / basetsd.h normally has these, but guard
 * against older MinGW toolchains that lack them.                          */
# ifndef INT32
typedef signed int   INT32;
# endif
# ifndef UINT32
typedef unsigned int UINT32;
# endif

#else  /* ================================================================
        * Linux / non-Windows: define the full baseline ourselves.
        * ================================================================ */

# include <stdint.h>
# include <string.h>   /* memcmp */

/* --- primitive types --- */
# ifndef WINAPI
#  define WINAPI
# endif

typedef unsigned int   ULONG;   /* 4 bytes everywhere (Windows ULONG is 32-bit) */
typedef int            LONG;    /* 4 bytes everywhere (Windows LONG  is 32-bit) */
typedef signed int     INT32;
typedef unsigned int   UINT32;
typedef int            BOOL;
typedef unsigned short WCHAR;
typedef const WCHAR   *LPCWSTR;
typedef void          *PVOID;
typedef PVOID          HANDLE;
typedef PVOID          HINSTANCE;
typedef uint32_t       DWORD;

# ifndef HRESULT
typedef LONG HRESULT;
# endif

# define S_OK          ((HRESULT)0x00000000L)
# define S_FALSE       ((HRESULT)0x00000001L)
# define E_NOTIMPL     ((HRESULT)0x80004001L)
# define E_NOINTERFACE ((HRESULT)0x80004002L)
# define E_POINTER     ((HRESULT)0x80004003L)
# define E_FAIL        ((HRESULT)0x80004005L)
# define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
# define E_INVALIDARG  ((HRESULT)0x80070057L)

# define SUCCEEDED(hr) ((HRESULT)(hr) >= 0)
# define FAILED(hr)    ((HRESULT)(hr) < 0)

# define TRUE  1
# define FALSE 0

/* --- GUID / IID --- */
typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID;

typedef GUID        IID;
typedef GUID        CLSID;
typedef const IID  *REFIID;
typedef const CLSID *REFCLSID;

# define IsEqualGUID(a, b) (memcmp((a), (b), sizeof(GUID)) == 0)
# define IsEqualIID(a, b)  IsEqualGUID(a, b)

/* --- DllMain / HINSTANCE stubs for Linux compilation --- */
# define DLL_PROCESS_ATTACH 1
# define DLL_PROCESS_DETACH 0

/* --- IUnknown --- */
typedef struct IUnknownVtbl {
    HRESULT (WINAPI *QueryInterface)(void *This, REFIID riid, void **ppvObject);
    ULONG   (WINAPI *AddRef)(void *This);
    ULONG   (WINAPI *Release)(void *This);
} IUnknownVtbl;

typedef struct IUnknown {
    const IUnknownVtbl *lpVtbl;
} IUnknown;

#endif /* _WIN32 */

/* WinRT-era HRESULTs absent from older MinGW winerror.h */
# ifndef E_BOUNDS
#  define E_BOUNDS  ((HRESULT)0x8000000BL)
# endif
# ifndef E_CHANGED_STATE
#  define E_CHANGED_STATE ((HRESULT)0x8000000CL)
# endif
# ifndef E_ILLEGAL_METHOD_CALL
#  define E_ILLEGAL_METHOD_CALL ((HRESULT)0x8000000EL)
# endif

/* =========================================================================
 * WinRT-specific types — not present in standard MinGW <windows.h>
 * (Available on Windows only from <roapi.h> / <hstring.h>, which require
 *  the Windows 8+ SDK and are often absent in MinGW distributions.)
 * ========================================================================= */

/* --- HSTRING --- */
# ifndef __HSTRING_DEFINED__
# define __HSTRING_DEFINED__
typedef struct HSTRING__ *HSTRING;

typedef struct HSTRING_HEADER {
    union {
        PVOID Reserved1;
# if defined(_WIN64) || defined(__LP64__)
        char Reserved2[24];
# else
        char Reserved2[20];
# endif
    } Reserved;
} HSTRING_HEADER;
# endif /* __HSTRING_DEFINED__ */

/* --- RO_INIT_TYPE --- */
# ifndef __RO_INIT_TYPE_DEFINED__
# define __RO_INIT_TYPE_DEFINED__
typedef enum RO_INIT_TYPE {
    RO_INIT_SINGLETHREADED = 0,
    RO_INIT_MULTITHREADED  = 1,
} RO_INIT_TYPE;
# endif

/* --- TrustLevel --- */
# ifndef __TRUST_LEVEL_DEFINED__
# define __TRUST_LEVEL_DEFINED__
typedef enum TrustLevel {
    BaseTrust    = 0,
    PartialTrust = 1,
    FullTrust    = 2,
} TrustLevel;
# endif

/* --- IInspectable --- */
# ifndef __IInspectable_DEFINED__
# define __IInspectable_DEFINED__
typedef struct IInspectableVtbl {
    HRESULT (WINAPI *QueryInterface)(void *This, REFIID riid, void **ppvObject);
    ULONG   (WINAPI *AddRef)(void *This);
    ULONG   (WINAPI *Release)(void *This);
    HRESULT (WINAPI *GetIids)(void *This, ULONG *iidCount, IID **iids);
    HRESULT (WINAPI *GetRuntimeClassName)(void *This, HSTRING *className);
    HRESULT (WINAPI *GetTrustLevel)(void *This, TrustLevel *trustLevel);
} IInspectableVtbl;

typedef struct IInspectable {
    const IInspectableVtbl *lpVtbl;
} IInspectable;
# endif

/* --- IActivationFactory --- */
# ifndef __IActivationFactory_DEFINED__
# define __IActivationFactory_DEFINED__
typedef struct IActivationFactoryVtbl {
    HRESULT (WINAPI *QueryInterface)(void *This, REFIID riid, void **ppvObject);
    ULONG   (WINAPI *AddRef)(void *This);
    ULONG   (WINAPI *Release)(void *This);
    HRESULT (WINAPI *GetIids)(void *This, ULONG *iidCount, IID **iids);
    HRESULT (WINAPI *GetRuntimeClassName)(void *This, HSTRING *className);
    HRESULT (WINAPI *GetTrustLevel)(void *This, TrustLevel *trustLevel);
    HRESULT (WINAPI *ActivateInstance)(void *This, IInspectable **instance);
} IActivationFactoryVtbl;

typedef struct IActivationFactory {
    const IActivationFactoryVtbl *lpVtbl;
} IActivationFactory;
# endif

/* --- IRestrictedErrorInfo (opaque stub) --- */
# ifndef __IRestrictedErrorInfo_DEFINED__
# define __IRestrictedErrorInfo_DEFINED__
typedef struct IRestrictedErrorInfo {
    const void *lpVtbl;
} IRestrictedErrorInfo;
# endif

/* --- Well-known IIDs --- */
# ifndef DEFINE_ANVIL_IID
# define DEFINE_ANVIL_IID

static const IID IID_IActivationFactory = {
    0x00000035, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

#  ifndef IID_IUnknown   /* MinGW defines this in unknwnbase.h */
static const IID IID_IUnknown = {
    0x00000000, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};
#  endif

static const IID IID_IInspectable = {
    0xAF86E2E0, 0xB12D, 0x4C6A,
    { 0x9C, 0x5A, 0xD7, 0xAA, 0x65, 0x10, 0x1E, 0x90 }
};

# endif /* DEFINE_ANVIL_IID */

/* --- IsEqualGUID for Windows builds (already defined for Linux above) --- */
# ifdef _WIN32
#  ifndef IsEqualGUID
#   include <string.h>
#   define IsEqualGUID(a, b) (memcmp((a), (b), sizeof(GUID)) == 0)
#  endif
#  ifndef IsEqualIID
#   define IsEqualIID(a, b)  IsEqualGUID(a, b)
#  endif
# endif

/* =========================================================================
 * WCHAR string utilities — Linux (non-_WIN32) builds only
 *
 * Linux wchar_t is 4 bytes (UTF-32); Windows/MinGW wchar_t is 2 bytes.
 * Compile native Linux targets with -fshort-wchar so that L"..." literals
 * are 2-byte wide (matching WCHAR / unsigned short).
 *
 * These helpers replace glibc's 4-byte-wchar_t functions in all Linux
 * translation units that include this header.  Do NOT include <wchar.h>
 * in the same unit on Linux — use these instead.
 * ========================================================================= */
#ifndef _WIN32
# include <stddef.h>   /* size_t */

static int anvil_u16cmp(const WCHAR *a, const WCHAR *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)*a - (int)*b;
}
static WCHAR *anvil_u16ncpy(WCHAR *d, const WCHAR *s, size_t n) {
    size_t i;
    for (i = 0; i < n && s[i]; i++) d[i] = s[i];
    while (i < n) d[i++] = 0;
    return d;
}
/* Scratch buffer for printing WCHAR class names (ASCII only — fine for WinRT) */
static char g_anvil_wbuf[512];
static const char *anvil_wcs2a(const WCHAR *s) {
    int i = 0;
    if (!s) return "";
    while (s[i] && i < 510) { g_anvil_wbuf[i] = (char)s[i]; i++; }
    g_anvil_wbuf[i] = 0;
    return g_anvil_wbuf;
}
# define wcscmp  anvil_u16cmp
# define wcsncpy anvil_u16ncpy
/* Cross-platform printf helpers for WCHAR* arguments */
# define FMT_WS  "%s"
# define WFMT(s) anvil_wcs2a(s)
#else
# define FMT_WS  "%ls"
# define WFMT(s) (s)
#endif /* !_WIN32 */

#endif /* ANVIL_WINRT_TYPES_H */

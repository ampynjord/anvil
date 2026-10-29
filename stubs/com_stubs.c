/*
 * com_stubs.c — Classic COM functions that Windows combase.dll exports.
 *
 * On modern Windows, combase.dll contains both WinRT (RoInit, RoGetActivationFactory…)
 * and classic COM (CoInitializeEx, CoTaskMemAlloc, StringFromGUID2…).
 * Wine 6.x ole32.dll forwards ~50 functions to combase, so our combase override must
 * export them all or ole32 crashes immediately on load.
 *
 * Strategy:
 *   - Memory functions   → HeapAlloc/HeapFree/HeapReAlloc (CoTaskMem*)
 *   - Apartment init     → simple reference-count, single-threaded stub
 *   - GUID formatting    → format into caller's buffer (StringFromGUID2)
 *   - GUID/CLSID parsing → scan hex digits (CLSIDFromString, IIDFromString)
 *   - CoCreateGuid       → xorshift PRNG seeded from tick count (good enough for PoC)
 *   - CoCreateInstance   → E_NOTIMPL (no real class factory yet)
 *   - Everything else    → S_OK or E_NOTIMPL with a TRACE
 *
 * Compiled as part of the Anvil combase DLL alongside combase_anvil.c.
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_com);
# define CSTUB(fmt, ...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# define CSTUB(fmt, ...) fprintf(stderr, "[anvil/com] " fmt "\n", ##__VA_ARGS__)
#endif

/* =========================================================================
 * COM apartment (minimal: one global init count)
 * ========================================================================= */

static volatile LONG g_com_init_count = 0;

HRESULT WINAPI CoInitializeEx(void *reserved, DWORD dwCoInit)
{
    LONG prev = InterlockedIncrement(&g_com_init_count) - 1;
    (void)reserved; (void)dwCoInit;
    CSTUB("CoInitializeEx(dwCoInit=0x%lx) → %s", (unsigned long)dwCoInit,
          prev == 0 ? "S_OK" : "S_FALSE");
    return prev == 0 ? S_OK : S_FALSE;
}

HRESULT WINAPI CoInitialize(void *reserved)
{
    (void)reserved;
    return CoInitializeEx(NULL, 0 /*COINIT_APARTMENTTHREADED*/);
}

void WINAPI CoUninitialize(void)
{
    InterlockedDecrement(&g_com_init_count);
    CSTUB("CoUninitialize");
}

/* =========================================================================
 * COM task-memory allocator (IMalloc default implementation)
 * Backed by the process heap — identical semantics to real CoTaskMem*.
 * ========================================================================= */

void WINAPI CoTaskMemFree(void *pv)
{
    if (pv) HeapFree(GetProcessHeap(), 0, pv);
}

void * WINAPI CoTaskMemAlloc(SIZE_T cb)
{
    return HeapAlloc(GetProcessHeap(), 0, cb);
}

void * WINAPI CoTaskMemRealloc(void *pv, SIZE_T cb)
{
    if (!pv) return CoTaskMemAlloc(cb);
    if (!cb) { CoTaskMemFree(pv); return NULL; }
    return HeapReAlloc(GetProcessHeap(), 0, pv, cb);
}

/* =========================================================================
 * GUID / CLSID string formatting
 * ========================================================================= */

/* REFIID is const IID * — IID == GUID */
typedef struct _GUID GUID;
typedef const GUID *REFGUID_t;

/* StringFromGUID2: fills pszGUID with "{xxxxxxxx-…}" (39 chars + NUL = 39+1).
   Returns char count including NUL on success, 0 if buffer too small.      */
INT WINAPI StringFromGUID2(REFGUID_t rguid, WCHAR *pszGUID, INT cchMax)
{
    /* Need 39 chars + NUL */
    if (cchMax < 39) return 0;
    const BYTE *b = (const BYTE *)rguid;
    /* Windows stores GUID bytes with mixed endianness:
       Data1 (4 bytes LE), Data2 (2 bytes LE), Data3 (2 bytes LE), Data4 (8 bytes big-endian) */
    DWORD  d1 = ((DWORD)b[0]) | ((DWORD)b[1]<<8) | ((DWORD)b[2]<<16) | ((DWORD)b[3]<<24);
    WORD   d2 = (WORD)(b[4] | (b[5]<<8));
    WORD   d3 = (WORD)(b[6] | (b[7]<<8));
    static const WCHAR hex[] = {
        '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
    };
#define W2(val) hex[((val)>>4)&0xF], hex[(val)&0xF]
    WCHAR tmp[39] = {
        '{',
        hex[(d1>>28)&0xF], hex[(d1>>24)&0xF],
        hex[(d1>>20)&0xF], hex[(d1>>16)&0xF],
        hex[(d1>>12)&0xF], hex[(d1>> 8)&0xF],
        hex[(d1>> 4)&0xF], hex[(d1>> 0)&0xF],
        '-',
        hex[(d2>>12)&0xF], hex[(d2>> 8)&0xF],
        hex[(d2>> 4)&0xF], hex[(d2>> 0)&0xF],
        '-',
        hex[(d3>>12)&0xF], hex[(d3>> 8)&0xF],
        hex[(d3>> 4)&0xF], hex[(d3>> 0)&0xF],
        '-',
        W2(b[8]),  W2(b[9]),
        '-',
        W2(b[10]), W2(b[11]), W2(b[12]),
        W2(b[13]), W2(b[14]), W2(b[15]),
        '}'
    };
#undef W2
    for (int i = 0; i < 39; i++) pszGUID[i] = tmp[i];
    pszGUID[38] = '}';
    pszGUID[39] = 0;
    return 40; /* 39 chars + NUL */
}

HRESULT WINAPI StringFromCLSID(REFGUID_t clsid, WCHAR **ppsz)
{
    *ppsz = (WCHAR *)CoTaskMemAlloc(40 * sizeof(WCHAR));
    if (!*ppsz) return E_OUTOFMEMORY;
    return StringFromGUID2(clsid, *ppsz, 40) ? S_OK : E_FAIL;
}

HRESULT WINAPI StringFromIID(REFGUID_t riid, WCHAR **ppsz)
{
    return StringFromCLSID(riid, ppsz);
}

/* Parse two hex chars, advance pointer */
static int parse2hex(const WCHAR **p)
{
    int hi = 0, lo = 0;
    WCHAR c = **p; (*p)++;
    if (c >= '0' && c <= '9') hi = c - '0';
    else if (c >= 'a' && c <= 'f') hi = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') hi = c - 'A' + 10;
    c = **p; (*p)++;
    if (c >= '0' && c <= '9') lo = c - '0';
    else if (c >= 'a' && c <= 'f') lo = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') lo = c - 'A' + 10;
    return (hi << 4) | lo;
}

static int parse4hex(const WCHAR **p)
{
    return (parse2hex(p) << 8) | parse2hex(p);
}

static DWORD parse8hex(const WCHAR **p)
{
    return ((DWORD)parse4hex(p) << 16) | (DWORD)parse4hex(p);
}

HRESULT WINAPI CLSIDFromString(const WCHAR *idstr, GUID *pclsid)
{
    if (!idstr || !pclsid) return E_INVALIDARG;
    /* Accept both "{...}" and "..." */
    const WCHAR *p = idstr;
    if (*p == '{') p++;
    DWORD d1 = parse8hex(&p);
    if (*p++ != '-') return CO_E_CLASSSTRING;
    WORD  d2 = (WORD)parse4hex(&p);
    if (*p++ != '-') return CO_E_CLASSSTRING;
    WORD  d3 = (WORD)parse4hex(&p);
    if (*p++ != '-') return CO_E_CLASSSTRING;
    BYTE  d4[8];
    d4[0] = (BYTE)parse2hex(&p);
    d4[1] = (BYTE)parse2hex(&p);
    if (*p++ != '-') return CO_E_CLASSSTRING;
    for (int i = 2; i < 8; i++) d4[i] = (BYTE)parse2hex(&p);
    /* Write out with Windows mixed endianness */
    BYTE *b = (BYTE *)pclsid;
    b[0]=(BYTE)(d1    ); b[1]=(BYTE)(d1>>8);
    b[2]=(BYTE)(d1>>16); b[3]=(BYTE)(d1>>24);
    b[4]=(BYTE)(d2    ); b[5]=(BYTE)(d2>>8);
    b[6]=(BYTE)(d3    ); b[7]=(BYTE)(d3>>8);
    for (int i = 0; i < 8; i++) b[8+i] = d4[i];
    return S_OK;
}

HRESULT WINAPI IIDFromString(const WCHAR *idstr, IID *piid)
{
    return CLSIDFromString(idstr, (GUID *)piid);
}

HRESULT WINAPI ProgIDFromCLSID(REFGUID_t clsid, WCHAR **ppszProgID)
{
    (void)clsid;
    *ppszProgID = NULL;
    return REGDB_E_CLASSNOTREG;
}

HRESULT WINAPI CLSIDFromProgID(const WCHAR *szProgID, GUID *pclsid)
{
    (void)szProgID; (void)pclsid;
    return CO_E_CLASSSTRING;
}

/* =========================================================================
 * CoCreateGuid — xorshift PRNG (good enough for object identity in a PoC)
 * ========================================================================= */

HRESULT WINAPI CoCreateGuid(GUID *pguid)
{
    if (!pguid) return E_INVALIDARG;
    static DWORD s_state = 0;
    if (!s_state) s_state = GetTickCount() ^ 0xDEADBEEFu;
    BYTE *b = (BYTE *)pguid;
    for (int i = 0; i < 16; i++) {
        s_state ^= s_state << 13;
        s_state ^= s_state >> 17;
        s_state ^= s_state <<  5;
        b[i] = (BYTE)s_state;
    }
    /* Set version=4 (random) and variant=RFC 4122 */
    b[6] = (b[6] & 0x0F) | 0x40;
    b[8] = (b[8] & 0x3F) | 0x80;
    return S_OK;
}

/* =========================================================================
 * COM security — stub (no real remoting in a PoC)
 * ========================================================================= */

HRESULT WINAPI CoInitializeSecurity(
    void *pSecDesc, LONG cAuthSvc, void *asAuthSvc,
    void *pReserved1, DWORD dwAuthnLevel, DWORD dwImpLevel,
    void *pAuthList, DWORD dwCapabilities, void *pReserved3)
{
    (void)pSecDesc; (void)cAuthSvc; (void)asAuthSvc; (void)pReserved1;
    (void)dwAuthnLevel; (void)dwImpLevel; (void)pAuthList;
    (void)dwCapabilities; (void)pReserved3;
    CSTUB("CoInitializeSecurity (no-op)");
    return S_OK;
}

/* =========================================================================
 * CoGetClassObject / CoCreateInstance — return REGDB_E_CLASSNOTREG.
 * A future milestone can wire classic CLSID → our factory registry.
 * ========================================================================= */

HRESULT WINAPI CoGetClassObject(
    REFGUID_t rclsid, DWORD dwClsContext, void *pvReserved, REFIID riid, void **ppv)
{
    (void)rclsid; (void)dwClsContext; (void)pvReserved; (void)riid;
    *ppv = NULL;
    CSTUB("CoGetClassObject → REGDB_E_CLASSNOTREG");
    return REGDB_E_CLASSNOTREG;
}

HRESULT WINAPI CoCreateInstance(
    REFGUID_t rclsid, void *pUnkOuter, DWORD dwClsContext, REFIID riid, void **ppv)
{
    (void)rclsid; (void)pUnkOuter; (void)dwClsContext; (void)riid;
    *ppv = NULL;
    CSTUB("CoCreateInstance → REGDB_E_CLASSNOTREG");
    return REGDB_E_CLASSNOTREG;
}

HRESULT WINAPI CoCreateInstanceEx(
    REFGUID_t rclsid, void *pUnkOuter, DWORD dwClsContext,
    void *pServerInfo, DWORD dwCount, void *pResults)
{
    (void)rclsid; (void)pUnkOuter; (void)dwClsContext;
    (void)pServerInfo; (void)dwCount; (void)pResults;
    CSTUB("CoCreateInstanceEx → E_NOTIMPL");
    return E_NOTIMPL;
}

/* =========================================================================
 * Apartment / context queries
 * ========================================================================= */

/* APTTYPE values */
typedef enum { APTTYPE_STA=0, APTTYPE_MTA=1, APTTYPE_NA=2, APTTYPE_MAINSTA=3 } APTTYPE;
typedef enum { APTTYPEQUALIFIER_NONE=0 } APTTYPEQUALIFIER;

HRESULT WINAPI CoGetApartmentType(APTTYPE *pAptType, APTTYPEQUALIFIER *pAptQualifier)
{
    if (!pAptType || !pAptQualifier) return E_INVALIDARG;
    *pAptType      = APTTYPE_MTA;
    *pAptQualifier = APTTYPEQUALIFIER_NONE;
    return S_OK;
}

HRESULT WINAPI CoGetCurrentProcess(void) { return S_OK; }

HRESULT WINAPI CoGetContextToken(ULONG_PTR *pToken)
{
    if (pToken) *pToken = 0;
    return S_OK;
}

/* =========================================================================
 * IMalloc stub (returned by CoGetMalloc)
 * ========================================================================= */

typedef struct IMallocVtbl_s {
    HRESULT (WINAPI *QI)(void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)(void *);
    ULONG   (WINAPI *Release)(void *);
    void *  (WINAPI *Alloc)(void *, SIZE_T);
    void *  (WINAPI *Realloc)(void *, void *, SIZE_T);
    void    (WINAPI *Free)(void *, void *);
    SIZE_T  (WINAPI *GetSize)(void *, void *);
    int     (WINAPI *DidAlloc)(void *, void *);
    void    (WINAPI *HeapMinimize)(void *);
} IMallocVtbl_s;

static HRESULT WINAPI imalloc_QI(void *T, REFIID iid, void **out) { (void)iid; *out=T; return S_OK; }
static ULONG   WINAPI imalloc_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI imalloc_Release(void *T) { (void)T; return 1; }
static void *  WINAPI imalloc_Alloc  (void *T, SIZE_T n) { (void)T; return CoTaskMemAlloc(n); }
static void *  WINAPI imalloc_Realloc(void *T, void *p, SIZE_T n) { (void)T; return CoTaskMemRealloc(p,n); }
static void    WINAPI imalloc_Free   (void *T, void *p) { (void)T; CoTaskMemFree(p); }
static SIZE_T  WINAPI imalloc_GetSize(void *T, void *p) { (void)T; return HeapSize(GetProcessHeap(),0,p); }
static int     WINAPI imalloc_DidAlloc(void *T, void *p) { (void)T; (void)p; return -1; }
static void    WINAPI imalloc_Minimize(void *T) { (void)T; }

static const IMallocVtbl_s s_imalloc_vtbl = {
    imalloc_QI, imalloc_AddRef, imalloc_Release,
    imalloc_Alloc, imalloc_Realloc, imalloc_Free,
    imalloc_GetSize, imalloc_DidAlloc, imalloc_Minimize
};
static struct { const IMallocVtbl_s *lpVtbl; } s_imalloc = { &s_imalloc_vtbl };

HRESULT WINAPI CoGetMalloc(DWORD dwMemContext, void **ppMalloc)
{
    (void)dwMemContext;
    *ppMalloc = &s_imalloc;
    return S_OK;
}

/* =========================================================================
 * PropVariant helpers (minimal — just clear to VT_EMPTY)
 * ========================================================================= */

typedef struct tagPROPVARIANT {
    WORD  vt;
    WORD  wReserved1, wReserved2, wReserved3;
    union {
        LONGLONG   hVal;
        void      *pv;
        WCHAR     *pwszVal;
    };
} PROPVARIANT_s;

HRESULT WINAPI PropVariantClear(PROPVARIANT_s *pvar)
{
    if (!pvar) return E_INVALIDARG;
    if (pvar->vt == 31 /*VT_BSTR*/ || pvar->vt == 30 /*VT_LPWSTR*/)
        CoTaskMemFree(pvar->pwszVal);
    pvar->vt = 0 /*VT_EMPTY*/;
    pvar->hVal = 0;
    return S_OK;
}

HRESULT WINAPI PropVariantCopy(PROPVARIANT_s *pDest, const PROPVARIANT_s *pSrc)
{
    if (!pDest || !pSrc) return E_INVALIDARG;
    *pDest = *pSrc;   /* shallow copy (adequate for VT_EMPTY/VT_I4/VT_I8) */
    return S_OK;
}

HRESULT WINAPI FreePropVariantArray(ULONG cVariants, PROPVARIANT_s *rgvars)
{
    for (ULONG i = 0; i < cVariants; i++) PropVariantClear(&rgvars[i]);
    return S_OK;
}

/* =========================================================================
 * Miscellaneous no-ops frequently called by ole32 forwarding chains
 * ========================================================================= */

HRESULT WINAPI CoRegisterClassObject(
    REFGUID_t rclsid, void *pUnk, DWORD dwClsContext, DWORD flags, DWORD *pdwReg)
{
    (void)rclsid; (void)pUnk; (void)dwClsContext; (void)flags;
    if (pdwReg) *pdwReg = 1;
    return S_OK;
}

HRESULT WINAPI CoRevokeClassObject(DWORD dwRegister)
{
    (void)dwRegister; return S_OK;
}

HRESULT WINAPI CoDisconnectObject(void *pUnk, DWORD dwReserved)
{
    (void)pUnk; (void)dwReserved; return S_OK;
}

HRESULT WINAPI CoSuspendClassObjects(void)  { return S_OK; }
HRESULT WINAPI CoResumeClassObjects(void)   { return S_OK; }

HRESULT WINAPI CoGetInterfaceAndReleaseStream(void *pStm, REFIID iid, void **ppv)
{
    (void)pStm; (void)iid; *ppv = NULL; return E_NOTIMPL;
}

HRESULT WINAPI CoMarshalInterThreadInterfaceInStream(REFIID iid, void *pUnk, void **ppStm)
{
    (void)iid; (void)pUnk; *ppStm = NULL; return E_NOTIMPL;
}

HRESULT WINAPI CoGetObjectContext(REFIID riid, void **ppv)
{
    (void)riid; *ppv = NULL; return E_NOINTERFACE;
}

BOOL WINAPI CoIsHandlerConnected(void *pUnk)
{
    (void)pUnk; return FALSE;
}

HRESULT WINAPI CoFileTimeNow(FILETIME *lpFileTime)
{
    GetSystemTimeAsFileTime(lpFileTime);
    return S_OK;
}

HRESULT WINAPI CoFreeLibrary(HMODULE hInst)
{
    FreeLibrary(hInst); return S_OK;
}

HRESULT WINAPI CoLockObjectExternal(void *pUnk, BOOL fLock, BOOL fLastUnlockReleases)
{
    (void)pUnk; (void)fLock; (void)fLastUnlockReleases; return S_OK;
}

HRESULT WINAPI CoRegisterInitializeSpy(void *pSpy, ULARGE_INTEGER *puliCookie)
{
    (void)pSpy;
    if (puliCookie) { puliCookie->QuadPart = 0; }
    return S_OK;
}

HRESULT WINAPI CoRevokeInitializeSpy(ULARGE_INTEGER uliCookie)
{
    (void)uliCookie; return S_OK;
}

HRESULT WINAPI CoAllowSetForegroundWindow(void *pUnk, void *lpvReserved)
{
    (void)pUnk; (void)lpvReserved; return S_OK;
}

LONG WINAPI CoAddRefServerProcess(void) { return 1; }
LONG WINAPI CoReleaseServerProcess(void) { return 1; }

HRESULT WINAPI CoGetDefaultContext(APTTYPE aptType, REFIID riid, void **ppv)
{
    (void)aptType; (void)riid; *ppv = NULL; return E_NOINTERFACE;
}

HRESULT WINAPI CoRegisterChannelHook(REFGUID_t ExtensionUuid, void *pChannelHook)
{
    (void)ExtensionUuid; (void)pChannelHook; return S_OK;
}

HRESULT WINAPI CoWaitForMultipleHandles(
    DWORD dwFlags, DWORD dwTimeout, ULONG cHandles, HANDLE *pHandles, DWORD *lpdwindex)
{
    DWORD res = WaitForMultipleObjects(cHandles, pHandles, FALSE, dwTimeout);
    if (lpdwindex) *lpdwindex = res;
    return (res == WAIT_FAILED) ? HRESULT_FROM_WIN32(GetLastError()) : S_OK;
}

/* CreateStreamOnHGlobal / GetHGlobalFromStream: stubs — not needed yet */
HRESULT WINAPI CreateStreamOnHGlobal(void *hGlobal, BOOL fDeleteOnRelease, void **ppStm)
{
    (void)hGlobal; (void)fDeleteOnRelease; *ppStm = NULL; return E_NOTIMPL;
}

HRESULT WINAPI GetHGlobalFromStream(void *pStm, void **phGlobal)
{
    (void)pStm; *phGlobal = NULL; return E_NOTIMPL;
}

/* DLL registration stubs (combase re-exports ole32 registration) */
HRESULT WINAPI DllRegisterServer(void)   { return S_OK; }
HRESULT WINAPI DllUnregisterServer(void) { return S_OK; }

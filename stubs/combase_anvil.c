/*
 * combase_anvil.c — Anvil stub override for combase.dll WinRT functions.
 *
 * BUILD TARGET: Wine DLL override (replaces combase.dll for UWP processes).
 *
 * PURPOSE:
 *   Intercept the WinRT activation pipeline at the combase layer so that
 *   Calculator (and other UWP apps) can reach past their first
 *   RoGetActivationFactory / RoActivateInstance call without crashing.
 *
 * STRATEGY:
 *   1. Implement HSTRING as a thin heap-allocated wrapper around a wchar_t*.
 *   2. Implement a global activation factory registry that Anvil layers above
 *      this one can populate (anvil_register_factory).
 *   3. Return E_NOTIMPL with a clear log line for unregistered factories so
 *      that the call graph in CALLGRAPH.md can be validated at runtime.
 *
 * HOW TO USE IN WINE:
 *   Place the compiled combase_anvil.dll in the Wine DLL override path and
 *   set the registry key (or WINEDLLOVERRIDES env-var):
 *
 *     WINEDLLOVERRIDES="combase=n,b" wine Calculator.exe
 *
 *   "n,b" means: try native first, fall back to builtin.  For Anvil we want
 *   the reverse ("b,n") so our stub is tried first:
 *
 *     WINEDLLOVERRIDES="combase=b,n" wine Calculator.exe
 *
 * COMPILATION (on Linux, cross-compiling with MinGW):
 *   See Makefile.wine in this directory.
 */

/*
 * Include strategy:
 *   __WINE__        : we're compiled inside the Wine build tree — use Wine's
 *                     own debug channel macros and SDK headers.
 *   _WIN32          : MinGW / MSVC host — winrt_types.h pulls <windows.h>
 *                     and adds only the WinRT-specific additions.
 *   (neither)       : Linux / non-Windows standalone (unit tests).
 */
#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_combase);
# define ANVIL_TRACE(fmt, ...) TRACE(fmt, ##__VA_ARGS__)
# define ANVIL_WARN(fmt, ...)  WARN(fmt, ##__VA_ARGS__)
# define ANVIL_ERR(fmt, ...)   ERR(fmt, ##__VA_ARGS__)
#else
# include "winrt_types.h"   /* portable: handles _WIN32 vs Linux internally */
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
/* Note: do NOT include <wchar.h> on Linux — winrt_types.h provides WCHAR-safe
 * wcscmp / wcsncpy replacements (glibc uses 4-byte wchar_t, ours is 2-byte). */
# define ANVIL_TRACE(fmt, ...) fprintf(stderr, "[anvil/TRACE] " fmt "\n", ##__VA_ARGS__)
# define ANVIL_WARN(fmt, ...)  fprintf(stderr, "[anvil/WARN]  " fmt "\n", ##__VA_ARGS__)
# define ANVIL_ERR(fmt, ...)   fprintf(stderr, "[anvil/ERR]   " fmt "\n", ##__VA_ARGS__)
#endif

/* =========================================================================
 * HSTRING implementation
 *
 * Real HSTRING is an opaque kernel32-managed object.  Our implementation is
 * a simple heap struct.  The public API is ABI-compatible because HSTRING
 * is an opaque pointer in both cases.
 * ========================================================================= */

typedef struct AnvilHString {
    WCHAR  *buf;   /* NUL-terminated wide string              */
    UINT32  len;   /* character count, not byte count          */
    LONG    refcount; /* for WindowsDuplicateString (future)   */
    int     is_ref;   /* 1 = StringReference (stack-allocated) */
} AnvilHString;

HRESULT WINAPI WindowsCreateString(LPCWSTR src, UINT32 length, HSTRING *str)
{
    AnvilHString *h;

    if (!str) return E_POINTER;
    *str = NULL;

    if (!src && length != 0) return E_POINTER;

    h = (AnvilHString *)malloc(sizeof(AnvilHString));
    if (!h) return E_OUTOFMEMORY;

    h->len      = length;
    h->refcount = 1;
    h->is_ref   = 0;

    if (length == 0 || !src) {
        h->buf = (WCHAR *)malloc(sizeof(WCHAR));
        if (!h->buf) { free(h); return E_OUTOFMEMORY; }
        h->buf[0] = L'\0';
    } else {
        h->buf = (WCHAR *)malloc((length + 1) * sizeof(WCHAR));
        if (!h->buf) { free(h); return E_OUTOFMEMORY; }
        memcpy(h->buf, src, length * sizeof(WCHAR));
        h->buf[length] = L'\0';
    }

    *str = (HSTRING)h;
    return S_OK;
}

/*
 * WindowsCreateStringReference — wraps a caller-stack buffer.
 * The caller must keep the buffer alive as long as the HSTRING is used.
 * We store a non-owning pointer; WindowsDeleteString on a ref is a no-op.
 */
HRESULT WINAPI WindowsCreateStringReference(LPCWSTR src, UINT32 length,
                                             HSTRING_HEADER *header,
                                             HSTRING *str)
{
    AnvilHString *h;

    if (!str || !header) return E_POINTER;
    *str = NULL;

    /* Abuse the header's reserved storage to hold our AnvilHString inline.
     * sizeof(HSTRING_HEADER) >= sizeof(AnvilHString) on both 32-bit and
     * 64-bit targets (verified: AnvilHString = 16 bytes on 64-bit). */
    h = (AnvilHString *)header;
    h->buf      = (WCHAR *)src;    /* non-owning */
    h->len      = length;
    h->refcount = 1;
    h->is_ref   = 1;

    *str = (HSTRING)h;
    return S_OK;
}

void WINAPI WindowsDeleteString(HSTRING str)
{
    AnvilHString *h = (AnvilHString *)str;
    if (!h) return;
    if (!h->is_ref) {
        free(h->buf);
        free(h);
    }
    /* stack-allocated ref: nothing to free */
}

LPCWSTR WINAPI WindowsGetStringRawBuffer(HSTRING str, UINT32 *length)
{
    static const WCHAR empty[] = { 0 };
    AnvilHString *h = (AnvilHString *)str;

    if (!h) {
        if (length) *length = 0;
        return empty;
    }
    if (length) *length = h->len;
    return h->buf;
}

BOOL WINAPI WindowsIsStringEmpty(HSTRING str)
{
    AnvilHString *h = (AnvilHString *)str;
    return (!h || h->len == 0);
}

HRESULT WINAPI WindowsConcatString(HSTRING s1, HSTRING s2, HSTRING *out)
{
    AnvilHString *h1 = (AnvilHString *)s1;
    AnvilHString *h2 = (AnvilHString *)s2;
    UINT32 l1 = h1 ? h1->len : 0;
    UINT32 l2 = h2 ? h2->len : 0;
    UINT32 total = l1 + l2;

    if (!out) return E_POINTER;

    if (total == 0) {
        *out = NULL;
        return S_OK;
    }

    AnvilHString *h = (AnvilHString *)malloc(sizeof(AnvilHString));
    if (!h) return E_OUTOFMEMORY;

    h->buf = (WCHAR *)malloc((total + 1) * sizeof(WCHAR));
    if (!h->buf) { free(h); return E_OUTOFMEMORY; }

    if (l1) memcpy(h->buf,      h1->buf, l1 * sizeof(WCHAR));
    if (l2) memcpy(h->buf + l1, h2->buf, l2 * sizeof(WCHAR));
    h->buf[total] = L'\0';
    h->len      = total;
    h->refcount = 1;
    h->is_ref   = 0;

    *out = (HSTRING)h;
    return S_OK;
}

HRESULT WINAPI WindowsDuplicateString(HSTRING src, HSTRING *out)
{
    AnvilHString *h = (AnvilHString *)src;
    if (!out) return E_POINTER;
    if (!h) { *out = NULL; return S_OK; }
    return WindowsCreateString(h->buf, h->len, out);
}

HRESULT WINAPI WindowsCompareStringOrdinal(HSTRING s1, HSTRING s2, INT32 *result)
{
    const WCHAR *b1 = WindowsGetStringRawBuffer(s1, NULL);
    const WCHAR *b2 = WindowsGetStringRawBuffer(s2, NULL);

    if (!result) return E_POINTER;

    {
        /* wcscmp gives <0 / 0 / >0; normalize to -1 / 0 / +1 */
        int cmp = wcscmp(b1, b2);
        *result = (cmp < 0) ? -1 : (cmp > 0) ? 1 : 0;
    }
    return S_OK;
}

UINT32 WINAPI WindowsGetStringLen(HSTRING str)
{
    AnvilHString *h = (AnvilHString *)str;
    return h ? h->len : 0;
}

/* =========================================================================
 * Activation Factory Registry
 *
 * Other Anvil modules (the XAML bridge, ApplicationModel stubs, etc.) call
 * anvil_register_factory() at DLL-load time to advertise their factories.
 * combase_anvil then routes RoGetActivationFactory to the correct factory.
 * ========================================================================= */

#define ANVIL_MAX_FACTORIES 128

typedef struct FactoryEntry {
    WCHAR  class_id[256];   /* e.g. L"Windows.UI.Xaml.Application" */
    void  *factory;         /* IActivationFactory* */
} FactoryEntry;

static FactoryEntry s_factories[ANVIL_MAX_FACTORIES];
static int          s_factory_count = 0;

/*
 * anvil_register_factory — called by other Anvil DLLs to register a stub.
 * Returns 0 on success, -1 if the table is full.
 */
int anvil_register_factory(const WCHAR *class_id, void *factory)
{
    int i;

    /* Overwrite if already registered. */
    for (i = 0; i < s_factory_count; i++) {
        if (wcscmp(s_factories[i].class_id, class_id) == 0) {
            s_factories[i].factory = factory;
            ANVIL_TRACE("Updated factory for " FMT_WS, WFMT(class_id));
            return 0;
        }
    }

    if (s_factory_count >= ANVIL_MAX_FACTORIES) {
        ANVIL_ERR("Factory table full — cannot register " FMT_WS, WFMT(class_id));
        return -1;
    }

    wcsncpy(s_factories[s_factory_count].class_id, class_id, 255);
    s_factories[s_factory_count].class_id[255] = L'\0';
    s_factories[s_factory_count].factory = factory;
    s_factory_count++;

    ANVIL_TRACE("Registered factory for " FMT_WS " (total: %d)", WFMT(class_id), s_factory_count);
    return 0;
}

static void *find_factory(HSTRING class_id_hstr)
{
    LPCWSTR class_id = WindowsGetStringRawBuffer(class_id_hstr, NULL);
    int i;

    for (i = 0; i < s_factory_count; i++) {
        if (wcscmp(s_factories[i].class_id, class_id) == 0)
            return s_factories[i].factory;
    }
    return NULL;
}

/* =========================================================================
 * RoInitialize / RoUninitialize
 * ========================================================================= */

static int s_ro_init_count = 0;

HRESULT WINAPI RoInitialize(RO_INIT_TYPE init_type)
{
    ANVIL_TRACE("RoInitialize(type=%d) — stub OK", (int)init_type);
    s_ro_init_count++;
    /*
     * Real combase enforces STA/MTA threading rules.  For the PoC we
     * always succeed and track the init depth for RoUninitialize.
     */
    return S_OK;
}

void WINAPI RoUninitialize(void)
{
    if (s_ro_init_count > 0) s_ro_init_count--;
    ANVIL_TRACE("RoUninitialize() — depth now %d", s_ro_init_count);
}

/* =========================================================================
 * RoGetActivationFactory
 *
 * This is the critical intercept point.  When Calculator's XAML-generated
 * InitializeComponent() fires, it calls this function for every WinRT
 * class it needs to instantiate.  We route known classes to Anvil stubs
 * and log unknown ones so the call graph can be populated.
 * ========================================================================= */

HRESULT WINAPI RoGetActivationFactory(HSTRING class_id,
                                       REFIID   iid,
                                       void   **factory)
{
    LPCWSTR name;
    void   *found;

    if (!class_id || !factory) return E_POINTER;
    *factory = NULL;

    name  = WindowsGetStringRawBuffer(class_id, NULL);
    found = find_factory(class_id);

    if (!found) {
        ANVIL_WARN("RoGetActivationFactory: NO STUB for '" FMT_WS "' — returning E_NOTIMPL", WFMT(name));
        /*
         * Log the missing class to stderr so the developer can see exactly
         * which factories need to be implemented next.  In a future version
         * this will append to a JSON "missing_factories.json" file.
         */
        fprintf(stderr,
            "[anvil][MISSING FACTORY] " FMT_WS "\n"
            "  → Add anvil_register_factory(L\"" FMT_WS "\", &your_factory);\n",
            WFMT(name), WFMT(name));
        return E_NOTIMPL;
    }

    ANVIL_TRACE("RoGetActivationFactory: '" FMT_WS "' → found stub factory", WFMT(name));

    /*
     * QI the factory for the requested interface.  If the factory only
     * implements IActivationFactory and the caller wants a more specific
     * factory interface (IApplicationFactory etc.), we return the base
     * factory.  Real stubs in later Anvil layers should override this.
     */
    if (IsEqualIID(iid, &IID_IActivationFactory) ||
        IsEqualIID(iid, &IID_IUnknown)            ||
        IsEqualIID(iid, &IID_IInspectable))
    {
        IActivationFactory *f = (IActivationFactory *)found;
        f->lpVtbl->AddRef(f);
        *factory = found;
        return S_OK;
    }

    /* Try QueryInterface for more specific factory interfaces. */
    {
        IActivationFactory *f = (IActivationFactory *)found;
        HRESULT hr = f->lpVtbl->QueryInterface(f, iid, factory);
        if (FAILED(hr)) {
            ANVIL_WARN("RoGetActivationFactory: '" FMT_WS "' factory QI failed for requested IID", WFMT(name));
        }
        return hr;
    }
}

/* =========================================================================
 * RoActivateInstance
 *
 * Convenience wrapper: get the IActivationFactory, call ActivateInstance.
 * ========================================================================= */

HRESULT WINAPI RoActivateInstance(HSTRING class_id, IInspectable **instance)
{
    IActivationFactory *factory = NULL;
    HRESULT hr;

    if (!instance) return E_POINTER;
    *instance = NULL;

    hr = RoGetActivationFactory(class_id, &IID_IActivationFactory,
                                (void **)&factory);
    if (FAILED(hr)) return hr;

    hr = factory->lpVtbl->ActivateInstance(factory, instance);
    factory->lpVtbl->Release(factory);

    ANVIL_TRACE("RoActivateInstance('" FMT_WS "') → hr=0x%08lx",
                WFMT(WindowsGetStringRawBuffer(class_id, NULL)), (unsigned long)hr);
    return hr;
}

/* =========================================================================
 * Error info stubs (RoGetErrorInfo / RoSetErrorInfo)
 * ========================================================================= */

static IRestrictedErrorInfo *s_error_info = NULL;

HRESULT WINAPI RoGetErrorInfo(IRestrictedErrorInfo **err_info)
{
    if (!err_info) return E_POINTER;
    *err_info = s_error_info;
    s_error_info = NULL;
    return S_OK;
}

HRESULT WINAPI RoSetErrorInfo(IRestrictedErrorInfo *err_info)
{
    s_error_info = err_info;
    return S_OK;
}

/* =========================================================================
 * DLL entry point (Wine / Windows)
 * ========================================================================= */

/* Provided by xaml_stubs.c when linked together; no-op otherwise. */
__attribute__((weak)) void anvil_register_all_factories(void) {}

/* Provided by pkg_identity.c when linked together; no-op otherwise. */
__attribute__((weak)) void anvil_install_pkg_hooks(void) {}
__attribute__((weak)) void anvil_remove_pkg_hooks(void) {}

/* Provided by dxgi_stubs.c when linked together; no-op otherwise. */
__attribute__((weak)) void anvil_install_dxgi_hooks(void) {}
__attribute__((weak)) void anvil_remove_dxgi_hooks(void) {}

#ifdef _WIN32

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)hinstDLL;
    (void)lpvReserved;

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        ANVIL_TRACE("combase_anvil.dll loaded — Anvil WinRT stub v0.1");
        anvil_register_all_factories();
        anvil_install_pkg_hooks();
        anvil_install_dxgi_hooks();
        break;
    case DLL_PROCESS_DETACH:
        anvil_remove_dxgi_hooks();
        anvil_remove_pkg_hooks();
        ANVIL_TRACE("combase_anvil.dll unloaded");
        break;
    }
    return TRUE;
}
#endif /* _WIN32 */

/* =========================================================================
 * Anvil-internal: dump the current factory registration table.
 * Call this from a debugger or inject it via a test harness.
 * ========================================================================= */

void anvil_dump_factories(void)
{
    int i;
    fprintf(stderr, "[anvil] Factory registry (%d entries):\n", s_factory_count);
    for (i = 0; i < s_factory_count; i++) {
        fprintf(stderr, "  [%02d] " FMT_WS " → %p\n",
                i, WFMT(s_factories[i].class_id), s_factories[i].factory);
    }
}

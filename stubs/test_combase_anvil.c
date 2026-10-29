/*
 * test_combase_anvil.c — Native Linux unit tests for combase_anvil.c.
 *
 * Compile + run with:
 *   make -f Makefile.wine test
 * OR manually:
 *   gcc -O0 -g -Wall -I. -DANVIL_UNIT_TEST -o out/test combase_anvil.c test_combase_anvil.c && ./out/test
 */

#include "winrt_types.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <assert.h>

/* ---- forward declarations for functions defined in combase_anvil.c ---- */
HRESULT WINAPI WindowsCreateString(LPCWSTR src, UINT32 len, HSTRING *out);
HRESULT WINAPI WindowsCreateStringReference(LPCWSTR src, UINT32 len,
                                             HSTRING_HEADER *hdr, HSTRING *out);
void    WINAPI WindowsDeleteString(HSTRING str);
LPCWSTR WINAPI WindowsGetStringRawBuffer(HSTRING str, UINT32 *len);
BOOL    WINAPI WindowsIsStringEmpty(HSTRING str);
HRESULT WINAPI WindowsConcatString(HSTRING s1, HSTRING s2, HSTRING *out);
HRESULT WINAPI WindowsCompareStringOrdinal(HSTRING s1, HSTRING s2, INT32 *result);

HRESULT WINAPI RoInitialize(RO_INIT_TYPE type);
void    WINAPI RoUninitialize(void);
HRESULT WINAPI RoGetActivationFactory(HSTRING class_id, REFIID iid, void **factory);
HRESULT WINAPI RoActivateInstance(HSTRING class_id, IInspectable **instance);

int anvil_register_factory(const WCHAR *class_id, void *factory);
void anvil_dump_factories(void);

/* ---- test helpers ---- */

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT_EQ(a, b) do { \
    if ((a) == (b)) { g_pass++; } \
    else { g_fail++; fprintf(stderr, "FAIL %s:%d  expected 0x%08lx got 0x%08lx\n", \
        __FILE__, __LINE__, (unsigned long)(b), (unsigned long)(a)); } \
} while(0)

#define EXPECT_TRUE(expr) do { \
    if (expr) { g_pass++; } \
    else { g_fail++; fprintf(stderr, "FAIL %s:%d  expected true: %s\n", \
        __FILE__, __LINE__, #expr); } \
} while(0)

#define EXPECT_WSTREQ(a, b) do { \
    if (wcscmp((a),(b))==0) { g_pass++; } \
    else { g_fail++; fprintf(stderr, "FAIL %s:%d  expected '" FMT_WS "' got '" FMT_WS "'\n", \
        __FILE__, __LINE__, WFMT(b), WFMT(a)); } \
} while(0)

/* =========================================================================
 * HSTRING tests
 * ========================================================================= */

static void test_hstring_basic(void)
{
    HSTRING h = NULL;
    HRESULT hr;
    LPCWSTR buf;
    UINT32 len;

    /* Create from literal */
    hr = WindowsCreateString(L"Hello", 5, &h);
    EXPECT_EQ(hr, S_OK);
    EXPECT_TRUE(h != NULL);

    buf = WindowsGetStringRawBuffer(h, &len);
    EXPECT_WSTREQ(buf, L"Hello");
    EXPECT_EQ(len, 5u);
    EXPECT_TRUE(!WindowsIsStringEmpty(h));

    WindowsDeleteString(h);
}

static void test_hstring_empty(void)
{
    HSTRING h = NULL;
    HRESULT hr;
    LPCWSTR buf;
    UINT32 len;

    hr = WindowsCreateString(L"", 0, &h);
    EXPECT_EQ(hr, S_OK);

    buf = WindowsGetStringRawBuffer(h, &len);
    EXPECT_EQ(len, 0u);
    EXPECT_TRUE(WindowsIsStringEmpty(h));

    WindowsDeleteString(h);

    /* NULL HSTRING is also "empty" */
    EXPECT_TRUE(WindowsIsStringEmpty(NULL));
    buf = WindowsGetStringRawBuffer(NULL, &len);
    EXPECT_EQ(len, 0u);
    EXPECT_WSTREQ(buf, L"");
}

static void test_hstring_null_src(void)
{
    HSTRING h = NULL;
    HRESULT hr;

    hr = WindowsCreateString(NULL, 0, &h);
    EXPECT_EQ(hr, S_OK);

    hr = WindowsCreateString(NULL, 5, &h);
    EXPECT_EQ(hr, E_POINTER);
}

static void test_hstring_concat(void)
{
    HSTRING h1, h2, hout;
    HRESULT hr;
    LPCWSTR buf;
    UINT32 len;

    WindowsCreateString(L"Foo", 3, &h1);
    WindowsCreateString(L"Bar", 3, &h2);

    hr = WindowsConcatString(h1, h2, &hout);
    EXPECT_EQ(hr, S_OK);

    buf = WindowsGetStringRawBuffer(hout, &len);
    EXPECT_WSTREQ(buf, L"FooBar");
    EXPECT_EQ(len, 6u);

    WindowsDeleteString(h1);
    WindowsDeleteString(h2);
    WindowsDeleteString(hout);
}

static void test_hstring_compare(void)
{
    HSTRING h1, h2, h3;
    INT32 result;

    WindowsCreateString(L"Alpha", 5, &h1);
    WindowsCreateString(L"Alpha", 5, &h2);
    WindowsCreateString(L"Beta",  4, &h3);

    WindowsCompareStringOrdinal(h1, h2, &result);
    EXPECT_EQ(result, 0);

    WindowsCompareStringOrdinal(h1, h3, &result);
    EXPECT_TRUE(result < 0);   /* Alpha < Beta */

    WindowsCompareStringOrdinal(h3, h1, &result);
    EXPECT_TRUE(result > 0);   /* Beta > Alpha */

    WindowsDeleteString(h1);
    WindowsDeleteString(h2);
    WindowsDeleteString(h3);
}

static void test_hstring_reference(void)
{
    /* WindowsCreateStringReference must not copy — points to original buffer. */
    HSTRING_HEADER hdr;
    HSTRING h = NULL;
    const WCHAR *src = L"RefString";
    HRESULT hr;
    LPCWSTR buf;
    UINT32 len;

    hr = WindowsCreateStringReference(src, 9, &hdr, &h);
    EXPECT_EQ(hr, S_OK);
    EXPECT_TRUE(h != NULL);

    buf = WindowsGetStringRawBuffer(h, &len);
    EXPECT_EQ(len, 9u);
    EXPECT_WSTREQ(buf, L"RefString");
    EXPECT_TRUE(buf == src);   /* Must point at the original, not a copy */

    WindowsDeleteString(h);   /* Must be a no-op (stack-allocated) */
}

/* =========================================================================
 * RoInitialize tests
 * ========================================================================= */

static void test_ro_initialize(void)
{
    HRESULT hr;

    hr = RoInitialize(RO_INIT_MULTITHREADED);
    EXPECT_EQ(hr, S_OK);

    RoUninitialize();
    /* No crash = pass */
    g_pass++;
}

/* =========================================================================
 * Factory registry tests
 * ========================================================================= */

/* Minimal stub factory that just counts AddRef/Release calls. */
static int g_stub_addref_count = 0;
static int g_stub_release_count = 0;
static int g_stub_activate_count = 0;

static HRESULT WINAPI stub_QI(void *This, REFIID iid, void **out)
{
    (void)This; (void)iid; (void)out;
    return E_NOINTERFACE;
}
static ULONG WINAPI stub_AddRef(void *This)
{
    (void)This; return ++g_stub_addref_count;
}
static ULONG WINAPI stub_Release(void *This)
{
    (void)This; return --g_stub_release_count;
}
static HRESULT WINAPI stub_GetIids(void *T, ULONG *n, IID **i)
{
    (void)T; *n=0; *i=NULL; return S_OK;
}
static HRESULT WINAPI stub_GetClassName(void *T, HSTRING *n)
{
    (void)T; return WindowsCreateString(L"AnvilStub", 9, n);
}
static HRESULT WINAPI stub_GetTrustLevel(void *T, TrustLevel *tl)
{
    (void)T; *tl = BaseTrust; return S_OK;
}
static HRESULT WINAPI stub_ActivateInstance(void *T, IInspectable **inst)
{
    (void)T; *inst = NULL; g_stub_activate_count++; return S_OK;
}

static const IActivationFactoryVtbl s_stub_vtbl = {
    stub_QI, stub_AddRef, stub_Release,
    stub_GetIids, stub_GetClassName, stub_GetTrustLevel,
    stub_ActivateInstance,
};
static IActivationFactory s_stub_factory = { &s_stub_vtbl };

static void test_factory_registry(void)
{
    HSTRING class_id;
    IActivationFactory *factory = NULL;
    IInspectable *instance = NULL;
    HRESULT hr;

    /* Unregistered class → E_NOTIMPL */
    WindowsCreateString(L"Windows.UI.Xaml.Application", 27, &class_id);
    hr = RoGetActivationFactory(class_id, &IID_IActivationFactory,
                                (void **)&factory);
    EXPECT_EQ(hr, E_NOTIMPL);
    EXPECT_TRUE(factory == NULL);
    WindowsDeleteString(class_id);

    /* Register our stub */
    anvil_register_factory(L"Windows.UI.Xaml.Application", &s_stub_factory);

    /* Now it should resolve */
    WindowsCreateString(L"Windows.UI.Xaml.Application", 27, &class_id);
    hr = RoGetActivationFactory(class_id, &IID_IActivationFactory,
                                (void **)&factory);
    EXPECT_EQ(hr, S_OK);
    EXPECT_TRUE(factory != NULL);
    WindowsDeleteString(class_id);

    /* RoActivateInstance should call our ActivateInstance */
    g_stub_activate_count = 0;
    WindowsCreateString(L"Windows.UI.Xaml.Application", 27, &class_id);
    hr = RoActivateInstance(class_id, &instance);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(g_stub_activate_count, 1);
    WindowsDeleteString(class_id);
}

static void test_factory_dump(void)
{
    /* Just ensure it doesn't crash */
    anvil_dump_factories();
    g_pass++;
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(void)
{
    printf("=== combase_anvil unit tests ===\n\n");

    printf("-- HSTRING --\n");
    test_hstring_basic();
    test_hstring_empty();
    test_hstring_null_src();
    test_hstring_concat();
    test_hstring_compare();
    test_hstring_reference();

    printf("-- RoInitialize --\n");
    test_ro_initialize();

    printf("-- Factory registry --\n");
    test_factory_registry();
    test_factory_dump();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

/*
 * p5_stubs.c — Anvil Milestone 5: Window, Application, DependencyProperty statics.
 *
 * IIDs extracted from Windows.UI.Xaml.winmd via ECMA-335 metadata parsing:
 *   IWindowStatics               {93328409-4EA1-4AFA-83DC-0C4E73E88BB1}
 *   IApplicationStatics          {06499997-F7B4-45FE-B763-7577D1D3CB4A}
 *   IDependencyPropertyStatics   {49E5F28F-8259-4D5C-AAE0-83D56DBB68D9}
 *
 * Replaces the P1 DEFINE_FACTORY stubs for the same class names by calling
 * anvil_register_factory() again (it overwrites duplicate entries).
 *
 * DependencyProperty::Register allocates stub DP objects from a fixed pool.
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_p5);
# define P5LOG(fmt, ...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# define P5LOG(fmt, ...) fprintf(stderr, "[anvil/p5] " fmt "\n", ##__VA_ARGS__)
#endif

#include <stddef.h>   /* offsetof */
#include <stdlib.h>   /* malloc */
#include <string.h>   /* memcmp */

extern HRESULT WINAPI WindowsCreateString(LPCWSTR src, UINT32 len, HSTRING *out);
extern int     anvil_register_factory(const WCHAR *class_id, void *factory);

#ifndef CONTAINING_RECORD
# define CONTAINING_RECORD(addr, type, field) \
    ((type *)((char *)(addr) - offsetof(type, field)))
#endif

/* =========================================================================
 * Shared no-op vtable helpers
 * ========================================================================= */
static HRESULT WINAPI p5_AddRef_fn(void *T)                       { (void)T; return 1; }
static ULONG   WINAPI p5_Release_fn(void *T)                      { (void)T; return 1; }
static HRESULT WINAPI p5_GetIids_fn(void *T, ULONG *n, IID **ids){ (void)T; *n=0; *ids=NULL; return S_OK; }
static HRESULT WINAPI p5_GetTL_fn(void *T, TrustLevel *tl)       { (void)T; *tl=BaseTrust; return S_OK; }
static HRESULT WINAPI p5_notimpl(void *T, ...)                    { (void)T; return E_NOTIMPL; }
#define NOTIMPL ((void *)p5_notimpl)

/* =========================================================================
 * IWindow stub object — returned by IWindowStatics::get_Current.
 *
 * Windows.UI.Xaml.IWindow ABI slots (Windows 8.1):
 *   6  get_Bounds(Rect*)
 *   7  get_Visible(BOOL*)
 *   8  get_Content(IUIElement**)
 *   9  put_Content(IUIElement*)
 *   10 add_SizeChanged(handler, token*)
 *   11 remove_SizeChanged(token)
 *   12 add_Activated(handler, token*)
 *   13 remove_Activated(token)
 *   14 add_Closed(handler, token*)
 *   15 remove_Closed(token)
 *   16 get_CoreWindow(ICoreWindow**)
 *   17 get_Dispatcher(ICoreDispatcher**)
 *   18 Activate()
 *   19 Close()
 * ========================================================================= */

typedef struct { float X, Y, Width, Height; } WinRTRect;

typedef struct IWindowVtbl {
    HRESULT (WINAPI *QI)      (void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)  (void *);
    ULONG   (WINAPI *Release) (void *);
    HRESULT (WINAPI *GetIids) (void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRCN)  (void *, HSTRING *);
    HRESULT (WINAPI *GetTL)   (void *, TrustLevel *);
    HRESULT (WINAPI *get_Bounds)  (void *, WinRTRect *);  /* 6 */
    HRESULT (WINAPI *get_Visible) (void *, BOOL *);       /* 7 */
    void *get_Content;   /* 8 */
    void *put_Content;   /* 9 */
    void *add_SizeChanged;      /* 10 */
    void *remove_SizeChanged;   /* 11 */
    void *add_Activated;        /* 12 */
    void *remove_Activated;     /* 13 */
    void *add_Closed;           /* 14 */
    void *remove_Closed;        /* 15 */
    void *get_CoreWindow;       /* 16 */
    void *get_Dispatcher;       /* 17 */
    HRESULT (WINAPI *Activate) (void *); /* 18 */
    HRESULT (WINAPI *Close)    (void *); /* 19 */
} IWindowVtbl;

typedef struct { const IWindowVtbl *lpVtbl; } AnvilWindow;

static HRESULT WINAPI win_QI(void *T, REFIID iid, void **out) { (void)iid; *out=T; return S_OK; }
static HRESULT WINAPI win_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(L"Windows.UI.Xaml.Window", 22, n); }
static HRESULT WINAPI win_get_Bounds(void *T, WinRTRect *r)
    { (void)T; r->X=0; r->Y=0; r->Width=1024; r->Height=768; return S_OK; }
static HRESULT WINAPI win_get_Visible(void *T, BOOL *b) { (void)T; *b=TRUE; return S_OK; }
static HRESULT WINAPI win_Activate(void *T)  { (void)T; P5LOG("Window::Activate"); return S_OK; }
static HRESULT WINAPI win_Close(void *T)     { (void)T; P5LOG("Window::Close"); return S_OK; }

static const IWindowVtbl s_window_vtbl = {
    win_QI, p5_AddRef_fn, p5_Release_fn,
    p5_GetIids_fn, win_GetRCN, p5_GetTL_fn,
    win_get_Bounds,    /* 6  */
    win_get_Visible,   /* 7  */
    NOTIMPL,           /* 8  get_Content */
    NOTIMPL,           /* 9  put_Content */
    NOTIMPL,           /* 10 add_SizeChanged */
    NOTIMPL,           /* 11 remove_SizeChanged */
    NOTIMPL,           /* 12 add_Activated */
    NOTIMPL,           /* 13 remove_Activated */
    NOTIMPL,           /* 14 add_Closed */
    NOTIMPL,           /* 15 remove_Closed */
    NOTIMPL,           /* 16 get_CoreWindow */
    NOTIMPL,           /* 17 get_Dispatcher */
    win_Activate,      /* 18 */
    win_Close,         /* 19 */
};
static AnvilWindow s_current_window = { &s_window_vtbl };

/* =========================================================================
 * Window statics — IWindowStatics {93328409-4EA1-4AFA-83DC-0C4E73E88BB1}
 *
 * IWindowStatics ABI:
 *   6  get_Current(IWindow**)
 * ========================================================================= */

static const IID IID_IWindowStatics = {
    0x93328409, 0x4EA1, 0x4AFA,
    { 0x83, 0xDC, 0x0C, 0x4E, 0x73, 0xE8, 0x8B, 0xB1 }
};

typedef struct AnvilWindowFactory AnvilWindowFactory;
static HRESULT WINAPI window_fact_QI(void *T, REFIID iid, void **out);

typedef struct IWindowStaticsVtbl {
    HRESULT (WINAPI *QI)          (void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)      (void *);
    ULONG   (WINAPI *Release)     (void *);
    HRESULT (WINAPI *GetIids)     (void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRCN)      (void *, HSTRING *);
    HRESULT (WINAPI *GetTL)       (void *, TrustLevel *);
    HRESULT (WINAPI *get_Current) (void *, void **); /* 6 */
} IWindowStaticsVtbl;

struct AnvilWindowFactory {
    const IActivationFactoryVtbl *lpActFact;
    const IWindowStaticsVtbl     *lpStatics;
};

static HRESULT WINAPI wins_QI(void *T, REFIID iid, void **out)
{
    AnvilWindowFactory *f = CONTAINING_RECORD(T, AnvilWindowFactory, lpStatics);
    return window_fact_QI((void *)f, iid, out);
}
static HRESULT WINAPI wins_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(L"Windows.UI.Xaml.Window", 22, n); }
static HRESULT WINAPI wins_get_Current(void *T, void **ppv)
    { (void)T; *ppv = &s_current_window; P5LOG("IWindowStatics::get_Current"); return S_OK; }

static const IWindowStaticsVtbl s_wins_vtbl = {
    wins_QI, p5_AddRef_fn, p5_Release_fn,
    p5_GetIids_fn, wins_GetRCN, p5_GetTL_fn,
    wins_get_Current,  /* 6 */
};

static HRESULT WINAPI window_fact_QI(void *T, REFIID iid, void **out)
{
    AnvilWindowFactory *f = (AnvilWindowFactory *)T;
    if (IsEqualIID(iid, &IID_IActivationFactory) ||
        IsEqualIID(iid, &IID_IUnknown)            ||
        IsEqualIID(iid, &IID_IInspectable)) {
        *out = T; return S_OK;
    }
    if (IsEqualIID(iid, &IID_IWindowStatics)) {
        *out = &f->lpStatics; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI window_fact_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(L"Windows.UI.Xaml.Window", 22, n); }
static HRESULT WINAPI window_fact_Activate(void *T, IInspectable **inst)
    { (void)T; *inst = NULL; return E_NOTIMPL; }

static const IActivationFactoryVtbl s_window_fact_vtbl = {
    window_fact_QI, p5_AddRef_fn, p5_Release_fn,
    p5_GetIids_fn, window_fact_GetRCN, p5_GetTL_fn,
    window_fact_Activate,
};
static AnvilWindowFactory s_window_factory = { &s_window_fact_vtbl, &s_wins_vtbl };

/* =========================================================================
 * Application statics — IApplicationStatics {06499997-F7B4-45FE-B763-7577D1D3CB4A}
 *
 * IApplicationStatics ABI:
 *   6  Start(IApplicationInitializationCallback*)
 * ========================================================================= */

static const IID IID_IApplicationStatics = {
    0x06499997, 0xF7B4, 0x45FE,
    { 0xB7, 0x63, 0x75, 0x77, 0xD1, 0xD3, 0xCB, 0x4A }
};

typedef struct AnvilAppFactory AnvilAppFactory;
static HRESULT WINAPI app_fact_QI(void *T, REFIID iid, void **out);

typedef struct IAppStaticsVtbl {
    HRESULT (WINAPI *QI)      (void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)  (void *);
    ULONG   (WINAPI *Release) (void *);
    HRESULT (WINAPI *GetIids) (void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRCN)  (void *, HSTRING *);
    HRESULT (WINAPI *GetTL)   (void *, TrustLevel *);
    HRESULT (WINAPI *Start)   (void *, void *callback); /* 6 */
} IAppStaticsVtbl;

struct AnvilAppFactory {
    const IActivationFactoryVtbl *lpActFact;
    const IAppStaticsVtbl        *lpStatics;
};

/* IApplicationInitializationCallback vtable:
 *   6: Invoke(IApplicationInitializationCallbackArgs*) */
static HRESULT WINAPI apps_Start(void *T, void *callback)
{
    (void)T;
    P5LOG("IApplicationStatics::Start — invoking callback");
    if (callback) {
        typedef struct { const void **lpVtbl; } ICB;
        ICB *cb = (ICB *)callback;
        typedef HRESULT (WINAPI *PFN_Invoke)(void *, void *);
        ((PFN_Invoke)cb->lpVtbl[6])(callback, NULL /* args stub */);
    }
    P5LOG("IApplicationStatics::Start — returning S_OK");
    return S_OK;
}

static HRESULT WINAPI apps_QI(void *T, REFIID iid, void **out)
{
    AnvilAppFactory *f = CONTAINING_RECORD(T, AnvilAppFactory, lpStatics);
    return app_fact_QI((void *)f, iid, out);
}
static HRESULT WINAPI apps_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(L"Windows.UI.Xaml.Application", 27, n); }

static const IAppStaticsVtbl s_apps_vtbl = {
    apps_QI, p5_AddRef_fn, p5_Release_fn,
    p5_GetIids_fn, apps_GetRCN, p5_GetTL_fn,
    apps_Start,  /* 6 */
};

static HRESULT WINAPI app_fact_QI(void *T, REFIID iid, void **out)
{
    AnvilAppFactory *f = (AnvilAppFactory *)T;
    if (IsEqualIID(iid, &IID_IActivationFactory) ||
        IsEqualIID(iid, &IID_IUnknown)            ||
        IsEqualIID(iid, &IID_IInspectable)) {
        *out = T; return S_OK;
    }
    if (IsEqualIID(iid, &IID_IApplicationStatics)) {
        *out = &f->lpStatics; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI app_fact_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(L"Windows.UI.Xaml.Application", 27, n); }
static HRESULT WINAPI app_fact_Activate(void *T, IInspectable **inst)
    { (void)T; *inst = NULL; return E_NOTIMPL; }

static const IActivationFactoryVtbl s_app_fact_vtbl = {
    app_fact_QI, p5_AddRef_fn, p5_Release_fn,
    p5_GetIids_fn, app_fact_GetRCN, p5_GetTL_fn,
    app_fact_Activate,
};
static AnvilAppFactory s_app_factory = { &s_app_fact_vtbl, &s_apps_vtbl };

/* =========================================================================
 * DependencyProperty statics — IDependencyPropertyStatics
 *   IID: {49E5F28F-8259-4D5C-AAE0-83D56DBB68D9}
 *
 * IDependencyPropertyStatics ABI (Windows 8.1):
 *   6  Register(name, propertyType, ownerType, defaultMetadata, *dp)
 *   7  get_UnsetValue(*IInspectable)
 *   8  RegisterAttached(name, propType, ownerType, meta, *dp)
 *   9  RegisterCore(name, propType, ownerType, meta, *dp)
 *
 * TypeName is passed by value: { HSTRING Name; UINT32 Kind; } (12 bytes on 64-bit).
 * ========================================================================= */

static const IID IID_IDependencyPropertyStatics = {
    0x49E5F28F, 0x8259, 0x4D5C,
    { 0xAA, 0xE0, 0x83, 0xD5, 0x6D, 0xBB, 0x68, 0xD9 }
};

/* Minimal stub DP object — each Register call returns a pointer from the pool */
typedef struct { const void **lpVtbl; } AnvilDP;

static HRESULT WINAPI dp_QI(void *T, REFIID iid, void **out) { (void)iid; *out=T; return S_OK; }
static HRESULT WINAPI dp_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(L"Windows.UI.Xaml.DependencyProperty", 34, n); }
/* get_Metadata (slot 6 of IDependencyProperty) — return S_OK with NULL */
static HRESULT WINAPI dp_GetMetadata(void *T, void *ownerType, void **meta)
    { (void)T; (void)ownerType; *meta=NULL; return S_OK; }

static const void *s_dp_vtbl[] = {
    (void *)dp_QI, (void *)p5_AddRef_fn, (void *)p5_Release_fn,
    (void *)p5_GetIids_fn, (void *)dp_GetRCN, (void *)p5_GetTL_fn,
    (void *)dp_GetMetadata,  /* 6: GetMetadata */
};

/* Pool of stub DP objects (one per Register call, up to MAX) */
#define ANVIL_MAX_DP 256
static AnvilDP s_dp_pool[ANVIL_MAX_DP];
static int     s_dp_count = 0;

/* Sentinel UnsetValue object */
static AnvilDP s_dp_unset = { s_dp_vtbl };

typedef struct { HSTRING Name; UINT32 Kind; } WinRTTypeName;

typedef struct AnvilDPFactory AnvilDPFactory;
static HRESULT WINAPI dp_fact_QI(void *T, REFIID iid, void **out);

typedef struct IDPStaticsVtbl {
    HRESULT (WINAPI *QI)      (void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)  (void *);
    ULONG   (WINAPI *Release) (void *);
    HRESULT (WINAPI *GetIids) (void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRCN)  (void *, HSTRING *);
    HRESULT (WINAPI *GetTL)   (void *, TrustLevel *);
    HRESULT (WINAPI *Register)        (void *, HSTRING, WinRTTypeName, WinRTTypeName, void *, void **); /* 6 */
    HRESULT (WINAPI *get_UnsetValue)  (void *, void **);                                                /* 7 */
    HRESULT (WINAPI *RegisterAttached)(void *, HSTRING, WinRTTypeName, WinRTTypeName, void *, void **); /* 8 */
    HRESULT (WINAPI *RegisterCore)    (void *, HSTRING, WinRTTypeName, WinRTTypeName, void *, void **); /* 9 */
} IDPStaticsVtbl;

struct AnvilDPFactory {
    const IActivationFactoryVtbl *lpActFact;
    const IDPStaticsVtbl         *lpStatics;
};

static HRESULT WINAPI dps_Register(void *T, HSTRING name, WinRTTypeName propType,
                                    WinRTTypeName ownerType, void *meta, void **dp)
{
    (void)T; (void)name; (void)propType; (void)ownerType; (void)meta;
    if (s_dp_count >= ANVIL_MAX_DP) { *dp = NULL; return E_OUTOFMEMORY; }
    s_dp_pool[s_dp_count].lpVtbl = s_dp_vtbl;
    *dp = &s_dp_pool[s_dp_count++];
    P5LOG("IDPStatics::Register → dp[%d] at %p", s_dp_count - 1, *dp);
    return S_OK;
}
static HRESULT WINAPI dps_get_UnsetValue(void *T, void **ppv)
    { (void)T; *ppv = &s_dp_unset; P5LOG("IDPStatics::get_UnsetValue"); return S_OK; }
static HRESULT WINAPI dps_RegisterAttached(void *T, HSTRING n, WinRTTypeName pt,
                                            WinRTTypeName ot, void *m, void **dp)
    { return dps_Register(T, n, pt, ot, m, dp); }
static HRESULT WINAPI dps_RegisterCore(void *T, HSTRING n, WinRTTypeName pt,
                                        WinRTTypeName ot, void *m, void **dp)
    { return dps_Register(T, n, pt, ot, m, dp); }

static HRESULT WINAPI dps_QI(void *T, REFIID iid, void **out)
{
    AnvilDPFactory *f = CONTAINING_RECORD(T, AnvilDPFactory, lpStatics);
    return dp_fact_QI((void *)f, iid, out);
}
static HRESULT WINAPI dps_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(L"Windows.UI.Xaml.DependencyProperty", 34, n); }

static const IDPStaticsVtbl s_dps_vtbl = {
    dps_QI, p5_AddRef_fn, p5_Release_fn,
    p5_GetIids_fn, dps_GetRCN, p5_GetTL_fn,
    dps_Register,         /* 6 */
    dps_get_UnsetValue,   /* 7 */
    dps_RegisterAttached, /* 8 */
    dps_RegisterCore,     /* 9 */
};

static HRESULT WINAPI dp_fact_QI(void *T, REFIID iid, void **out)
{
    AnvilDPFactory *f = (AnvilDPFactory *)T;
    if (IsEqualIID(iid, &IID_IActivationFactory) ||
        IsEqualIID(iid, &IID_IUnknown)            ||
        IsEqualIID(iid, &IID_IInspectable)) {
        *out = T; return S_OK;
    }
    if (IsEqualIID(iid, &IID_IDependencyPropertyStatics)) {
        *out = &f->lpStatics; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI dp_fact_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(L"Windows.UI.Xaml.DependencyProperty", 34, n); }
static HRESULT WINAPI dp_fact_Activate(void *T, IInspectable **inst)
    { (void)T; *inst = NULL; return E_NOTIMPL; }

static const IActivationFactoryVtbl s_dp_fact_vtbl = {
    dp_fact_QI, p5_AddRef_fn, p5_Release_fn,
    p5_GetIids_fn, dp_fact_GetRCN, p5_GetTL_fn,
    dp_fact_Activate,
};
static AnvilDPFactory s_dp_factory = { &s_dp_fact_vtbl, &s_dps_vtbl };

/* =========================================================================
 * Registration — overwrites the P1 DEFINE_FACTORY stubs for the same names.
 * ========================================================================= */
void anvil_register_p5_factories(void)
{
    anvil_register_factory(L"Windows.UI.Xaml.Window",              (IActivationFactory *)&s_window_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Application",         (IActivationFactory *)&s_app_factory);
    anvil_register_factory(L"Windows.UI.Xaml.DependencyProperty",  (IActivationFactory *)&s_dp_factory);
    P5LOG("anvil_register_p5_factories: Window + Application + DependencyProperty statics registered");
}

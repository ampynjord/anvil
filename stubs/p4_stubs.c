/*
 * p4_stubs.c — Anvil Milestone 4: static-interface stubs for critical WinRT classes.
 *
 * Beyond P1/P2/P3 which only need IActivationFactory::ActivateInstance, these
 * classes expose *static interfaces* that apps QI for immediately after getting
 * the factory.  Each implemented here with the correct vtable slot layout
 * (Windows 8.1 ABI) and safe default return values.
 *
 * Classes implemented with full statics:
 *   ApiInformation           — IsTypePresent / IsApiContractPresent → FALSE
 *   UISettings               — ActivateInstance → stub object (fake colors)
 *   DisplayInformation       — GetForCurrentView → stub (96 DPI, scale=1.0)
 *   ResourceLoader           — GetForCurrentView → stub (empty strings)
 *
 * Simple DEFINE_FACTORY stubs (E_NOTIMPL ActivateInstance):
 *   DecimalFormatter, AnalyticsInfo, DispatcherTimer,
 *   DataTransferManager, UserActivitySession, AccessibilitySettings,
 *   CoreApplicationViewTitleBar
 *
 * Registration: add a call to anvil_register_p4_factories() from xaml_stubs.c's
 * anvil_register_all_factories().
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_p4);
# define P4LOG(fmt, ...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# define P4LOG(fmt, ...) fprintf(stderr, "[anvil/p4] " fmt "\n", ##__VA_ARGS__)
#endif

#include <stddef.h>   /* offsetof */
#include <stdlib.h>   /* malloc / free */

extern HRESULT WINAPI WindowsCreateString(LPCWSTR src, UINT32 len, HSTRING *out);
extern void    WINAPI WindowsDeleteString(HSTRING h);
extern int     anvil_register_factory(const WCHAR *class_id, void *factory);

#ifndef CONTAINING_RECORD
# define CONTAINING_RECORD(addr, type, field) \
    ((type *)((char *)(addr) - offsetof(type, field)))
#endif

/* =========================================================================
 * Shared no-op vtable helpers (copy from xaml_stubs.c pattern)
 * ========================================================================= */
static HRESULT WINAPI p4_QI(void *T, REFIID iid, void **out)   { (void)iid; *out=T; return S_OK; }
static ULONG   WINAPI p4_AddRef (void *T)                       { (void)T; return 1; }
static ULONG   WINAPI p4_Release(void *T)                       { (void)T; return 1; }
static HRESULT WINAPI p4_GetIids(void *T, ULONG *n, IID **ids)  { (void)T; *n=0; *ids=NULL; return S_OK; }
static HRESULT WINAPI p4_GetTL(void *T, TrustLevel *tl)         { (void)T; *tl=BaseTrust; return S_OK; }
static HRESULT WINAPI p4_notimpl(void *T, void *a, void *b, void *c, void *d)
    { (void)T;(void)a;(void)b;(void)c;(void)d; return E_NOTIMPL; }
#define NOTIMPL ((void *)p4_notimpl)

/* Simple DEFINE_FACTORY macro (no statics — ActivateInstance → E_NOTIMPL) */
#define DEFINE_FACTORY(tag, wname, wlen, msg)                                       \
static HRESULT WINAPI tag##_GetRCN(void *T, HSTRING *n)                            \
    { (void)T; return WindowsCreateString(wname, wlen, n); }                        \
static HRESULT WINAPI tag##_Activate(void *T, IInspectable **inst)                 \
    { (void)T; *inst=NULL;                                                          \
      P4LOG(msg " — ActivateInstance (E_NOTIMPL)"); return E_NOTIMPL; }             \
static const IActivationFactoryVtbl tag##_vtbl = {                                  \
    p4_QI, p4_AddRef, p4_Release,                                                   \
    p4_GetIids, tag##_GetRCN, p4_GetTL, tag##_Activate };                          \
static IActivationFactory tag##_factory = { &tag##_vtbl }

/* =========================================================================
 * Simple P4 stubs
 * ========================================================================= */
DEFINE_FACTORY(decfmt,
    L"Windows.Globalization.NumberFormatting.DecimalFormatter", 55,
    "DecimalFormatter");

DEFINE_FACTORY(analytics,
    L"Windows.System.Profile.AnalyticsInfo", 37,
    "AnalyticsInfo");

DEFINE_FACTORY(dtimer,
    L"Windows.UI.Xaml.DispatcherTimer", 32,
    "DispatcherTimer");

DEFINE_FACTORY(datatransfer,
    L"Windows.ApplicationModel.DataTransfer.DataTransferManager", 57,
    "DataTransferManager");

DEFINE_FACTORY(useractivity,
    L"Windows.ApplicationModel.UserActivities.UserActivitySession", 59,
    "UserActivitySession");

DEFINE_FACTORY(a11ysettings,
    L"Windows.UI.ViewManagement.AccessibilitySettings", 48,
    "AccessibilitySettings");

DEFINE_FACTORY(titlebar,
    L"Windows.ApplicationModel.Core.CoreApplicationViewTitleBar", 57,
    "CoreApplicationViewTitleBar");

/* =========================================================================
 * ApiInformation — IApiInformationStatics (all checks → FALSE)
 *
 * WinRT ABI slot layout (Windows.Foundation.Metadata.IApiInformationStatics):
 *   6  IsTypePresent(typeName, result*)
 *   7  IsMethodPresent(typeName, methodName, result*)
 *   8  IsMethodPresentWithArity(typeName, methodName, arity, result*)
 *   9  IsEventPresent(typeName, eventName, result*)
 *   10 IsPropertyPresent(typeName, propertyName, result*)
 *   11 IsReadOnlyPropertyPresent(typeName, propertyName, result*)
 *   12 IsWriteablePropertyPresent(typeName, propertyName, result*)
 *   13 IsEnumNamedValuePresent(enumTypeName, valueName, result*)
 *   14 IsApiContractPresent(contractName, majorVersion, result*)
 *   15 IsApiContractPresentByMajorAndMinor(contractName, major, minor, result*)
 * ========================================================================= */

/* IID for IApiInformationStatics {997439FE-F681-4A11-B416-C13A47E8BA36} */
static const IID IID_IApiInformationStatics = {
    0x997439FE, 0xF681, 0x4A11,
    { 0xB4, 0x16, 0xC1, 0x3A, 0x47, 0xE8, 0xBA, 0x36 }
};

typedef struct AnvilApiInfo AnvilApiInfo;

/* Forward-declare QI so the statics vtable can back-reference it */
static HRESULT WINAPI apiinfo_fact_QI(void *T, REFIID iid, void **out);

typedef struct IApiInfoStaticsVtbl {
    HRESULT (WINAPI *QI)      (void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)  (void *);
    ULONG   (WINAPI *Release) (void *);
    HRESULT (WINAPI *GetIids) (void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRCN)  (void *, HSTRING *);
    HRESULT (WINAPI *GetTL)   (void *, TrustLevel *);
    /* Static methods — all set *result=FALSE and return S_OK */
    HRESULT (WINAPI *IsTypePresent)(void *T, HSTRING typeName, BOOL *result);
    HRESULT (WINAPI *IsMethodPresent)(void *T, HSTRING typeName, HSTRING methodName, BOOL *result);
    HRESULT (WINAPI *IsMethodPresentWithArity)(void *T, HSTRING typeName, HSTRING methodName, UINT32 arity, BOOL *result);
    HRESULT (WINAPI *IsEventPresent)(void *T, HSTRING typeName, HSTRING eventName, BOOL *result);
    HRESULT (WINAPI *IsPropertyPresent)(void *T, HSTRING typeName, HSTRING propName, BOOL *result);
    HRESULT (WINAPI *IsReadOnlyPropertyPresent)(void *T, HSTRING typeName, HSTRING propName, BOOL *result);
    HRESULT (WINAPI *IsWriteablePropertyPresent)(void *T, HSTRING typeName, HSTRING propName, BOOL *result);
    HRESULT (WINAPI *IsEnumNamedValuePresent)(void *T, HSTRING enumTypeName, HSTRING valueName, BOOL *result);
    HRESULT (WINAPI *IsApiContractPresent)(void *T, HSTRING contractName, UINT16 majorVersion, BOOL *result);
    HRESULT (WINAPI *IsApiContractPresentByMajorAndMinor)(void *T, HSTRING contractName, UINT16 major, UINT16 minor, BOOL *result);
} IApiInfoStaticsVtbl;

struct AnvilApiInfo {
    const IActivationFactoryVtbl  *lpActFact;
    const IApiInfoStaticsVtbl     *lpStatics;
};

/* All static methods: set result=FALSE, log, return S_OK */
static HRESULT WINAPI ais_IsTypePresent(void *T, HSTRING tn, BOOL *r)
    { (void)T; (void)tn; *r=FALSE; return S_OK; }
static HRESULT WINAPI ais_IsMethodPresent(void *T, HSTRING tn, HSTRING mn, BOOL *r)
    { (void)T; (void)tn; (void)mn; *r=FALSE; return S_OK; }
static HRESULT WINAPI ais_IsMethodPresentWithArity(void *T, HSTRING tn, HSTRING mn, UINT32 a, BOOL *r)
    { (void)T; (void)tn; (void)mn; (void)a; *r=FALSE; return S_OK; }
static HRESULT WINAPI ais_IsEventPresent(void *T, HSTRING tn, HSTRING en, BOOL *r)
    { (void)T; (void)tn; (void)en; *r=FALSE; return S_OK; }
static HRESULT WINAPI ais_IsPropertyPresent(void *T, HSTRING tn, HSTRING pn, BOOL *r)
    { (void)T; (void)tn; (void)pn; *r=FALSE; return S_OK; }
static HRESULT WINAPI ais_IsReadOnlyPropertyPresent(void *T, HSTRING tn, HSTRING pn, BOOL *r)
    { (void)T; (void)tn; (void)pn; *r=FALSE; return S_OK; }
static HRESULT WINAPI ais_IsWriteablePropertyPresent(void *T, HSTRING tn, HSTRING pn, BOOL *r)
    { (void)T; (void)tn; (void)pn; *r=FALSE; return S_OK; }
static HRESULT WINAPI ais_IsEnumNamedValuePresent(void *T, HSTRING tn, HSTRING vn, BOOL *r)
    { (void)T; (void)tn; (void)vn; *r=FALSE; return S_OK; }
static HRESULT WINAPI ais_IsApiContractPresent(void *T, HSTRING cn, UINT16 major, BOOL *r)
    { (void)T; (void)cn; (void)major; *r=FALSE; return S_OK; }
static HRESULT WINAPI ais_IsApiContractPresentByMajorAndMinor(void *T, HSTRING cn, UINT16 maj, UINT16 min, BOOL *r)
    { (void)T; (void)cn; (void)maj; (void)min; *r=FALSE; return S_OK; }

static HRESULT WINAPI ais_QI(void *T, REFIID iid, void **out)
{
    AnvilApiInfo *obj = CONTAINING_RECORD(T, AnvilApiInfo, lpStatics);
    return apiinfo_fact_QI((void *)obj, iid, out);
}
static HRESULT WINAPI ais_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(
        L"Windows.Foundation.Metadata.ApiInformation", 42, n); }

static const IApiInfoStaticsVtbl s_ais_vtbl = {
    ais_QI, p4_AddRef, p4_Release,
    p4_GetIids, ais_GetRCN, p4_GetTL,
    ais_IsTypePresent,
    ais_IsMethodPresent,
    ais_IsMethodPresentWithArity,
    ais_IsEventPresent,
    ais_IsPropertyPresent,
    ais_IsReadOnlyPropertyPresent,
    ais_IsWriteablePropertyPresent,
    ais_IsEnumNamedValuePresent,
    ais_IsApiContractPresent,
    ais_IsApiContractPresentByMajorAndMinor,
};

/* IActivationFactory side */
static HRESULT WINAPI apiinfo_fact_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(
        L"Windows.Foundation.Metadata.ApiInformation", 42, n); }
static HRESULT WINAPI apiinfo_fact_Activate(void *T, IInspectable **inst)
    { (void)T; *inst=NULL; return E_NOTIMPL; }  /* static-only class */

static HRESULT WINAPI apiinfo_fact_QI(void *T, REFIID iid, void **out)
{
    AnvilApiInfo *obj = (AnvilApiInfo *)T;
    if (IsEqualIID(iid, &IID_IActivationFactory) ||
        IsEqualIID(iid, &IID_IUnknown)            ||
        IsEqualIID(iid, &IID_IInspectable)) {
        *out = T; return S_OK;
    }
    if (IsEqualIID(iid, &IID_IApiInformationStatics)) {
        *out = &obj->lpStatics; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}

static const IActivationFactoryVtbl s_apiinfo_fact_vtbl = {
    apiinfo_fact_QI, p4_AddRef, p4_Release,
    p4_GetIids, apiinfo_fact_GetRCN, p4_GetTL,
    apiinfo_fact_Activate,
};

static AnvilApiInfo s_api_info = { &s_apiinfo_fact_vtbl, &s_ais_vtbl };

/* =========================================================================
 * UISettings — ActivateInstance returns a stub object.
 *
 * WinRT ABI (Windows.UI.ViewManagement.IUISettings, Windows 8):
 *   6  get_HandPreference(out: HandPreference*)  → 0 (right)
 *   7  get_CursorSize(out: Size*)               → {32, 32}
 *   8  get_ScrollBarSize(out: Size*)             → {17, 17}
 *   9  get_ScrollArrowSize(out: Size*)           → {17, 17}
 *   10 get_ScrollBarThumbBoxSize(out: Size*)     → {17, 17}
 *   11 get_MessageDuration(out: UINT32*)         → 5
 *   12 get_AnimationsEnabled(out: BOOL*)         → FALSE
 *   13 get_CaretBrowsingEnabled(out: BOOL*)      → FALSE
 *   14 get_CaretBlinkRate(out: UINT32*)          → 530
 *   15 get_CaretWidth(out: UINT32*)              → 2
 *   16 get_DoubleClickTime(out: UINT32*)         → 500
 *   17 get_MouseHoverTime(out: UINT32*)          → 400
 *   18 GetColorValue(type: UIColorType, out: Color*) → black (0,0,0,255)
 *
 * IUISettings3 (Windows 10, slot 6 of that interface):
 *   get_ColorValuesChanged event (add/remove) — not implemented
 * ========================================================================= */

typedef struct { BYTE A, R, G, B; } WinRTColor;
typedef struct { float Width, Height; } WinRTSize;

typedef struct IUISettingsVtbl {
    HRESULT (WINAPI *QI)      (void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)  (void *);
    ULONG   (WINAPI *Release) (void *);
    HRESULT (WINAPI *GetIids) (void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRCN)  (void *, HSTRING *);
    HRESULT (WINAPI *GetTL)   (void *, TrustLevel *);
    HRESULT (WINAPI *get_HandPreference)(void *, UINT32 *);          /* 6  */
    HRESULT (WINAPI *get_CursorSize)    (void *, WinRTSize *);       /* 7  */
    HRESULT (WINAPI *get_ScrollBarSize) (void *, WinRTSize *);       /* 8  */
    HRESULT (WINAPI *get_ScrollArrowSize)(void *, WinRTSize *);      /* 9  */
    HRESULT (WINAPI *get_ScrollBarThumbBoxSize)(void *, WinRTSize *);/* 10 */
    HRESULT (WINAPI *get_MessageDuration)(void *, UINT32 *);         /* 11 */
    HRESULT (WINAPI *get_AnimationsEnabled)(void *, BOOL *);         /* 12 */
    HRESULT (WINAPI *get_CaretBrowsingEnabled)(void *, BOOL *);      /* 13 */
    HRESULT (WINAPI *get_CaretBlinkRate)(void *, UINT32 *);          /* 14 */
    HRESULT (WINAPI *get_CaretWidth)(void *, UINT32 *);              /* 15 */
    HRESULT (WINAPI *get_DoubleClickTime)(void *, UINT32 *);         /* 16 */
    HRESULT (WINAPI *get_MouseHoverTime)(void *, UINT32 *);          /* 17 */
    HRESULT (WINAPI *GetColorValue)(void *, UINT32 type, WinRTColor *); /* 18 */
} IUISettingsVtbl;

typedef struct { const IUISettingsVtbl *lpVtbl; } AnvilUISettings;

static HRESULT WINAPI uis_QI(void *T, REFIID iid, void **out)   { (void)iid; *out=T; return S_OK; }
static HRESULT WINAPI uis_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(L"Windows.UI.ViewManagement.UISettings",36,n); }
static HRESULT WINAPI uis_get_HandPreference(void *T, UINT32 *v) { (void)T; *v=0; return S_OK; }
static HRESULT WINAPI uis_get_CursorSize(void *T, WinRTSize *s)  { (void)T; s->Width=32; s->Height=32; return S_OK; }
static HRESULT WINAPI uis_get_ScrollBarSize(void *T, WinRTSize *s){ (void)T; s->Width=17; s->Height=17; return S_OK; }
static HRESULT WINAPI uis_get_ScrollArrowSize(void *T, WinRTSize *s){ (void)T; s->Width=17; s->Height=17; return S_OK; }
static HRESULT WINAPI uis_get_ScrollBarThumbBoxSize(void *T, WinRTSize *s){ (void)T; s->Width=17; s->Height=17; return S_OK; }
static HRESULT WINAPI uis_get_MessageDuration(void *T, UINT32 *v) { (void)T; *v=5; return S_OK; }
static HRESULT WINAPI uis_get_AnimationsEnabled(void *T, BOOL *v) { (void)T; *v=FALSE; return S_OK; }
static HRESULT WINAPI uis_get_CaretBrowsingEnabled(void *T, BOOL *v){ (void)T; *v=FALSE; return S_OK; }
static HRESULT WINAPI uis_get_CaretBlinkRate(void *T, UINT32 *v)  { (void)T; *v=530; return S_OK; }
static HRESULT WINAPI uis_get_CaretWidth(void *T, UINT32 *v)      { (void)T; *v=2; return S_OK; }
static HRESULT WINAPI uis_get_DoubleClickTime(void *T, UINT32 *v) { (void)T; *v=500; return S_OK; }
static HRESULT WINAPI uis_get_MouseHoverTime(void *T, UINT32 *v)  { (void)T; *v=400; return S_OK; }
static HRESULT WINAPI uis_GetColorValue(void *T, UINT32 type, WinRTColor *c)
{
    (void)T;
    /* UIColorType: Background=0 (black), Foreground=1 (white), AccentDark3=2..7,
       Accent=8 (blue), Complement=9..                                       */
    switch (type) {
    case 1:  /* Foreground */ c->A=255; c->R=255; c->G=255; c->B=255; break;
    case 8:  /* Accent     */ c->A=255; c->R=0;   c->G=120; c->B=215; break;
    default: /* Background / dark */ c->A=255; c->R=0; c->G=0; c->B=0; break;
    }
    return S_OK;
}

static const IUISettingsVtbl s_uisettings_vtbl = {
    uis_QI, p4_AddRef, p4_Release,
    p4_GetIids, uis_GetRCN, p4_GetTL,
    uis_get_HandPreference,
    uis_get_CursorSize,
    uis_get_ScrollBarSize,
    uis_get_ScrollArrowSize,
    uis_get_ScrollBarThumbBoxSize,
    uis_get_MessageDuration,
    uis_get_AnimationsEnabled,
    uis_get_CaretBrowsingEnabled,
    uis_get_CaretBlinkRate,
    uis_get_CaretWidth,
    uis_get_DoubleClickTime,
    uis_get_MouseHoverTime,
    uis_GetColorValue,
};

static HRESULT WINAPI uisettings_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(L"Windows.UI.ViewManagement.UISettings",36,n); }
static HRESULT WINAPI uisettings_Activate(void *T, IInspectable **inst)
{
    (void)T;
    AnvilUISettings *obj = (AnvilUISettings *)malloc(sizeof *obj);
    if (!obj) { *inst = NULL; return E_OUTOFMEMORY; }
    obj->lpVtbl = &s_uisettings_vtbl;
    *inst = (IInspectable *)obj;
    P4LOG("UISettings — ActivateInstance → stub object %p", (void *)obj);
    return S_OK;
}
static const IActivationFactoryVtbl s_uisettings_fact_vtbl = {
    p4_QI, p4_AddRef, p4_Release,
    p4_GetIids, uisettings_GetRCN, p4_GetTL,
    uisettings_Activate,
};
static IActivationFactory uisettings_factory = { &s_uisettings_fact_vtbl };

/* =========================================================================
 * DisplayInformation — stub static GetForCurrentView → 96 DPI object.
 *
 * IDisplayInformationStatics (IID: {BC4F2374-8B68-41F0-B146-DE6F3E720A7D})
 *   6: GetForCurrentView(out: IDisplayInformation**)
 *   7: get_RawPixelsPerViewPixel ... (more static members)
 * ========================================================================= */

static const IID IID_IDisplayInformationStatics = {
    0xBC4F2374, 0x8B68, 0x41F0,
    { 0xB1, 0x46, 0xDE, 0x6F, 0x3E, 0x72, 0x0A, 0x7D }
};

typedef struct AnvilDisplayInfo AnvilDisplayInfo;

typedef struct IDisplayInfoVtbl {
    HRESULT (WINAPI *QI)      (void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)  (void *);
    ULONG   (WINAPI *Release) (void *);
    HRESULT (WINAPI *GetIids) (void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRCN)  (void *, HSTRING *);
    HRESULT (WINAPI *GetTL)   (void *, TrustLevel *);
    /* IDisplayInformation slots 6..N */
    HRESULT (WINAPI *get_CurrentOrientation)(void *, UINT32 *);  /* 6  */
    HRESULT (WINAPI *get_NativeOrientation) (void *, UINT32 *);  /* 7  */
    HRESULT (WINAPI *add_OrientationChanged)(void *, void *, void *); /* 8 */
    HRESULT (WINAPI *remove_OrientationChanged)(void *, void *);     /* 9 */
    HRESULT (WINAPI *get_IsStereoscopicDisplayMode)(void *, BOOL *); /* 10 */
    HRESULT (WINAPI *add_StereoEnabledChanged)(void *, void *, void *); /* 11 */
    HRESULT (WINAPI *remove_StereoEnabledChanged)(void *, void *);     /* 12 */
    HRESULT (WINAPI *get_LogicalDpi)         (void *, float *);  /* 13 */
    HRESULT (WINAPI *get_RawDpiX)            (void *, float *);  /* 14 */
    HRESULT (WINAPI *get_RawDpiY)            (void *, float *);  /* 15 */
    HRESULT (WINAPI *add_DpiChanged)         (void *, void *, void *); /* 16 */
    HRESULT (WINAPI *remove_DpiChanged)      (void *, void *);        /* 17 */
    HRESULT (WINAPI *get_ResolutionScale)    (void *, UINT32 *); /* 18 */
    HRESULT (WINAPI *get_RawPixelsPerViewPixel)(void *, double *); /* 19 */
} IDisplayInfoVtbl;

typedef struct { const IDisplayInfoVtbl *lpVtbl; } AnvilDisplayInfoObj;

static HRESULT WINAPI di_QI(void *T, REFIID iid, void **out)   { (void)iid; *out=T; return S_OK; }
static HRESULT WINAPI di_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(
        L"Windows.Graphics.Display.DisplayInformation", 43, n); }
static HRESULT WINAPI di_get_CurrentOrientation(void *T, UINT32 *v) { (void)T; *v=1; return S_OK; } /* Landscape */
static HRESULT WINAPI di_get_NativeOrientation (void *T, UINT32 *v) { (void)T; *v=1; return S_OK; }
static HRESULT WINAPI di_get_IsStereoscopic    (void *T, BOOL *v)   { (void)T; *v=FALSE; return S_OK; }
static HRESULT WINAPI di_get_LogicalDpi        (void *T, float *v)  { (void)T; *v=96.0f; return S_OK; }
static HRESULT WINAPI di_get_RawDpiX           (void *T, float *v)  { (void)T; *v=96.0f; return S_OK; }
static HRESULT WINAPI di_get_RawDpiY           (void *T, float *v)  { (void)T; *v=96.0f; return S_OK; }
static HRESULT WINAPI di_get_ResolutionScale   (void *T, UINT32 *v) { (void)T; *v=100; return S_OK; }
static HRESULT WINAPI di_get_RawPixelsPerViewPixel(void *T, double *v){ (void)T; *v=1.0; return S_OK; }

static const IDisplayInfoVtbl s_di_vtbl = {
    di_QI, p4_AddRef, p4_Release,
    p4_GetIids, di_GetRCN, p4_GetTL,
    di_get_CurrentOrientation,
    di_get_NativeOrientation,
    NOTIMPL,  /* add_OrientationChanged */
    NOTIMPL,  /* remove_OrientationChanged */
    di_get_IsStereoscopic,
    NOTIMPL,  /* add_StereoEnabledChanged */
    NOTIMPL,  /* remove_StereoEnabledChanged */
    di_get_LogicalDpi,
    di_get_RawDpiX,
    di_get_RawDpiY,
    NOTIMPL,  /* add_DpiChanged */
    NOTIMPL,  /* remove_DpiChanged */
    di_get_ResolutionScale,
    di_get_RawPixelsPerViewPixel,
};
static AnvilDisplayInfoObj s_display_info = { &s_di_vtbl };

/* IDisplayInformationStatics vtable */
typedef struct AnvilDisplayInfoStatics AnvilDisplayInfoStatics;
static HRESULT WINAPI displayinfo_fact_QI(void *T, REFIID iid, void **out);

typedef struct IDispInfoStaticsVtbl {
    HRESULT (WINAPI *QI)      (void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)  (void *);
    ULONG   (WINAPI *Release) (void *);
    HRESULT (WINAPI *GetIids) (void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRCN)  (void *, HSTRING *);
    HRESULT (WINAPI *GetTL)   (void *, TrustLevel *);
    HRESULT (WINAPI *GetForCurrentView)(void *, void **);   /* 6  */
    HRESULT (WINAPI *add_DisplayContentsInvalidated)(void *, void *, void *); /* 7 */
    HRESULT (WINAPI *remove_DisplayContentsInvalidated)(void *, void *);      /* 8 */
} IDispInfoStaticsVtbl;

struct AnvilDisplayInfoStatics {
    const IActivationFactoryVtbl  *lpActFact;
    const IDispInfoStaticsVtbl    *lpStatics;
};

static HRESULT WINAPI dis_QI(void *T, REFIID iid, void **out)
{
    AnvilDisplayInfoStatics *obj = CONTAINING_RECORD(T, AnvilDisplayInfoStatics, lpStatics);
    return displayinfo_fact_QI((void *)obj, iid, out);
}
static HRESULT WINAPI dis_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(
        L"Windows.Graphics.Display.DisplayInformation", 43, n); }
static HRESULT WINAPI dis_GetForCurrentView(void *T, void **ppv)
    { (void)T; *ppv = &s_display_info; P4LOG("DisplayInformation::GetForCurrentView"); return S_OK; }

static const IDispInfoStaticsVtbl s_dis_vtbl = {
    dis_QI, p4_AddRef, p4_Release,
    p4_GetIids, dis_GetRCN, p4_GetTL,
    dis_GetForCurrentView,
    NOTIMPL,  /* add_DisplayContentsInvalidated */
    NOTIMPL,  /* remove_DisplayContentsInvalidated */
};

static HRESULT WINAPI displayinfo_fact_QI(void *T, REFIID iid, void **out)
{
    AnvilDisplayInfoStatics *obj = (AnvilDisplayInfoStatics *)T;
    if (IsEqualIID(iid, &IID_IActivationFactory) ||
        IsEqualIID(iid, &IID_IUnknown)            ||
        IsEqualIID(iid, &IID_IInspectable)) {
        *out = T; return S_OK;
    }
    if (IsEqualIID(iid, &IID_IDisplayInformationStatics)) {
        *out = &obj->lpStatics; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI displayinfo_fact_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(
        L"Windows.Graphics.Display.DisplayInformation", 43, n); }
static HRESULT WINAPI displayinfo_fact_Activate(void *T, IInspectable **inst)
    { (void)T; *inst=NULL; return E_NOTIMPL; }

static const IActivationFactoryVtbl s_displayinfo_fact_vtbl = {
    displayinfo_fact_QI, p4_AddRef, p4_Release,
    p4_GetIids, displayinfo_fact_GetRCN, p4_GetTL,
    displayinfo_fact_Activate,
};
static AnvilDisplayInfoStatics s_dispinfo_statics = {
    &s_displayinfo_fact_vtbl, &s_dis_vtbl
};

/* =========================================================================
 * ResourceLoader — GetForCurrentView / GetForViewIndependentUse → stub that
 * returns empty strings for any resource key.
 *
 * IResourceLoaderStatics2 (IID: {B1F9FB8B-8BF7-4B79-8E88-FE2E2E7E8E7F})
 *   6: GetForCurrentView() → IResourceLoader*
 *   7: GetForCurrentView(name) → IResourceLoader*
 *   8: GetForViewIndependentUse() → IResourceLoader*
 *   9: GetForViewIndependentUse(name) → IResourceLoader*
 * ========================================================================= */

typedef struct IResourceLoaderVtbl {
    HRESULT (WINAPI *QI)      (void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)  (void *);
    ULONG   (WINAPI *Release) (void *);
    HRESULT (WINAPI *GetIids) (void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRCN)  (void *, HSTRING *);
    HRESULT (WINAPI *GetTL)   (void *, TrustLevel *);
    HRESULT (WINAPI *GetString)(void *, HSTRING resource, HSTRING *value); /* 6 */
} IResourceLoaderVtbl;

typedef struct { const IResourceLoaderVtbl *lpVtbl; } AnvilResourceLoader;

static HRESULT WINAPI rl_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(
        L"Windows.ApplicationModel.Resources.ResourceLoader", 50, n); }
static HRESULT WINAPI rl_GetString(void *T, HSTRING resource, HSTRING *value)
{
    (void)T; (void)resource;
    return WindowsCreateString(L"", 0, value);  /* empty string for any resource */
}
static const IResourceLoaderVtbl s_rl_vtbl = {
    p4_QI, p4_AddRef, p4_Release,
    p4_GetIids, rl_GetRCN, p4_GetTL,
    rl_GetString,
};
static AnvilResourceLoader s_resource_loader = { &s_rl_vtbl };

typedef struct AnvilResLoaderStatics AnvilResLoaderStatics;
static HRESULT WINAPI resloader_fact_QI(void *T, REFIID iid, void **out);

static const IID IID_IResourceLoaderStatics2 = {
    0xB1F9FB8B, 0x8BF7, 0x4B79,
    { 0x8E, 0x88, 0xFE, 0x2E, 0x2E, 0x7E, 0x8E, 0x7F }
};

typedef struct IResLoaderStaticsVtbl {
    HRESULT (WINAPI *QI)      (void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)  (void *);
    ULONG   (WINAPI *Release) (void *);
    HRESULT (WINAPI *GetIids) (void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRCN)  (void *, HSTRING *);
    HRESULT (WINAPI *GetTL)   (void *, TrustLevel *);
    HRESULT (WINAPI *GetForCurrentView)(void *, void **);            /* 6 */
    HRESULT (WINAPI *GetForCurrentViewByName)(void *, HSTRING, void **); /* 7 */
    HRESULT (WINAPI *GetForViewIndependentUse)(void *, void **);     /* 8 */
    HRESULT (WINAPI *GetForViewIndependentUseByName)(void *, HSTRING, void **); /* 9 */
} IResLoaderStaticsVtbl;

struct AnvilResLoaderStatics {
    const IActivationFactoryVtbl  *lpActFact;
    const IResLoaderStaticsVtbl   *lpStatics;
};

static HRESULT WINAPI rls_QI(void *T, REFIID iid, void **out)
{
    AnvilResLoaderStatics *obj = CONTAINING_RECORD(T, AnvilResLoaderStatics, lpStatics);
    return resloader_fact_QI((void *)obj, iid, out);
}
static HRESULT WINAPI rls_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(
        L"Windows.ApplicationModel.Resources.ResourceLoader", 50, n); }
static HRESULT WINAPI rls_GetForCurrentView(void *T, void **ppv)
    { (void)T; *ppv = &s_resource_loader; return S_OK; }
static HRESULT WINAPI rls_GetForCurrentViewByName(void *T, HSTRING n, void **ppv)
    { (void)T; (void)n; *ppv = &s_resource_loader; return S_OK; }
static HRESULT WINAPI rls_GetForViewIndependentUse(void *T, void **ppv)
    { (void)T; *ppv = &s_resource_loader; return S_OK; }
static HRESULT WINAPI rls_GetForViewIndependentUseByName(void *T, HSTRING n, void **ppv)
    { (void)T; (void)n; *ppv = &s_resource_loader; return S_OK; }

static const IResLoaderStaticsVtbl s_rls_vtbl = {
    rls_QI, p4_AddRef, p4_Release,
    p4_GetIids, rls_GetRCN, p4_GetTL,
    rls_GetForCurrentView,
    rls_GetForCurrentViewByName,
    rls_GetForViewIndependentUse,
    rls_GetForViewIndependentUseByName,
};

static HRESULT WINAPI resloader_fact_QI(void *T, REFIID iid, void **out)
{
    AnvilResLoaderStatics *obj = (AnvilResLoaderStatics *)T;
    if (IsEqualIID(iid, &IID_IActivationFactory) ||
        IsEqualIID(iid, &IID_IUnknown)            ||
        IsEqualIID(iid, &IID_IInspectable)) {
        *out = T; return S_OK;
    }
    if (IsEqualIID(iid, &IID_IResourceLoaderStatics2)) {
        *out = &obj->lpStatics; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI resloader_fact_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(
        L"Windows.ApplicationModel.Resources.ResourceLoader", 50, n); }
static HRESULT WINAPI resloader_fact_Activate(void *T, IInspectable **inst)
    { (void)T; *inst = (IInspectable *)&s_resource_loader; return S_OK; }

static const IActivationFactoryVtbl s_resloader_fact_vtbl = {
    resloader_fact_QI, p4_AddRef, p4_Release,
    p4_GetIids, resloader_fact_GetRCN, p4_GetTL,
    resloader_fact_Activate,
};
static AnvilResLoaderStatics s_resloader_statics = {
    &s_resloader_fact_vtbl, &s_rls_vtbl
};

/* =========================================================================
 * Registration
 * ========================================================================= */
void anvil_register_p4_factories(void)
{
    /* Simple stubs */
    anvil_register_factory(L"Windows.Globalization.NumberFormatting.DecimalFormatter", &decfmt_factory);
    anvil_register_factory(L"Windows.System.Profile.AnalyticsInfo",                  &analytics_factory);
    anvil_register_factory(L"Windows.UI.Xaml.DispatcherTimer",                       &dtimer_factory);
    anvil_register_factory(L"Windows.ApplicationModel.DataTransfer.DataTransferManager", &datatransfer_factory);
    anvil_register_factory(L"Windows.ApplicationModel.UserActivities.UserActivitySession", &useractivity_factory);
    anvil_register_factory(L"Windows.UI.ViewManagement.AccessibilitySettings",       &a11ysettings_factory);
    anvil_register_factory(L"Windows.ApplicationModel.Core.CoreApplicationViewTitleBar", &titlebar_factory);
    /* Statics-enabled */
    anvil_register_factory(L"Windows.Foundation.Metadata.ApiInformation",
                           (IActivationFactory *)&s_api_info);
    anvil_register_factory(L"Windows.UI.ViewManagement.UISettings",                  &uisettings_factory);
    anvil_register_factory(L"Windows.Graphics.Display.DisplayInformation",
                           (IActivationFactory *)&s_dispinfo_statics);
    anvil_register_factory(L"Windows.ApplicationModel.Resources.ResourceLoader",
                           (IActivationFactory *)&s_resloader_statics);

    P4LOG("anvil_register_p4_factories: 11 P4 stubs registered");
}

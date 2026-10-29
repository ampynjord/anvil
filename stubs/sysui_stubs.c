/*
 * sysui_stubs.c — Anvil: DisplayInformation, ApplicationView, PropertyValue,
 *                         ThreadPool, Gamepad stubs.
 *
 * IDisplayInformationStatics / IDisplayInformation:
 *   returns 1920×1080, 96 DPI, Landscape, Scale100Percent.
 *
 * IApplicationViewStatics / IApplicationView:
 *   put_Title sets the Win32 window title.
 *
 * IPropertyValueStatics:
 *   boxes primitive values; returned objects have minimal IPropertyValue vtable.
 *
 * IThreadPoolStatics:
 *   RunAsync runs the work item synchronously and returns a completed IAsyncAction.
 *
 * IGamepadStatics:
 *   get_Gamepads returns an empty IVectorView.
 *
 * Registered via anvil_register_sysui_factories() called from xaml_stubs.c.
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_sysui);
# define SLOG(fmt,...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# define SLOG(fmt,...) fprintf(stderr, "[anvil/sysui] " fmt "\n", ##__VA_ARGS__)
#endif

extern HRESULT WINAPI WindowsCreateString(LPCWSTR src, UINT32 len, HSTRING *out);
extern LPCWSTR WINAPI WindowsGetStringRawBuffer(HSTRING str, UINT32 *len);
extern int     anvil_register_factory(const WCHAR *class_id, void *factory);
extern void   *anvil_completed_action(void);  /* dispatcher_stubs.c */
extern HWND    anvil_get_corewindow_hwnd(void); /* core_objects.c */

#include <stddef.h>

#ifndef CONTAINING_RECORD
# define CONTAINING_RECORD(addr, type, field) \
    ((type *)((char *)(addr) - offsetof(type, field)))
#endif

static HRESULT WINAPI su_notimpl(void *T, ...) { (void)T; return E_NOTIMPL; }
#define NOTIMPL ((void *)su_notimpl)

static ULONG   WINAPI su_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI su_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI su_GetIids(void *T, ULONG *n, IID **ids)
    { (void)T; *n = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI su_GetTL(void *T, TrustLevel *tl)
    { (void)T; *tl = BaseTrust; return S_OK; }
static HRESULT WINAPI su_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }

/* =========================================================================
 * Empty IVectorView — used by Gamepad.get_Gamepads
 * vtable:
 *   0-5  IInspectable
 *   6    GetAt(UINT32, T**)
 *   7    get_Size(UINT32*)
 *   8    IndexOf(T, UINT32*, BOOL*)
 *   9    GetMany(UINT32, UINT32, T*, UINT32*)
 * ========================================================================= */

static HRESULT WINAPI evv_GetAt(void *T, UINT32 i, void **out)
    { (void)T; (void)i; *out = NULL; return E_BOUNDS; }
static HRESULT WINAPI evv_get_Size(void *T, UINT32 *out)
    { (void)T; *out = 0; return S_OK; }
static HRESULT WINAPI evv_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(L"Windows.Foundation.Collections.IVectorView`1", 44, s); }

typedef struct { void **lpVtbl; } EmptyVectorView;

static void *s_evv_vtbl[10] = {
    (void *)su_QI, (void *)su_AddRef, (void *)su_Release,
    (void *)su_GetIids, (void *)evv_GetRCN, (void *)su_GetTL,
    (void *)evv_GetAt,    /* 6 GetAt      */
    (void *)evv_get_Size, /* 7 get_Size   */
    NOTIMPL,              /* 8 IndexOf    */
    NOTIMPL,              /* 9 GetMany    */
};

static EmptyVectorView g_empty_vv = { s_evv_vtbl };

/* =========================================================================
 * IDisplayInformation singleton
 *
 * vtable:
 *   0-5  IInspectable
 *   6    get_CurrentOrientation (DisplayOrientations* / UINT32*)
 *   7    get_NativeOrientation
 *   8    get_AutoRotationPreferences
 *   9    put_AutoRotationPreferences
 *   10   add_OrientationChanged
 *   11   remove_OrientationChanged
 *   12   get_ResolutionScale (ResolutionScale* / UINT32*)
 *   13   get_LogicalDpi (float*)
 *   14   get_RawDpiX (float*)
 *   15   get_RawDpiY (float*)
 *   16   get_RawPixelsPerViewPixel (double*)
 *   17   add_DpiChanged
 *   18   remove_DpiChanged
 *   19   get_StereoEnabled (BOOL*)
 *   20   add_StereoEnabledChanged
 *   21   remove_StereoEnabledChanged
 *   22   GetColorProfileAsync
 *
 * IDisplayInformation2 (additional statics, slot 23+):
 *   23   get_ScreenWidthInRawPixels (UINT32*)
 *   24   get_ScreenHeightInRawPixels (UINT32*)
 * ========================================================================= */

typedef struct { void **lpVtbl; } AnvilDisplayInfo;

static HRESULT WINAPI di_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.Graphics.Display.DisplayInformation", 43, s); }

static HRESULT WINAPI di_get_CurrentOrientation(void *T, UINT32 *out)
    { (void)T; *out = 1; /* Landscape */ return S_OK; }
static HRESULT WINAPI di_get_NativeOrientation(void *T, UINT32 *out)
    { (void)T; *out = 1; /* Landscape */ return S_OK; }
static HRESULT WINAPI di_get_ResolutionScale(void *T, UINT32 *out)
    { (void)T; *out = 100; /* Scale100Percent */ return S_OK; }
static HRESULT WINAPI di_get_LogicalDpi(void *T, float *out)
    { (void)T; *out = 96.0f; return S_OK; }
static HRESULT WINAPI di_get_RawDpiX(void *T, float *out)
    { (void)T; *out = 96.0f; return S_OK; }
static HRESULT WINAPI di_get_RawDpiY(void *T, float *out)
    { (void)T; *out = 96.0f; return S_OK; }
static HRESULT WINAPI di_get_RawPixelsPerViewPixel(void *T, double *out)
    { (void)T; *out = 1.0; return S_OK; }
static HRESULT WINAPI di_get_StereoEnabled(void *T, BOOL *out)
    { (void)T; *out = FALSE; return S_OK; }
static HRESULT WINAPI di_get_ScreenWidth(void *T, UINT32 *out)
    { (void)T; *out = 1920; return S_OK; }
static HRESULT WINAPI di_get_ScreenHeight(void *T, UINT32 *out)
    { (void)T; *out = 1080; return S_OK; }

static void *s_di_vtbl[25] = {
    (void *)su_QI, (void *)su_AddRef, (void *)su_Release,
    (void *)su_GetIids, (void *)di_GetRCN, (void *)su_GetTL,
    (void *)di_get_CurrentOrientation,     /* 6  */
    (void *)di_get_NativeOrientation,      /* 7  */
    NOTIMPL,                               /* 8  get_AutoRotationPreferences */
    NOTIMPL,                               /* 9  put_AutoRotationPreferences */
    NOTIMPL,                               /* 10 add_OrientationChanged      */
    NOTIMPL,                               /* 11 remove_OrientationChanged   */
    (void *)di_get_ResolutionScale,        /* 12 */
    (void *)di_get_LogicalDpi,             /* 13 */
    (void *)di_get_RawDpiX,               /* 14 */
    (void *)di_get_RawDpiY,               /* 15 */
    (void *)di_get_RawPixelsPerViewPixel,  /* 16 */
    NOTIMPL,                               /* 17 add_DpiChanged              */
    NOTIMPL,                               /* 18 remove_DpiChanged           */
    (void *)di_get_StereoEnabled,          /* 19 */
    NOTIMPL,                               /* 20 add_StereoEnabledChanged    */
    NOTIMPL,                               /* 21 remove_StereoEnabledChanged */
    NOTIMPL,                               /* 22 GetColorProfileAsync        */
    (void *)di_get_ScreenWidth,            /* 23 get_ScreenWidthInRawPixels  */
    (void *)di_get_ScreenHeight,           /* 24 get_ScreenHeightInRawPixels */
};

static AnvilDisplayInfo g_display_info = { s_di_vtbl };

/* IDisplayInformationStatics */
typedef struct { void **lpVtbl; } DiStaticsObj;

static HRESULT WINAPI dis_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.Graphics.Display.DisplayInformation", 43, s); }

static HRESULT WINAPI dis_GetForCurrentView(void *T, void **out)
    { (void)T; *out = &g_display_info; return S_OK; }

/* IActivationFactory slot 6 = ActivateInstance — we forward to the statics via QI */
static HRESULT WINAPI dis_fact_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }

/* IDisplayInformationStatics (not IActivationFactory) — GetForCurrentView at slot 6 */
static void *s_dis_fact_vtbl[7] = {
    (void *)dis_fact_QI, (void *)su_AddRef, (void *)su_Release,
    (void *)su_GetIids, (void *)dis_GetRCN, (void *)su_GetTL,
    (void *)dis_GetForCurrentView,  /* 6 GetForCurrentView */
};

static DiStaticsObj g_di_factory = { s_dis_fact_vtbl };

/* =========================================================================
 * IApplicationView singleton + IApplicationViewStatics
 *
 * IApplicationView vtable:
 *   0-5  IInspectable
 *   6    get_Id (int*)
 *   7    add_Consolidated
 *   8    remove_Consolidated
 *   9    get_Title (HSTRING*)
 *   10   put_Title (HSTRING)
 *   11   get_IsFullScreen (BOOL*)
 *   12   get_IsOnLockScreen (BOOL*)
 *   13   get_IsScreenCaptureEnabled (BOOL*)
 *   14   put_IsScreenCaptureEnabled (BOOL)
 *   15   get_Bounds (Rect*)
 *   16   add_Bounds (event)
 *   17   remove_Bounds
 *   18   SetDesiredBoundsMode
 *   19   get_DesiredBoundsMode
 *   20   get_TitleBar (IApplicationViewTitleBar**)
 *   21   get_SuppressSystemOverlays (BOOL*)
 *   22   put_SuppressSystemOverlays
 *   23   get_AdjacentToLeftDisplayEdge (BOOL*)
 *   24   get_AdjacentToRightDisplayEdge (BOOL*)
 *   25   get_IsFullScreenMode (BOOL*)
 *   26   get_IsTabStop (BOOL*)
 *   27   put_IsTabStop (BOOL)
 *   28   get_WindowingMode
 *   29   put_WindowingMode
 *
 * IApplicationViewStatics (extends IActivationFactory):
 *   6    ActivateInstance
 *   7    get_Value (deprecated, NOTIMPL)
 *   8    TryUnsnapOrMaximize (deprecated)
 *   9    GetForCurrentView (IApplicationView**)
 * ========================================================================= */

typedef struct { void **lpVtbl; } AnvilAppView;

static HRESULT WINAPI av_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.UI.ViewManagement.ApplicationView", 41, s); }

static HRESULT WINAPI av_get_Id(void *T, INT32 *out)
    { (void)T; *out = 1; return S_OK; }

static HRESULT WINAPI av_get_Title(void *T, HSTRING *out)
    { (void)T; return WindowsCreateString(L"Anvil WinRT PoC", 15, out); }

static HRESULT WINAPI av_put_Title(void *T, HSTRING title)
{
    (void)T;
    if (!title) return S_OK;
    UINT32 len = 0;
    LPCWSTR buf = WindowsGetStringRawBuffer(title, &len);
    HWND hwnd = anvil_get_corewindow_hwnd();
    if (hwnd && buf && len > 0)
        SetWindowTextW(hwnd, buf);
    return S_OK;
}

static HRESULT WINAPI av_get_IsFullScreen(void *T, BOOL *out)
    { (void)T; *out = FALSE; return S_OK; }
static HRESULT WINAPI av_get_IsOnLockScreen(void *T, BOOL *out)
    { (void)T; *out = FALSE; return S_OK; }
static HRESULT WINAPI av_get_IsScreenCaptureEnabled(void *T, BOOL *out)
    { (void)T; *out = FALSE; return S_OK; }
static HRESULT WINAPI av_get_IsFullScreenMode(void *T, BOOL *out)
    { (void)T; *out = FALSE; return S_OK; }

static void *s_av_vtbl[30] = {
    (void *)su_QI, (void *)su_AddRef, (void *)su_Release,
    (void *)su_GetIids, (void *)av_GetRCN, (void *)su_GetTL,
    (void *)av_get_Id,                    /* 6  */
    NOTIMPL,                              /* 7  add_Consolidated    */
    NOTIMPL,                              /* 8  remove_Consolidated */
    (void *)av_get_Title,                 /* 9  */
    (void *)av_put_Title,                 /* 10 */
    (void *)av_get_IsFullScreen,          /* 11 */
    (void *)av_get_IsOnLockScreen,        /* 12 */
    (void *)av_get_IsScreenCaptureEnabled,/* 13 */
    NOTIMPL,                              /* 14 put_IsScreenCaptureEnabled */
    NOTIMPL,                              /* 15 get_Bounds          */
    NOTIMPL,                              /* 16 add_Bounds          */
    NOTIMPL,                              /* 17 remove_Bounds       */
    NOTIMPL,                              /* 18 SetDesiredBoundsMode */
    NOTIMPL,                              /* 19 get_DesiredBoundsMode */
    NOTIMPL,                              /* 20 get_TitleBar        */
    NOTIMPL,                              /* 21 get_SuppressSystemOverlays */
    NOTIMPL,                              /* 22 put_SuppressSystemOverlays */
    NOTIMPL,                              /* 23 get_AdjacentToLeftDisplayEdge */
    NOTIMPL,                              /* 24 get_AdjacentToRightDisplayEdge */
    (void *)av_get_IsFullScreenMode,      /* 25 */
    NOTIMPL,                              /* 26 get_IsTabStop       */
    NOTIMPL,                              /* 27 put_IsTabStop       */
    NOTIMPL,                              /* 28 get_WindowingMode   */
    NOTIMPL,                              /* 29 put_WindowingMode   */
};

static AnvilAppView g_app_view = { s_av_vtbl };

/* IApplicationViewStatics factory */
typedef struct { void **lpVtbl; } AvStaticsObj;

static HRESULT WINAPI avs_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.UI.ViewManagement.ApplicationView", 41, s); }

static HRESULT WINAPI avs_GetForCurrentView(void *T, void **out)
    { (void)T; *out = &g_app_view; return S_OK; }

static void *s_avs_fact_vtbl[10] = {
    (void *)su_QI, (void *)su_AddRef, (void *)su_Release,
    (void *)su_GetIids, (void *)avs_GetRCN, (void *)su_GetTL,
    NOTIMPL,                          /* 6 ActivateInstance  */
    NOTIMPL,                          /* 7 get_Value         */
    NOTIMPL,                          /* 8 TryUnsnapOrMaximize */
    (void *)avs_GetForCurrentView,    /* 9 GetForCurrentView */
};

static AvStaticsObj g_av_factory = { s_avs_fact_vtbl };

/* =========================================================================
 * IPropertyValue — a boxed primitive value
 *
 * vtable:
 *   0-5  IInspectable
 *   6    get_Type (PropertyType* / UINT32*)
 *   7    get_IsNumericScalar (BOOL*)
 *   8    GetUInt8  (BYTE*)
 *   9    GetInt16  (INT16*)
 *   10   GetUInt16 (UINT16*)
 *   11   GetInt32  (INT32*)
 *   12   GetUInt32 (UINT32*)
 *   13   GetInt64  (INT64*)
 *   14   GetUInt64 (UINT64*)
 *   15   GetSingle (float*)
 *   16   GetDouble (double*)
 *   17   GetChar16 (WCHAR*)
 *   18   GetBoolean (BOOL*)
 *   19   GetString (HSTRING*)
 *   20   GetInspectable (IInspectable**)
 *
 * PropertyType: Int32=4, Single=8, Double=9, Boolean=11, String=12
 * ========================================================================= */

#define PROP_TYPE_INT32   4
#define PROP_TYPE_SINGLE  8
#define PROP_TYPE_DOUBLE  9
#define PROP_TYPE_BOOL   11
#define PROP_TYPE_STRING 12

typedef struct {
    void  **lpVtbl;
    UINT32  type;
    union { INT32 i32; float f32; double f64; BOOL b; HSTRING str; } v;
} AnvilPropVal;

static HRESULT WINAPI pv_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.Foundation.IPropertyValue", 33, s); }
static HRESULT WINAPI pv_get_Type(void *T, UINT32 *out)
    { AnvilPropVal *p = (AnvilPropVal *)T; *out = p->type; return S_OK; }
static HRESULT WINAPI pv_get_IsNumericScalar(void *T, BOOL *out)
{
    AnvilPropVal *p = (AnvilPropVal *)T;
    *out = (p->type == PROP_TYPE_INT32 || p->type == PROP_TYPE_SINGLE ||
            p->type == PROP_TYPE_DOUBLE);
    return S_OK;
}
static HRESULT WINAPI pv_GetInt32 (void *T, INT32  *out)
    { AnvilPropVal *p = (AnvilPropVal *)T; *out = p->v.i32; return S_OK; }
static HRESULT WINAPI pv_GetSingle(void *T, float  *out)
    { AnvilPropVal *p = (AnvilPropVal *)T; *out = p->v.f32; return S_OK; }
static HRESULT WINAPI pv_GetDouble(void *T, double *out)
    { AnvilPropVal *p = (AnvilPropVal *)T; *out = p->v.f64; return S_OK; }
static HRESULT WINAPI pv_GetBoolean(void *T, BOOL *out)
    { AnvilPropVal *p = (AnvilPropVal *)T; *out = p->v.b; return S_OK; }
static HRESULT WINAPI pv_GetString(void *T, HSTRING *out)
    { AnvilPropVal *p = (AnvilPropVal *)T; *out = p->v.str; return S_OK; }

static void *s_pv_vtbl[21] = {
    (void *)su_QI, (void *)su_AddRef, (void *)su_Release,
    (void *)su_GetIids, (void *)pv_GetRCN, (void *)su_GetTL,
    (void *)pv_get_Type,            /* 6  */
    (void *)pv_get_IsNumericScalar, /* 7  */
    NOTIMPL,                        /* 8  GetUInt8  */
    NOTIMPL,                        /* 9  GetInt16  */
    NOTIMPL,                        /* 10 GetUInt16 */
    (void *)pv_GetInt32,            /* 11 */
    NOTIMPL,                        /* 12 GetUInt32 */
    NOTIMPL,                        /* 13 GetInt64  */
    NOTIMPL,                        /* 14 GetUInt64 */
    (void *)pv_GetSingle,           /* 15 */
    (void *)pv_GetDouble,           /* 16 */
    NOTIMPL,                        /* 17 GetChar16 */
    (void *)pv_GetBoolean,          /* 18 */
    (void *)pv_GetString,           /* 19 */
    NOTIMPL,                        /* 20 GetInspectable */
};

static AnvilPropVal *pv_alloc(UINT32 type)
{
    AnvilPropVal *p = (AnvilPropVal *)malloc(sizeof *p);
    if (!p) return NULL;
    memset(p, 0, sizeof *p);
    p->lpVtbl = s_pv_vtbl;
    p->type   = type;
    return p;
}

/* IPropertyValueStatics factory
 *
 * vtable:
 *   0-5  IInspectable (factory)
 *   6    ActivateInstance
 *   7    CreateEmpty
 *   8    CreateUInt8
 *   9    CreateInt16
 *   10   CreateUInt16
 *   11   CreateInt32
 *   12   CreateUInt32
 *   13   CreateInt64
 *   14   CreateUInt64
 *   15   CreateSingle
 *   16   CreateDouble
 *   17   CreateChar16
 *   18   CreateBoolean
 *   19   CreateString
 *   20   CreateInspectable
 */
typedef struct { void **lpVtbl; } PvStaticsObj;

static HRESULT WINAPI pvs_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.Foundation.PropertyValue", 32, s); }

static HRESULT WINAPI pvs_CreateInt32(void *T, INT32 val, void **out)
{
    (void)T;
    AnvilPropVal *p = pv_alloc(PROP_TYPE_INT32);
    if (!p) { *out = NULL; return E_OUTOFMEMORY; }
    p->v.i32 = val; *out = p; return S_OK;
}
static HRESULT WINAPI pvs_CreateSingle(void *T, float val, void **out)
{
    (void)T;
    AnvilPropVal *p = pv_alloc(PROP_TYPE_SINGLE);
    if (!p) { *out = NULL; return E_OUTOFMEMORY; }
    p->v.f32 = val; *out = p; return S_OK;
}
static HRESULT WINAPI pvs_CreateDouble(void *T, double val, void **out)
{
    (void)T;
    AnvilPropVal *p = pv_alloc(PROP_TYPE_DOUBLE);
    if (!p) { *out = NULL; return E_OUTOFMEMORY; }
    p->v.f64 = val; *out = p; return S_OK;
}
static HRESULT WINAPI pvs_CreateBoolean(void *T, BOOL val, void **out)
{
    (void)T;
    AnvilPropVal *p = pv_alloc(PROP_TYPE_BOOL);
    if (!p) { *out = NULL; return E_OUTOFMEMORY; }
    p->v.b = val; *out = p; return S_OK;
}
static HRESULT WINAPI pvs_CreateString(void *T, HSTRING val, void **out)
{
    (void)T;
    AnvilPropVal *p = pv_alloc(PROP_TYPE_STRING);
    if (!p) { *out = NULL; return E_OUTOFMEMORY; }
    p->v.str = val; *out = p; return S_OK;
}

static void *s_pvs_fact_vtbl[21] = {
    (void *)su_QI, (void *)su_AddRef, (void *)su_Release,
    (void *)su_GetIids, (void *)pvs_GetRCN, (void *)su_GetTL,
    NOTIMPL,                     /* 6  ActivateInstance */
    NOTIMPL,                     /* 7  CreateEmpty      */
    NOTIMPL,                     /* 8  CreateUInt8      */
    NOTIMPL,                     /* 9  CreateInt16      */
    NOTIMPL,                     /* 10 CreateUInt16     */
    (void *)pvs_CreateInt32,     /* 11                  */
    NOTIMPL,                     /* 12 CreateUInt32     */
    NOTIMPL,                     /* 13 CreateInt64      */
    NOTIMPL,                     /* 14 CreateUInt64     */
    (void *)pvs_CreateSingle,    /* 15                  */
    (void *)pvs_CreateDouble,    /* 16                  */
    NOTIMPL,                     /* 17 CreateChar16     */
    (void *)pvs_CreateBoolean,   /* 18                  */
    (void *)pvs_CreateString,    /* 19                  */
    NOTIMPL,                     /* 20 CreateInspectable */
};

static PvStaticsObj g_pv_factory = { s_pvs_fact_vtbl };

/* =========================================================================
 * IThreadPoolStatics — runs work items synchronously
 *
 * vtable:
 *   0-5  IInspectable
 *   6    ActivateInstance
 *   7    RunAsync(IWorkItemHandler*, IAsyncAction**)
 *   8    RunAsyncWithPriority(IWorkItemHandler*, WorkItemPriority, IAsyncAction**)
 *   9    RunAsyncWithPriorityAndOptions(IWorkItemHandler*, Priority, Options, IAsyncAction**)
 * ========================================================================= */

typedef struct { void **lpVtbl; } TpStaticsObj;

static HRESULT WINAPI tp_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.System.Threading.ThreadPool", 35, s); }

static HRESULT WINAPI tp_RunAsync(void *T, void *handler, void **ppAction)
{
    (void)T;
    if (handler) {
        /* IWorkItemHandler::Invoke(IAsyncAction*) at vtable slot 3 */
        typedef HRESULT (WINAPI *PFN_Invoke)(void *, void *);
        PFN_Invoke invoke = (PFN_Invoke)(*(void ***)handler)[3];
        invoke(handler, anvil_completed_action());
    }
    if (ppAction) *ppAction = anvil_completed_action();
    return S_OK;
}

/* Priority variants have extra args in different registers — need separate stubs */
static HRESULT WINAPI tp_RunAsyncPri(void *T, void *handler,
                                     INT32 priority, void **ppAction)
    { (void)priority; return tp_RunAsync(T, handler, ppAction); }

static HRESULT WINAPI tp_RunAsyncPriOpt(void *T, void *handler,
                                         INT32 priority, INT32 options, void **ppAction)
    { (void)priority; (void)options; return tp_RunAsync(T, handler, ppAction); }

static void *s_tp_fact_vtbl[10] = {
    (void *)su_QI, (void *)su_AddRef, (void *)su_Release,
    (void *)su_GetIids, (void *)tp_GetRCN, (void *)su_GetTL,
    NOTIMPL,                        /* 6 ActivateInstance                    */
    (void *)tp_RunAsync,            /* 7 RunAsync(handler, action**)         */
    (void *)tp_RunAsyncPri,         /* 8 RunAsync(handler, priority, action**) */
    (void *)tp_RunAsyncPriOpt,      /* 9 RunAsync(handler, priority, options, action**) */
};

static TpStaticsObj g_tp_factory = { s_tp_fact_vtbl };

/* =========================================================================
 * IGamepadStatics — returns empty gamepad list
 *
 * vtable:
 *   0-5  IInspectable
 *   6    ActivateInstance
 *   7    add_GamepadAdded
 *   8    remove_GamepadAdded
 *   9    add_GamepadRemoved
 *   10   remove_GamepadRemoved
 *   11   get_Gamepads (IVectorView<IGamepad>**)
 * ========================================================================= */

typedef struct { void **lpVtbl; } GpStaticsObj;

static HRESULT WINAPI gp_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.Gaming.Input.Gamepad", 28, s); }

static HRESULT WINAPI gp_get_Gamepads(void *T, void **out)
    { (void)T; *out = &g_empty_vv; return S_OK; }

static void *s_gp_fact_vtbl[12] = {
    (void *)su_QI, (void *)su_AddRef, (void *)su_Release,
    (void *)su_GetIids, (void *)gp_GetRCN, (void *)su_GetTL,
    NOTIMPL,                     /* 6  ActivateInstance     */
    NOTIMPL,                     /* 7  add_GamepadAdded     */
    NOTIMPL,                     /* 8  remove_GamepadAdded  */
    NOTIMPL,                     /* 9  add_GamepadRemoved   */
    NOTIMPL,                     /* 10 remove_GamepadRemoved */
    (void *)gp_get_Gamepads,     /* 11 get_Gamepads         */
};

static GpStaticsObj g_gp_factory = { s_gp_fact_vtbl };

/* =========================================================================
 * Registration
 * ========================================================================= */

void anvil_register_sysui_factories(void)
{
    anvil_register_factory(L"Windows.Graphics.Display.DisplayInformation",
                           &g_di_factory);
    anvil_register_factory(L"Windows.UI.ViewManagement.ApplicationView",
                           &g_av_factory);
    anvil_register_factory(L"Windows.Foundation.PropertyValue",
                           &g_pv_factory);
    anvil_register_factory(L"Windows.System.Threading.ThreadPool",
                           &g_tp_factory);
    anvil_register_factory(L"Windows.Gaming.Input.Gamepad",
                           &g_gp_factory);
    SLOG("anvil_register_sysui_factories: 5 stubs registered");
}

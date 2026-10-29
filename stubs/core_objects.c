/*
 * core_objects.c — Anvil real WinRT COM objects (Milestone 3 + 7a/7b)
 *
 * AnvilCoreWindow  — ICoreWindow backed by a Win32 HWND (55-slot vtable)
 * AnvilCoreAppView — ICoreApplicationView wrapping CoreWindow
 * AnvilCoreApp     — two-vtable COM (IActivationFactory + ICoreApplicationStatics)
 *
 * ICoreWindow vtable (Windows.UI.Core.ICoreWindow, Windows 8.1 ABI):
 *   0-5   IInspectable
 *   6     get_AutomationHostProvider
 *   7     get_Bounds (Rect*)
 *   8     get_CustomProperties
 *   9     get_Dispatcher (ICoreDispatcher**)
 *   10    get_FlowDirection
 *   11    put_FlowDirection
 *   12    get_IsInputEnabled (BOOL*)
 *   13    put_IsInputEnabled
 *   14    get_PointerCursor
 *   15    put_PointerCursor
 *   16    get_Title (HSTRING*)
 *   17    put_Title
 *   18    get_Visible (BOOL*)
 *   19    Activate
 *   20    Close
 *   21-22 add/remove_AutomationProviderRequested
 *   23-24 add/remove_CharacterReceived
 *   25-26 add/remove_Closed
 *   27-28 add/remove_InputEnabled
 *   29-30 add/remove_InputMethodResult
 *   31-32 add/remove_KeyDown
 *   33-34 add/remove_KeyUp
 *   35-36 add/remove_PointerCaptureLost
 *   37-38 add/remove_PointerEntered
 *   39-40 add/remove_PointerExited
 *   41-42 add/remove_PointerMoved
 *   43-44 add/remove_PointerPressed
 *   45-46 add/remove_PointerReleased
 *   47-48 add/remove_PointerWheelChanged
 *   49-50 add/remove_SizeChanged
 *   51-52 add/remove_TouchHitTesting
 *   53-54 add/remove_VisibilityChanged
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_core);
# define CLOG(fmt, ...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
# endif
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# define CLOG(fmt, ...) fprintf(stderr, "[anvil/core] " fmt "\n", ##__VA_ARGS__)
#endif

extern HRESULT WINAPI WindowsCreateString(LPCWSTR src, UINT32 len, HSTRING *out);
extern void    WINAPI WindowsDeleteString(HSTRING str);
extern int     anvil_register_factory(const WCHAR *class_id, void *factory);
extern void   *anvil_get_dispatcher(void);   /* dispatcher_stubs.c */
extern void    anvil_renderer_paint(HWND hwnd);  /* xaml_renderer.c */
extern void    anvil_renderer_click(HWND hwnd, int x, int y);

#include <stddef.h>

#ifndef CONTAINING_RECORD
# define CONTAINING_RECORD(addr, type, field) \
    ((type *)((char *)(addr) - offsetof(type, field)))
#endif

static HRESULT WINAPI slot_notimpl(void *T, ...)
    { (void)T; return E_NOTIMPL; }
#define NOTIMPL ((void *)slot_notimpl)

/* =========================================================================
 * IFrameworkViewSource / IFrameworkView — caller-provided interfaces
 * ========================================================================= */

typedef struct IFVSVtbl {
    void *QI, *AddRef, *Release, *GetIids, *GetRCN, *GetTL;
    HRESULT (WINAPI *CreateView)(void *T, void **view);
} IFVSVtbl;
typedef struct { const IFVSVtbl *lpVtbl; } IFVS;

typedef struct IFVVtbl {
    void *QI, *AddRef, *Release, *GetIids, *GetRCN, *GetTL;
    HRESULT (WINAPI *Initialize)(void *T, void *appView);
    HRESULT (WINAPI *SetWindow )(void *T, void *coreWindow);
    HRESULT (WINAPI *Load      )(void *T, HSTRING entryPoint);
    HRESULT (WINAPI *Run       )(void *T);
    HRESULT (WINAPI *Uninit    )(void *T);
} IFVVtbl;
typedef struct { const IFVVtbl *lpVtbl; } IFV;

/* =========================================================================
 * WinRT geometric types
 * ========================================================================= */

typedef struct { float X; float Y; }                   AnvilPoint;
typedef struct { float X; float Y; float Width; float Height; } AnvilRect;

/* =========================================================================
 * AnvilKeyEventArgs — IKeyEventArgs
 *
 * vtable:
 *   0-5  IInspectable
 *   6    get_VirtualKey (VirtualKey* / UINT32*)
 *   7    get_KeyStatus  (CorePhysicalKeyStatus* / UINT32*)
 * ========================================================================= */

typedef struct {
    void  **lpVtbl;
    UINT32  vkey;
    UINT32  key_status;   /* CorePhysicalKeyStatus packed as UINT32 */
} AnvilKeyEventArgs;

static HRESULT WINAPI kea_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static ULONG   WINAPI kea_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI kea_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI kea_GetIids(void *T, ULONG *n, IID **ids)
    { (void)T; *n = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI kea_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(L"Windows.UI.Core.KeyEventArgs", 28, s); }
static HRESULT WINAPI kea_GetTL(void *T, TrustLevel *tl)
    { (void)T; *tl = BaseTrust; return S_OK; }
static HRESULT WINAPI kea_get_VirtualKey(void *T, UINT32 *out)
    { AnvilKeyEventArgs *a = (AnvilKeyEventArgs *)T; *out = a->vkey; return S_OK; }
static HRESULT WINAPI kea_get_KeyStatus(void *T, UINT32 *out)
    { AnvilKeyEventArgs *a = (AnvilKeyEventArgs *)T; *out = a->key_status; return S_OK; }

static void *s_keyargs_vtbl[8] = {
    (void *)kea_QI, (void *)kea_AddRef, (void *)kea_Release,
    (void *)kea_GetIids, (void *)kea_GetRCN, (void *)kea_GetTL,
    (void *)kea_get_VirtualKey,   /* 6 */
    (void *)kea_get_KeyStatus,    /* 7 */
};

/* =========================================================================
 * AnvilPointerPoint — IPointerPoint (simplified)
 *
 * vtable:
 *   0-5  IInspectable
 *   6    get_PointerDevice
 *   7    get_Position (Point*)
 *   8    get_RawPosition (Point*)
 *   9    get_PointerId (UINT32*)
 *   10   get_FrameId
 *   11   get_Timestamp (UINT64*)
 *   12   get_IsInContact (BOOL*)
 *   13   get_Properties
 * ========================================================================= */

typedef struct {
    void      **lpVtbl;
    AnvilPoint  pos;
    UINT32      id;
    BOOL        in_contact;
} AnvilPointerPoint;

static HRESULT WINAPI pp_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static ULONG   WINAPI pp_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI pp_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI pp_GetIids(void *T, ULONG *n, IID **ids)
    { (void)T; *n = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI pp_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(L"Windows.UI.Input.PointerPoint", 29, s); }
static HRESULT WINAPI pp_GetTL(void *T, TrustLevel *tl)
    { (void)T; *tl = BaseTrust; return S_OK; }
static HRESULT WINAPI pp_get_Position(void *T, AnvilPoint *out)
    { AnvilPointerPoint *p = (AnvilPointerPoint *)T; *out = p->pos; return S_OK; }
static HRESULT WINAPI pp_get_PointerId(void *T, UINT32 *out)
    { AnvilPointerPoint *p = (AnvilPointerPoint *)T; *out = p->id; return S_OK; }
static HRESULT WINAPI pp_get_IsInContact(void *T, BOOL *out)
    { AnvilPointerPoint *p = (AnvilPointerPoint *)T; *out = p->in_contact; return S_OK; }

static void *s_ppoint_vtbl[14] = {
    (void *)pp_QI, (void *)pp_AddRef, (void *)pp_Release,
    (void *)pp_GetIids, (void *)pp_GetRCN, (void *)pp_GetTL,
    NOTIMPL,                       /* 6  get_PointerDevice   */
    (void *)pp_get_Position,       /* 7  get_Position        */
    (void *)pp_get_Position,       /* 8  get_RawPosition     */
    (void *)pp_get_PointerId,      /* 9  get_PointerId       */
    NOTIMPL,                       /* 10 get_FrameId         */
    NOTIMPL,                       /* 11 get_Timestamp       */
    (void *)pp_get_IsInContact,    /* 12 get_IsInContact     */
    NOTIMPL,                       /* 13 get_Properties      */
};

/* =========================================================================
 * AnvilPointerEventArgs — IPointerEventArgs
 *
 * vtable:
 *   0-5  IInspectable
 *   6    get_CurrentPoint (IPointerPoint**)
 *   7    get_KeyModifiers (VirtualKeyModifiers*)
 *   8    GetIntermediatePoints
 * ========================================================================= */

typedef struct {
    void            **lpVtbl;
    AnvilPointerPoint point;
} AnvilPointerEventArgs;

static HRESULT WINAPI pea_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static ULONG   WINAPI pea_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI pea_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI pea_GetIids(void *T, ULONG *n, IID **ids)
    { (void)T; *n = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI pea_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(L"Windows.UI.Core.PointerEventArgs", 32, s); }
static HRESULT WINAPI pea_GetTL(void *T, TrustLevel *tl)
    { (void)T; *tl = BaseTrust; return S_OK; }
static HRESULT WINAPI pea_get_CurrentPoint(void *T, void **out)
    { AnvilPointerEventArgs *a = (AnvilPointerEventArgs *)T; *out = &a->point; return S_OK; }
static HRESULT WINAPI pea_get_KeyModifiers(void *T, UINT32 *out)
    { (void)T; *out = 0; return S_OK; }

static void *s_pea_vtbl[9] = {
    (void *)pea_QI, (void *)pea_AddRef, (void *)pea_Release,
    (void *)pea_GetIids, (void *)pea_GetRCN, (void *)pea_GetTL,
    (void *)pea_get_CurrentPoint,  /* 6 */
    (void *)pea_get_KeyModifiers,  /* 7 */
    NOTIMPL,                       /* 8 GetIntermediatePoints */
};

/* =========================================================================
 * AnvilCoreWindow — ICoreWindow (55-slot vtable, flat void* array)
 * ========================================================================= */

typedef struct AnvilCoreWindow {
    void **lpVtbl;
#ifdef _WIN32
    HWND hwnd;
#endif
    BOOL  visible;
    /* Input event handlers (TypedEventHandler*) */
    void *handler_key_down;
    void *handler_key_up;
    void *handler_pointer_moved;
    void *handler_pointer_pressed;
    void *handler_pointer_released;
} AnvilCoreWindow;

static void *s_corewin_vtbl[55];   /* filled by corewin_vtbl_once() */

/* Global HWND accessor for DXGI hookup */
AnvilCoreWindow *g_active_corewindow = NULL;

HWND anvil_get_corewindow_hwnd(void)
{
#ifdef _WIN32
    return g_active_corewindow ? g_active_corewindow->hwnd : NULL;
#else
    return NULL;
#endif
}

/* --- Event firing helpers --- */

static void fire_key_event(AnvilCoreWindow *cw, void *handler,
                           UINT32 vkey, UINT32 key_status)
{
    if (!handler) return;
    AnvilKeyEventArgs args;
    args.lpVtbl     = s_keyargs_vtbl;
    args.vkey       = vkey;
    args.key_status = key_status;
    typedef HRESULT (WINAPI *PFN_Invoke)(void *, void *, void *);
    PFN_Invoke invoke = (PFN_Invoke)(*(void ***)handler)[3];
    invoke(handler, cw, &args);
}

static void fire_pointer_event(AnvilCoreWindow *cw, void *handler,
                               float x, float y, BOOL in_contact)
{
    if (!handler) return;
    AnvilPointerEventArgs args;
    memset(&args, 0, sizeof args);
    args.lpVtbl              = s_pea_vtbl;
    args.point.lpVtbl        = s_ppoint_vtbl;
    args.point.pos.X         = x;
    args.point.pos.Y         = y;
    args.point.id            = 1;
    args.point.in_contact    = in_contact;
    typedef HRESULT (WINAPI *PFN_Invoke)(void *, void *, void *);
    PFN_Invoke invoke = (PFN_Invoke)(*(void ***)handler)[3];
    invoke(handler, cw, &args);
}

/* --- Win32 window proc --- */
#ifdef _WIN32
static const WCHAR s_wclass[] = { 'A','n','v','i','l','W','n','d',0 };

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    AnvilCoreWindow *cw =
        (AnvilCoreWindow *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_ERASEBKGND:
        return 1;   /* renderer handles background */

    case WM_PAINT:
        anvil_renderer_paint(hwnd);
        return 0;

    case WM_KEYDOWN:
        if (cw) fire_key_event(cw, cw->handler_key_down,
                               (UINT32)wp, (UINT32)lp & 0xFFFF);
        break;

    case WM_KEYUP:
        if (cw) fire_key_event(cw, cw->handler_key_up,
                               (UINT32)wp, (UINT32)lp | 0xC0000000u);
        break;

    case WM_MOUSEMOVE:
        if (cw) fire_pointer_event(cw, cw->handler_pointer_moved,
                                   (float)(short)LOWORD(lp),
                                   (float)(short)HIWORD(lp), FALSE);
        break;

    case WM_LBUTTONDOWN:
        anvil_renderer_click(hwnd, (short)LOWORD(lp), (short)HIWORD(lp));
        if (cw) fire_pointer_event(cw, cw->handler_pointer_pressed,
                                   (float)(short)LOWORD(lp),
                                   (float)(short)HIWORD(lp), TRUE);
        break;

    case WM_LBUTTONUP:
        if (cw) fire_pointer_event(cw, cw->handler_pointer_released,
                                   (float)(short)LOWORD(lp),
                                   (float)(short)HIWORD(lp), FALSE);
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HWND create_hwnd(void)
{
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSW wc = {0};
        wc.lpfnWndProc   = wnd_proc;
        wc.hInstance     = GetModuleHandleW(NULL);
        wc.lpszClassName = s_wclass;
        wc.hCursor       = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassW(&wc);
        registered = TRUE;
    }
    return CreateWindowExW(0, s_wclass, L"Calculator",
                           WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, 396, 597,
                           NULL, NULL, GetModuleHandleW(NULL), NULL);
}
#endif /* _WIN32 */

/* --- ICoreWindow vtable methods --- */

static HRESULT WINAPI cw_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static ULONG   WINAPI cw_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI cw_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI cw_GetIids(void *T, ULONG *n, IID **i)
    { (void)T; *n = 0; *i = NULL; return S_OK; }
static HRESULT WINAPI cw_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(L"Windows.UI.Core.CoreWindow", 26, n); }
static HRESULT WINAPI cw_GetTL(void *T, TrustLevel *tl)
    { (void)T; *tl = BaseTrust; return S_OK; }

static HRESULT WINAPI cw_get_Bounds(void *T, AnvilRect *out)
{
    (void)T;
    out->X = 0.0f; out->Y = 0.0f;
    out->Width = 800.0f; out->Height = 600.0f;
    return S_OK;
}

static HRESULT WINAPI cw_get_Dispatcher(void *T, void **disp)
    { (void)T; *disp = anvil_get_dispatcher(); return S_OK; }

static HRESULT WINAPI cw_get_IsInputEnabled(void *T, BOOL *out)
    { (void)T; *out = TRUE; return S_OK; }

static HRESULT WINAPI cw_get_Visible(void *T, BOOL *v)
    { AnvilCoreWindow *w = (AnvilCoreWindow *)T; *v = w->visible; return S_OK; }

static HRESULT WINAPI cw_Activate(void *T)
{
    AnvilCoreWindow *w = (AnvilCoreWindow *)T;
    CLOG("CoreWindow::Activate");
#ifdef _WIN32
    if (w->hwnd) { ShowWindow(w->hwnd, SW_SHOWNORMAL); UpdateWindow(w->hwnd); }
#endif
    w->visible = TRUE;
    return S_OK;
}

static HRESULT WINAPI cw_Close(void *T)
{
    AnvilCoreWindow *w = (AnvilCoreWindow *)T;
    CLOG("CoreWindow::Close");
#ifdef _WIN32
    if (w->hwnd) { DestroyWindow(w->hwnd); w->hwnd = NULL; }
#endif
    w->visible = FALSE;
    return S_OK;
}

/* --- Event add/remove --- */

#define DEF_EVENT_ADD(name, field) \
static HRESULT WINAPI cw_add_##name(void *T, void *handler, INT64 *token) { \
    AnvilCoreWindow *w = (AnvilCoreWindow *)T; \
    w->field = handler; \
    if (token) *token = (INT64)(UINT_PTR)handler; \
    return S_OK; \
}
#define DEF_EVENT_REMOVE(name, field) \
static HRESULT WINAPI cw_remove_##name(void *T, INT64 token) { \
    AnvilCoreWindow *w = (AnvilCoreWindow *)T; (void)token; \
    w->field = NULL; return S_OK; \
}

DEF_EVENT_ADD   (KeyDown,          handler_key_down)
DEF_EVENT_REMOVE(KeyDown,          handler_key_down)
DEF_EVENT_ADD   (KeyUp,            handler_key_up)
DEF_EVENT_REMOVE(KeyUp,            handler_key_up)
DEF_EVENT_ADD   (PointerMoved,     handler_pointer_moved)
DEF_EVENT_REMOVE(PointerMoved,     handler_pointer_moved)
DEF_EVENT_ADD   (PointerPressed,   handler_pointer_pressed)
DEF_EVENT_REMOVE(PointerPressed,   handler_pointer_pressed)
DEF_EVENT_ADD   (PointerReleased,  handler_pointer_released)
DEF_EVENT_REMOVE(PointerReleased,  handler_pointer_released)

/* --- Vtable init (called once on first CoreWindow creation) --- */

static void corewin_vtbl_once(void)
{
    static BOOL ready = FALSE;
    if (ready) return;
    ready = TRUE;

    for (int i = 0; i < 55; i++) s_corewin_vtbl[i] = NOTIMPL;

    s_corewin_vtbl[0]  = (void *)cw_QI;
    s_corewin_vtbl[1]  = (void *)cw_AddRef;
    s_corewin_vtbl[2]  = (void *)cw_Release;
    s_corewin_vtbl[3]  = (void *)cw_GetIids;
    s_corewin_vtbl[4]  = (void *)cw_GetRCN;
    s_corewin_vtbl[5]  = (void *)cw_GetTL;
    /* 6: get_AutomationHostProvider — NOTIMPL */
    s_corewin_vtbl[7]  = (void *)cw_get_Bounds;
    /* 8: get_CustomProperties — NOTIMPL */
    s_corewin_vtbl[9]  = (void *)cw_get_Dispatcher;
    /* 10: get_FlowDirection, 11: put_FlowDirection — NOTIMPL */
    s_corewin_vtbl[12] = (void *)cw_get_IsInputEnabled;
    /* 13: put_IsInputEnabled, 14-17: cursor/title — NOTIMPL */
    s_corewin_vtbl[18] = (void *)cw_get_Visible;
    s_corewin_vtbl[19] = (void *)cw_Activate;
    s_corewin_vtbl[20] = (void *)cw_Close;
    /* 21-30: event add/remove pairs — NOTIMPL */
    s_corewin_vtbl[31] = (void *)cw_add_KeyDown;
    s_corewin_vtbl[32] = (void *)cw_remove_KeyDown;
    s_corewin_vtbl[33] = (void *)cw_add_KeyUp;
    s_corewin_vtbl[34] = (void *)cw_remove_KeyUp;
    /* 35-40: PointerCaptureLost/Entered/Exited — NOTIMPL */
    s_corewin_vtbl[41] = (void *)cw_add_PointerMoved;
    s_corewin_vtbl[42] = (void *)cw_remove_PointerMoved;
    s_corewin_vtbl[43] = (void *)cw_add_PointerPressed;
    s_corewin_vtbl[44] = (void *)cw_remove_PointerPressed;
    s_corewin_vtbl[45] = (void *)cw_add_PointerReleased;
    s_corewin_vtbl[46] = (void *)cw_remove_PointerReleased;
    /* 47-54: WheelChanged/SizeChanged/TouchHitTesting/VisibilityChanged — NOTIMPL */
}

/* --- CoreWindow lifecycle --- */

static AnvilCoreWindow *corewin_new(void)
{
    corewin_vtbl_once();
    AnvilCoreWindow *w = (AnvilCoreWindow *)malloc(sizeof *w);
    if (!w) return NULL;
    memset(w, 0, sizeof *w);
    w->lpVtbl  = s_corewin_vtbl;
    w->visible = FALSE;
#ifdef _WIN32
    w->hwnd = create_hwnd();
    if (w->hwnd)
        SetWindowLongPtrW(w->hwnd, GWLP_USERDATA, (LONG_PTR)w);
    CLOG("CoreWindow: HWND=%p", (void *)w->hwnd);
#else
    CLOG("CoreWindow: no HWND (non-Win32 build)");
#endif
    g_active_corewindow = w;
    return w;
}

static void corewin_free(AnvilCoreWindow *w)
{
    if (!w) return;
    if (g_active_corewindow == w) g_active_corewindow = NULL;
    cw_Close(w);
    free(w);
}

/* =========================================================================
 * AnvilCoreAppView — ICoreApplicationView
 *
 *   0-5  IInspectable
 *   6    get_CoreWindow (ICoreWindow**)
 *   7    add_Activated
 *   8    remove_Activated
 *   9    get_IsMain (BOOL*)
 *   10   get_IsHosted (BOOL*)
 * ========================================================================= */

typedef struct CAVVtbl {
    HRESULT (WINAPI *QI)      (void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)  (void *);
    ULONG   (WINAPI *Release) (void *);
    HRESULT (WINAPI *GetIids) (void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRCN)  (void *, HSTRING *);
    HRESULT (WINAPI *GetTL)   (void *, TrustLevel *);
    HRESULT (WINAPI *get_CoreWindow)(void *, void **);
    void *add_Activated;
    void *remove_Activated;
    HRESULT (WINAPI *get_IsMain)  (void *, BOOL *);
    HRESULT (WINAPI *get_IsHosted)(void *, BOOL *);
} CAVVtbl;

typedef struct AnvilCoreAppView {
    const CAVVtbl   *lpVtbl;
    AnvilCoreWindow *window;
} AnvilCoreAppView;

static HRESULT WINAPI cav_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static ULONG   WINAPI cav_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI cav_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI cav_GetIids(void *T, ULONG *n, IID **i)
    { (void)T; *n = 0; *i = NULL; return S_OK; }
static HRESULT WINAPI cav_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(
        L"Windows.ApplicationModel.Core.CoreApplicationView", 50, n); }
static HRESULT WINAPI cav_GetTL(void *T, TrustLevel *tl)
    { (void)T; *tl = BaseTrust; return S_OK; }
static HRESULT WINAPI cav_get_CoreWindow(void *T, void **w)
    { AnvilCoreAppView *v = (AnvilCoreAppView *)T; *w = v->window; return S_OK; }
static HRESULT WINAPI cav_get_IsMain  (void *T, BOOL *b) { (void)T; *b = TRUE;  return S_OK; }
static HRESULT WINAPI cav_get_IsHosted(void *T, BOOL *b) { (void)T; *b = FALSE; return S_OK; }

static const CAVVtbl s_appview_vtbl = {
    cav_QI, cav_AddRef, cav_Release,
    cav_GetIids, cav_GetRCN, cav_GetTL,
    cav_get_CoreWindow,  /* 6  */
    NOTIMPL,             /* 7  add_Activated    */
    NOTIMPL,             /* 8  remove_Activated */
    cav_get_IsMain,      /* 9  */
    cav_get_IsHosted,    /* 10 */
};

static AnvilCoreAppView *appview_new(AnvilCoreWindow *win)
{
    AnvilCoreAppView *v = (AnvilCoreAppView *)malloc(sizeof *v);
    if (!v) return NULL;
    v->lpVtbl = &s_appview_vtbl;
    v->window = win;
    return v;
}

/* =========================================================================
 * AnvilCoreApp — two-vtable COM (IActivationFactory + ICoreApplicationStatics)
 * ========================================================================= */

typedef struct AnvilCoreApp AnvilCoreApp;

static HRESULT WINAPI ca_fact_QI(void *T, REFIID iid, void **out);
static ULONG   WINAPI ca_fact_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI ca_fact_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI ca_fact_GetIids(void *T, ULONG *n, IID **i)
    { (void)T; *n = 0; *i = NULL; return S_OK; }
static HRESULT WINAPI ca_fact_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(
        L"Windows.ApplicationModel.Core.CoreApplication", 46, n); }
static HRESULT WINAPI ca_fact_GetTL(void *T, TrustLevel *tl)
    { (void)T; *tl = BaseTrust; return S_OK; }
static HRESULT WINAPI ca_fact_ActivateInstance(void *T, IInspectable **inst)
    { (void)T; *inst = NULL; return E_NOTIMPL; }

typedef struct ICASVtbl {
    HRESULT (WINAPI *QI)      (void *, REFIID, void **);
    ULONG   (WINAPI *AddRef)  (void *);
    ULONG   (WINAPI *Release) (void *);
    HRESULT (WINAPI *GetIids) (void *, ULONG *, IID **);
    HRESULT (WINAPI *GetRCN)  (void *, HSTRING *);
    HRESULT (WINAPI *GetTL)   (void *, TrustLevel *);
    void   *get_Properties;
    HRESULT (WINAPI *Run)(void *, void *viewSource);
    void   *RunWithActivationFactories;
} ICASVtbl;

struct AnvilCoreApp {
    const IActivationFactoryVtbl *lpActFact;
    const ICASVtbl               *lpStatics;
};

static const IID IID_ICoreApplicationStatics = {
    0x638BB2DB, 0x451D, 0x4661,
    { 0xB0, 0x99, 0x41, 0x4F, 0x34, 0xFF, 0xB9, 0xF1 }
};

static HRESULT WINAPI ca_fact_QI(void *T, REFIID iid, void **out)
{
    AnvilCoreApp *obj = (AnvilCoreApp *)T;
    if (IsEqualIID(iid, &IID_IActivationFactory) ||
        IsEqualIID(iid, &IID_IUnknown)            ||
        IsEqualIID(iid, &IID_IInspectable)) {
        *out = T; return S_OK;
    }
    if (IsEqualIID(iid, &IID_ICoreApplicationStatics)) {
        *out = &obj->lpStatics; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}

static HRESULT WINAPI cas_QI(void *T, REFIID iid, void **out)
{
    AnvilCoreApp *obj = CONTAINING_RECORD(T, AnvilCoreApp, lpStatics);
    return ca_fact_QI((void *)obj, iid, out);
}
static ULONG   WINAPI cas_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI cas_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI cas_GetIids(void *T, ULONG *n, IID **i)
    { (void)T; *n = 0; *i = NULL; return S_OK; }
static HRESULT WINAPI cas_GetRCN(void *T, HSTRING *n)
    { (void)T; return WindowsCreateString(
        L"Windows.ApplicationModel.Core.CoreApplication", 46, n); }
static HRESULT WINAPI cas_GetTL(void *T, TrustLevel *tl)
    { (void)T; *tl = BaseTrust; return S_OK; }

static HRESULT WINAPI cas_Run(void *T, void *viewSourcePtr)
{
    (void)T;
    IFVS *viewSource = (IFVS *)viewSourcePtr;
    IFV  *view       = NULL;
    HRESULT hr;
    AnvilCoreWindow  *win     = NULL;
    AnvilCoreAppView *appView = NULL;
    HSTRING empty = NULL;

    CLOG("CoreApplication::Run() — entering UWP lifecycle");

    win     = corewin_new();
    appView = appview_new(win);
    if (!win || !appView) { hr = E_OUTOFMEMORY; goto done; }

    hr = viewSource->lpVtbl->CreateView(viewSource, (void **)&view);
    if (FAILED(hr) || !view) {
        CLOG("Run: CreateView failed hr=0x%08lx", (unsigned long)hr);
        goto done;
    }
    CLOG("Run: CreateView → view=%p", (void *)view);

    hr = view->lpVtbl->Initialize(view, appView);
    CLOG("Run: Initialize hr=0x%08lx", (unsigned long)hr);

    hr = view->lpVtbl->SetWindow(view, win);
    CLOG("Run: SetWindow hr=0x%08lx", (unsigned long)hr);

    WindowsCreateString(L"", 0, &empty);
    hr = view->lpVtbl->Load(view, empty);
    WindowsDeleteString(empty);
    CLOG("Run: Load hr=0x%08lx", (unsigned long)hr);

    CLOG("Run: entering view->Run()");
    hr = view->lpVtbl->Run(view);
    CLOG("Run: view->Run() returned hr=0x%08lx", (unsigned long)hr);

    view->lpVtbl->Uninit(view);
    CLOG("Run: Uninitialize done");

done:
    if (appView) free(appView);
    if (win)     corewin_free(win);
    CLOG("CoreApplication::Run() — exiting hr=0x%08lx", (unsigned long)hr);
    return hr;
}

static const IActivationFactoryVtbl s_ca_fact_vtbl = {
    ca_fact_QI, ca_fact_AddRef, ca_fact_Release,
    ca_fact_GetIids, ca_fact_GetRCN, ca_fact_GetTL,
    ca_fact_ActivateInstance,
};

static const ICASVtbl s_ca_statics_vtbl = {
    cas_QI, cas_AddRef, cas_Release,
    cas_GetIids, cas_GetRCN, cas_GetTL,
    NOTIMPL,    /* 6  get_Properties              */
    cas_Run,    /* 7  Run                          */
    NOTIMPL,    /* 8  RunWithActivationFactories   */
};

static AnvilCoreApp s_core_app = {
    &s_ca_fact_vtbl,
    &s_ca_statics_vtbl,
};

IActivationFactory *anvil_get_core_app_factory(void)
{
    return (IActivationFactory *)&s_core_app;
}

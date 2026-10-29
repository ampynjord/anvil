/*
 * xaml_controls_stubs.c — Anvil: Windows.UI.Xaml controls layer.
 *
 * Provides ActivationFactory and runtime objects for XAML controls.
 * All factories return heap-allocated AnvilElem objects whose vtables
 * are populated with NOTIMPL for slots we don't implement.
 * Specific slots (put_Text, put_Content, Children.Append, add_Click)
 * are wired to store data in the element struct, building a visual tree
 * that xaml_renderer.c can optionally walk.
 *
 * ABI slot numbers follow the Windows 8.1 / Windows 10 WinRT metadata.
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_xaml);
# define XLOG(fmt,...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# define XLOG(fmt,...) fprintf(stderr, "[anvil/xaml] " fmt "\n", ##__VA_ARGS__)
#endif

#include <stddef.h>

extern HRESULT WINAPI WindowsCreateString(LPCWSTR src, UINT32 len, HSTRING *out);
extern LPCWSTR WINAPI WindowsGetStringRawBuffer(HSTRING str, UINT32 *len);
extern int     anvil_register_factory(const WCHAR *class_id, void *factory);
extern void    anvil_renderer_set_display(const WCHAR *text); /* xaml_renderer.c */

/* =========================================================================
 * Shared helpers
 * ========================================================================= */

static HRESULT WINAPI xc_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static ULONG   WINAPI xc_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI xc_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI xc_GetIids(void *T, ULONG *n, IID **ids)
    { (void)T; *n = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI xc_GetTL(void *T, TrustLevel *tl)
    { (void)T; *tl = BaseTrust; return S_OK; }
static HRESULT WINAPI xc_notimpl(void *T, ...)
    { (void)T; return E_NOTIMPL; }
static HRESULT WINAPI xc_ok(void *T, ...)
    { (void)T; return S_OK; }
#define NOTIMPL ((void *)xc_notimpl)
#define STUB_OK ((void *)xc_ok)

/* =========================================================================
 * AnvilElem — unified heap object for all XAML elements
 * ========================================================================= */

#define ELEM_UNKNOWN     0
#define ELEM_GRID        1
#define ELEM_STACKPANEL  2
#define ELEM_BUTTON      3
#define ELEM_TEXTBLOCK   4
#define ELEM_TEXTBOX     5
#define ELEM_FRAME       6
#define ELEM_PAGE        7
#define ELEM_BORDER      8
#define ELEM_SCROLLVIEWER 9
#define ELEM_TOGGLEBTN   10
#define ELEM_GENERIC     11

#define MAX_ELEM_CHILDREN 128

typedef struct AnvilElem {
    void **lpVtbl;      /* primary vtable — see s_elem_vtbl below */
    int   type;
    WCHAR text[256];    /* TextBlock.Text, Button.Content text, etc. */
    struct AnvilElem *children[MAX_ELEM_CHILDREN];
    int   childCount;
    void *clickHandler; /* Button.add_Click handler */
    INT64 clickToken;
    int   gridRow, gridCol, gridRowSpan, gridColSpan;
    /* UIElementCollection vtable (second interface, returned by get_Children) */
    void **lpCollVtbl;
    struct AnvilElem *collOwner; /* back-ptr */
} AnvilElem;

/* ---- UIElementCollection vtable ---- */
/* IVector<UIElement>: slot 6=GetAt, 7=get_Size, 8=GetView, 9=IndexOf,
 *  10=SetAt, 11=InsertAt, 12=RemoveAt, 13=Append, 14=RemoveAtEnd, 15=Clear */

static HRESULT WINAPI coll_Append(void *T, void *elem)
{
    AnvilElem *coll_owner = ((AnvilElem *)(
        (char *)T - offsetof(AnvilElem, lpCollVtbl)));
    if (coll_owner->childCount < MAX_ELEM_CHILDREN && elem)
        coll_owner->children[coll_owner->childCount++] = (AnvilElem *)elem;
    return S_OK;
}
static HRESULT WINAPI coll_get_Size(void *T, UINT32 *out)
{
    AnvilElem *owner = (AnvilElem *)((char *)T - offsetof(AnvilElem, lpCollVtbl));
    *out = (UINT32)owner->childCount;
    return S_OK;
}
static HRESULT WINAPI coll_GetAt(void *T, UINT32 idx, void **out)
{
    AnvilElem *owner = (AnvilElem *)((char *)T - offsetof(AnvilElem, lpCollVtbl));
    if (idx < (UINT32)owner->childCount) { *out = owner->children[idx]; return S_OK; }
    *out = NULL; return E_BOUNDS;
}
static HRESULT WINAPI coll_Clear(void *T, ...)
{
    AnvilElem *owner = (AnvilElem *)((char *)T - offsetof(AnvilElem, lpCollVtbl));
    owner->childCount = 0;
    return S_OK;
}

static void *s_coll_vtbl[18] = {
    (void *)xc_QI, (void *)xc_AddRef, (void *)xc_Release,
    (void *)xc_GetIids, NOTIMPL, (void *)xc_GetTL,
    (void *)coll_GetAt,       /* 6  GetAt(UINT32, UIElement**)    */
    (void *)coll_get_Size,    /* 7  get_Size(UINT32*)             */
    NOTIMPL,                  /* 8  GetView                       */
    NOTIMPL,                  /* 9  IndexOf                       */
    NOTIMPL,                  /* 10 SetAt                         */
    NOTIMPL,                  /* 11 InsertAt                      */
    NOTIMPL,                  /* 12 RemoveAt                      */
    (void *)coll_Append,      /* 13 Append(UIElement*)            */
    NOTIMPL,                  /* 14 RemoveAtEnd                   */
    (void *)coll_Clear,       /* 15 Clear                         */
    NOTIMPL,                  /* 16 GetMany                       */
    NOTIMPL,                  /* 17 ReplaceAll                    */
};

/* ---- Primary element vtable (200 slots) ---- */
/* We fill in only the slots we know; everything else is NOTIMPL or S_OK. */

/* Slot 27: ITextBlock.put_Text(HSTRING)                                       */
static HRESULT WINAPI elem_put_Text(void *T, HSTRING hs)
{
    AnvilElem *e = (AnvilElem *)T;
    UINT32 len = 0;
    LPCWSTR buf = WindowsGetStringRawBuffer(hs, &len);
    if (buf && len < 256) { wcsncpy(e->text, buf, 255); e->text[255] = 0; }
    if (e->type == ELEM_TEXTBLOCK)
        anvil_renderer_set_display(e->text);
    return S_OK;
}
/* Slot 26: ITextBlock.get_Text(HSTRING*) */
static HRESULT WINAPI elem_get_Text(void *T, HSTRING *out)
{
    AnvilElem *e = (AnvilElem *)T;
    return WindowsCreateString(e->text, (UINT32)wcslen(e->text), out);
}

/* Slot 9 (IWindow): put_Content(IUIElement*)  — also used by ContentControl */
static HRESULT WINAPI elem_put_Content(void *T, void *elem)
{
    AnvilElem *e = (AnvilElem *)T;
    if (e->childCount < MAX_ELEM_CHILDREN && elem)
        e->children[e->childCount++] = (AnvilElem *)elem;
    XLOG("put_Content: parent=%p child=%p", T, elem);
    return S_OK;
}
/* Slot 8: get_Content */
static HRESULT WINAPI elem_get_Content(void *T, void **out)
{
    AnvilElem *e = (AnvilElem *)T;
    *out = (e->childCount > 0) ? e->children[0] : NULL;
    return S_OK;
}

/* Slot for get_Children (Grid/Panel) — returns &e->lpCollVtbl */
static HRESULT WINAPI elem_get_Children(void *T, void **out)
{
    AnvilElem *e = (AnvilElem *)T;
    *out = &e->lpCollVtbl;
    return S_OK;
}

/* Button add_Click handler (IButtonBase slot 10) */
static HRESULT WINAPI elem_add_Click(void *T, void *handler, INT64 *token)
{
    AnvilElem *e = (AnvilElem *)T;
    e->clickHandler = handler;
    if (token) { e->clickToken = (INT64)(UINT_PTR)handler; *token = e->clickToken; }
    return S_OK;
}
static HRESULT WINAPI elem_remove_Click(void *T, INT64 token)
    { (void)T; (void)token; return S_OK; }

/* IFrameworkElement: get_Tag / put_Tag (slots 28/29 approx) */
static HRESULT WINAPI elem_put_Tag(void *T, void *val)
    { (void)T; (void)val; return S_OK; }
static HRESULT WINAPI elem_get_Tag(void *T, void **out)
    { (void)T; *out = NULL; return S_OK; }

/* ---- Build the 200-slot vtable ---- */
#define VTBL_SZ 200
static void *s_elem_vtbl[VTBL_SZ];

static void elem_vtbl_init(void)
{
    static int done = 0;
    if (done) return;
    done = 1;
    for (int i = 0; i < VTBL_SZ; i++) s_elem_vtbl[i] = NOTIMPL;
    /* IInspectable */
    s_elem_vtbl[0] = (void *)xc_QI;
    s_elem_vtbl[1] = (void *)xc_AddRef;
    s_elem_vtbl[2] = (void *)xc_Release;
    s_elem_vtbl[3] = (void *)xc_GetIids;
    s_elem_vtbl[4] = NOTIMPL;  /* GetRCN — set per-type */
    s_elem_vtbl[5] = (void *)xc_GetTL;

    /* ITextBlock primary: get/put Text at 26/27 */
    s_elem_vtbl[26] = (void *)elem_get_Text;
    s_elem_vtbl[27] = (void *)elem_put_Text;

    /* IContentControl: get/put_Content at 8/9 */
    s_elem_vtbl[8]  = (void *)elem_get_Content;
    s_elem_vtbl[9]  = (void *)elem_put_Content;

    /* IPanel (Grid/StackPanel): get_Children at slot 7 */
    s_elem_vtbl[7]  = (void *)elem_get_Children;

    /* IButtonBase: add_Click=10, remove_Click=11 */
    s_elem_vtbl[10] = (void *)elem_add_Click;
    s_elem_vtbl[11] = (void *)elem_remove_Click;

    /* IFrameworkElement: get/put Tag around slot 28 */
    s_elem_vtbl[28] = (void *)elem_get_Tag;
    s_elem_vtbl[29] = (void *)elem_put_Tag;

    /* Mark many bool/int getters as S_OK to avoid crashes */
    for (int i = 6; i < 30; i++)
        if (s_elem_vtbl[i] == NOTIMPL) s_elem_vtbl[i] = STUB_OK;
}

static AnvilElem *elem_alloc(int type)
{
    elem_vtbl_init();
    AnvilElem *e = (AnvilElem *)calloc(1, sizeof(AnvilElem));
    if (!e) return NULL;
    e->lpVtbl    = s_elem_vtbl;
    e->lpCollVtbl = s_coll_vtbl;
    e->type      = type;
    e->gridRowSpan = 1; e->gridColSpan = 1;
    wcscpy(e->text, L"");
    return e;
}

/* =========================================================================
 * DefinitionCollection stubs (ColumnDefinitionCollection, RowDefinitionCollection)
 * Used by Grid.ColumnDefinitions and Grid.RowDefinitions.
 * We track count only — actual sizes are uniform for our renderer.
 * ========================================================================= */

typedef struct { void **lpVtbl; int count; } AnvilDefColl;

static HRESULT WINAPI defcoll_Append(void *T, void *def)
    { (void)def; ((AnvilDefColl *)T)->count++; return S_OK; }
static HRESULT WINAPI defcoll_get_Size(void *T, UINT32 *out)
    { *out = (UINT32)((AnvilDefColl *)T)->count; return S_OK; }

static void *s_defcoll_vtbl[16] = {
    (void *)xc_QI, (void *)xc_AddRef, (void *)xc_Release,
    (void *)xc_GetIids, NOTIMPL, (void *)xc_GetTL,
    NOTIMPL,                   /* 6  GetAt */
    (void *)defcoll_get_Size,  /* 7  get_Size */
    NOTIMPL,                   /* 8  GetView */
    NOTIMPL,                   /* 9  IndexOf */
    NOTIMPL, NOTIMPL, NOTIMPL, /* 10-12 */
    (void *)defcoll_Append,    /* 13 Append */
    NOTIMPL, NOTIMPL,
};

typedef struct {
    void **lpVtbl;
    AnvilDefColl colDefs;
    AnvilDefColl rowDefs;
    AnvilElem    elem;
} AnvilGrid;

static HRESULT WINAPI grid_get_ColDefs(void *T, void **out)
    { AnvilGrid *g = (AnvilGrid *)T; *out = &g->colDefs; return S_OK; }
static HRESULT WINAPI grid_get_RowDefs(void *T, void **out)
    { AnvilGrid *g = (AnvilGrid *)T; *out = &g->rowDefs; return S_OK; }

/* =========================================================================
 * Grid attached-property statics
 * RowProperty, ColumnProperty, RowSpanProperty, ColumnSpanProperty
 * The statics expose SetRow(elem, val) / SetColumn(elem, val) etc.
 * ========================================================================= */

static HRESULT WINAPI gs_SetRow(void *T, void *elem, INT32 row)
    { (void)T; if (elem) ((AnvilElem *)elem)->gridRow = row; return S_OK; }
static HRESULT WINAPI gs_GetRow(void *T, void *elem, INT32 *out)
    { (void)T; *out = elem ? ((AnvilElem *)elem)->gridRow : 0; return S_OK; }
static HRESULT WINAPI gs_SetCol(void *T, void *elem, INT32 col)
    { (void)T; if (elem) ((AnvilElem *)elem)->gridCol = col; return S_OK; }
static HRESULT WINAPI gs_GetCol(void *T, void *elem, INT32 *out)
    { (void)T; *out = elem ? ((AnvilElem *)elem)->gridCol : 0; return S_OK; }

/* IGridStatics vtable (extends IActivationFactory):
 *   6  ActivateInstance
 *   7  get_RowProperty (DP)
 *   8  GetRow(UIElement, INT32*)
 *   9  SetRow(UIElement, INT32)
 *   10 get_ColumnProperty (DP)
 *   11 GetColumn(UIElement, INT32*)
 *   12 SetColumn(UIElement, INT32)
 *   13 get_RowSpanProperty
 *   14 GetRowSpan
 *   15 SetRowSpan
 *   16 get_ColumnSpanProperty
 *   17 GetColumnSpan
 *   18 SetColumnSpan
 */
static void *s_grid_statics_vtbl[20] = {
    (void *)xc_QI, (void *)xc_AddRef, (void *)xc_Release,
    (void *)xc_GetIids, NOTIMPL, (void *)xc_GetTL,
    NOTIMPL,                  /* 6  ActivateInstance */
    NOTIMPL,                  /* 7  get_RowProperty */
    (void *)gs_GetRow,        /* 8  GetRow */
    (void *)gs_SetRow,        /* 9  SetRow */
    NOTIMPL,                  /* 10 get_ColumnProperty */
    (void *)gs_GetCol,        /* 11 GetColumn */
    (void *)gs_SetCol,        /* 12 SetColumn */
    NOTIMPL, NOTIMPL, NOTIMPL,/* 13-15 RowSpan */
    NOTIMPL, NOTIMPL, NOTIMPL,/* 16-18 ColumnSpan */
    NOTIMPL,
};
typedef struct { void **lpVtbl; } GridStaticsObj;
static GridStaticsObj g_grid_statics = { s_grid_statics_vtbl };

/* =========================================================================
 * Generic factory macro (ActivateInstance → AnvilElem)
 * ========================================================================= */

typedef struct { void **lpVtbl; int elem_type; } XcFactory;

static HRESULT WINAPI xc_factory_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }

static HRESULT xc_activate(void *T, void **out)
{
    XcFactory *f = (XcFactory *)T;
    AnvilElem *e = elem_alloc(f->elem_type);
    if (!e) { *out = NULL; return E_OUTOFMEMORY; }
    *out = e;
    return S_OK;
}

#define MAKE_FACTORY(tag, type_code, wname, wlen)                            \
static HRESULT WINAPI tag##_GetRCN(void *T, HSTRING *s)                     \
    { (void)T; return WindowsCreateString(wname, wlen, s); }                 \
static HRESULT WINAPI tag##_Activate(void *T, void **out)                   \
    { (void)T; AnvilElem *e = elem_alloc(type_code);                         \
      if (!e) { *out = NULL; return E_OUTOFMEMORY; }                         \
      *out = e; return S_OK; }                                               \
static void *s_##tag##_fact_vtbl[8] = {                                      \
    (void *)xc_factory_QI, (void *)xc_AddRef, (void *)xc_Release,           \
    (void *)xc_GetIids, (void *)tag##_GetRCN, (void *)xc_GetTL,             \
    (void *)tag##_Activate,  /* 6 ActivateInstance */                        \
    NOTIMPL,                                                                  \
};                                                                            \
static XcFactory g_##tag##_factory = { s_##tag##_fact_vtbl, type_code }

MAKE_FACTORY(frame,    ELEM_FRAME,       L"Windows.UI.Xaml.Controls.Frame",        30);
MAKE_FACTORY(page,     ELEM_PAGE,        L"Windows.UI.Xaml.Controls.Page",         28);
MAKE_FACTORY(sp,       ELEM_STACKPANEL,  L"Windows.UI.Xaml.Controls.StackPanel",   34);
MAKE_FACTORY(button,   ELEM_BUTTON,      L"Windows.UI.Xaml.Controls.Button",       30);
MAKE_FACTORY(togbtn,   ELEM_TOGGLEBTN,   L"Windows.UI.Xaml.Controls.ToggleButton", 35);
MAKE_FACTORY(textblk,  ELEM_TEXTBLOCK,   L"Windows.UI.Xaml.Controls.TextBlock",    32);
MAKE_FACTORY(textbox,  ELEM_TEXTBOX,     L"Windows.UI.Xaml.Controls.TextBox",      31);
MAKE_FACTORY(scroll,   ELEM_SCROLLVIEWER,L"Windows.UI.Xaml.Controls.ScrollViewer", 35);
MAKE_FACTORY(border,   ELEM_BORDER,      L"Windows.UI.Xaml.Controls.Border",       30);
MAKE_FACTORY(listview, ELEM_GENERIC,     L"Windows.UI.Xaml.Controls.ListView",     31);
MAKE_FACTORY(gridview, ELEM_GENERIC,     L"Windows.UI.Xaml.Controls.GridView",     31);
MAKE_FACTORY(cb,       ELEM_GENERIC,     L"Windows.UI.Xaml.Controls.ComboBox",     32);
MAKE_FACTORY(image,    ELEM_GENERIC,     L"Windows.UI.Xaml.Controls.Image",        27);
MAKE_FACTORY(canvas,   ELEM_GENERIC,     L"Windows.UI.Xaml.Controls.Canvas",       29);
MAKE_FACTORY(relativepanel, ELEM_GENERIC,L"Windows.UI.Xaml.Controls.RelativePanel",37);
MAKE_FACTORY(splitview, ELEM_GENERIC,    L"Windows.UI.Xaml.Controls.SplitView",    32);

/* Grid — special factory that also registers as IGridStatics */
static HRESULT WINAPI grid_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.UI.Xaml.Controls.Grid", 29, s); }
static HRESULT WINAPI grid_factory_QI(void *T, REFIID iid, void **out)
{
    /* QI for IGridStatics returns the statics vtable */
    static const IID IID_IGridStatics = {
        0x4D8B6B57, 0x4C36, 0x4E94,
        {0xA5, 0xF6, 0x88, 0x73, 0x9E, 0xA0, 0x65, 0xEA}
    };
    if (!memcmp(iid, &IID_IGridStatics, sizeof(IID)))
        { *out = &g_grid_statics; return S_OK; }
    *out = T; return S_OK;
}
static HRESULT WINAPI grid_Activate(void *T, void **out)
{
    (void)T;
    AnvilElem *e = elem_alloc(ELEM_GRID);
    if (!e) { *out = NULL; return E_OUTOFMEMORY; }
    /* Wire grid-specific slots over the generic vtable */
    *out = e; return S_OK;
}
static void *s_grid_fact_vtbl[8] = {
    (void *)grid_factory_QI, (void *)xc_AddRef, (void *)xc_Release,
    (void *)xc_GetIids, (void *)grid_GetRCN, (void *)xc_GetTL,
    (void *)grid_Activate,  /* 6 ActivateInstance */
    NOTIMPL,
};
static XcFactory g_grid_factory = { s_grid_fact_vtbl, ELEM_GRID };

/* =========================================================================
 * Brush stubs
 * ========================================================================= */

typedef struct { void **lpVtbl; UINT32 color; } AnvilBrush;

static HRESULT WINAPI brush_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.UI.Xaml.Media.SolidColorBrush", 38, s); }
static HRESULT WINAPI brush_Activate(void *T, void **out)
{
    (void)T;
    AnvilBrush *b = (AnvilBrush *)calloc(1, sizeof *b);
    if (!b) { *out = NULL; return E_OUTOFMEMORY; }
    static void *s_br_vtbl[16];
    static int vdone = 0;
    if (!vdone) {
        for (int i = 0; i < 16; i++) s_br_vtbl[i] = NOTIMPL;
        s_br_vtbl[0] = (void *)xc_QI; s_br_vtbl[1] = (void *)xc_AddRef;
        s_br_vtbl[2] = (void *)xc_Release; s_br_vtbl[3] = (void *)xc_GetIids;
        s_br_vtbl[5] = (void *)xc_GetTL;
        vdone = 1;
    }
    b->lpVtbl = s_br_vtbl;
    *out = b; return S_OK;
}
static void *s_brush_fact_vtbl[8] = {
    (void *)xc_QI, (void *)xc_AddRef, (void *)xc_Release,
    (void *)xc_GetIids, (void *)brush_GetRCN, (void *)xc_GetTL,
    (void *)brush_Activate,
    NOTIMPL,
};
static AnvilBrush g_brush_factory = { s_brush_fact_vtbl, 0 };

/* =========================================================================
 * Style, Setter, VisualStateManager, Storyboard (pure stubs)
 * ========================================================================= */

MAKE_FACTORY(style,     ELEM_GENERIC, L"Windows.UI.Xaml.Style",  22);
MAKE_FACTORY(setter,    ELEM_GENERIC, L"Windows.UI.Xaml.Setter", 23);
MAKE_FACTORY(vsm,       ELEM_GENERIC,
    L"Windows.UI.Xaml.VisualStateManager", 36);
MAKE_FACTORY(storyboard,ELEM_GENERIC,
    L"Windows.UI.Xaml.Media.Animation.Storyboard", 44);
MAKE_FACTORY(resdictentry, ELEM_GENERIC,
    L"Windows.UI.Xaml.ResourceDictionary", 35);
MAKE_FACTORY(gradbrush,    ELEM_GENERIC,
    L"Windows.UI.Xaml.Media.LinearGradientBrush", 43);
MAKE_FACTORY(gradstop,     ELEM_GENERIC,
    L"Windows.UI.Xaml.Media.GradientStop", 35);
MAKE_FACTORY(themeresource, ELEM_GENERIC,
    L"Windows.UI.Xaml.ThicknessHelper", 32);
MAKE_FACTORY(cornerradius,  ELEM_GENERIC,
    L"Windows.UI.Xaml.CornerRadiusHelper", 35);
MAKE_FACTORY(durationhelper, ELEM_GENERIC,
    L"Windows.UI.Xaml.DurationHelper", 31);
MAKE_FACTORY(pointhelper, ELEM_GENERIC,
    L"Windows.UI.Xaml.Input.PointerPoint", 35);
MAKE_FACTORY(recthelper, ELEM_GENERIC,
    L"Windows.UI.Xaml.RectHelper", 26);
MAKE_FACTORY(sizehelpx, ELEM_GENERIC,
    L"Windows.UI.Xaml.SizeHelper", 26);
MAKE_FACTORY(flyout, ELEM_GENERIC,
    L"Windows.UI.Xaml.Controls.Flyout", 31);
MAKE_FACTORY(menuflyi, ELEM_GENERIC,
    L"Windows.UI.Xaml.Controls.MenuFlyout", 35);
MAKE_FACTORY(menuflitem, ELEM_GENERIC,
    L"Windows.UI.Xaml.Controls.MenuFlyoutItem", 40);
MAKE_FACTORY(tooltip, ELEM_GENERIC,
    L"Windows.UI.Xaml.Controls.ToolTip", 32);
MAKE_FACTORY(hyplink, ELEM_GENERIC,
    L"Windows.UI.Xaml.Documents.Hyperlink", 36);
MAKE_FACTORY(run_elem, ELEM_GENERIC,
    L"Windows.UI.Xaml.Documents.Run", 30);

/* =========================================================================
 * Registration
 * ========================================================================= */

void anvil_register_xaml_controls(void)
{
    elem_vtbl_init();

    anvil_register_factory(L"Windows.UI.Xaml.Controls.Frame",         &g_frame_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.Page",          &g_page_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.Grid",          &g_grid_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.StackPanel",    &g_sp_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.Button",        &g_button_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.ToggleButton",  &g_togbtn_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.TextBlock",     &g_textblk_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.TextBox",       &g_textbox_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.ScrollViewer",  &g_scroll_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.Border",        &g_border_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.ListView",      &g_listview_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.GridView",      &g_gridview_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.ComboBox",      &g_cb_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.Image",         &g_image_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.Canvas",        &g_canvas_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.RelativePanel", &g_relativepanel_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.SplitView",     &g_splitview_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Media.SolidColorBrush",  &g_brush_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Style",                  &g_style_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Setter",                 &g_setter_factory);
    anvil_register_factory(L"Windows.UI.Xaml.VisualStateManager",     &g_vsm_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Media.Animation.Storyboard", &g_storyboard_factory);
    anvil_register_factory(L"Windows.UI.Xaml.ResourceDictionary",     &g_resdictentry_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Media.LinearGradientBrush", &g_gradbrush_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Media.GradientStop",     &g_gradstop_factory);
    anvil_register_factory(L"Windows.UI.Xaml.ThicknessHelper",        &g_themeresource_factory);
    anvil_register_factory(L"Windows.UI.Xaml.CornerRadiusHelper",     &g_cornerradius_factory);
    anvil_register_factory(L"Windows.UI.Xaml.DurationHelper",         &g_durationhelper_factory);
    anvil_register_factory(L"Windows.UI.Xaml.RectHelper",             &g_recthelper_factory);
    anvil_register_factory(L"Windows.UI.Xaml.SizeHelper",             &g_sizehelpx_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.Flyout",        &g_flyout_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.MenuFlyout",    &g_menuflyi_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.MenuFlyoutItem",&g_menuflitem_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Controls.ToolTip",       &g_tooltip_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Documents.Hyperlink",    &g_hyplink_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Documents.Run",          &g_run_elem_factory);

    XLOG("anvil_register_xaml_controls: %d XAML control stubs registered", 36);
}

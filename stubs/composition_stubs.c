/*
 * composition_stubs.c — Anvil: Windows.UI.Composition API stubs.
 *
 * XAML uses the Composition API internally to manage its visual tree and
 * submit frames to DXGI.  These stubs prevent crashes when XAML code
 * creates Compositor / Visual / CompositionTarget objects.
 *
 * Rendering is handled entirely by xaml_renderer.c (GDI), so Composition
 * objects are intentionally no-ops.
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_comp);
# define COMPLOG(fmt,...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# define COMPLOG(fmt,...) fprintf(stderr, "[anvil/comp] " fmt "\n", ##__VA_ARGS__)
#endif

#include <stddef.h>

extern HRESULT WINAPI WindowsCreateString(LPCWSTR src, UINT32 len, HSTRING *out);
extern int     anvil_register_factory(const WCHAR *class_id, void *factory);

static HRESULT WINAPI cp_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static ULONG   WINAPI cp_AddRef (void *T) { (void)T; return 1; }
static ULONG   WINAPI cp_Release(void *T) { (void)T; return 1; }
static HRESULT WINAPI cp_GetIids(void *T, ULONG *n, IID **ids)
    { (void)T; *n = 0; *ids = NULL; return S_OK; }
static HRESULT WINAPI cp_GetTL(void *T, TrustLevel *tl)
    { (void)T; *tl = BaseTrust; return S_OK; }
static HRESULT WINAPI cp_notimpl(void *T, ...)
    { (void)T; return E_NOTIMPL; }
static HRESULT WINAPI cp_ok(void *T, ...)
    { (void)T; return S_OK; }

#define NOTIMPL ((void *)cp_notimpl)
#define STUB_OK ((void *)cp_ok)

/* =========================================================================
 * Generic Composition object — 80-slot NOTIMPL vtable
 * ========================================================================= */

#define COMP_VTBL_SZ 80
static void *s_comp_vtbl[COMP_VTBL_SZ];
static int   s_comp_vtbl_done = 0;

static void comp_vtbl_init(void)
{
    if (s_comp_vtbl_done) return;
    s_comp_vtbl_done = 1;
    for (int i = 0; i < COMP_VTBL_SZ; i++) s_comp_vtbl[i] = NOTIMPL;
    s_comp_vtbl[0] = (void *)cp_QI;
    s_comp_vtbl[1] = (void *)cp_AddRef;
    s_comp_vtbl[2] = (void *)cp_Release;
    s_comp_vtbl[3] = (void *)cp_GetIids;
    s_comp_vtbl[5] = (void *)cp_GetTL;
    /* Mark common property slots S_OK so callers don't bail on E_NOTIMPL */
    for (int i = 6; i < 30; i++) s_comp_vtbl[i] = STUB_OK;
}

typedef struct { void **lpVtbl; } CompObj;

static CompObj *comp_alloc(void)
{
    comp_vtbl_init();
    CompObj *o = (CompObj *)calloc(1, sizeof *o);
    if (o) o->lpVtbl = s_comp_vtbl;
    return o;
}

/* =========================================================================
 * Compositor factory
 *
 * ICompositorStatics (no static methods beyond IActivationFactory).
 * ActivateInstance creates a Compositor.
 *
 * ICompositor methods (starting at slot 6):
 *   6  CreateColorKeyFrameAnimation
 *   7  CreateColorAnimation
 *   8  CreateColorBrush()            → CompObj
 *   9  CreateColorBrushWithColor     → CompObj
 *   10 CreateContainerVisual()       → CompObj
 *   11 CreateCubicBezierEasingFunction
 *   12 CreateEffectFactory
 *   13 CreateExpressionAnimation
 *   14 CreateInsetClip
 *   15 CreateLinearEasingFunction
 *   16 CreatePropertySet
 *   17 CreateQuaternionKeyFrameAnimation
 *   18 CreateScalarAnimation
 *   19 CreateScopedBatch
 *   20 CreateSpriteVisual()          → CompObj
 *   21 CreateStepEasingFunction
 *   22 CreateVector2Animation
 *   23 CreateVector3Animation
 *   24 CreateVector4Animation
 *   25 GetCommitBatch
 * ========================================================================= */

static HRESULT WINAPI comp_create_visual(void *T, void **out)
    { (void)T; CompObj *o = comp_alloc(); *out = o; return o ? S_OK : E_OUTOFMEMORY; }

static void *s_compositor_vtbl[COMP_VTBL_SZ];
static int   s_compositor_vtbl_done = 0;

static void compositor_vtbl_init(void)
{
    if (s_compositor_vtbl_done) return;
    comp_vtbl_init();
    s_compositor_vtbl_done = 1;
    memcpy(s_compositor_vtbl, s_comp_vtbl, sizeof s_compositor_vtbl);
    /* CreateColorBrush (slot 8), CreateContainerVisual (10), CreateSpriteVisual (20) */
    s_compositor_vtbl[8]  = (void *)comp_create_visual;
    s_compositor_vtbl[9]  = (void *)comp_create_visual;
    s_compositor_vtbl[10] = (void *)comp_create_visual;
    s_compositor_vtbl[20] = (void *)comp_create_visual;
}

static HRESULT WINAPI compositor_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.UI.Composition.Compositor", 34, s); }
static HRESULT WINAPI compositor_Activate(void *T, void **out)
{
    (void)T;
    compositor_vtbl_init();
    CompObj *o = (CompObj *)calloc(1, sizeof *o);
    if (!o) { *out = NULL; return E_OUTOFMEMORY; }
    o->lpVtbl = s_compositor_vtbl;
    *out = o; return S_OK;
}
static void *s_compositor_fact_vtbl[8] = {
    (void *)cp_QI, (void *)cp_AddRef, (void *)cp_Release,
    (void *)cp_GetIids, (void *)compositor_GetRCN, (void *)cp_GetTL,
    (void *)compositor_Activate,
    NOTIMPL,
};
static CompObj g_compositor_factory = { s_compositor_fact_vtbl };

/* =========================================================================
 * CompositionTarget factory
 * ICompositionTargetStatics: CreateWithWindowId, etc.
 * For our stub, ActivateInstance returns a CompObj (no real rendering).
 * ========================================================================= */

static HRESULT WINAPI comtgt_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.UI.Composition.CompositionTarget", 41, s); }
static void *s_comtgt_fact_vtbl[10] = {
    (void *)cp_QI, (void *)cp_AddRef, (void *)cp_Release,
    (void *)cp_GetIids, (void *)comtgt_GetRCN, (void *)cp_GetTL,
    NOTIMPL,       /* 6  ActivateInstance */
    NOTIMPL,       /* 7  CreateWithWindowId */
    NOTIMPL,       /* 8  CreateDesktopWindowTarget */
    NOTIMPL,
};
static CompObj g_comtgt_factory = { s_comtgt_fact_vtbl };

/* =========================================================================
 * DesktopWindowContentBridge — used to embed XAML into an HWND
 * ========================================================================= */

static HRESULT WINAPI dwcb_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(
        L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource", 49, s); }
static HRESULT WINAPI dwcb_Activate(void *T, void **out)
    { (void)T; CompObj *o = comp_alloc(); *out = o; return o ? S_OK : E_OUTOFMEMORY; }
static void *s_dwcb_fact_vtbl[8] = {
    (void *)cp_QI, (void *)cp_AddRef, (void *)cp_Release,
    (void *)cp_GetIids, (void *)dwcb_GetRCN, (void *)cp_GetTL,
    (void *)dwcb_Activate,
    NOTIMPL,
};
static CompObj g_dwcb_factory = { s_dwcb_fact_vtbl };

/* =========================================================================
 * Additional composition types (pure stubs)
 * ========================================================================= */

#define COMP_FACTORY(tag, wname, wlen)                                         \
static HRESULT WINAPI tag##_GetRCN(void *T, HSTRING *s)                       \
    { (void)T; return WindowsCreateString(wname, wlen, s); }                   \
static HRESULT WINAPI tag##_Act(void *T, void **out)                           \
    { (void)T; CompObj *o = comp_alloc(); *out = o; return o ? S_OK : E_OUTOFMEMORY; } \
static void *s_##tag##_fact_vtbl[8] = {                                        \
    (void *)cp_QI, (void *)cp_AddRef, (void *)cp_Release,                      \
    (void *)cp_GetIids, (void *)tag##_GetRCN, (void *)cp_GetTL,                \
    (void *)tag##_Act, NOTIMPL,                                                 \
};                                                                              \
static CompObj g_##tag##_factory = { s_##tag##_fact_vtbl }

COMP_FACTORY(contvis,
    L"Windows.UI.Composition.ContainerVisual", 40);
COMP_FACTORY(spritevis,
    L"Windows.UI.Composition.SpriteVisual", 36);
COMP_FACTORY(compcolorbr,
    L"Windows.UI.Composition.CompositionColorBrush", 46);
COMP_FACTORY(compeffbr,
    L"Windows.UI.Composition.CompositionEffectBrush", 47);
COMP_FACTORY(compsurfbr,
    L"Windows.UI.Composition.CompositionSurfaceBrush", 48);
COMP_FACTORY(compshape,
    L"Windows.UI.Composition.CompositionShape", 40);
COMP_FACTORY(compgeom,
    L"Windows.UI.Composition.CompositionGeometry", 43);
COMP_FACTORY(shapevis,
    L"Windows.UI.Composition.ShapeVisual", 35);
COMP_FACTORY(compclip,
    L"Windows.UI.Composition.InsetClip", 33);

/* =========================================================================
 * Registration
 * ========================================================================= */

void anvil_register_composition_factories(void)
{
    comp_vtbl_init();
    compositor_vtbl_init();

    anvil_register_factory(L"Windows.UI.Composition.Compositor",
                           &g_compositor_factory);
    anvil_register_factory(L"Windows.UI.Composition.CompositionTarget",
                           &g_comtgt_factory);
    anvil_register_factory(L"Windows.UI.Xaml.Hosting.DesktopWindowXamlSource",
                           &g_dwcb_factory);
    anvil_register_factory(L"Windows.UI.Composition.ContainerVisual",
                           &g_contvis_factory);
    anvil_register_factory(L"Windows.UI.Composition.SpriteVisual",
                           &g_spritevis_factory);
    anvil_register_factory(L"Windows.UI.Composition.CompositionColorBrush",
                           &g_compcolorbr_factory);
    anvil_register_factory(L"Windows.UI.Composition.CompositionEffectBrush",
                           &g_compeffbr_factory);
    anvil_register_factory(L"Windows.UI.Composition.CompositionSurfaceBrush",
                           &g_compsurfbr_factory);
    anvil_register_factory(L"Windows.UI.Composition.CompositionShape",
                           &g_compshape_factory);
    anvil_register_factory(L"Windows.UI.Composition.CompositionGeometry",
                           &g_compgeom_factory);
    anvil_register_factory(L"Windows.UI.Composition.ShapeVisual",
                           &g_shapevis_factory);
    anvil_register_factory(L"Windows.UI.Composition.InsetClip",
                           &g_compclip_factory);

    COMPLOG("anvil_register_composition_factories: 12 stubs registered");
}

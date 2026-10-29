/*
 * dispatcher_stubs.c — Anvil Milestone 7a: CoreDispatcher + IAsyncAction.
 *
 * ICoreDispatcher ABI slots (Windows.UI.Core.ICoreDispatcher):
 *   6  get_HasThreadAccess(BOOL*)
 *   7  ProcessEvents(CoreProcessEventsOption)
 *   8  RunAsync(CoreDispatcherPriority, IDispatchedHandler*, IAsyncAction**)
 *   9  RunIdleAsync(IIdleDispatchedHandler*, IAsyncAction**)
 *
 * IAsyncAction ABI slots:
 *   6  put_Completed(IAsyncActionCompletedHandler*)
 *   7  get_Completed(IAsyncActionCompletedHandler**)
 *   8  GetResults()
 *
 * IAsyncInfo ABI slots:
 *   6  get_Id(UINT32*)
 *   7  get_Status(AsyncStatus*)
 *   8  get_ErrorCode(HRESULT*)
 *   9  Cancel()
 *   10 Close()
 *
 * CoreProcessEventsOption enum:
 *   0  ProcessOneIfPresent
 *   1  ProcessUntilQuit
 *   2  ProcessOneAndAllPending
 *   3  ProcessAllIfPresent
 *
 * AsyncStatus enum: 0=Started, 1=Completed, 2=Cancelled, 3=Error
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_disp);
# define DLOG(fmt,...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# define DLOG(fmt,...) fprintf(stderr, "[anvil/disp] " fmt "\n", ##__VA_ARGS__)
#endif

#include <stddef.h>
#include <string.h>

extern HRESULT WINAPI WindowsCreateString(LPCWSTR src, UINT32 len, HSTRING *out);

#ifndef CONTAINING_RECORD
# define CONTAINING_RECORD(addr, type, field) \
    ((type *)((char *)(addr) - offsetof(type, field)))
#endif

static ULONG   WINAPI d_AddRef(void *T)                        { (void)T; return 1; }
static ULONG   WINAPI d_Release(void *T)                       { (void)T; return 1; }
static HRESULT WINAPI d_GetIids(void *T, ULONG *n, IID **ids) { (void)T; *n=0; *ids=NULL; return S_OK; }
static HRESULT WINAPI d_GetTL(void *T, TrustLevel *tl)        { (void)T; *tl=BaseTrust; return S_OK; }
static HRESULT WINAPI d_notimpl(void *T, ...)                  { (void)T; return E_NOTIMPL; }
#define NOTIMPL ((void *)d_notimpl)

/* =========================================================================
 * IAsyncAction + IAsyncInfo — two-vtable "already completed" singleton.
 * ========================================================================= */

typedef struct AnvilAsyncAction {
    void *lpAction; /* IAsyncAction; offset 0 */
    void *lpInfo;   /* IAsyncInfo;   offset 8 */
} AnvilAsyncAction;

#define ACTION_T(ppv)  ((AnvilAsyncAction *)(ppv))
#define INFO_T(ppv)    CONTAINING_RECORD(ppv, AnvilAsyncAction, lpInfo)

static void *s_action_vtbl[];
static void *s_info_vtbl[];

static const IID IID_IAsyncAction = {
    0x5A648006, 0x843A, 0x4DA9,
    { 0x86, 0x5B, 0x9D, 0x26, 0xE5, 0xDF, 0xAD, 0x7B }
};
static const IID IID_IAsyncInfo = {
    0x00000036, 0x0000, 0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

/* IAsyncAction methods */
static HRESULT WINAPI action_QI(void *ppv, REFIID iid, void **out)
{
    AnvilAsyncAction *T = ACTION_T(ppv);
    if (IsEqualIID(iid, &IID_IAsyncAction) || IsEqualIID(iid, &IID_IUnknown) ||
        IsEqualIID(iid, &IID_IInspectable)) {
        *out = &T->lpAction; return S_OK;
    }
    if (IsEqualIID(iid, &IID_IAsyncInfo)) {
        *out = &T->lpInfo; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI action_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(L"Windows.Foundation.IAsyncAction", 32, s); }

static HRESULT WINAPI action_put_Completed(void *ppv, void *handler)
{
    AnvilAsyncAction *T = ACTION_T(ppv);
    if (!handler) return S_OK;
    /* Fire immediately: handler->Invoke(this, AsyncStatus_Completed=1) */
    typedef HRESULT (WINAPI *PFN_Invoke)(void*, void*, UINT32);
    PFN_Invoke invoke = (PFN_Invoke)(*(void***)handler)[3];
    invoke(handler, &T->lpAction, 1 /*Completed*/);
    return S_OK;
}
static HRESULT WINAPI action_get_Completed(void *T, void **out)
    { (void)T; *out = NULL; return S_OK; }  /* already fired */
static HRESULT WINAPI action_GetResults(void *T)
    { (void)T; return S_OK; }

static void *s_action_vtbl[] = {
    action_QI, d_AddRef, d_Release,
    d_GetIids, action_GetRCN, d_GetTL,
    action_put_Completed,  /* 6 */
    action_get_Completed,  /* 7 */
    action_GetResults,     /* 8 */
};

/* IAsyncInfo methods */
static HRESULT WINAPI info_QI(void *ppv, REFIID iid, void **out)
    { return action_QI(&INFO_T(ppv)->lpAction, iid, out); }
static HRESULT WINAPI info_GetRCN(void *T, HSTRING *s)
    { return action_GetRCN(T, s); }
static HRESULT WINAPI info_get_Id    (void *T, UINT32  *out) { (void)T; *out = 1; return S_OK; }
static HRESULT WINAPI info_get_Status(void *T, UINT32  *out) { (void)T; *out = 1; return S_OK; } /* Completed */
static HRESULT WINAPI info_get_Error (void *T, HRESULT *out) { (void)T; *out = S_OK; return S_OK; }

static void *s_info_vtbl[] = {
    info_QI, d_AddRef, d_Release,
    d_GetIids, info_GetRCN, d_GetTL,
    info_get_Id,    /* 6 */
    info_get_Status,/* 7 */
    info_get_Error, /* 8 */
    NOTIMPL,        /* 9  Cancel */
    NOTIMPL,        /* 10 Close  */
};

/* Singleton completed action — returned from RunAsync */
static AnvilAsyncAction g_completed_action = { s_action_vtbl, s_info_vtbl };

void *anvil_completed_action(void) { return &g_completed_action.lpAction; }

/* =========================================================================
 * ICoreDispatcher singleton
 * ========================================================================= */

typedef struct { void *lpVtbl; } AnvilCoreDispatcher;

static HRESULT WINAPI disp_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static HRESULT WINAPI disp_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(L"Windows.UI.Core.CoreDispatcher", 30, s); }

static HRESULT WINAPI disp_get_HasThreadAccess(void *T, BOOL *out)
    { (void)T; *out = TRUE; return S_OK; }

static HRESULT WINAPI disp_ProcessEvents(void *T, UINT32 option)
{
    (void)T;
    MSG msg;
    switch (option) {
    case 1: /* ProcessUntilQuit — blocking message loop */
        while (GetMessageW(&msg, NULL, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        break;
    case 0: /* ProcessOneIfPresent */
        if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        break;
    default: /* 2=ProcessOneAndAllPending, 3=ProcessAllIfPresent */
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return S_OK;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        break;
    }
    return S_OK;
}

static HRESULT WINAPI disp_RunAsync(void *T, UINT32 priority, void *handler, void **ppAction)
{
    (void)T; (void)priority;
    if (handler) {
        /* IDispatchedHandler::Invoke() at slot 3 */
        typedef HRESULT (WINAPI *PFN_Invoke)(void*);
        PFN_Invoke invoke = (PFN_Invoke)(*(void***)handler)[3];
        invoke(handler);
    }
    if (ppAction) *ppAction = anvil_completed_action();
    return S_OK;
}

static HRESULT WINAPI disp_RunIdleAsync(void *T, void *handler, void **ppAction)
{
    return disp_RunAsync(T, 0, handler, ppAction);
}

static void *s_disp_vtbl[] = {
    disp_QI, d_AddRef, d_Release,
    d_GetIids, disp_GetRCN, d_GetTL,
    disp_get_HasThreadAccess, /* 6 */
    disp_ProcessEvents,       /* 7 */
    disp_RunAsync,            /* 8 */
    disp_RunIdleAsync,        /* 9 */
};

static AnvilCoreDispatcher g_dispatcher = { s_disp_vtbl };

void *anvil_get_dispatcher(void) { return &g_dispatcher; }

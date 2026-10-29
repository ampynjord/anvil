/*
 * storage_stubs.c — Anvil Milestone 6: Windows.Storage stubs.
 *
 * Implements:
 *   Windows.Storage.ApplicationData  (IApplicationDataStatics + IApplicationData)
 *   Windows.Storage.StorageFolder    (IStorageItem + IStorageFolder)
 *   Windows.Storage.ApplicationDataContainer
 *
 * IIDs extracted from Windows.Storage.ApplicationData.winmd:
 *   IApplicationDataStatics  {5612147B-E843-45E3-94D8-06169E3C8E17}
 *   IApplicationData         {C3DA6FB7-B744-4B45-B0B8-223A0938D0DC}
 *   IStorageItem             {4207A996-CA2F-42F7-BDE8-8B10457A7F30}
 *   IStorageFolder           {72D1CB78-B3EF-4F75-A80B-6FD9DAE2944B}
 *   IApplicationDataContainer{C5AEFD1E-F467-40BA-8566-AB640A441E1D}
 *
 * Folder paths (created at DLL load time):
 *   LocalFolder   %LOCALAPPDATA%\Packages\<ANVIL_PKG_FAMILYNAME>\LocalState
 *   RoamingFolder %LOCALAPPDATA%\Packages\<ANVIL_PKG_FAMILYNAME>\RoamingState
 *   TempFolder    %TEMP%\Packages\<ANVIL_PKG_FAMILYNAME>\TempState
 *
 * Replaces the P2 stub for Windows.Storage.ApplicationData.
 */

#ifdef __WINE__
# include <windows.h>
# include <wine/debug.h>
WINE_DEFAULT_DEBUG_CHANNEL(anvil_storage);
# define SLOG(fmt,...) TRACE(fmt "\n", ##__VA_ARGS__)
#else
# include "winrt_types.h"
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <stdio.h>
# define SLOG(fmt,...) fprintf(stderr, "[anvil/storage] " fmt "\n", ##__VA_ARGS__)
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

extern HRESULT WINAPI WindowsCreateString(LPCWSTR src, UINT32 len, HSTRING *out);
extern LPCWSTR WINAPI WindowsGetStringRawBuffer(HSTRING str, UINT32 *len);
extern int     anvil_register_factory(const WCHAR *class_id, void *factory);

#ifndef CONTAINING_RECORD
# define CONTAINING_RECORD(addr, type, field) \
    ((type *)((char *)(addr) - offsetof(type, field)))
#endif

/* =========================================================================
 * IIDs
 * ========================================================================= */
static const IID IID_IApplicationDataStatics = {
    0x5612147B, 0xE843, 0x45E3,
    { 0x94, 0xD8, 0x06, 0x16, 0x9E, 0x3C, 0x8E, 0x17 }
};
static const IID IID_IApplicationData = {
    0xC3DA6FB7, 0xB744, 0x4B45,
    { 0xB0, 0xB8, 0x22, 0x3A, 0x09, 0x38, 0xD0, 0xDC }
};
static const IID IID_IStorageItem = {
    0x4207A996, 0xCA2F, 0x42F7,
    { 0xBD, 0xE8, 0x8B, 0x10, 0x45, 0x7A, 0x7F, 0x30 }
};
static const IID IID_IStorageFolder = {
    0x72D1CB78, 0xB3EF, 0x4F75,
    { 0xA8, 0x0B, 0x6F, 0xD9, 0xDA, 0xE2, 0x94, 0x4B }
};
static const IID IID_IApplicationDataContainer = {
    0xC5AEFD1E, 0xF467, 0x40BA,
    { 0x85, 0x66, 0xAB, 0x64, 0x0A, 0x44, 0x1E, 0x1D }
};

/* =========================================================================
 * Shared no-op helpers
 * ========================================================================= */
static ULONG   WINAPI s6_AddRef(void *T)                        { (void)T; return 1; }
static ULONG   WINAPI s6_Release(void *T)                       { (void)T; return 1; }
static HRESULT WINAPI s6_GetIids(void *T, ULONG *n, IID **ids) { (void)T; *n=0; *ids=NULL; return S_OK; }
static HRESULT WINAPI s6_GetTL(void *T, TrustLevel *tl)        { (void)T; *tl=BaseTrust; return S_OK; }
static HRESULT WINAPI s6_notimpl(void *T, ...)                  { (void)T; return E_NOTIMPL; }
#define NOTIMPL ((void *)s6_notimpl)

/* =========================================================================
 * AnvilStorageFolder — implements IStorageItem (lpItem) + IStorageFolder (lpFolder)
 *
 * IStorageItem ABI slots (after IInspectable, 0-indexed from 6):
 *   6  RenameAsyncOverloadDefaultOptions(HSTRING, IAsyncAction**)
 *   7  RenameAsync(HSTRING, NameCollisionOption, IAsyncAction**)
 *   8  DeleteAsyncOverloadDefaultOptions(IAsyncAction**)
 *   9  DeleteAsync(StorageDeleteOption, IAsyncAction**)
 *   10 GetBasicPropertiesAsync(IAsyncOperation<BasicProperties*>**)
 *   11 get_Name(HSTRING*)
 *   12 get_Path(HSTRING*)
 *   13 get_Attributes(FileAttributes*)
 *   14 get_DateCreated(DateTime*)
 *   15 IsOfType(StorageItemTypes, boolean*)
 *
 * IStorageFolder ABI slots (after IInspectable):
 *   6  CreateFileAsyncOverloadDefaultOptions(HSTRING, IAsyncOperation<StorageFile*>**)
 *   7  CreateFileAsync(HSTRING, CreationCollisionOption, IAsyncOperation<StorageFile*>**)
 *   8  CreateFolderAsyncOverloadDefaultOptions(HSTRING, IAsyncOperation<StorageFolder*>**)
 *   9  CreateFolderAsync(HSTRING, CreationCollisionOption, IAsyncOperation<StorageFolder*>**)
 *   10 GetFileAsync(HSTRING, IAsyncOperation<StorageFile*>**)
 *   11 GetFolderAsync(HSTRING, IAsyncOperation<StorageFolder*>**)
 *   12 GetItemAsync(HSTRING, IAsyncOperation<IStorageItem*>**)
 *   13 GetFilesAsyncOverloadDefaultOptionsStartAndCount(IAsyncOperation<IVectorView<StorageFile*>*>**)
 *   14 GetFoldersAsyncOverloadDefaultOptionsStartAndCount(IAsyncOperation<IVectorView<StorageFolder*>*>**)
 *   15 GetItemsAsyncOverloadDefaultStartAndCount(IAsyncOperation<IVectorView<IStorageItem*>*>**)
 * ========================================================================= */

typedef struct {
    void *lpItem;      /* → s_item_vtbl  (IStorageItem);   offset 0 */
    void *lpFolder;    /* → s_folder_vtbl (IStorageFolder); offset 8 */
    WCHAR path[512];
    WCHAR name[64];
} AnvilStorageFolder;

#define ITEM_T(ppv)   ((AnvilStorageFolder *)(ppv))   /* lpItem is at offset 0 */
#define FOLDER_T(ppv) CONTAINING_RECORD(ppv, AnvilStorageFolder, lpFolder)

/* Forward */
static void *s_item_vtbl[];
static void *s_folder_vtbl[];

/* Three singleton folder objects — paths filled in storage_init() */
static AnvilStorageFolder g_local_folder   = { NULL, NULL, {0}, L"LocalState"   };
static AnvilStorageFolder g_roaming_folder = { NULL, NULL, {0}, L"RoamingState" };
static AnvilStorageFolder g_temp_folder    = { NULL, NULL, {0}, L"TempState"    };

/* ---- IStorageItem vtable functions --------------------------------------- */

static HRESULT WINAPI item_QI(void *ppv, REFIID iid, void **out)
{
    AnvilStorageFolder *T = ITEM_T(ppv);
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IStorageItem)) {
        *out = &T->lpItem; return S_OK;
    }
    if (IsEqualIID(iid, &IID_IStorageFolder) || IsEqualIID(iid, &IID_IInspectable)) {
        *out = &T->lpFolder; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI item_GetRCN(void *ppv, HSTRING *s)
    { (void)ppv; return WindowsCreateString(L"Windows.Storage.StorageFolder", 29, s); }
static HRESULT WINAPI item_get_Name(void *ppv, HSTRING *out)
{
    AnvilStorageFolder *T = ITEM_T(ppv);
    return WindowsCreateString(T->name, (UINT32)wcslen(T->name), out);
}
static HRESULT WINAPI item_get_Path(void *ppv, HSTRING *out)
{
    AnvilStorageFolder *T = ITEM_T(ppv);
    return WindowsCreateString(T->path, (UINT32)wcslen(T->path), out);
}
static HRESULT WINAPI item_get_Attributes(void *ppv, UINT32 *out)
    { (void)ppv; *out = 16; /* FILE_ATTRIBUTE_DIRECTORY */ return S_OK; }
static HRESULT WINAPI item_get_DateCreated(void *ppv, INT64 *out)
    { (void)ppv; *out = 0; return S_OK; }
static HRESULT WINAPI item_IsOfType(void *ppv, UINT32 type, BOOL *out)
    { (void)ppv; *out = (type == 1); /* StorageItemTypes.Folder */ return S_OK; }

static void *s_item_vtbl[] = {
    item_QI, s6_AddRef, s6_Release,
    s6_GetIids, item_GetRCN, s6_GetTL,
    NOTIMPL,                /* 6  RenameAsyncOverloadDefaultOptions */
    NOTIMPL,                /* 7  RenameAsync */
    NOTIMPL,                /* 8  DeleteAsyncOverloadDefaultOptions */
    NOTIMPL,                /* 9  DeleteAsync */
    NOTIMPL,                /* 10 GetBasicPropertiesAsync */
    item_get_Name,          /* 11 get_Name */
    item_get_Path,          /* 12 get_Path */
    item_get_Attributes,    /* 13 get_Attributes */
    item_get_DateCreated,   /* 14 get_DateCreated */
    item_IsOfType,          /* 15 IsOfType */
};

/* ---- IStorageFolder vtable functions ------------------------------------- */

static HRESULT WINAPI folder_QI(void *ppv, REFIID iid, void **out)
{
    AnvilStorageFolder *T = FOLDER_T(ppv);
    return item_QI(&T->lpItem, iid, out);
}
static HRESULT WINAPI folder_GetRCN(void *ppv, HSTRING *s)
    { (void)ppv; return WindowsCreateString(L"Windows.Storage.StorageFolder", 29, s); }

static void *s_folder_vtbl[] = {
    folder_QI, s6_AddRef, s6_Release,
    s6_GetIids, folder_GetRCN, s6_GetTL,
    NOTIMPL,  /* 6  CreateFileAsyncOverloadDefaultOptions */
    NOTIMPL,  /* 7  CreateFileAsync */
    NOTIMPL,  /* 8  CreateFolderAsyncOverloadDefaultOptions */
    NOTIMPL,  /* 9  CreateFolderAsync */
    NOTIMPL,  /* 10 GetFileAsync */
    NOTIMPL,  /* 11 GetFolderAsync */
    NOTIMPL,  /* 12 GetItemAsync */
    NOTIMPL,  /* 13 GetFilesAsync */
    NOTIMPL,  /* 14 GetFoldersAsync */
    NOTIMPL,  /* 15 GetItemsAsync */
};

/* =========================================================================
 * IPropertySet stub — returned by IApplicationDataContainer::get_Values.
 * All map operations return E_NOTIMPL (settings read/write gracefully degrade).
 * ========================================================================= */

typedef struct { void *lpVtbl; } AnvilPropertySet;

static HRESULT WINAPI propset_QI(void *T, REFIID iid, void **out)
    { (void)iid; *out = T; return S_OK; }
static HRESULT WINAPI propset_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(L"Windows.Foundation.Collections.PropertySet", 42, s); }

static void *s_propset_vtbl[] = {
    propset_QI, s6_AddRef, s6_Release,
    s6_GetIids, propset_GetRCN, s6_GetTL,
    NOTIMPL, NOTIMPL, NOTIMPL, NOTIMPL, NOTIMPL,  /* map methods */
    NOTIMPL, NOTIMPL, NOTIMPL, NOTIMPL, NOTIMPL,
};

static AnvilPropertySet s_local_propset   = { s_propset_vtbl };
static AnvilPropertySet s_roaming_propset = { s_propset_vtbl };

/* =========================================================================
 * IApplicationDataContainer
 *
 * IApplicationDataContainer ABI slots:
 *   6  get_Locality(ApplicationDataLocality*)
 *   7  get_Name(HSTRING*)
 *   8  get_Containers(IMapView<HSTRING, IApplicationDataContainer*>**)
 *   9  CreateContainer(HSTRING, ApplicationDataCreateDisposition, IApplicationDataContainer**)
 *   10 DeleteContainer(HSTRING)
 *   11 get_Values(IPropertySet**)
 * ========================================================================= */

typedef struct {
    void   *lpVtbl;
    UINT32  locality;  /* 0=Local, 1=Roaming, 2=Temp */
    WCHAR   name[64];
    AnvilPropertySet *propset;
} AnvilAppDataContainer;

static HRESULT WINAPI cont_QI(void *T, REFIID iid, void **out)
{
    if (IsEqualIID(iid, &IID_IApplicationDataContainer) ||
        IsEqualIID(iid, &IID_IUnknown) ||
        IsEqualIID(iid, &IID_IInspectable)) {
        *out = T; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI cont_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(L"Windows.Storage.ApplicationDataContainer", 40, s); }
static HRESULT WINAPI cont_get_Locality(void *T, UINT32 *out)
    { AnvilAppDataContainer *C = (AnvilAppDataContainer *)T; *out = C->locality; return S_OK; }
static HRESULT WINAPI cont_get_Name(void *T, HSTRING *out)
{
    AnvilAppDataContainer *C = (AnvilAppDataContainer *)T;
    return WindowsCreateString(C->name, (UINT32)wcslen(C->name), out);
}
static HRESULT WINAPI cont_CreateContainer(void *T, HSTRING name, UINT32 disp, void **cont)
    { (void)T; (void)name; (void)disp; *cont = T; return S_OK; } /* return self as stub sub-container */
static HRESULT WINAPI cont_get_Values(void *T, void **ppSet)
{
    AnvilAppDataContainer *C = (AnvilAppDataContainer *)T;
    *ppSet = C->propset;
    return S_OK;
}

static void *s_cont_vtbl[] = {
    cont_QI, s6_AddRef, s6_Release,
    s6_GetIids, cont_GetRCN, s6_GetTL,
    cont_get_Locality,    /* 6  get_Locality */
    cont_get_Name,        /* 7  get_Name */
    NOTIMPL,              /* 8  get_Containers */
    cont_CreateContainer, /* 9  CreateContainer */
    NOTIMPL,              /* 10 DeleteContainer */
    cont_get_Values,      /* 11 get_Values */
};

static AnvilAppDataContainer g_local_cont = {
    s_cont_vtbl, 0, L"Local", &s_local_propset
};
static AnvilAppDataContainer g_roaming_cont = {
    s_cont_vtbl, 1, L"Roaming", &s_roaming_propset
};

/* =========================================================================
 * IApplicationData singleton
 *
 * IApplicationData ABI slots:
 *   6  get_Version(UINT32*)
 *   7  SetVersionAsync(UINT32, handler, IAsyncAction**)
 *   8  ClearAllAsync(IAsyncAction**)
 *   9  ClearAsync(ApplicationDataLocality, IAsyncAction**)
 *   10 get_LocalSettings(IApplicationDataContainer**)
 *   11 get_RoamingSettings(IApplicationDataContainer**)
 *   12 get_LocalFolder(IStorageFolder**)
 *   13 get_RoamingFolder(IStorageFolder**)
 *   14 get_TemporaryFolder(IStorageFolder**)
 *   15 DataChanged_add(handler, EventRegistrationToken*)
 *   16 DataChanged_remove(EventRegistrationToken)
 *   17 SignalDataChanged()
 *   18 get_RoamingStorageQuota(UINT64*)
 * ========================================================================= */

typedef struct { void *lpVtbl; } AnvilApplicationData;

static HRESULT WINAPI appdata_QI(void *T, REFIID iid, void **out)
{
    if (IsEqualIID(iid, &IID_IApplicationData) ||
        IsEqualIID(iid, &IID_IUnknown)          ||
        IsEqualIID(iid, &IID_IInspectable)) {
        *out = T; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI appdata_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(L"Windows.Storage.ApplicationData", 31, s); }
static HRESULT WINAPI appdata_get_Version(void *T, UINT32 *v)
    { (void)T; *v = 0; return S_OK; }
static HRESULT WINAPI appdata_get_LocalSettings(void *T, void **ppCont)
    { (void)T; *ppCont = &g_local_cont; return S_OK; }
static HRESULT WINAPI appdata_get_RoamingSettings(void *T, void **ppCont)
    { (void)T; *ppCont = &g_roaming_cont; return S_OK; }
static HRESULT WINAPI appdata_get_LocalFolder(void *T, void **ppFolder)
{
    (void)T;
    *ppFolder = &g_local_folder.lpFolder;
    SLOG("get_LocalFolder → %ls", g_local_folder.path);
    return S_OK;
}
static HRESULT WINAPI appdata_get_RoamingFolder(void *T, void **ppFolder)
{
    (void)T;
    *ppFolder = &g_roaming_folder.lpFolder;
    SLOG("get_RoamingFolder → %ls", g_roaming_folder.path);
    return S_OK;
}
static HRESULT WINAPI appdata_get_TemporaryFolder(void *T, void **ppFolder)
{
    (void)T;
    *ppFolder = &g_temp_folder.lpFolder;
    SLOG("get_TemporaryFolder → %ls", g_temp_folder.path);
    return S_OK;
}
static HRESULT WINAPI appdata_SignalDataChanged(void *T) { (void)T; return S_OK; }
static HRESULT WINAPI appdata_get_RoamingQuota(void *T, UINT64 *q)
    { (void)T; *q = 0; return S_OK; }

static void *s_appdata_vtbl[] = {
    appdata_QI, s6_AddRef, s6_Release,
    s6_GetIids, appdata_GetRCN, s6_GetTL,
    appdata_get_Version,          /* 6  get_Version */
    NOTIMPL,                      /* 7  SetVersionAsync */
    NOTIMPL,                      /* 8  ClearAllAsync */
    NOTIMPL,                      /* 9  ClearAsync */
    appdata_get_LocalSettings,    /* 10 get_LocalSettings */
    appdata_get_RoamingSettings,  /* 11 get_RoamingSettings */
    appdata_get_LocalFolder,      /* 12 get_LocalFolder */
    appdata_get_RoamingFolder,    /* 13 get_RoamingFolder */
    appdata_get_TemporaryFolder,  /* 14 get_TemporaryFolder */
    NOTIMPL,                      /* 15 DataChanged_add */
    NOTIMPL,                      /* 16 DataChanged_remove */
    appdata_SignalDataChanged,     /* 17 SignalDataChanged */
    appdata_get_RoamingQuota,     /* 18 get_RoamingStorageQuota */
};

static AnvilApplicationData g_app_data = { s_appdata_vtbl };

/* =========================================================================
 * IApplicationDataStatics + IActivationFactory (two-vtable COM)
 *
 * IApplicationDataStatics ABI:
 *   6  get_Current(IApplicationData**)
 * ========================================================================= */

typedef struct AnvilAppDataFactory AnvilAppDataFactory;
struct AnvilAppDataFactory {
    const IActivationFactoryVtbl *lpActFact;
    void                         *lpStatics;  /* → s_datastatics_vtbl */
};

static HRESULT WINAPI appdata_fact_QI(void *T, REFIID iid, void **out);

static HRESULT WINAPI datastatics_QI(void *ppv, REFIID iid, void **out)
{
    AnvilAppDataFactory *f = CONTAINING_RECORD(ppv, AnvilAppDataFactory, lpStatics);
    return appdata_fact_QI((void *)f, iid, out);
}
static HRESULT WINAPI datastatics_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(L"Windows.Storage.ApplicationData", 31, s); }
static HRESULT WINAPI datastatics_get_Current(void *T, void **ppData)
    { (void)T; *ppData = &g_app_data; SLOG("IApplicationDataStatics::get_Current"); return S_OK; }

static void *s_datastatics_vtbl[] = {
    datastatics_QI, s6_AddRef, s6_Release,
    s6_GetIids, datastatics_GetRCN, s6_GetTL,
    datastatics_get_Current,  /* 6 */
};

static HRESULT WINAPI appdata_fact_QI(void *T, REFIID iid, void **out)
{
    AnvilAppDataFactory *f = (AnvilAppDataFactory *)T;
    if (IsEqualIID(iid, &IID_IActivationFactory) ||
        IsEqualIID(iid, &IID_IUnknown)            ||
        IsEqualIID(iid, &IID_IInspectable)) {
        *out = T; return S_OK;
    }
    if (IsEqualIID(iid, &IID_IApplicationDataStatics)) {
        *out = &f->lpStatics; return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI appdata_fact_GetRCN(void *T, HSTRING *s)
    { (void)T; return WindowsCreateString(L"Windows.Storage.ApplicationData", 31, s); }
static HRESULT WINAPI appdata_fact_Activate(void *T, IInspectable **inst)
    { (void)T; *inst = NULL; return E_NOTIMPL; }

static const IActivationFactoryVtbl s_appdata_fact_vtbl = {
    appdata_fact_QI, s6_AddRef, s6_Release,
    s6_GetIids, appdata_fact_GetRCN, s6_GetTL,
    appdata_fact_Activate,
};
static AnvilAppDataFactory s_appdata_factory = { &s_appdata_fact_vtbl, s_datastatics_vtbl };

/* =========================================================================
 * Initialisation — compute folder paths and create them on disk.
 * ========================================================================= */

static void mkdir_chain(const WCHAR *path)
{
    WCHAR tmp[512];
    WCHAR *p;
    wcsncpy(tmp, path, 511); tmp[511] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == L'\\') {
            *p = 0;
            CreateDirectoryW(tmp, NULL);
            *p = L'\\';
        }
    }
    CreateDirectoryW(tmp, NULL);
}

static void storage_init(void)
{
    WCHAR local_appdata[512] = L"C:\\users\\Public\\AppData\\Local";
    WCHAR temp_path[512]     = L"C:\\Windows\\Temp";
    WCHAR family[256]        = L"UnknownApp_xxxx";

    GetEnvironmentVariableW(L"LOCALAPPDATA", local_appdata, 512);
    GetEnvironmentVariableW(L"TEMP",         temp_path,     512);

    {
        char fn[256] = {0};
        if (GetEnvironmentVariableA("ANVIL_PKG_FAMILYNAME", fn, 256))
            MultiByteToWideChar(CP_UTF8, 0, fn, -1, family, 256);
    }

    /* Initialise vtable pointers (can't use static init due to forward refs) */
    g_local_folder.lpItem    = s_item_vtbl;
    g_local_folder.lpFolder  = s_folder_vtbl;
    g_roaming_folder.lpItem  = s_item_vtbl;
    g_roaming_folder.lpFolder = s_folder_vtbl;
    g_temp_folder.lpItem     = s_item_vtbl;
    g_temp_folder.lpFolder   = s_folder_vtbl;

    /* LocalState */
    swprintf(g_local_folder.path, 512,
             L"%ls\\Packages\\%ls\\LocalState", local_appdata, family);
    /* RoamingState */
    swprintf(g_roaming_folder.path, 512,
             L"%ls\\Packages\\%ls\\RoamingState", local_appdata, family);
    /* TempState */
    swprintf(g_temp_folder.path, 512,
             L"%ls\\Packages\\%ls\\TempState", temp_path, family);

    mkdir_chain(g_local_folder.path);
    mkdir_chain(g_roaming_folder.path);
    mkdir_chain(g_temp_folder.path);

    SLOG("storage_init: LocalState   = %ls", g_local_folder.path);
    SLOG("storage_init: RoamingState = %ls", g_roaming_folder.path);
    SLOG("storage_init: TempState    = %ls", g_temp_folder.path);
}

/* =========================================================================
 * Registration — called from xaml_stubs.c anvil_register_all_factories()
 * Overwrites the P2 stub for Windows.Storage.ApplicationData.
 * ========================================================================= */

void anvil_register_storage_factories(void)
{
    storage_init();
    anvil_register_factory(L"Windows.Storage.ApplicationData", &s_appdata_factory);
    SLOG("anvil_register_storage_factories: ApplicationData registered");
}

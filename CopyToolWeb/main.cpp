#define CINTERFACE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <objbase.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>

#include "WebView2.h"
#include "webui_res.h"
#ifndef IDI_APP_ICON
#define IDI_APP_ICON 101
#endif


typedef HRESULT (WINAPI *PFN_CreateCoreWebView2EnvironmentWithOptions)(
    PCWSTR browserExecutableFolder,
    PCWSTR userDataFolder,
    ICoreWebView2EnvironmentOptions* environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* environmentCreatedHandler);

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif


//copycore.dll动态接口
typedef BOOL  (__cdecl *CC_Init)(void);
typedef void  (__cdecl *CC_Shutdown)(void);
typedef BOOL  (__cdecl *CC_Start)(const wchar_t*, const wchar_t*);
typedef void  (__cdecl *CC_Pause)(void);
typedef void  (__cdecl *CC_Resume)(void);
typedef void  (__cdecl *CC_Stop)(void);
typedef void  (__cdecl *CC_TogglePause)(void);
typedef void  (__cdecl *CC_GetStatus)(int*,int*,int*,double*,long long*,long long*,
                                      int*,int*,int*,int*,int*,wchar_t*,int);
typedef int   (__cdecl *CC_GetFailList)(wchar_t*, int);
typedef void  (__cdecl *CC_SetInt)(int);
typedef int   (__cdecl *CC_GetInt)(void);
typedef void  (__cdecl *CC_SetPath)(const wchar_t*);
typedef void  (__cdecl *CC_GetPath)(wchar_t*, int);

static HMODULE g_coreDll = NULL;
static CC_Init       cc_Init = NULL;
static CC_Shutdown   cc_Shutdown = NULL;
static CC_Start      cc_Start = NULL;
static CC_Pause      cc_Pause = NULL;
static CC_Resume     cc_Resume = NULL;
static CC_Stop       cc_Stop = NULL;
static CC_GetStatus  cc_GetStatus = NULL;
static CC_GetFailList cc_GetFailList = NULL;
static CC_SetInt     cc_SetSpeedLimit = NULL, cc_SetVerify = NULL,
                     cc_SetMode = NULL, cc_SetStrategy = NULL, cc_SetLanguage = NULL;
static CC_GetInt     cc_GetSpeedLimit = NULL, cc_GetVerify = NULL, cc_GetMode = NULL,
                     cc_GetStrategy = NULL, cc_GetLanguage = NULL;
static CC_SetPath    cc_SetSrcPath = NULL, cc_SetDstPath = NULL;
static CC_GetPath    cc_GetSrcPath = NULL, cc_GetDstPath = NULL;


static HWND g_hwnd = NULL;
static ICoreWebView2Controller*      g_controller = NULL;
static ICoreWebView2*                g_webview    = NULL;
static ICoreWebView2Environment*     g_env        = NULL;
static HMODULE g_loaderDll = NULL;
static std::wstring g_exeDir;
static std::wstring g_webUiDir;
static std::wstring g_userDataDir;
static std::wstring g_configPath;   // exe 同目录 config.ini（宿主侧持久化）

static int  g_prevCopying = 0;
static bool g_hostStopRequested = false;
static bool g_disableStatusPost = false;

static const UINT_PTR TIMER_STATUS = 1;

#ifdef _DEBUG
static FILE* g_logFile = NULL;

static void HostLog(const wchar_t* fmt, ...) {
    if (!g_logFile) {
        wchar_t exePath[MAX_PATH] = {0};
        GetModuleFileNameW(NULL, exePath, MAX_PATH);
        std::wstring p(exePath);
        size_t slash = p.find_last_of(L"\\/");
        std::wstring logPath = p.substr(0, slash) + L"\\host_log.txt";
        _wfopen_s(&g_logFile, logPath.c_str(), L"a");
        if (!g_logFile) return;
    }
    va_list ap; va_start(ap, fmt);
    fwprintf(g_logFile, L"[%lu] ", GetTickCount64());
    vfwprintf(g_logFile, fmt, ap);
    fwprintf(g_logFile, L"\n");
    fflush(g_logFile);
    va_end(ap);
}
#else
static void HostLog(const wchar_t*, ...) {}
#endif

static std::wstring ExeDir() {
    wchar_t buf[MAX_PATH] = {0};
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    std::wstring p(buf);
    size_t pos = p.find_last_of(L"\\/");
    return p.substr(0, pos);
}

static std::wstring JsonEscape(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 16);
    for (wchar_t c : s) {
        switch (c) {
            case L'"':  out += L"\\\""; break;
            case L'\\': out += L"\\\\"; break;
            case L'\n': out += L"\\n"; break;
            case L'\r': out += L"\\r"; break;
            case L'\t': out += L"\\t"; break;
            default:
                if (c < 0x20) {
                    wchar_t buf[8];
                    swprintf(buf, 8, L"\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

static void PostJson(const std::wstring& json) {
    if (g_webview) g_webview->lpVtbl->PostWebMessageAsJson(g_webview, json.c_str());
}

static bool JsonGetString(const std::wstring& j, const std::wstring& key,
                          std::wstring& out) {
    std::wstring pat = L"\"" + key + L"\"";
    size_t k = j.find(pat);
    if (k == std::wstring::npos) return false;
    k = j.find(L':', k + pat.size());
    if (k == std::wstring::npos) return false;
    k = j.find(L'"', k + 1);
    if (k == std::wstring::npos) return false;
    out.clear();
    for (size_t i = k + 1; i < j.size(); ++i) {
        wchar_t c = j[i];
        if (c == L'"') return true;
        if (c == L'\\' && i + 1 < j.size()) {
            wchar_t n = j[++i];
            switch (n) {
                case L'n': out += L'\n'; break;
                case L'r': out += L'\r'; break;
                case L't': out += L'\t'; break;
                case L'"': out += L'"'; break;
                case L'\\': out += L'\\'; break;
                case L'/': out += L'/'; break;
                case L'u': {
                    if (i + 4 < j.size()) {
                        std::wstring hex = j.substr(i + 1, 4);
                        out += (wchar_t)wcstoul(hex.c_str(), NULL, 16);
                        i += 4;
                    }
                    break;
                }
                default: out += n;
            }
        } else {
            out += c;
        }
    }
    return false;
}

static bool JsonGetInt(const std::wstring& j, const std::wstring& key, int& out) {
    std::wstring pat = L"\"" + key + L"\"";
    size_t k = j.find(pat);
    if (k == std::wstring::npos) return false;
    k = j.find(L':', k + pat.size());
    if (k == std::wstring::npos) return false;
    ++k;
    while (k < j.size() && (j[k] == L' ' || j[k] == L'\t')) ++k;
    int sign = 1;
    if (k < j.size() && j[k] == L'-') { sign = -1; ++k; }
    int v = 0; bool any = false;
    while (k < j.size() && j[k] >= L'0' && j[k] <= L'9') {
        v = v * 10 + (j[k] - L'0'); ++k; any = true;
    }
    if (!any) return false;
    out = v * sign;
    return true;
}


struct EmbeddedAsset { const wchar_t* path; int id; const wchar_t* mime; };
static const EmbeddedAsset g_assets[] = {
    { L"/index.html",                     IDR_WEBUI_INDEX,        L"text/html; charset=utf-8" },
    { L"/renderer.js",                    IDR_WEBUI_RENDERER_JS,  L"application/javascript; charset=utf-8" },
    { L"/copytool-bridge.js",             IDR_WEBUI_BRIDGE_JS,    L"application/javascript; charset=utf-8" },
    { L"/ui.js",                          IDR_WEBUI_UI_JS,        L"application/javascript; charset=utf-8" },
    { L"/css/styles.css",                 IDR_WEBUI_STYLES_CSS,   L"text/css; charset=utf-8" },
    { L"/css/materials.css",              IDR_WEBUI_MATERIALS_CSS,L"text/css; charset=utf-8" },
    /* WebGL 已下线：desktop.js / shaders/*.glsl 不再嵌入 */
};

static const EmbeddedAsset* FindAsset(const std::wstring& path) {
    for (const auto& a : g_assets)
        if (path == a.path) return &a;
    return NULL;
}

static bool GetResourceBytes(int id, const BYTE** outData, DWORD* outSize) {
    HRSRC h = FindResourceW(NULL, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!h) return false;
    HGLOBAL hg = LoadResource(NULL, h);
    if (!hg) return false;
    void* p = LockResource(hg);
    if (!p) return false;
    *outData = (const BYTE*)p;
    *outSize = SizeofResource(NULL, h);
    return true;
}

//只读内存IStream
struct MemStream {
    const IStreamVtbl* lpVtbl;
    LONG   ref;
    const BYTE* data;
    ULONG  size;
    ULONG  pos;
};
static HRESULT WINAPI MS_QueryInterface(IStream* This, REFIID riid, void** pp) {
    if (!pp) return E_POINTER;
    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ISequentialStream) ||
        IsEqualIID(riid, IID_IStream)) {
        *pp = This; This->lpVtbl->AddRef(This); return S_OK;
    }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI MS_AddRef(IStream* This) {
    return InterlockedIncrement(&((MemStream*)This)->ref);
}
static ULONG WINAPI MS_Release(IStream* This) {
    LONG r = InterlockedDecrement(&((MemStream*)This)->ref);
    if (!r) free((MemStream*)This);
    return (ULONG)r;
}
static HRESULT WINAPI MS_Read(IStream* This, void* pv, ULONG cb, ULONG* read) {
    MemStream* m = (MemStream*)This;
    ULONG avail = (m->pos < m->size) ? (m->size - m->pos) : 0;
    ULONG n = (cb < avail) ? cb : avail;
    if (n) memcpy(pv, m->data + m->pos, n);
    m->pos += n;
    if (read) *read = n;
    return (n == cb) ? S_OK : S_FALSE;
}
static HRESULT WINAPI MS_Write(IStream*, const void*, ULONG, ULONG*) { return E_NOTIMPL; }
static HRESULT WINAPI MS_Seek(IStream* This, LARGE_INTEGER d, DWORD origin, ULARGE_INTEGER* np) {
    MemStream* m = (MemStream*)This;
    LONGLONG base;
    switch (origin) {
        case STREAM_SEEK_SET: base = 0; break;
        case STREAM_SEEK_CUR: base = (LONGLONG)m->pos; break;
        case STREAM_SEEK_END: base = (LONGLONG)m->size; break;
        default: return E_INVALIDARG;
    }
    LONGLONG v = base + d.QuadPart;
    if (v < 0 || v > (LONGLONG)m->size) return E_INVALIDARG;
    m->pos = (ULONG)v;
    if (np) np->QuadPart = (ULONGLONG)v;
    return S_OK;
}
static HRESULT WINAPI MS_SetSize(IStream*, ULARGE_INTEGER) { return E_NOTIMPL; }
static HRESULT WINAPI MS_CopyTo(IStream*, IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*) { return E_NOTIMPL; }
static HRESULT WINAPI MS_Commit(IStream*, DWORD) { return S_OK; }
static HRESULT WINAPI MS_Revert(IStream*) { return S_OK; }
static HRESULT WINAPI MS_LockRegion(IStream*, ULARGE_INTEGER, ULARGE_INTEGER, DWORD) { return E_NOTIMPL; }
static HRESULT WINAPI MS_UnlockRegion(IStream*, ULARGE_INTEGER, ULARGE_INTEGER, DWORD) { return E_NOTIMPL; }
static HRESULT WINAPI MS_Stat(IStream* This, STATSTG* s, DWORD) {
    MemStream* m = (MemStream*)This;
    if (!s) return E_POINTER;
    ZeroMemory(s, sizeof(*s));
    s->type = STGTY_STREAM;
    s->cbSize.QuadPart = m->size;
    s->grfMode = STGM_READ;
    s->clsid = GUID_NULL;
    return S_OK;
}
static HRESULT WINAPI MS_Clone(IStream*, IStream**) { return E_NOTIMPL; }

static IStream* NewMemStream(const BYTE* data, ULONG size) {
    static const IStreamVtbl vt = {
        MS_QueryInterface, MS_AddRef, MS_Release,
        MS_Read, MS_Write,
        MS_Seek, MS_SetSize, MS_CopyTo, MS_Commit, MS_Revert,
        MS_LockRegion, MS_UnlockRegion, MS_Stat, MS_Clone
    };
    MemStream* m = (MemStream*)calloc(1, sizeof(MemStream));
    if (!m) return NULL;
    m->lpVtbl = &vt; m->ref = 1; m->data = data; m->size = size; m->pos = 0;
    return (IStream*)m;
}


struct EnvHandler;
static HRESULT WINAPI Env_QueryInterface(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*, REFIID, void**);
static ULONG   WINAPI Env_AddRef(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
static ULONG   WINAPI Env_Release(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
static HRESULT WINAPI Env_Invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*, HRESULT, ICoreWebView2Environment*);

struct EnvHandler : ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    ULONG ref;
    EnvHandler() : ref(1) {
        static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl vt = {
            Env_QueryInterface, Env_AddRef, Env_Release, Env_Invoke
        };
        lpVtbl = &vt;
    }
};

struct CtrlHandler;
static HRESULT WINAPI Ctrl_QueryInterface(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*, REFIID, void**);
static ULONG   WINAPI Ctrl_AddRef(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*);
static ULONG   WINAPI Ctrl_Release(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*);
static HRESULT WINAPI Ctrl_Invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*, HRESULT, ICoreWebView2Controller*);

struct CtrlHandler : ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    ULONG ref;
    CtrlHandler() : ref(1) {
        static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl vt = {
            Ctrl_QueryInterface, Ctrl_AddRef, Ctrl_Release, Ctrl_Invoke
        };
        lpVtbl = &vt;
    }
};

struct MsgHandler;
static HRESULT WINAPI Msg_QueryInterface(ICoreWebView2WebMessageReceivedEventHandler*, REFIID, void**);
static ULONG   WINAPI Msg_AddRef(ICoreWebView2WebMessageReceivedEventHandler*);
static ULONG   WINAPI Msg_Release(ICoreWebView2WebMessageReceivedEventHandler*);
static HRESULT WINAPI Msg_Invoke(ICoreWebView2WebMessageReceivedEventHandler*, ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*);

struct MsgHandler : ICoreWebView2WebMessageReceivedEventHandler {
    ULONG ref;
    MsgHandler() : ref(1) {
        static ICoreWebView2WebMessageReceivedEventHandlerVtbl vt = {
            Msg_QueryInterface, Msg_AddRef, Msg_Release, Msg_Invoke
        };
        lpVtbl = &vt;
    }
};

struct NavHandler;
static HRESULT WINAPI Nav_QueryInterface(ICoreWebView2NavigationCompletedEventHandler*, REFIID, void**);
static ULONG   WINAPI Nav_AddRef(ICoreWebView2NavigationCompletedEventHandler*);
static ULONG   WINAPI Nav_Release(ICoreWebView2NavigationCompletedEventHandler*);
static HRESULT WINAPI Nav_Invoke(ICoreWebView2NavigationCompletedEventHandler*, ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*);

struct NavHandler : ICoreWebView2NavigationCompletedEventHandler {
    ULONG ref;
    NavHandler() : ref(1) {
        static ICoreWebView2NavigationCompletedEventHandlerVtbl vt = {
            Nav_QueryInterface, Nav_AddRef, Nav_Release, Nav_Invoke
        };
        lpVtbl = &vt;
    }
};

struct ResHandler;
static HRESULT WINAPI Res_QueryInterface(ICoreWebView2WebResourceRequestedEventHandler*, REFIID, void**);
static ULONG   WINAPI Res_AddRef(ICoreWebView2WebResourceRequestedEventHandler*);
static ULONG   WINAPI Res_Release(ICoreWebView2WebResourceRequestedEventHandler*);
static HRESULT WINAPI Res_Invoke(ICoreWebView2WebResourceRequestedEventHandler*, ICoreWebView2*,
                                 ICoreWebView2WebResourceRequestedEventArgs*);

struct ResHandler : ICoreWebView2WebResourceRequestedEventHandler {
    ULONG ref;
    ResHandler() : ref(1) {
        static ICoreWebView2WebResourceRequestedEventHandlerVtbl vt = {
            Res_QueryInterface, Res_AddRef, Res_Release, Res_Invoke
        };
        lpVtbl = &vt;
    }
};

struct StdRefLayout { const void* vt; ULONG ref; };
static HRESULT StdQI(void* p, REFIID riid, void** ppv, REFGUID myIid) {
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, myIid)) {
        *ppv = p;
        ((IUnknown*)*ppv)->lpVtbl->AddRef((IUnknown*)*ppv);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
#define STD_HANDLER_IMPL(FnPrefix, PtrType, MyIid) \
static HRESULT WINAPI FnPrefix##_QueryInterface(PtrType* This, REFIID riid, void** ppv){ return StdQI(This, riid, ppv, MyIid); } \
static ULONG   WINAPI FnPrefix##_AddRef(PtrType* This){ return InterlockedIncrement((LONG*)&((StdRefLayout*)This)->ref); } \
static ULONG   WINAPI FnPrefix##_Release(PtrType* This){ ULONG r = InterlockedDecrement((LONG*)&((StdRefLayout*)This)->ref); if(!r) delete This; return r; }

STD_HANDLER_IMPL(Env,  ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)
STD_HANDLER_IMPL(Ctrl, ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)
STD_HANDLER_IMPL(Msg,  ICoreWebView2WebMessageReceivedEventHandler, IID_ICoreWebView2WebMessageReceivedEventHandler)
STD_HANDLER_IMPL(Nav,  ICoreWebView2NavigationCompletedEventHandler, IID_ICoreWebView2NavigationCompletedEventHandler)
STD_HANDLER_IMPL(Res,  ICoreWebView2WebResourceRequestedEventHandler, IID_ICoreWebView2WebResourceRequestedEventHandler)

// ---- WebResourceRequested：命中内嵌资源就从内存回 200 ----
static HRESULT WINAPI Res_Invoke(ICoreWebView2WebResourceRequestedEventHandler* This,
                                 ICoreWebView2* sender,
                                 ICoreWebView2WebResourceRequestedEventArgs* args) {
    (void)This; (void)sender;
    if (!args || !g_env) return S_OK;

    ICoreWebView2WebResourceRequest* req = NULL;
    if (FAILED(args->lpVtbl->get_Request(args, &req)) || !req) return S_OK;
    LPWSTR uri = NULL;
    if (FAILED(req->lpVtbl->get_Uri(req, &uri)) || !uri) {
        if (req) req->lpVtbl->Release(req);
        return S_OK;
    }

    std::wstring u(uri);
    std::wstring path = u;
    size_t scheme = u.find(L"://");
    if (scheme != std::wstring::npos) {
        size_t slash = u.find(L'/', scheme + 3);
        path = (slash == std::wstring::npos) ? L"/" : u.substr(slash);
    }
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == L'?' || path[i] == L'#') { path.resize(i); break; }
    }
    if (path == L"/") path = L"/index.html";

    const EmbeddedAsset* a = FindAsset(path);
    if (a) {
        const BYTE* data = NULL; DWORD size = 0;
        if (GetResourceBytes(a->id, &data, &size)) {
            IStream* stm = NewMemStream(data, size);
            std::wstring hdrs = std::wstring(L"Content-Type: ") + a->mime +
                L"\r\nAccess-Control-Allow-Origin: *\r\nCache-Control: no-cache\r\n";
            ICoreWebView2WebResourceResponse* resp = NULL;
            HRESULT hr = g_env->lpVtbl->CreateWebResourceResponse(
                g_env, stm, 200, L"OK", hdrs.c_str(), &resp);
            if (SUCCEEDED(hr) && resp) {
                args->lpVtbl->put_Response(args, resp);
                resp->lpVtbl->Release(resp);
                HostLog(L"asset served: %ls (%lu bytes)", path.c_str(), (unsigned long)size);
            } else {
                HostLog(L"CreateWebResourceResponse failed 0x%08X path=%ls",
                        (unsigned)hr, path.c_str());
            }
            if (stm) stm->lpVtbl->Release(stm);
        } else {
            HostLog(L"embedded resource missing: %ls id=%d", path.c_str(), a->id);
        }
    }
    CoTaskMemFree(uri);
    req->lpVtbl->Release(req);
    return S_OK;
}

static std::wstring BuildStatusJson() {
    int isCopying=0,isPaused=0,pct=0,copiedFiles=0,totalFiles=0,failed=0,scanned=0,scanDone=0;
    double speed=0; long long copiedBytes=0,totalBytes=0;
    wchar_t status[256]=L"";
    cc_GetStatus(&isCopying,&isPaused,&pct,&speed,&copiedBytes,&totalBytes,
                 &copiedFiles,&totalFiles,&failed,&scanned,&scanDone,status,256);
    wchar_t buf[1024];
    swprintf(buf, 1024,
        L"{\"type\":\"status\",\"isCopying\":%d,\"isPaused\":%d,"
        L"\"progressPct\":%d,\"speed\":%.2f,"
        L"\"copiedBytes\":%lld,\"totalBytes\":%lld,"
        L"\"copiedFiles\":%d,\"totalFiles\":%d,\"failedFiles\":%d,"
        L"\"scanned\":%d,\"scanDone\":%d,"
        L"\"statusText\":\"%ls\"}",
        isCopying, isPaused, pct, speed,
        copiedBytes, totalBytes,
        copiedFiles, totalFiles, failed,
        scanned, scanDone,
        JsonEscape(status).c_str());

    return std::wstring(buf);
}

// ------------------------------------------------------------
// 动态加载 CopyCore.dll
// ------------------------------------------------------------
static bool LoadCoreDll() {
    if (g_coreDll) return true;
    std::wstring dll = g_exeDir + L"\\CopyCore.dll";
    g_coreDll = LoadLibraryW(dll.c_str());
    if (!g_coreDll) {
        HostLog(L"LoadLibrary CopyCore.dll failed err=%lu path=%ls", GetLastError(), dll.c_str());
        return false;
    }
    #define GETPROC(var, type, name) \
        var = (type)GetProcAddress(g_coreDll, name); \
        if (!var) { HostLog(L"GetProcAddress %hs failed", name); }
    GETPROC(cc_Init,        CC_Init,        "CopyCore_Init")
    GETPROC(cc_Shutdown,    CC_Shutdown,    "CopyCore_Shutdown")
    GETPROC(cc_Start,       CC_Start,       "CopyCore_Start")
    GETPROC(cc_Pause,       CC_Pause,       "CopyCore_Pause")
    GETPROC(cc_Resume,      CC_Resume,      "CopyCore_Resume")
    GETPROC(cc_Stop,        CC_Stop,        "CopyCore_Stop")
    GETPROC(cc_GetStatus,   CC_GetStatus,   "CopyCore_GetStatus")
    GETPROC(cc_GetFailList, CC_GetFailList, "CopyCore_GetFailList")
    GETPROC(cc_SetSpeedLimit, CC_SetInt,    "CopyCore_SetSpeedLimit")
    GETPROC(cc_SetVerify,     CC_SetInt,    "CopyCore_SetVerify")
    GETPROC(cc_SetMode,       CC_SetInt,    "CopyCore_SetMode")
    GETPROC(cc_SetStrategy,   CC_SetInt,    "CopyCore_SetStrategy")
    GETPROC(cc_SetLanguage,   CC_SetInt,    "CopyCore_SetLanguage")
    GETPROC(cc_GetSpeedLimit, CC_GetInt,    "CopyCore_GetSpeedLimit")
    GETPROC(cc_GetVerify,     CC_GetInt,    "CopyCore_GetVerify")
    GETPROC(cc_GetMode,       CC_GetInt,    "CopyCore_GetMode")
    GETPROC(cc_GetStrategy,   CC_GetInt,    "CopyCore_GetStrategy")
    GETPROC(cc_GetLanguage,   CC_GetInt,    "CopyCore_GetLanguage")
    GETPROC(cc_SetSrcPath,    CC_SetPath,   "CopyCore_SetSrcPath")
    GETPROC(cc_SetDstPath,    CC_SetPath,   "CopyCore_SetDstPath")
    GETPROC(cc_GetSrcPath,    CC_GetPath,   "CopyCore_GetSrcPath")
    GETPROC(cc_GetDstPath,    CC_GetPath,   "CopyCore_GetDstPath")
    #undef GETPROC

    if (cc_Init && !cc_Init()) {
        HostLog(L"CC_Init returned FALSE");
        return false;
    }
    return true;
}

static void UnloadCoreDll() {
    if (cc_Shutdown) cc_Shutdown();
    if (g_coreDll) { FreeLibrary(g_coreDll); g_coreDll = NULL; }
    cc_Init = NULL; cc_Shutdown = NULL; cc_Start = NULL;
    cc_Pause = NULL; cc_Resume = NULL; cc_Stop = NULL;
    cc_GetStatus = NULL; cc_GetFailList = NULL;
    cc_SetSpeedLimit = NULL; cc_SetVerify = NULL; cc_SetMode = NULL;
    cc_SetStrategy = NULL; cc_SetLanguage = NULL;
    cc_GetSpeedLimit = NULL; cc_GetVerify = NULL; cc_GetMode = NULL;
    cc_GetStrategy = NULL; cc_GetLanguage = NULL;
    cc_SetSrcPath = NULL; cc_SetDstPath = NULL;
    cc_GetSrcPath = NULL; cc_GetDstPath = NULL;
}

// ------------------------------------------------------------
// 宿主侧配置持久化（内核 CopyCore 保持无状态，读写全在宿主层）
//
// 存 exe 同目录 config.ini，只存 4 个字段：
//   Language / Strategy / Verify / SpeedLimit
//
// 不存 Src/Dst 路径 —— 路径是任务级的，跨启动保留会让用户
// 每次开程序都看到一个陈旧的目标目录，是负体验。
//
// 时机：启动读一次、退出写一次。崩溃/强杀会丢最后一次改动，
// 但对这几个低频的用户配置项来说可以接受。
// ------------------------------------------------------------
static void SaveConfigToIni() {
    if (g_configPath.empty() || !g_coreDll) return;

    auto wInt = [](const wchar_t* sec, const wchar_t* key, int v) {
        wchar_t buf[32];
        swprintf(buf, 32, L"%d", v);
        WritePrivateProfileStringW(sec, key, buf, g_configPath.c_str());
    };

    wInt(L"General", L"Language",   cc_GetLanguage   ? cc_GetLanguage()   : 0);
    wInt(L"General", L"Strategy",   cc_GetStrategy   ? cc_GetStrategy()   : 0);
    wInt(L"General", L"Verify",     cc_GetVerify     ? cc_GetVerify()     : 0);
    wInt(L"General", L"SpeedLimit", cc_GetSpeedLimit ? cc_GetSpeedLimit() : 0);

    HostLog(L"config saved: %ls", g_configPath.c_str());
}

static void LoadConfigFromIni() {
    if (g_configPath.empty() || !g_coreDll) return;

    if (GetFileAttributesW(g_configPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        HostLog(L"no config.ini, using defaults");
        return;
    }

    auto rInt = [](const wchar_t* sec, const wchar_t* key, int def) {
        return (int)GetPrivateProfileIntW(sec, key, def, g_configPath.c_str());
    };

    int lang     = rInt(L"General", L"Language",   0);
    int strategy = rInt(L"General", L"Strategy",   0);
    int verify   = rInt(L"General", L"Verify",     0);
    int speed    = rInt(L"General", L"SpeedLimit", 0);

    if (cc_SetLanguage)   cc_SetLanguage(lang);
    if (cc_SetStrategy)   cc_SetStrategy(strategy);
    if (cc_SetVerify)     cc_SetVerify(verify);
    if (cc_SetSpeedLimit) cc_SetSpeedLimit(speed);

    HostLog(L"config loaded: lang=%d strategy=%d verify=%d speed=%d",
            lang, strategy, verify, speed);
}

// ------------------------------------------------------------
// 宿主自身少量文案的本地化（语言编号与内核一致：0简中 1繁中 2英 3韩 4日）
// ------------------------------------------------------------
static const wchar_t* HostL(int kind) {
    static const wchar_t* kBrowse[5] = {
        L"选择文件夹", L"選擇資料夾", L"Select Folder",
        L"폴더 선택", L"フォルダーを選択" };
    static const wchar_t* kCoreFail[5] = {
        L"CopyCore.dll 加载失败", L"CopyCore.dll 載入失敗", L"Failed to load CopyCore.dll",
        L"CopyCore.dll 로드 실패", L"CopyCore.dll の読み込みに失敗しました" };
    static const wchar_t* kWvFail[5] = {
        L"WebView2 初始化失败", L"WebView2 初始化失敗", L"WebView2 initialization failed",
        L"WebView2 초기화 실패", L"WebView2 の初期化に失敗しました" };
    static const wchar_t* kFilterFail[5] = {
        L"WebView2 资源拦截注册失败", L"WebView2 資源攔截註冊失敗",
        L"WebView2 resource filter registration failed",
        L"WebView2 리소스 필터 등록 실패", L"WebView2 リソースフィルタの登録に失敗しました" };
    const wchar_t** arr = NULL;
    if (kind == 0) arr = kBrowse;
    else if (kind == 1) arr = kCoreFail;
    else if (kind == 2) arr = kWvFail;
    else if (kind == 3) arr = kFilterFail;
    int lang = cc_GetLanguage ? cc_GetLanguage() : 0;
    if (lang < 0 || lang > 4) lang = 0;
    return arr ? arr[lang] : L"";
}

// ------------------------------------------------------------
// config JSON
// ------------------------------------------------------------
static std::wstring BuildConfigJson() {
    wchar_t src[1024] = L"", dst[1024] = L"";
    if (cc_GetSrcPath) cc_GetSrcPath(src, 1024);
    if (cc_GetDstPath) cc_GetDstPath(dst, 1024);
    int strategy = cc_GetStrategy ? cc_GetStrategy() : 0;
    int verify   = cc_GetVerify   ? cc_GetVerify()   : 0;
    int lang     = cc_GetLanguage ? cc_GetLanguage() : 0;
    int speed    = cc_GetSpeedLimit ? cc_GetSpeedLimit() : 0;

    wchar_t buf[2048];
    swprintf(buf, 2048,
        L"{\"type\":\"config\",\"src\":\"%ls\",\"dst\":\"%ls\","
        L"\"strategy\":%d,\"verify\":%d,\"lang\":%d,\"speed\":%d}",
        JsonEscape(src).c_str(), JsonEscape(dst).c_str(),
        strategy, verify, lang, speed);
    return std::wstring(buf);
}

// ------------------------------------------------------------
// finished JSON（含失败列表）
// ------------------------------------------------------------
static std::wstring BuildFinishedJson(bool wasStopped) {
    int isCopying=0,isPaused=0,pct=0,copiedFiles=0,totalFiles=0,failed=0,scanned=0,scanDone=0;
    double speed=0; long long copiedBytes=0,totalBytes=0;
    wchar_t status[256]=L"";
    if (cc_GetStatus)
        cc_GetStatus(&isCopying,&isPaused,&pct,&speed,&copiedBytes,&totalBytes,
                     &copiedFiles,&totalFiles,&failed,&scanned,&scanDone,status,256);

    // MAX_FAIL_LIST=256，每条最长 MAX_PATH+32 ≈ 300 字符，
    // 按 64KB 预留，覆盖满列表且留足 JSON 转义冗余。
    std::wstring failList;
    if (cc_GetFailList) {
        const int TMP_LEN = 65536;
        std::vector<wchar_t> tmp(TMP_LEN, 0);
        int n = cc_GetFailList(tmp.data(), TMP_LEN);
        HostLog(L"GetFailList: n=%d, content_len=%d", n, (int)wcslen(tmp.data()));
        if (n > 0) failList = tmp.data();
    }

    std::wstring json;
    json.reserve(131072);
    wchar_t buf[512];
    swprintf(buf, 512,
        L"{\"type\":\"finished\",\"wasStopped\":%ls,"
        L"\"totalFiles\":%d,\"failedFiles\":%d,\"verifyFailed\":%d,"
        L"\"failList\":\"",
        wasStopped ? L"true" : L"false",
        totalFiles, failed, 0);
    json += buf;
    json += JsonEscape(failList);
    json += L"\"}";
    return json;
}

// ------------------------------------------------------------
// browse：SHBrowseForFolderW
// ------------------------------------------------------------
static void HandleBrowse(const std::wstring& target) {
    BROWSEINFOW bi = {0};
    bi.hwndOwner = g_hwnd;
    bi.lpszTitle = HostL(0);
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t path[MAX_PATH] = L"";
    if (SHGetPathFromIDListW(pidl, path)) {
        // 先把路径写入复制内核
        if (target == L"src" && cc_SetSrcPath) cc_SetSrcPath(path);
        else if (target == L"dst" && cc_SetDstPath) cc_SetDstPath(path);
        // 再回推完整 config：前端 onConfig 会把 src/dst 回填到输入框
        // （旧实现发 type:"path"，但桥接层没有对应处理器，选完文件夹
        //  输入框一直为空，看起来像"选不上"）
        PostJson(BuildConfigJson());
    }
    CoTaskMemFree(pidl);
}

// ------------------------------------------------------------
// HandleWebMessage：分发 JS 命令
// ------------------------------------------------------------
static void HandleWebMessage(const std::wstring& json) {
    std::wstring cmd;
    if (!JsonGetString(json, L"cmd", cmd)) return;
    HostLog(L"cmd: %ls", cmd.c_str());

    if (cmd == L"getConfig") {
        PostJson(BuildConfigJson());
    } else if (cmd == L"browse") {
        std::wstring target;
        JsonGetString(json, L"target", target);
        HandleBrowse(target);
    } else if (cmd == L"setSrcPath") {
        std::wstring v; JsonGetString(json, L"value", v);
        if (cc_SetSrcPath) cc_SetSrcPath(v.c_str());
        PostJson(BuildConfigJson());
    } else if (cmd == L"setDstPath") {
        std::wstring v; JsonGetString(json, L"value", v);
        if (cc_SetDstPath) cc_SetDstPath(v.c_str());
        PostJson(BuildConfigJson());
    } else if (cmd == L"start") {
        // 拦截在复制中的重复点击，防止第二次 Start 清空第一次的失败列表
        int isCopying=0,isPaused=0,pct=0,cf=0,tf=0,ff=0,sc=0,sd=0;
        double sp=0; long long cb=0,tb=0; wchar_t st[256]=L"";
        if (cc_GetStatus)
            cc_GetStatus(&isCopying,&isPaused,&pct,&sp,&cb,&tb,
                         &cf,&tf,&ff,&sc,&sd,st,256);
        if (isCopying) {
            HostLog(L"start ignored: already copying");
            return;
        }
        wchar_t src[1024] = L"", dst[1024] = L"";
        if (cc_GetSrcPath) cc_GetSrcPath(src, 1024);
        if (cc_GetDstPath) cc_GetDstPath(dst, 1024);
        if (cc_Start) {
            BOOL ok = cc_Start(src, dst);
            HostLog(L"cc_Start src=%ls dst=%ls => %d", src, dst, (int)ok);
            if (!ok) {
                // failList 约定为 "错误码|路径\n"；1000 是宿主自定义的
                // "源或目标无效"，前端按当前语言翻译。
                PostJson(L"{\"type\":\"finished\",\"wasStopped\":false,"
                         L"\"totalFiles\":0,\"failedFiles\":1,\"verifyFailed\":0,"
                         L"\"failList\":\"1000|\"}");
            } else {
                // 内核的"断点跳过"会让一次任务在毫秒级完成（目标文件
                // 大小/时间一致则秒过），可能整个生命周期都落在两次 500ms
                // 状态轮询之间，边沿检测漏掉完成事件、前端永远不弹完成框。
                // 这里预置边沿，保证下一拍必发 finished。
                g_hostStopRequested = false;
                g_prevCopying = 1;
            }
        } else {
            HostLog(L"cc_Start function pointer NULL");
        }
    } else if (cmd == L"pause") {
        // 前端的按钮文案在"暂停/继续"间切换，但始终发 cmd:"pause"。
        // 这里按内核当前状态做切换：已暂停→Resume，否则→Pause。
        int isCopying=0,isPaused=0,pct=0,cf=0,tf=0,ff=0,sc=0,sd=0;
        double sp=0; long long cb=0,tb=0; wchar_t st[256]=L"";
        if (cc_GetStatus)
            cc_GetStatus(&isCopying,&isPaused,&pct,&sp,&cb,&tb,
                         &cf,&tf,&ff,&sc,&sd,st,256);
        if (isPaused) { if (cc_Resume) cc_Resume(); }
        else          { if (cc_Pause)  cc_Pause();  }
    } else if (cmd == L"resume") {
        if (cc_Resume) cc_Resume();
    } else if (cmd == L"stop") {
        g_hostStopRequested = true;
        if (cc_Stop) cc_Stop();
    } else if (cmd == L"setStrategy") {
        int v = 0; JsonGetInt(json, L"value", v);
        if (cc_SetStrategy) cc_SetStrategy(v);
    } else if (cmd == L"setVerify") {
        int v = 0; JsonGetInt(json, L"value", v);
        if (cc_SetVerify) cc_SetVerify(v);
    } else if (cmd == L"setSpeedLimit") {
        // 前端单位 unit：0=KB/s 1=MB/s(默认) 2=GB/s；
        // 内核 CopyCore_SetSpeedLimit 只接受 KB/s（int），必须换算。
        int v = 0, unit = 1;
        JsonGetInt(json, L"value", v);
        JsonGetInt(json, L"unit", unit);
        if (v < 0) v = 0;
        long long kb = v;
        if (unit == 1) kb = (long long)v * 1024LL;
        else if (unit == 2) kb = (long long)v * 1024LL * 1024LL;
        if (kb > 2147483647LL) kb = 2147483647LL;
        if (cc_SetSpeedLimit) cc_SetSpeedLimit((int)kb);
    } else if (cmd == L"setLang") {
        int v = 0; JsonGetInt(json, L"value", v);
        if (cc_SetLanguage) cc_SetLanguage(v);
        // 回推 config：前端缓存"已保存值"，"取消"才能正确回退
        PostJson(BuildConfigJson());
    }
}

// ------------------------------------------------------------
// Msg_Invoke：WebView2 消息入口
// ------------------------------------------------------------
static HRESULT WINAPI Msg_Invoke(ICoreWebView2WebMessageReceivedEventHandler* This,
                                 ICoreWebView2* sender,
                                 ICoreWebView2WebMessageReceivedEventArgs* args) {
    (void)This;
    if (!args) return S_OK;
    LPWSTR raw = NULL;
    if (SUCCEEDED(args->lpVtbl->TryGetWebMessageAsString(args, &raw)) && raw) {
        HandleWebMessage(raw);
        CoTaskMemFree(raw);
    }
    return S_OK;
}

// ------------------------------------------------------------
// Nav_Invoke：导航完成
// ------------------------------------------------------------
static HRESULT WINAPI Nav_Invoke(ICoreWebView2NavigationCompletedEventHandler* This,
                                 ICoreWebView2* sender,
                                 ICoreWebView2NavigationCompletedEventArgs* args) {
    (void)This; (void)sender; (void)args;
    HostLog(L"navigation completed");
    // 主动推一次 config：即使 bridge 启动时的 getConfig 与页面加载竞态，
    // 前端也一定能拿到内核当前配置（路径/语言/速度等）。
    PostJson(BuildConfigJson());
    return S_OK;
}

// ------------------------------------------------------------
// Ctrl_Invoke：WebView2 创建完成
// ------------------------------------------------------------
static HRESULT WINAPI Ctrl_Invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* This,
                                  HRESULT result,
                                  ICoreWebView2Controller* controller) {
    (void)This;
    if (FAILED(result) || !controller) {
        HostLog(L"controller creation failed 0x%08X", (unsigned)result);
        return result;
    }

    g_controller = controller;
    controller->lpVtbl->AddRef(controller);

    controller->lpVtbl->get_CoreWebView2(controller, &g_webview);
    if (!g_webview) return E_FAIL;

    HRESULT hrFilter = g_webview->lpVtbl->AddWebResourceRequestedFilter(
        g_webview, L"https://copytool.invalid/*",
        (COREWEBVIEW2_WEB_RESOURCE_CONTEXT)COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    ResHandler* rh = new ResHandler();
    EventRegistrationToken resToken;
    HRESULT hrResEvt = g_webview->lpVtbl->add_WebResourceRequested(g_webview, rh, &resToken);
    HostLog(L"AddWebResourceRequestedFilter hr=0x%08X add_WebResourceRequested hr=0x%08X webUiDir=%ls",
            (unsigned)hrFilter, (unsigned)hrResEvt, g_webUiDir.c_str());
    if (FAILED(hrFilter) || FAILED(hrResEvt)) {
        MessageBoxW(g_hwnd, HostL(3), L"CopyTool", MB_ICONERROR);
        PostQuitMessage(1);
        return E_FAIL;
    }

    ICoreWebView2Controller2* c2 = NULL;
    controller->lpVtbl->QueryInterface(controller, IID_ICoreWebView2Controller2, (void**)&c2);
    if (c2) {
        COREWEBVIEW2_COLOR transparent = {0, 0, 0, 0};
        c2->lpVtbl->put_DefaultBackgroundColor(c2, transparent);
        c2->lpVtbl->Release(c2);
    }

    RECT rc;
    GetClientRect(g_hwnd, &rc);
    controller->lpVtbl->put_Bounds(controller, rc);

    MsgHandler* mh = new MsgHandler();
    EventRegistrationToken msgToken;
    g_webview->lpVtbl->add_WebMessageReceived(g_webview, mh, &msgToken);

    NavHandler* nh = new NavHandler();
    EventRegistrationToken navToken;
    g_webview->lpVtbl->add_NavigationCompleted(g_webview, nh, &navToken);

    g_webview->lpVtbl->Navigate(g_webview, L"https://copytool.invalid/index.html");

    SetTimer(g_hwnd, TIMER_STATUS, 500, NULL);

    HostLog(L"controller ready");
    return S_OK;
}

// ------------------------------------------------------------
// Env_Invoke：环境创建完成
// ------------------------------------------------------------
static HRESULT WINAPI Env_Invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* This,
                                 HRESULT result,
                                 ICoreWebView2Environment* env) {
    (void)This;
    if (FAILED(result) || !env) {
        HostLog(L"env creation failed 0x%08X", (unsigned)result);
        return result;
    }
    g_env = env;
    env->lpVtbl->AddRef(env);

    CtrlHandler* ch = new CtrlHandler();
    env->lpVtbl->CreateCoreWebView2Controller(env, g_hwnd, ch);
    return S_OK;
}

// ------------------------------------------------------------
// 初始化 WebView2
// ------------------------------------------------------------
static bool InitWebView2() {
    std::wstring loader = g_exeDir + L"\\WebView2Loader.dll";
    g_loaderDll = LoadLibraryW(loader.c_str());
    if (!g_loaderDll) {
        HostLog(L"LoadLibrary WebView2Loader.dll failed err=%lu", GetLastError());
        return false;
    }
    auto fn = (PFN_CreateCoreWebView2EnvironmentWithOptions)
        GetProcAddress(g_loaderDll, "CreateCoreWebView2EnvironmentWithOptions");
    if (!fn) {
        HostLog(L"GetProcAddress CreateCoreWebView2EnvironmentWithOptions failed");
        return false;
    }

    EnvHandler* eh = new EnvHandler();
    HRESULT hr = fn(NULL, g_userDataDir.c_str(), NULL, eh);
    if (FAILED(hr)) {
        HostLog(L"CreateCoreWebView2EnvironmentWithOptions failed 0x%08X", (unsigned)hr);
        return false;
    }
    return true;
}

// ------------------------------------------------------------
// WndProc
// ------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:
            if (g_controller) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                g_controller->lpVtbl->put_Bounds(g_controller, rc);
            }
            return 0;

        case WM_TIMER:
            if (wp == TIMER_STATUS && cc_GetStatus && g_webview) {
                int isCopying=0;
                int isPaused=0, pct=0, cf=0, tf=0, ff=0, sc=0, sd=0;
                double sp=0; long long cb=0, tb=0;
                wchar_t st[256]=L"";
                cc_GetStatus(&isCopying,&isPaused,&pct,&sp,&cb,&tb,
                             &cf,&tf,&ff,&sc,&sd,st,256);

                if (g_prevCopying && !isCopying) {
                    PostJson(BuildFinishedJson(g_hostStopRequested));
                    g_hostStopRequested = false;
                }
                g_prevCopying = isCopying;

                if (isCopying || g_disableStatusPost == false) {
                    PostJson(BuildStatusJson());
                }
            }
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, TIMER_STATUS);
            SaveConfigToIni();       // 退出时统一落盘
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ------------------------------------------------------------
// wWinMain
// ------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)cmdLine;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    g_exeDir      = ExeDir();
    g_webUiDir    = g_exeDir + L"\\WebUI";
    g_userDataDir = g_exeDir + L"\\userdata";
    g_configPath  = g_exeDir + L"\\config.ini";

    if (!LoadCoreDll()) {
        MessageBoxW(NULL, HostL(1), L"CopyTool", MB_ICONERROR);
        return 1;
    }

    // 内核已就绪：从 config.ini 读回上次保存的配置。
    // 必须在 LoadCoreDll 之后（函数指针已就位），
    // 且在 InitWebView2 之前（页面导航完成时宿主会推一次 config，
    // 保证前端第一帧看到的就是持久化后的语言）。
    LoadConfigFromIni();

    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"CopyToolWebClass";
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"CopyToolWebClass", L"CopyTool",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 620,
        NULL, NULL, hInst, NULL);

    // ====== 把图标代码放到窗口创建完毕之后 ======
    if (g_hwnd) {
        HICON hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
        if (hIcon) {
            SendMessageW(g_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessageW(g_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }
    }
    // ============================================

    if (!g_hwnd) return 1;

    MARGINS margins = {-1};
    DwmExtendFrameIntoClientArea(g_hwnd, &margins);
    DWORD cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(g_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));

    SetWindowDisplayAffinity(g_hwnd, WDA_EXCLUDEFROMCAPTURE);

    ShowWindow(g_hwnd, nShow);
    UpdateWindow(g_hwnd);

    if (!InitWebView2()) {
        MessageBoxW(g_hwnd, HostL(2), L"CopyTool", MB_ICONERROR);
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnloadCoreDll();
    if (g_webview)   { g_webview->lpVtbl->Release(g_webview);     g_webview = NULL; }
    if (g_env)       { g_env->lpVtbl->Release(g_env);             g_env = NULL; }
    if (g_controller){ g_controller->lpVtbl->Release(g_controller); g_controller = NULL; }
    if (g_loaderDll) { FreeLibrary(g_loaderDll); g_loaderDll = NULL; }
    #ifdef _DEBUG
    if (g_logFile)   { fclose(g_logFile); g_logFile = NULL; }
#endif
    CoUninitialize();
    return 0;
}
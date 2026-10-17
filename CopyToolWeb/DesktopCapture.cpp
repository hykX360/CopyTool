// ============================================================
// DesktopCapture.cpp — DXGI Desktop Duplication 实时桌面抓取
//
// 链路：
//   捕获线程 AcquireNextFrame -> CopyResource 到 STAGING 纹理
//   -> Map 读回 -> 整屏缩放到 512x384 -> BGRA 转 RGBA
//   -> 互斥锁写入最新帧 -> PostMessage 通知 UI 线程
//
// ★ 抓整屏，不按窗口裁剪。窗口位置由 JS 端做 UV 偏移。
//
// 桌面静止时 AcquireNextFrame 超时返回，线程阻塞等待、零 CPU；
// ACCESS_LOST（锁屏/UAC/分辨率变化）时自动重建 Duplication。
// ============================================================
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>
#include <algorithm>

#include "DesktopCapture.h"

static void DcLog(const char* fmt, ...) {
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (n == 0) return;
    std::string p(path, n);
    size_t slash = p.find_last_of("\\/");
    if (slash == std::string::npos) return;
    p = p.substr(0, slash + 1) + "host_log.txt";
    FILE* f = nullptr;
    if (fopen_s(&f, p.c_str(), "a") != 0 || !f) return;
    fprintf(f, "[DC %lu] ", (unsigned long)GetTickCount64());
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

#ifndef WM_APP_DESKTOP_FRAME
#define WM_APP_DESKTOP_FRAME (WM_APP + 7)
#endif

#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = nullptr; } } while (0)

namespace {

HWND                     g_hwnd = nullptr;
std::thread              g_thread;
std::atomic<bool>        g_running{false};
std::atomic<bool>        g_available{false};

std::mutex               g_frameMutex;
uint8_t*                 g_frame = nullptr;
std::atomic<bool>        g_frameDirty{false};
std::atomic<ULONGLONG>   g_frameTs{0};
volatile LONG            g_uiPending = 0;

uint8_t*                 g_work = nullptr;
uint8_t*                 g_lastPosted = nullptr;
bool                     g_haveLast = false;

// 帧去重阈值：512x384 共 196608 像素、589824 个 RGB 通道，
// 平均每通道绝对差 < ~0.5（总和 < 300000）视为静止。
static const uint64_t    kFrameSADThreshold = 300000;

ID3D11Device*            g_dev = nullptr;
ID3D11DeviceContext*     g_ctx = nullptr;
IDXGIOutputDuplication*  g_dup = nullptr;
ID3D11Texture2D*         g_staging = nullptr;
UINT                     g_deskW = 0, g_deskH = 0;

LONG ClampL(LONG v, LONG lo, LONG hi) { return v < lo ? lo : (v > hi ? hi : v); }

bool CreateCapture() {
    SAFE_RELEASE(g_staging);
    SAFE_RELEASE(g_dup);
    SAFE_RELEASE(g_ctx);
    SAFE_RELEASE(g_dev);
    g_deskW = g_deskH = 0;

    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0,
        D3D11_SDK_VERSION,
        &g_dev, &fl, &g_ctx);
    DcLog("D3D11CreateDevice HARDWARE hr=0x%08X dev=%p", (unsigned)hr, (void*)g_dev);
    if (FAILED(hr) || !g_dev) {
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0, D3D11_SDK_VERSION,
            &g_dev, &fl, &g_ctx);
        DcLog("D3D11CreateDevice WARP hr=0x%08X dev=%p", (unsigned)hr, (void*)g_dev);
        if (FAILED(hr) || !g_dev) return false;
    }

    IDXGIDevice* dxgiDev = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIOutput* output = nullptr;
    IDXGIOutput1* output1 = nullptr;
    bool ok = false;
    do {
        if (FAILED(g_dev->QueryInterface(IID_PPV_ARGS(&dxgiDev)))) break;
        if (FAILED(dxgiDev->GetAdapter(&adapter))) break;
        if (FAILED(adapter->EnumOutputs(0, &output))) break;
        if (FAILED(output->QueryInterface(IID_PPV_ARGS(&output1)))) break;
        hr = output1->DuplicateOutput(g_dev, &g_dup);
        DcLog("DuplicateOutput hr=0x%08X dup=%p", (unsigned)hr, (void*)g_dup);
        if (FAILED(hr)) break;
        ok = true;
    } while (false);

    SAFE_RELEASE(output1);
    SAFE_RELEASE(output);
    SAFE_RELEASE(adapter);
    SAFE_RELEASE(dxgiDev);

    if (ok) g_available = true;
    return ok;
}

void DestroyCapture() {
    SAFE_RELEASE(g_staging);
    SAFE_RELEASE(g_dup);
    SAFE_RELEASE(g_ctx);
    SAFE_RELEASE(g_dev);
    g_deskW = g_deskH = 0;
}

bool EnsureStaging(const D3D11_TEXTURE2D_DESC& srcDesc) {
    if (g_staging && g_deskW == srcDesc.Width && g_deskH == srcDesc.Height) return true;
    SAFE_RELEASE(g_staging);
    D3D11_TEXTURE2D_DESC d = srcDesc;
    d.Usage = D3D11_USAGE_STAGING;
    d.BindFlags = 0;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    d.MiscFlags = 0;
    d.MipLevels = 1;
    d.ArraySize = 1;
    d.SampleDesc.Count = 1;
    d.SampleDesc.Quality = 0;
    HRESULT hr = g_dev->CreateTexture2D(&d, nullptr, &g_staging);
    if (FAILED(hr)) { g_staging = nullptr; return false; }
    g_deskW = srcDesc.Width;
    g_deskH = srcDesc.Height;
    return true;
}

// ★ 整屏抓取：不按窗口裁剪，从 (0,0) 到 (deskW, deskH) 缩到 DCAPT_W×DCAPT_H
//   窗口在屏幕上的位置由 JS 端在 shader 里做 UV 偏移。
void ConvertFrame(const uint8_t* src, UINT srcPitch) {
    const double stepX = (double)g_deskW / DCAPT_W;
    const double stepY = (double)g_deskH / DCAPT_H;

    for (int y = 0; y < DCAPT_H; ++y) {
        int sy = (int)(y * stepY);
        if (sy >= (int)g_deskH) sy = (int)g_deskH - 1;
        const uint8_t* srow = src + (size_t)sy * srcPitch;
        uint8_t* drow = g_work + (size_t)y * DCAPT_W * 4;
        for (int x = 0; x < DCAPT_W; ++x) {
            int sx = (int)(x * stepX);
            if (sx >= (int)g_deskW) sx = (int)g_deskW - 1;
            const uint8_t* sp = srow + (size_t)sx * 4;
            // BGRA -> RGBA
            drow[x * 4 + 0] = sp[2];
            drow[x * 4 + 1] = sp[1];
            drow[x * 4 + 2] = sp[0];
            drow[x * 4 + 3] = 255;
        }
    }

    if (g_haveLast && g_lastPosted) {
        uint64_t sad = 0;
        for (size_t i = 0; i < DCAPT_BYTES; i += 4) {
            int dr = (int)g_work[i + 0] - (int)g_lastPosted[i + 0];
            int dg = (int)g_work[i + 1] - (int)g_lastPosted[i + 1];
            int db = (int)g_work[i + 2] - (int)g_lastPosted[i + 2];
            sad += (uint64_t)(dr < 0 ? -dr : dr);
            sad += (uint64_t)(dg < 0 ? -dg : dg);
            sad += (uint64_t)(db < 0 ? -db : db);
        }
        if (sad < kFrameSADThreshold) {
            return;
        }
    }

    {
        std::lock_guard<std::mutex> lk(g_frameMutex);
        memcpy(g_frame, g_work, DCAPT_BYTES);
        g_frameTs = GetTickCount64();
        g_frameDirty = true;
    }
    memcpy(g_lastPosted, g_work, DCAPT_BYTES);
    g_haveLast = true;
    if (InterlockedExchange(&g_uiPending, 1) == 0 && g_hwnd) {
        PostMessageW(g_hwnd, WM_APP_DESKTOP_FRAME, 0, 0);
    }
}

// DXGI Desktop Duplication 在桌面出现新的 Present 之前不会返回任何帧，
// 静止桌面可能几十秒甚至更久没有第一帧，导致启动时玻璃后面是黑的。
// 用 GDI 抓一次全屏作为初始帧（WDA_EXCLUDEFROMCAPTURE 对 BitBlt 同样
// 生效，抓到的是本窗口背后的内容）；之后 DXGI 实时帧自动接管。
bool CaptureInitialFrameGdi() {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (sw <= 0 || sh <= 0 || !g_work || !g_frame || !g_lastPosted) return false;

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) return false;
    HDC memDc = CreateCompatibleDC(screenDc);
    HBITMAP bmp = CreateCompatibleBitmap(screenDc, sw, sh);
    bool ok = false;
    if (memDc && bmp) {
        HGDIOBJ oldSel = SelectObject(memDc, bmp);
        if (BitBlt(memDc, 0, 0, sw, sh, screenDc, 0, 0, SRCCOPY)) {
            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = sw;
            bi.bmiHeader.biHeight = -sh;   // top-down：首行即屏幕顶部
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            std::vector<uint8_t> px((size_t)sw * sh * 4);
            if (GetDIBits(memDc, bmp, 0, sh, px.data(), &bi, DIB_RGB_COLORS)) {
                const double stepX = (double)sw / DCAPT_W;
                const double stepY = (double)sh / DCAPT_H;
                for (int y = 0; y < DCAPT_H; ++y) {
                    int sy = (int)(y * stepY);
                    if (sy >= sh) sy = sh - 1;
                    uint8_t* drow = g_work + (size_t)y * DCAPT_W * 4;
                    const uint8_t* srow = px.data() + (size_t)sy * sw * 4;
                    for (int x = 0; x < DCAPT_W; ++x) {
                        int sx = (int)(x * stepX);
                        if (sx >= sw) sx = sw - 1;
                        const uint8_t* sp = srow + (size_t)sx * 4;
                        drow[x * 4 + 0] = sp[2];   // BGRA -> RGBA
                        drow[x * 4 + 1] = sp[1];
                        drow[x * 4 + 2] = sp[0];
                        drow[x * 4 + 3] = 255;
                    }
                }
                {
                    std::lock_guard<std::mutex> lk(g_frameMutex);
                    memcpy(g_frame, g_work, DCAPT_BYTES);
                    g_frameTs = GetTickCount64();
                    g_frameDirty = true;
                }
                // 记为 lastPosted，静止桌面的首个 DXGI 帧若几乎相同会被
                // SAD 去重，避免无意义的重复推送。
                memcpy(g_lastPosted, g_work, DCAPT_BYTES);
                g_haveLast = true;
                if (InterlockedExchange(&g_uiPending, 1) == 0 && g_hwnd)
                    PostMessageW(g_hwnd, WM_APP_DESKTOP_FRAME, 0, 0);
                ok = true;
                DcLog("GDI initial frame captured %dx%d", sw, sh);
            }
        }
        SelectObject(memDc, oldSel);
    }
    if (bmp)   DeleteObject(bmp);
    if (memDc) DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
    return ok;
}

void CaptureThreadMain() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    DcLog("capture thread start tid=%lu", GetCurrentThreadId());

    int backoffMs = 200;
    int frameLogCount = 0;
    while (g_running) {
        if (!g_dup) {
            if (CreateCapture()) {
                backoffMs = 200;
                // 立即用 GDI 补一帧，避免静止桌面下长时间黑屏
                CaptureInitialFrameGdi();
            } else {
                DcLog("CreateCapture failed, retry in 500ms");
                for (int i = 0; i < 50 && g_running; ++i) Sleep(10);
                continue;
            }
        }

        IDXGIResource* deskRes = nullptr;
        DXGI_OUTDUPL_FRAME_INFO info;
        ZeroMemory(&info, sizeof(info));
        HRESULT hr = g_dup->AcquireNextFrame(80, &info, &deskRes);

        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            // 静止桌面持续超时是正常的（线程在系统调用里等待，零 CPU）。
            continue;
        }
        if (hr == DXGI_ERROR_ACCESS_LOST) {
            DcLog("ACCESS_LOST, recreating duplication");
            SAFE_RELEASE(g_staging);
            SAFE_RELEASE(g_dup);
            g_available = false;
            Sleep(150);
            continue;
        }
        if (FAILED(hr) || !deskRes) {
            DcLog("AcquireNextFrame failed hr=0x%08X", (unsigned)hr);
            SAFE_RELEASE(deskRes);
            SAFE_RELEASE(g_staging);
            SAFE_RELEASE(g_dup);
            Sleep(backoffMs);
            continue;
        }

        ID3D11Texture2D* srcTex = nullptr;
        bool handled = false;
        if (SUCCEEDED(deskRes->QueryInterface(IID_PPV_ARGS(&srcTex)))) {
            D3D11_TEXTURE2D_DESC desc;
            srcTex->GetDesc(&desc);
            if (EnsureStaging(desc)) {
                g_ctx->CopyResource(g_staging, srcTex);
                D3D11_MAPPED_SUBRESOURCE mapped;
                ZeroMemory(&mapped, sizeof(mapped));
                if (SUCCEEDED(g_ctx->Map(g_staging, 0, D3D11_MAP_READ, 0, &mapped))) {
                    ConvertFrame((const uint8_t*)mapped.pData, (UINT)mapped.RowPitch);
                    g_ctx->Unmap(g_staging, 0);
                    handled = true;
                    if (frameLogCount < 1) {
                        DcLog("first frame captured: desk=%ux%u pitch=%ld",
                              desc.Width, desc.Height, (long)mapped.RowPitch);
                        ++frameLogCount;
                    }
                } else {
                    DcLog("Map staging failed");
                }
            }
        }
        SAFE_RELEASE(srcTex);
        SAFE_RELEASE(deskRes);
        g_dup->ReleaseFrame();
        (void)handled;
    }

    DestroyCapture();
    CoUninitialize();
}

} // namespace

bool DesktopCapture_Init(HWND hwnd) {
    g_hwnd = hwnd;
    if (!g_frame)      g_frame      = new (std::nothrow) uint8_t[DCAPT_BYTES];
    if (!g_work)       g_work       = new (std::nothrow) uint8_t[DCAPT_BYTES];
    if (!g_lastPosted) g_lastPosted = new (std::nothrow) uint8_t[DCAPT_BYTES];
    if (!g_frame || !g_work || !g_lastPosted) return false;
    memset(g_frame, 0, DCAPT_BYTES);
    g_haveLast = false;
    return true;
}

bool DesktopCapture_Start() {
    if (g_running.exchange(true)) return true;
    if (!g_frame || !g_work) return false;
    g_thread = std::thread(CaptureThreadMain);
    return true;
}

void DesktopCapture_Stop() {
    if (!g_running.exchange(false)) return;
    if (g_thread.joinable()) g_thread.join();
    g_available = false;
}

bool DesktopCapture_TryGetFrame(const unsigned char** outData, unsigned long long* outTsMs) {
    if (!outData) return false;
    g_frameMutex.lock();
    if (g_frameDirty.load()) {
        *outData = g_frame;
        if (outTsMs) *outTsMs = g_frameTs.load();
        return true;
    }
    g_frameMutex.unlock();
    return false;
}

static void RescheduleIfDirtyLocked() {
    InterlockedExchange(&g_uiPending, 0);
    bool redirty = false;
    {
        std::lock_guard<std::mutex> lk(g_frameMutex);
        redirty = g_frameDirty.load();
    }
    if (redirty && g_hwnd && InterlockedExchange(&g_uiPending, 1) == 0) {
        PostMessageW(g_hwnd, WM_APP_DESKTOP_FRAME, 0, 0);
    }
}

void DesktopCapture_ReleaseFrame() {
    g_frameDirty = false;
    g_frameMutex.unlock();
    RescheduleIfDirtyLocked();
}

void DesktopCapture_MessageProcessed() {
    RescheduleIfDirtyLocked();
}

bool DesktopCapture_IsAvailable() {
    return g_available.load();
}

void DesktopCapture_Poke() {
    if (!g_frame) return;
    {
        std::lock_guard<std::mutex> lk(g_frameMutex);
        g_frameDirty = true;
    }
    if (g_hwnd && InterlockedExchange(&g_uiPending, 1) == 0) {
        PostMessageW(g_hwnd, WM_APP_DESKTOP_FRAME, 0, 0);
    }
}
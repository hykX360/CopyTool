// ============================================================
// DesktopCapture.h — DXGI Desktop Duplication 实时桌面抓取
//   - 独立捕获线程，不阻塞 UI / WebView2
//   - 输出固定 512x384 RGBA（在 C++ 侧完成 BGRA->RGBA）
//   - 裁剪窗口客户区对应的屏幕区域（玻璃看到的是"窗口背后"）
// ============================================================
#pragma once
#include <windows.h>

#define DCAPT_W 512
#define DCAPT_H 384
#define DCAPT_BYTES (DCAPT_W * DCAPT_H * 4)

// hwnd 用于确定裁剪区域（窗口客户区在屏幕上的位置）
bool DesktopCapture_Init(HWND hwnd);
bool DesktopCapture_Start();
void DesktopCapture_Stop();

// UI 线程调用：若有新帧，返回内部缓冲区指针（只读），
// 指针在 ReleaseFrame 之前有效。outTsMs 为帧时间戳（GetTickCount64）。
bool DesktopCapture_TryGetFrame(const unsigned char** outData, unsigned long long* outTsMs);
void DesktopCapture_ReleaseFrame();
// TryGetFrame 返回 false（无新帧）时也要调用，重置通知合并标志，防止通知永久丢失
void DesktopCapture_MessageProcessed();
// 页面就绪后补推最新一帧（桌面静止时不会有新帧到达）
void DesktopCapture_Poke();

// 最近一次状态，供日志/降级判断
bool DesktopCapture_IsAvailable();

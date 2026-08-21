#define _WIN32_IE 0x0600
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <strsafe.h>
#include <windowsx.h>
#include <stdio.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <math.h>
#include <shlobj.h>
#include <process.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <malloc.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

using Microsoft::WRL::ComPtr;

#define WM_UPDATE_PROGRESS (WM_USER + 100)
#define WM_THREAD_FINISHED (WM_USER + 101)
#define WM_SETTINGS_CLOSED (WM_USER + 102)

#define MAX_WORKER_THREADS 32
#define LARGE_FILE_THRESHOLD (64 * 1024 * 1024)

enum ButtonState { BTN_NORMAL, BTN_HOVER, BTN_PRESSED, BTN_DISABLED };

enum CopyMode {
    MODE_AUTO,
    MODE_BALANCED,
    MODE_SSD_SSD,
    MODE_HDD_HDD,
    MODE_HDD_SSD,
    MODE_SSD_HDD,
    MODE_SSD_UFD,
    MODE_UFD_SSD,
    MODE_HDD_UFD,
    MODE_UFD_HDD,
    MODE_UFD_UFD,
    MODE_EXTREME
};

typedef struct {
    const wchar_t* displayName;
    const wchar_t* shortName;
    int threads;
    size_t bufferSize;
    DWORD srcFlags;
    DWORD dstFlags;
    BOOL preallocate;
    int speedPriority;
} ModeConfig;

static const ModeConfig g_ModeConfigs[] = {
    {L"自动识别", L"自动", 0, 0, 0, 0, TRUE, 5},
    {L"均衡模式", L"均衡", 8, 32*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 5},
    {L"SSD→SSD", L"SSD→SSD", 32, 256*1024*1024, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 10},
    {L"HDD→HDD", L"HDD→HDD", 4, 32*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 4},
    {L"HDD→SSD", L"HDD→SSD", 8, 64*1024*1024, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 7},
    {L"SSD→HDD", L"SSD→HDD", 4, 64*1024*1024, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 6},
    {L"SSD→UFD", L"SSD→UFD", 2, 4*1024*1024, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, FALSE, 3},
    {L"UFD→SSD", L"UFD→SSD", 4, 16*1024*1024, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 6},
    {L"HDD→UFD", L"HDD→UFD", 2, 4*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, FALSE, 2},
    {L"UFD→HDD", L"UFD→HDD", 2, 8*1024*1024, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 3},
    {L"UFD→UFD", L"UFD→UFD", 1, 2*1024*1024, FILE_FLAG_OVERLAPPED, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED, FALSE, 1},
    {L"极速狂飙", L"极速", 32, 256*1024*1024, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 10}
};

int currentLang = 0;
const wchar_t* langNames[] = {L"简体中文", L"繁體中文", L"English", L"한국어", L"日本語"};

typedef struct {
    const char* id;
    const wchar_t* zhCN;
    const wchar_t* zhTW;
    const wchar_t* enUS;
    const wchar_t* koKR;
    const wchar_t* jaJP;
} LangEntry;

static const LangEntry g_LangTable[] = {
    {"btn_setting", L"设置", L"設定", L"Settings", L"설정", L"設定"},
    {"label_src", L"源文件夹", L"來源資料夾", L"Source", L"원본", L"元"},
    {"label_dst", L"目标文件夹", L"目標資料夾", L"Target", L"대상", L"先"},
    {"btn_browse", L"浏览", L"瀏覽", L"Browse", L"찾기", L"参照"},
    {"btn_copy", L"开始复制", L"開始複製", L"Start", L"시작", L"開始"},
    {"btn_pause", L"暂停", L"暫停", L"Pause", L"일시중지", L"一時停止"},
    {"btn_resume", L"继续", L"繼續", L"Resume", L"재개", L"再開"},
    {"btn_stop", L"终止", L"終止", L"Stop", L"중지", L"停止"},
    {"status_ready", L"准备就绪", L"準備就緒", L"Ready", L"준비됨", L"準備完了"},
    {"status_copying", L"复制中", L"複製中", L"Copying", L"복사중", L"コピー中"},
    {"status_paused", L"已暂停", L"已暫停", L"Paused", L"일시중지됨", L"一時停止中"},
    {"status_stopped", L"已终止", L"已終止", L"Stopped", L"중지됨", L"停止済み"},
    {"status_done", L"完成", L"完成", L"Done", L"완료", L"完了"},
    {"status_scanning", L"正在扫描...", L"正在掃描...", L"Scanning...", L"스캔중...", L"スキャン中..."},
    {"status_terminating", L"正在终止...", L"正在終止...", L"Terminating...", L"종료중...", L"終了中..."},
    {"mode", L"复制模式", L"複製模式", L"Mode", L"모드", L"モード"},
    {"mode_auto", L"自动", L"自動", L"Auto", L"자동", L"自動"},
    {"mode_balanced", L"均衡", L"均衡", L"Balanced", L"균형", L"バランス"},
    {"mode_ssd_ssd", L"SSD→SSD", L"SSD→SSD", L"SSD→SSD", L"SSD→SSD", L"SSD→SSD"},
    {"mode_hdd_hdd", L"HDD→HDD", L"HDD→HDD", L"HDD→HDD", L"HDD→HDD", L"HDD→HDD"},
    {"mode_hdd_ssd", L"HDD→SSD", L"HDD→SSD", L"HDD→SSD", L"HDD→SSD", L"HDD→SSD"},
    {"mode_ssd_hdd", L"SSD→HDD", L"SSD→HDD", L"SSD→HDD", L"SSD→HDD", L"SSD→HDD"},
    {"mode_ssd_ufd", L"SSD→UFD", L"SSD→UFD", L"SSD→UFD", L"SSD→UFD", L"SSD→UFD"},
    {"mode_ufd_ssd", L"UFD→SSD", L"UFD→SSD", L"UFD→SSD", L"UFD→SSD", L"UFD→SSD"},
    {"mode_hdd_ufd", L"HDD→UFD", L"HDD→UFD", L"HDD→UFD", L"HDD→UFD", L"HDD→UFD"},
    {"mode_ufd_hdd", L"UFD→HDD", L"UFD→HDD", L"UFD→HDD", L"UFD→HDD", L"UFD→HDD"},
    {"mode_ufd_ufd", L"UFD→UFD", L"UFD→UFD", L"UFD→UFD", L"UFD→UFD", L"UFD→UFD"},
    {"mode_extreme", L"极速", L"極速", L"Extreme", L"익스트림", L"エクストリーム"},
    {"settings_title", L"设置", L"設定", L"Settings", L"설정", L"設定"},
    {"settings_speed_limit", L"速度限制", L"速度限制", L"Speed Limit", L"속도제한", L"速度制限"},
    {"settings_verify", L"校验完整性", L"校驗完整性", L"Verify", L"검증", L"検証"},
    {"settings_language", L"语言", L"語言", L"Language", L"언어", L"言語"},
    {"settings_save", L"保存", L"儲存", L"Save", L"저장", L"保存"},
    {"settings_cancel", L"取消", L"取消", L"Cancel", L"취소", L"キャンセル"},
    {"settings_saved", L"已保存！", L"已儲存！", L"Saved!", L"저장됨!", L"保存しました！"},
    {"settings_hint_zero", L"0 = 不限速", L"0 = 不限速", L"0 = Unlimited", L"0 = 무제한", L"0 = 無制限"},
    {"dialog_done", L"全部复制完成！", L"全部複製完成！", L"All done!", L"모두 완료!", L"完了しました！"},
    {"dialog_stopped", L"复制已终止", L"複製已終止", L"Stopped", L"중지됨", L"停止しました"},
    {"dialog_error_unable", L"无法完成任务", L"無法完成任務", L"Cannot complete task", L"작업을 완료할 수 없음", L"タスクを完了できません"},
    {"dialog_error_access_src", L"无法访问源文件夹", L"無法訪問來源資料夾", L"Cannot access source folder", L"원본 폴더에 접근할 수 없음", L"ソースフォルダにアクセスできません"},
    {"dialog_error_write_dst", L"无法写入目标文件夹", L"無法寫入目標資料夾", L"Cannot write to target folder", L"대상 폴더에 쓸 수 없음", L"先フォルダに書き込めません"},
    {"dialog_error_space", L"磁盘空间不足", L"磁碟空間不足", L"Not enough disk space", L"디스크 공간이 부족함", L"ディスク容量が不足しています"},
    {"dialog_error_path", L"路径过长", L"路徑過長", L"Path too long", L"경로가 너무 김", L"パスが長すぎます"},
    {"dialog_error_locked", L"目标文件被占用", L"目標檔案被佔用", L"Target file is locked", L"대상 파일이 잠겨 있음", L"先ファイルが使用中です"},
    {"dialog_error_read", L"源文件读取失败", L"來源檔案讀取失敗", L"Failed to read source file", L"원본 파일 읽기 실패", L"ソースファイルの読み取りに失敗しました"},
    {"dialog_error_write_file", L"目标文件写入失败", L"目標檔案寫入失敗", L"Failed to write target file", L"대상 파일 쓰기 실패", L"先ファイルの書き込みに失敗しました"},
    {"dialog_error_verify", L"文件缺失！将重新复制", L"檔案缺失！將重新複製", L"File missing! Recopying", L"파일 누락! 다시 복사", L"ファイルが不足しています！再コピーします"},
    {"dialog_error_thread", L"系统资源不足", L"系統資源不足", L"Not enough system resources", L"시스템 리소스 부족", L"システムリソースが不足しています"},
    {"dialog_error_memory", L"内存不足", L"記憶體不足", L"Not enough memory", L"메모리 부족", L"メモリが不足しています"},
};

const wchar_t* GetText(const char* id) {
    int count = sizeof(g_LangTable) / sizeof(LangEntry);
    for (int i = 0; i < count; i++) {
        if (strcmp(g_LangTable[i].id, id) == 0) {
            const wchar_t* texts[] = {
                g_LangTable[i].zhCN,
                g_LangTable[i].zhTW,
                g_LangTable[i].enUS,
                g_LangTable[i].koKR,
                g_LangTable[i].jaJP
            };
            return texts[currentLang];
        }
    }
    return L"";
}

ComPtr<ID2D1Factory> pD2DFactory;
ComPtr<ID2D1HwndRenderTarget> pRenderTarget;
ComPtr<ID2D1SolidColorBrush> pWhiteBrush;
ComPtr<ID2D1SolidColorBrush> pTextBrush;
ComPtr<ID2D1SolidColorBrush> pTextLightBrush;
ComPtr<ID2D1SolidColorBrush> pTextSecondaryBrush;
ComPtr<ID2D1SolidColorBrush> pProgressBgBrush;
ComPtr<ID2D1SolidColorBrush> pButtonTextBrush;

ComPtr<ID2D1HwndRenderTarget> pSettingsRenderTarget;
ComPtr<ID2D1SolidColorBrush> pSettingsWhiteBrush;
ComPtr<ID2D1SolidColorBrush> pSettingsTextBrush;
ComPtr<ID2D1SolidColorBrush> pSettingsSecondaryBrush;

ComPtr<IDWriteFactory> pDWriteFactory;
ComPtr<IDWriteTextFormat> pTextFormat;
ComPtr<IDWriteTextFormat> pTextFormatBold;
ComPtr<IDWriteTextFormat> pTextFormatTitle;
ComPtr<IDWriteTextFormat> pTextFormatPercent;
ComPtr<IDWriteTextFormat> pTextFormatSmall;

HWND hMainWnd, hSettingsWnd;
int progressValue = 0, isCopying = 0, isPaused = 0, stopRequested = 0;
HANDLE hCopyThread = NULL;
CRITICAL_SECTION cs;

int speedLimitKB = 0;
int enableVerify = 0;
int currentMode = MODE_AUTO;

wchar_t srcPath[MAX_PATH] = L"";
wchar_t dstPath[MAX_PATH] = L"";
wchar_t statusText[64] = L"准备就绪";
int srcCaretPos = 0, dstCaretPos = 0;
int srcLen = 0, dstLen = 0;
BOOL srcFocus = FALSE, dstFocus = FALSE;

ButtonState btnCopyState = BTN_NORMAL, btnPauseState = BTN_DISABLED, btnStopState = BTN_DISABLED;
ButtonState btnBrowseSrcState = BTN_NORMAL, btnBrowseDstState = BTN_NORMAL, btnSettingState = BTN_NORMAL;
ButtonState btnSettingsSaveState = BTN_NORMAL, btnSettingsCancelState = BTN_NORMAL;

RECT btnCopyRect = {30, 268, 110, 308};
RECT btnPauseRect = {120, 268, 200, 308};
RECT btnStopRect = {210, 268, 290, 308};
RECT btnBrowseSrcRect = {318, 84, 358, 114};
RECT btnBrowseDstRect = {318, 148, 358, 178};
RECT btnSettingRect = {340, 18, 385, 48};
RECT editSrcRect = {30, 84, 310, 114};
RECT editDstRect = {30, 148, 310, 178};
RECT statusRect = {30, 234, 358, 254};
RECT progressRect = {30, 196, 358, 226};

RECT settingsSpeedInputRect = {30, 94, 160, 126};
RECT settingsUnitRect = {170, 94, 230, 126};
RECT settingsSaveRect = {30, 244, 170, 280};
RECT settingsCancelRect = {190, 244, 330, 280};
RECT settingsCloseRect = {358, 8, 380, 30};
RECT settingsVerifyRect = {220, 163, 238, 181};
RECT settingsLangRect = {220, 186, 310, 214};

wchar_t speedInputText[32] = L"0";
int speedInputLen = 1;
int speedCaretPos = 1;
BOOL speedFocus = FALSE;
int selectedUnit = 0;
const wchar_t* units[] = {L"KB/s", L"MB/s", L"GB/s"};
int unitMultipliers[] = {1, 1024, 1024 * 1024};

int g_workerThreads = 8;
size_t g_bufferSize = 32 * 1024 * 1024;
DWORD g_srcFlags = FILE_FLAG_SEQUENTIAL_SCAN;
DWORD g_dstFlags = FILE_FLAG_SEQUENTIAL_SCAN;
BOOL g_preallocate = TRUE;

typedef struct FileNode {
    wchar_t src[MAX_PATH];
    wchar_t dst[MAX_PATH];
    struct FileNode* next;
} FileNode;

FileNode* queueHead = NULL;
FileNode* queueTail = NULL;
long long totalBytes = 0;
long long copiedBytes = 0;
int totalFiles = 0;
int copiedFiles = 0;
int workerThreadCount = 8;

DWORD WINAPI WorkerThreadProc(LPVOID lpParam);
BOOL CopySingleFile(const wchar_t* src, const wchar_t* dst);
void CreateDirectoriesRecursive(const wchar_t* path);

void ShowError(HWND hWnd, const char* id) {
    const wchar_t* msg = GetText(id);
    if (msg && *msg) {
        MessageBoxW(hWnd, msg, L"CopyTool", MB_OK | MB_ICONERROR);
    }
}

int GetDriveTypeEx(const wchar_t* path) {
    wchar_t root[4];
    wcsncpy(root, path, 3);
    root[3] = L'\0';
    UINT driveType = GetDriveTypeW(root);
    if (driveType == DRIVE_REMOVABLE) return 3;
    if (driveType != DRIVE_FIXED) return 0;
    wchar_t drivePath[64];
    StringCchPrintfW(drivePath, 64, L"\\\\.\\%c:", root[0]);
    HANDLE hDrive = CreateFileW(drivePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) return 2;
    STORAGE_PROPERTY_QUERY query;
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    query.AdditionalParameters[0] = 0;
    BYTE buffer[1024] = {0};
    DWORD bytesReturned = 0;
    BOOL isSSD = FALSE;
    if (DeviceIoControl(hDrive, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer, sizeof(buffer), &bytesReturned, NULL)) {
        STORAGE_DEVICE_DESCRIPTOR* desc = (STORAGE_DEVICE_DESCRIPTOR*)buffer;
        int busType = desc->BusType;
        if (busType == 3 || busType == 7 || busType == 9 || busType == 10 || busType == 4 || busType == 17) {
            isSSD = TRUE;
        }
    }
    CloseHandle(hDrive);
    return isSSD ? 1 : 2;
}

int SelectBestMode(const wchar_t* src, const wchar_t* dst) {
    if (currentMode != MODE_AUTO) return currentMode;
    int srcType = GetDriveTypeEx(src);
    int dstType = GetDriveTypeEx(dst);
    if (srcType == 1 && dstType == 1) return MODE_SSD_SSD;
    if (srcType == 2 && dstType == 2) return MODE_HDD_HDD;
    if (srcType == 2 && dstType == 1) return MODE_HDD_SSD;
    if (srcType == 1 && dstType == 2) return MODE_SSD_HDD;
    if (srcType == 1 && dstType == 3) return MODE_SSD_UFD;
    if (srcType == 3 && dstType == 1) return MODE_UFD_SSD;
    if (srcType == 2 && dstType == 3) return MODE_HDD_UFD;
    if (srcType == 3 && dstType == 2) return MODE_UFD_HDD;
    if (srcType == 3 && dstType == 3) return MODE_UFD_UFD;
    return MODE_BALANCED;
}

void ApplyModeConfig(int mode) {
    const ModeConfig* cfg = &g_ModeConfigs[mode];
    if (mode == MODE_AUTO) {
        int autoMode = SelectBestMode(srcPath, dstPath);
        cfg = &g_ModeConfigs[autoMode];
    }
    g_workerThreads = cfg->threads;
    g_bufferSize = cfg->bufferSize;
    g_srcFlags = cfg->srcFlags;
    g_dstFlags = cfg->dstFlags;
    g_preallocate = cfg->preallocate;
    workerThreadCount = cfg->threads > 0 ? cfg->threads : 8;
}

void EnqueueFile(const wchar_t* src, const wchar_t* dst) {
    FileNode* node = (FileNode*)malloc(sizeof(FileNode));
    if (!node) return;
    wcscpy(node->src, src);
    wcscpy(node->dst, dst);
    node->next = NULL;
    if (queueTail) { queueTail->next = node; queueTail = node; }
    else { queueHead = queueTail = node; }
}

BOOL DequeueFile(wchar_t* src, wchar_t* dst) {
    if (!queueHead) return FALSE;
    FileNode* node = queueHead;
    wcscpy(src, node->src);
    wcscpy(dst, node->dst);
    queueHead = queueHead->next;
    if (!queueHead) queueTail = NULL;
    free(node);
    return TRUE;
}

void ClearQueue() {
    wchar_t src[MAX_PATH], dst[MAX_PATH];
    while (DequeueFile(src, dst)) {}
}

void DrawAestheticBackground(ID2D1RenderTarget* pDC, RECT rect) {
    if (!pDC) return;
    D2D1_GRADIENT_STOP stops[2];
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(0.91f, 0.96f, 0.99f, 1.0f);
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    ComPtr<ID2D1GradientStopCollection> pStops;
    pDC->CreateGradientStopCollection(stops, 2, pStops.GetAddressOf());
    if (pStops) {
        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props;
        props.startPoint = D2D1::Point2F(0, 0);
        props.endPoint = D2D1::Point2F(0, (float)rect.bottom);
        ComPtr<ID2D1LinearGradientBrush> pBgBrush;
        pDC->CreateLinearGradientBrush(&props, NULL, pStops.Get(), pBgBrush.GetAddressOf());
        if (pBgBrush) {
            D2D1_RECT_F r = {0, 0, (float)rect.right, (float)rect.bottom};
            pDC->FillRectangle(&r, pBgBrush.Get());
        }
    }
}

void DrawGlassCardAdvanced(ID2D1RenderTarget* pDC, RECT rect) {
    if (!pDC) return;
    D2D1_ROUNDED_RECT rounded;
    rounded.rect = D2D1::RectF((float)rect.left, (float)rect.top, (float)rect.right, (float)rect.bottom);
    rounded.radiusX = 20.0f;
    rounded.radiusY = 20.0f;
    ComPtr<ID2D1RoundedRectangleGeometry> pGeo;
    pD2DFactory->CreateRoundedRectangleGeometry(&rounded, pGeo.GetAddressOf());
    if (!pGeo) return;
    ComPtr<ID2D1SolidColorBrush> pGlassBrush;
    D2D1_COLOR_F glassColor = {1.0f, 1.0f, 1.0f, 0.55f};
    pDC->CreateSolidColorBrush(&glassColor, NULL, pGlassBrush.GetAddressOf());
    if (pGlassBrush) pDC->FillGeometry(pGeo.Get(), pGlassBrush.Get());
    ComPtr<ID2D1SolidColorBrush> pBorderBrush;
    D2D1_COLOR_F borderColor = {0.91f, 0.93f, 0.95f, 0.5f};
    pDC->CreateSolidColorBrush(&borderColor, NULL, pBorderBrush.GetAddressOf());
    if (pBorderBrush) pDC->DrawGeometry(pGeo.Get(), pBorderBrush.Get(), 1.0f, NULL);
}

void DrawDecorations(ID2D1RenderTarget* pDC, RECT rect) {
    if (!pDC) return;
    ComPtr<ID2D1SolidColorBrush> pPurpleBrush;
    D2D1_COLOR_F purpleColor = {0.75f, 0.52f, 0.98f, 0.35f};
    pDC->CreateSolidColorBrush(&purpleColor, NULL, pPurpleBrush.GetAddressOf());
    if (pPurpleBrush) {
        D2D1_ELLIPSE ellipse1;
        ellipse1.point = D2D1::Point2F(40.0f, 30.0f);
        ellipse1.radiusX = 50.0f;
        ellipse1.radiusY = 50.0f;
        pDC->FillEllipse(&ellipse1, pPurpleBrush.Get());
    }
    ComPtr<ID2D1SolidColorBrush> pTealBrush;
    D2D1_COLOR_F tealColor = {0.20f, 0.83f, 0.60f, 0.30f};
    pDC->CreateSolidColorBrush(&tealColor, NULL, pTealBrush.GetAddressOf());
    if (pTealBrush) {
        D2D1_ELLIPSE ellipse2;
        ellipse2.point = D2D1::Point2F(65.0f, 48.0f);
        ellipse2.radiusX = 32.0f;
        ellipse2.radiusY = 32.0f;
        pDC->FillEllipse(&ellipse2, pTealBrush.Get());
    }
}

HRESULT InitD2D(HWND hWnd) {
    D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_NONE };
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), &options, (void**)pD2DFactory.GetAddressOf());
    if (FAILED(hr)) return hr;
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)pDWriteFactory.GetAddressOf());
    if (FAILED(hr)) return hr;
    RECT rc; GetClientRect(hWnd, &rc);
    D2D1_SIZE_U size = { (UINT)(rc.right - rc.left), (UINT)(rc.bottom - rc.top) };
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(hWnd, size);
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
    hr = pD2DFactory->CreateHwndRenderTarget(&rtProps, &hwndProps, &pRenderTarget);
    if (FAILED(hr)) return hr;
    D2D1_COLOR_F white = {1,1,1,0.95f};
    D2D1_COLOR_F textColor = {0.17f, 0.24f, 0.31f, 1.0f};
    D2D1_COLOR_F textLight = {0.35f, 0.42f, 0.49f, 1.0f};
    D2D1_COLOR_F textSecondary = {0.54f, 0.61f, 0.71f, 1.0f};
    D2D1_COLOR_F progressBg = {0.96f,0.97f,0.98f,1};
    D2D1_COLOR_F btnText = {1,1,1,1};
    pRenderTarget->CreateSolidColorBrush(&white, NULL, pWhiteBrush.GetAddressOf());
    pRenderTarget->CreateSolidColorBrush(&textColor, NULL, pTextBrush.GetAddressOf());
    pRenderTarget->CreateSolidColorBrush(&textLight, NULL, pTextLightBrush.GetAddressOf());
    pRenderTarget->CreateSolidColorBrush(&textSecondary, NULL, pTextSecondaryBrush.GetAddressOf());
    pRenderTarget->CreateSolidColorBrush(&progressBg, NULL, pProgressBgBrush.GetAddressOf());
    pRenderTarget->CreateSolidColorBrush(&btnText, NULL, pButtonTextBrush.GetAddressOf());
    pDWriteFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14, L"en-us", pTextFormat.GetAddressOf());
    if (pTextFormat) { pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR); }
    pDWriteFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16, L"en-us", pTextFormatBold.GetAddressOf());
    if (pTextFormatBold) { pTextFormatBold->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); pTextFormatBold->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
    pDWriteFactory->CreateTextFormat(L"Malgun Gothic", NULL, DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 22, L"ko-kr", pTextFormatTitle.GetAddressOf());
    if (pTextFormatTitle) { pTextFormatTitle->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); pTextFormatTitle->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
    pDWriteFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13, L"en-us", pTextFormatPercent.GetAddressOf());
    if (pTextFormatPercent) { pTextFormatPercent->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING); pTextFormatPercent->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
    pDWriteFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12, L"en-us", pTextFormatSmall.GetAddressOf());
    if (pTextFormatSmall) { pTextFormatSmall->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); pTextFormatSmall->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
    return S_OK;
}

HRESULT InitSettingsD2D(HWND hWnd) {
    RECT rc; GetClientRect(hWnd, &rc);
    D2D1_SIZE_U size = { (UINT)(rc.right - rc.left), (UINT)(rc.bottom - rc.top) };
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(hWnd, size);
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
    HRESULT hr = pD2DFactory->CreateHwndRenderTarget(&rtProps, &hwndProps, &pSettingsRenderTarget);
    if (FAILED(hr)) return hr;
    D2D1_COLOR_F white = {1,1,1,0.95f};
    D2D1_COLOR_F textColor = {0.17f, 0.24f, 0.31f, 1.0f};
    D2D1_COLOR_F textSecondary = {0.54f, 0.61f, 0.71f, 1.0f};
    pSettingsRenderTarget->CreateSolidColorBrush(&white, NULL, pSettingsWhiteBrush.GetAddressOf());
    pSettingsRenderTarget->CreateSolidColorBrush(&textColor, NULL, pSettingsTextBrush.GetAddressOf());
    pSettingsRenderTarget->CreateSolidColorBrush(&textSecondary, NULL, pSettingsSecondaryBrush.GetAddressOf());
    return S_OK;
}

void DrawEditBoxAdvanced(ID2D1RenderTarget* pDC, RECT rect, const wchar_t* text, BOOL focus) {
    if (!pDC) return;
    D2D1_ROUNDED_RECT rounded = {D2D1::RectF((float)rect.left,(float)rect.top,(float)rect.right,(float)rect.bottom), 10, 10};
    ComPtr<ID2D1RoundedRectangleGeometry> pGeo;
    pD2DFactory->CreateRoundedRectangleGeometry(&rounded, pGeo.GetAddressOf());
    if (pGeo) {
        ComPtr<ID2D1SolidColorBrush> pBg;
        D2D1_COLOR_F bg = {1.0f, 1.0f, 1.0f, 0.85f};
        pDC->CreateSolidColorBrush(&bg, NULL, pBg.GetAddressOf());
        if (pBg) pDC->FillGeometry(pGeo.Get(), pBg.Get());
        ComPtr<ID2D1SolidColorBrush> pBorder;
        D2D1_COLOR_F bc = focus ? D2D1::ColorF(0.29f, 0.56f, 0.85f, 1.0f) : D2D1::ColorF(0.88f, 0.90f, 0.93f, 1.0f);
        pDC->CreateSolidColorBrush(&bc, NULL, pBorder.GetAddressOf());
        if (pBorder) pDC->DrawGeometry(pGeo.Get(), pBorder.Get(), 1.5f, NULL);
    }
    if (text && *text) {
        D2D1_COLOR_F textColor = {0.17f, 0.24f, 0.31f, 1.0f};
        ComPtr<ID2D1SolidColorBrush> pTextBrushLocal;
        pDC->CreateSolidColorBrush(&textColor, NULL, pTextBrushLocal.GetAddressOf());
        if (pTextFormat && pTextBrushLocal) {
            D2D1_RECT_F lr = {(float)rect.left+12,(float)rect.top+4,(float)rect.right-8,(float)rect.bottom-4};
            pDC->DrawText(text, wcslen(text), pTextFormat.Get(), &lr, pTextBrushLocal.Get());
        }
    }
}

void DrawProgressBarAdvanced(ID2D1RenderTarget* pDC, RECT rect, int percent) {
    if (!pDC) return;
    D2D1_ROUNDED_RECT bg = {D2D1::RectF((float)rect.left,(float)rect.top,(float)rect.right,(float)rect.bottom), 8, 8};
    ComPtr<ID2D1RoundedRectangleGeometry> pBg;
    pD2DFactory->CreateRoundedRectangleGeometry(&bg, pBg.GetAddressOf());
    if (!pBg) return;
    ComPtr<ID2D1SolidColorBrush> pBgBrush;
    D2D1_COLOR_F bgc = {0.96f, 0.97f, 0.98f, 1.0f};
    pDC->CreateSolidColorBrush(&bgc, NULL, pBgBrush.GetAddressOf());
    if (pBgBrush) pDC->FillGeometry(pBg.Get(), pBgBrush.Get());
    pBg.Reset();
    if (percent > 0) {
        float fillW = (float)(rect.right - rect.left) * percent / 100;
        if (fillW < 4) fillW = 4;
        D2D1_GRADIENT_STOP stops[2] = {
            {0, D2D1::ColorF(0.29f, 0.56f, 0.85f, 0.9f)},
            {1, D2D1::ColorF(0.42f, 0.71f, 1.0f, 0.9f)}
        };
        ComPtr<ID2D1GradientStopCollection> pS;
        pDC->CreateGradientStopCollection(stops, 2, pS.GetAddressOf());
        if (pS) {
            D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = {
                D2D1::Point2F((float)rect.left,0),
                D2D1::Point2F((float)rect.left+fillW,0)
            };
            ComPtr<ID2D1LinearGradientBrush> pB;
            pDC->CreateLinearGradientBrush(&props, NULL, pS.Get(), pB.GetAddressOf());
            if (pB) {
                D2D1_ROUNDED_RECT fr = {
                    D2D1::RectF((float)rect.left,(float)rect.top,(float)rect.left+fillW,(float)rect.bottom),
                    8, 8
                };
                ComPtr<ID2D1RoundedRectangleGeometry> pF;
                pD2DFactory->CreateRoundedRectangleGeometry(&fr, pF.GetAddressOf());
                if (pF) pDC->FillGeometry(pF.Get(), pB.Get());
            }
        }
    }
    if (pTextFormatPercent && pDC) {
        wchar_t t[16]; StringCchPrintfW(t, ARRAYSIZE(t), L"%d%%", percent);
        ComPtr<ID2D1SolidColorBrush> pP;
        D2D1_COLOR_F pc = {0.47f, 0.53f, 0.60f, 1.0f};
        pDC->CreateSolidColorBrush(&pc, NULL, pP.GetAddressOf());
        if (pP) {
            D2D1_RECT_F lr = {(float)rect.left,(float)rect.top,(float)rect.right-6,(float)rect.bottom};
            pDC->DrawText(t, wcslen(t), pTextFormatPercent.Get(), &lr, pP.Get());
        }
    }
}

void DrawCheckBox(ID2D1RenderTarget* pDC, RECT rect, int checked) {
    if (!pDC) return;
    D2D1_ROUNDED_RECT rounded;
    rounded.rect = D2D1::RectF((float)rect.left, (float)rect.top, (float)rect.right, (float)rect.bottom);
    rounded.radiusX = 4.0f;
    rounded.radiusY = 4.0f;
    ComPtr<ID2D1RoundedRectangleGeometry> pGeo;
    pD2DFactory->CreateRoundedRectangleGeometry(&rounded, pGeo.GetAddressOf());
    if (!pGeo) return;
    ComPtr<ID2D1SolidColorBrush> pBgBrush;
    D2D1_COLOR_F bgColor = checked ? D2D1::ColorF(0.29f, 0.56f, 0.85f, 1.0f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    pDC->CreateSolidColorBrush(&bgColor, NULL, pBgBrush.GetAddressOf());
    if (pBgBrush) pDC->FillGeometry(pGeo.Get(), pBgBrush.Get());
    ComPtr<ID2D1SolidColorBrush> pBorderBrush;
    D2D1_COLOR_F borderColor = checked ? D2D1::ColorF(0.29f, 0.56f, 0.85f, 1.0f) : D2D1::ColorF(0.8f, 0.85f, 0.9f, 1.0f);
    pDC->CreateSolidColorBrush(&borderColor, NULL, pBorderBrush.GetAddressOf());
    if (pBorderBrush) pDC->DrawGeometry(pGeo.Get(), pBorderBrush.Get(), 1.5f, NULL);
    if (checked) {
        ComPtr<ID2D1SolidColorBrush> pCheckBrush;
        D2D1_COLOR_F checkColor = {1.0f, 1.0f, 1.0f, 1.0f};
        pDC->CreateSolidColorBrush(&checkColor, NULL, pCheckBrush.GetAddressOf());
        if (pCheckBrush && pTextFormat) {
            WCHAR check[] = L"✓";
            D2D1_RECT_F lr = {(float)rect.left + 4, (float)rect.top + 2, (float)rect.right - 4, (float)rect.bottom - 2};
            pDC->DrawText(check, 1, pTextFormat.Get(), &lr, pCheckBrush.Get());
        }
    }
}

void DrawButtonAdvanced(ID2D1RenderTarget* pDC, RECT rc, const wchar_t* text, ButtonState state, BOOL primary) {
    if (!pDC) return;
    BOOL dis = (state == BTN_DISABLED), pres = (state == BTN_PRESSED), hover = (state == BTN_HOVER);
    D2D1_COLOR_F tc;
    D2D1_ROUNDED_RECT rounded = {
        D2D1::RectF((float)rc.left,(float)rc.top,(float)rc.right,(float)rc.bottom),
        12, 12
    };
    ComPtr<ID2D1RoundedRectangleGeometry> pGeo;
    pD2DFactory->CreateRoundedRectangleGeometry(&rounded, pGeo.GetAddressOf());
    if (!pGeo) return;
    if (primary) {
        D2D1_GRADIENT_STOP stops[2];
        if (dis) {
            stops[0].color = D2D1::ColorF(0.85f, 0.88f, 0.92f, 1.0f);
            stops[1].color = D2D1::ColorF(0.80f, 0.83f, 0.87f, 1.0f);
            tc = D2D1::ColorF(0.6f, 0.65f, 0.7f, 0.8f);
        } else if (pres) {
            stops[0].color = D2D1::ColorF(0.20f, 0.45f, 0.75f, 1.0f);
            stops[1].color = D2D1::ColorF(0.15f, 0.35f, 0.65f, 1.0f);
            tc = D2D1::ColorF(1,1,1,1);
        } else if (hover) {
            stops[0].color = D2D1::ColorF(0.32f, 0.60f, 0.90f, 1.0f);
            stops[1].color = D2D1::ColorF(0.25f, 0.50f, 0.80f, 1.0f);
            tc = D2D1::ColorF(1,1,1,1);
        } else {
            stops[0].color = D2D1::ColorF(0.29f, 0.56f, 0.85f, 1.0f);
            stops[1].color = D2D1::ColorF(0.22f, 0.46f, 0.75f, 1.0f);
            tc = D2D1::ColorF(1,1,1,1);
        }
        stops[0].position = 0; stops[1].position = 1;
        ComPtr<ID2D1GradientStopCollection> pS;
        pDC->CreateGradientStopCollection(stops, 2, pS.GetAddressOf());
        if (pS) {
            D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES props = {
                D2D1::Point2F(0,(float)rc.top),
                D2D1::Point2F(0,(float)rc.bottom)
            };
            ComPtr<ID2D1LinearGradientBrush> pB;
            pDC->CreateLinearGradientBrush(&props, NULL, pS.Get(), pB.GetAddressOf());
            if (pB) pDC->FillGeometry(pGeo.Get(), pB.Get());
        }
    } else {
        ComPtr<ID2D1SolidColorBrush> pBg;
        D2D1_COLOR_F bgc = {1.0f, 1.0f, 1.0f, dis ? 0.6f : (hover ? 0.95f : 1.0f)};
        pDC->CreateSolidColorBrush(&bgc, NULL, pBg.GetAddressOf());
        if (pBg) pDC->FillGeometry(pGeo.Get(), pBg.Get());
        ComPtr<ID2D1SolidColorBrush> pBord;
        D2D1_COLOR_F bc = dis ? D2D1::ColorF(0.88f, 0.90f, 0.93f, 1.0f) : (hover ? D2D1::ColorF(0.29f, 0.56f, 0.85f, 1.0f) : D2D1::ColorF(0.88f, 0.90f, 0.93f, 1.0f));
        pDC->CreateSolidColorBrush(&bc, NULL, pBord.GetAddressOf());
        if (pBord) pDC->DrawGeometry(pGeo.Get(), pBord.Get(), 1.5f, NULL);
        tc = dis ? D2D1::ColorF(0.6f, 0.65f, 0.7f, 0.8f) : (hover ? D2D1::ColorF(0.29f, 0.56f, 0.85f, 1.0f) : D2D1::ColorF(0.35f, 0.42f, 0.50f, 1.0f));
    }
    pGeo.Reset();
    if (pTextFormatBold && pDC) {
        ComPtr<ID2D1SolidColorBrush> pT;
        pDC->CreateSolidColorBrush(&tc, NULL, pT.GetAddressOf());
        if (pT) {
            D2D1_RECT_F lr = {(float)rc.left,(float)rc.top,(float)rc.right,(float)rc.bottom};
            pDC->DrawText(text, wcslen(text), pTextFormatBold.Get(), &lr, pT.Get());
        }
    }
}

void DrawSettingsWindowContent(HWND hWnd) {
    RECT rc; GetClientRect(hWnd, &rc);
    if (!pSettingsRenderTarget) return;
    pSettingsRenderTarget->BeginDraw();
    pSettingsRenderTarget->SetTransform(D2D1::IdentityMatrix());
    pSettingsRenderTarget->Clear(D2D1::ColorF(0.94f, 0.85f, 0.98f, 0.05f));
    DrawAestheticBackground(pSettingsRenderTarget.Get(), rc);
    RECT card = {15, 10, 365, 290};
    DrawGlassCardAdvanced(pSettingsRenderTarget.Get(), card);

    const wchar_t* title = GetText("settings_title");
    if (pSettingsWhiteBrush) {
        D2D1_RECT_F lr = {20, 16, 280, 52};
        ComPtr<IDWriteTextFormat> pTitleFont;
        pDWriteFactory->CreateTextFormat(L"Malgun Gothic", NULL, DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 22, L"ko-kr", &pTitleFont);
        if (pTitleFont) {
            pTitleFont->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            pTitleFont->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            pSettingsRenderTarget->DrawText(title, wcslen(title), pTitleFont.Get(), &lr, pSettingsTextBrush.Get());
        }
    }

    const wchar_t* speedLabel = GetText("settings_speed_limit");
    if (pTextFormatSmall && pSettingsTextBrush) {
        D2D1_RECT_F lr = {30, 68, 280, 90};
        pSettingsRenderTarget->DrawText(speedLabel, wcslen(speedLabel), pTextFormatSmall.Get(), &lr, pSettingsTextBrush.Get());
    }

    DrawEditBoxAdvanced(pSettingsRenderTarget.Get(), settingsSpeedInputRect, speedInputText, speedFocus);
    DrawButtonAdvanced(pSettingsRenderTarget.Get(), settingsUnitRect, units[selectedUnit], BTN_NORMAL, FALSE);

    const wchar_t* hint = GetText("settings_hint_zero");
    if (pTextFormatSmall && pSettingsSecondaryBrush) {
        D2D1_RECT_F lr = {30, 132, 150, 152};
        pSettingsRenderTarget->DrawText(hint, wcslen(hint), pTextFormatSmall.Get(), &lr, pSettingsSecondaryBrush.Get());
    }

    const wchar_t* verifyLabel = GetText("settings_verify");
    if (pTextFormatSmall && pSettingsTextBrush) {
        D2D1_RECT_F lr = {30, 162, 200, 182};
        pSettingsRenderTarget->DrawText(verifyLabel, wcslen(verifyLabel), pTextFormatSmall.Get(), &lr, pSettingsTextBrush.Get());
    }
    DrawCheckBox(pSettingsRenderTarget.Get(), settingsVerifyRect, enableVerify);

    const wchar_t* langLabel = GetText("settings_language");
    if (pTextFormatSmall && pSettingsTextBrush) {
        D2D1_RECT_F lr = {30, 190, 200, 210};
        pSettingsRenderTarget->DrawText(langLabel, wcslen(langLabel), pTextFormatSmall.Get(), &lr, pSettingsTextBrush.Get());
    }
    DrawButtonAdvanced(pSettingsRenderTarget.Get(), settingsLangRect, langNames[currentLang], BTN_NORMAL, FALSE);

    const wchar_t* saveLabel = GetText("settings_save");
    const wchar_t* cancelLabel = GetText("settings_cancel");
    DrawButtonAdvanced(pSettingsRenderTarget.Get(), settingsSaveRect, saveLabel, btnSettingsSaveState, TRUE);
    DrawButtonAdvanced(pSettingsRenderTarget.Get(), settingsCancelRect, cancelLabel, btnSettingsCancelState, FALSE);
    DrawButtonAdvanced(pSettingsRenderTarget.Get(), settingsCloseRect, L"×", BTN_NORMAL, FALSE);
    pSettingsRenderTarget->EndDraw();
}

LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            hSettingsWnd = hWnd;
            SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(hWnd, 0, 200, LWA_ALPHA);
            HRGN hrgn = CreateRoundRectRgn(0, 0, 400, 330, 28, 28);
            SetWindowRgn(hWnd, hrgn, TRUE);
            if (FAILED(InitSettingsD2D(hWnd))) {
                MessageBoxW(hWnd, L"设置窗口初始化失败！", L"错误", MB_OK);
            }
            speedFocus = TRUE;
            SetFocus(hWnd);
            break;
        }
        case WM_PAINT: {
            DrawSettingsWindowContent(hWnd);
            ValidateRect(hWnd, NULL);
            break;
        }
        case WM_LBUTTONDOWN: {
            POINT pt; pt.x = GET_X_LPARAM(lParam); pt.y = GET_Y_LPARAM(lParam);
            if (PtInRect(&settingsVerifyRect, pt)) {
                enableVerify = !enableVerify;
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (PtInRect(&settingsLangRect, pt)) {
                currentLang = (currentLang + 1) % 5;
                if (!isCopying) {
                    wcscpy(statusText, GetText("status_ready"));
                }
                InvalidateRect(hWnd, NULL, FALSE);
                InvalidateRect(hMainWnd, NULL, FALSE);
                return 0;
            }
            if (PtInRect(&settingsSpeedInputRect, pt)) {
                speedFocus = TRUE;
                InvalidateRect(hWnd, &settingsSpeedInputRect, FALSE);
                return 0;
            }
            speedFocus = FALSE;
            InvalidateRect(hWnd, &settingsSpeedInputRect, FALSE);
            if (PtInRect(&settingsUnitRect, pt)) {
                selectedUnit = (selectedUnit + 1) % 3;
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (PtInRect(&settingsSaveRect, pt)) {
                int val = _wtoi(speedInputText);
                if (val < 0) val = 0;
                if (val > 10240) val = 10240;
                speedLimitKB = val * unitMultipliers[selectedUnit];
                if (speedLimitKB < 0) speedLimitKB = 0;
                if (speedLimitKB > 10 * 1024 * 1024) speedLimitKB = 10 * 1024 * 1024;
                AnimateWindow(hWnd, 150, AW_HIDE | AW_CENTER);
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (PtInRect(&settingsCancelRect, pt)) {
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                return 0;
            }
            if (PtInRect(&settingsCloseRect, pt)) {
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        }
        case WM_KEYDOWN: {
            if (speedFocus) {
                if (wParam == VK_BACK) {
                    if (speedCaretPos > 0) {
                        for (int i = speedCaretPos - 1; i < speedInputLen - 1; i++) {
                            speedInputText[i] = speedInputText[i + 1];
                        }
                        speedCaretPos--;
                        speedInputLen--;
                        speedInputText[speedInputLen] = L'\0';
                        InvalidateRect(hWnd, &settingsSpeedInputRect, FALSE);
                    }
                } else if (wParam == VK_LEFT) {
                    if (speedCaretPos > 0) speedCaretPos--;
                } else if (wParam == VK_RIGHT) {
                    if (speedCaretPos < speedInputLen) speedCaretPos++;
                } else if (wParam == VK_ESCAPE) {
                    PostMessage(hWnd, WM_CLOSE, 0, 0);
                }
            }
            break;
        }
        case WM_CHAR: {
            if (speedFocus && wParam >= L'0' && wParam <= L'9') {
                wchar_t ch = (wchar_t)wParam;
                if (speedInputLen < 31) {
                    for (int i = speedInputLen; i > speedCaretPos; i--) {
                        speedInputText[i] = speedInputText[i - 1];
                    }
                    speedInputText[speedCaretPos] = ch;
                    speedCaretPos++;
                    speedInputLen++;
                    speedInputText[speedInputLen] = L'\0';
                    InvalidateRect(hWnd, &settingsSpeedInputRect, FALSE);
                }
            }
            break;
        }
        case WM_CLOSE: {
            DestroyWindow(hWnd);
            break;
        }
        case WM_DESTROY: {
            hSettingsWnd = NULL;
            pSettingsRenderTarget.Reset();
            pSettingsWhiteBrush.Reset();
            pSettingsTextBrush.Reset();
            pSettingsSecondaryBrush.Reset();
            PostMessage(hMainWnd, WM_SETTINGS_CLOSED, 0, 0);
            break;
        }
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

void ShowSettingsWindow(HINSTANCE hInst) {
    if (hSettingsWnd) {
        SetForegroundWindow(hSettingsWnd);
        return;
    }
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"CopyToolSettings";
    RegisterClassW(&wc);
    int x = 100, y = 100;
    RECT rc; GetWindowRect(hMainWnd, &rc);
    x = rc.left + 10; y = rc.top + 20;
    hSettingsWnd = CreateWindowW(L"CopyToolSettings", L"",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        x, y, 400, 330,
        hMainWnd, NULL, hInst, NULL);
    ShowWindow(hSettingsWnd, SW_SHOW);
    UpdateWindow(hSettingsWnd);
}

void DrawMainWindow(HWND hWnd) {
    RECT rect; GetClientRect(hWnd, &rect);
    if (!pRenderTarget) return;
    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::IdentityMatrix());
    pRenderTarget->Clear(D2D1::ColorF(0,0,0,0));
    DrawAestheticBackground(pRenderTarget.Get(), rect);
    DrawDecorations(pRenderTarget.Get(), rect);
    RECT card = {14, 12, 386, 336};
    DrawGlassCardAdvanced(pRenderTarget.Get(), card);

    if (pTextFormatTitle && pTextBrush) {
        WCHAR t[] = L"CopyTool";
        D2D1_RECT_F lr = {20, 18, 160, 54};
        pRenderTarget->DrawText(t, 8, pTextFormatTitle.Get(), &lr, pTextBrush.Get());
    }

    DrawButtonAdvanced(pRenderTarget.Get(), btnSettingRect, L"⚙", btnSettingState, FALSE);

    if (pTextFormat && pTextBrush) {
        const wchar_t* srcLabel = GetText("label_src");
        D2D1_RECT_F l1 = {30, 62, 110, 82};
        pRenderTarget->DrawText(srcLabel, wcslen(srcLabel), pTextFormat.Get(), &l1, pTextBrush.Get());

        const wchar_t* dstLabel = GetText("label_dst");
        D2D1_RECT_F l2 = {30, 126, 110, 146};
        pRenderTarget->DrawText(dstLabel, wcslen(dstLabel), pTextFormat.Get(), &l2, pTextBrush.Get());

        D2D1_RECT_F l3 = {30, 234, 180, 254};
        pRenderTarget->DrawText(statusText, wcslen(statusText), pTextFormat.Get(), &l3, pTextBrush.Get());
    }
    DrawEditBoxAdvanced(pRenderTarget.Get(), editSrcRect, srcPath, srcFocus);
    DrawEditBoxAdvanced(pRenderTarget.Get(), editDstRect, dstPath, dstFocus);

    DrawButtonAdvanced(pRenderTarget.Get(), btnBrowseSrcRect, L"...", btnBrowseSrcState, FALSE);
    DrawButtonAdvanced(pRenderTarget.Get(), btnBrowseDstRect, L"...", btnBrowseDstState, FALSE);
    DrawProgressBarAdvanced(pRenderTarget.Get(), progressRect, progressValue);

    const wchar_t* copyLabel = GetText("btn_copy");
    const wchar_t* pauseLabel = isPaused ? GetText("btn_resume") : GetText("btn_pause");
    const wchar_t* stopLabel = GetText("btn_stop");
    DrawButtonAdvanced(pRenderTarget.Get(), btnCopyRect, copyLabel, btnCopyState, TRUE);
    DrawButtonAdvanced(pRenderTarget.Get(), btnPauseRect, pauseLabel, btnPauseState, FALSE);
    DrawButtonAdvanced(pRenderTarget.Get(), btnStopRect, stopLabel, btnStopState, FALSE);
    pRenderTarget->EndDraw();
}

void GetTotalSize(const wchar_t* path, long long* pSize, int* pFileCount) {
    wchar_t sp[MAX_PATH]; WIN32_FIND_DATAW fd; StringCchPrintfW(sp, ARRAYSIZE(sp), L"%s\\*", path);
    HANDLE h = FindFirstFileW(sp, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == L'\0' || (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0'))) continue;
        StringCchPrintfW(sp, ARRAYSIZE(sp), L"%s\\%s", path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) GetTotalSize(sp, pSize, pFileCount);
        else { *pSize += (long long)fd.nFileSizeHigh << 32 | fd.nFileSizeLow; (*pFileCount)++; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

void EnumerateFiles(const wchar_t* path, const wchar_t* baseSrc, const wchar_t* baseDst) {
    wchar_t sp[MAX_PATH]; WIN32_FIND_DATAW fd; StringCchPrintfW(sp, ARRAYSIZE(sp), L"%s\\*", path);
    HANDLE h = FindFirstFileW(sp, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    wchar_t baseDstFixed[MAX_PATH];
    wcscpy(baseDstFixed, baseDst);
    size_t len = wcslen(baseDstFixed);
    if (len > 0 && baseDstFixed[len-1] != L'\\') {
        StringCchPrintfW(baseDstFixed, ARRAYSIZE(baseDstFixed), L"%s\\", baseDst);
    }
    do {
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == L'\0' || (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0'))) continue;
        wchar_t src[MAX_PATH], dst[MAX_PATH];
        StringCchPrintfW(src, ARRAYSIZE(src), L"%s\\%s", path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            EnumerateFiles(src, baseSrc, baseDstFixed);
        } else {
            wchar_t rel[MAX_PATH];
            const wchar_t* relStart = src + wcslen(baseSrc);
            while (*relStart == L'\\') relStart++;
            wcscpy(rel, relStart);
            StringCchPrintfW(dst, ARRAYSIZE(dst), L"%s%s", baseDstFixed, rel);
            EnqueueFile(src, dst);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

int CheckImpossibleTask(const wchar_t* src, const wchar_t* dst) {
    wchar_t srcFull[MAX_PATH];
    wchar_t dstFull[MAX_PATH];
    GetFullPathNameW(src, MAX_PATH, srcFull, NULL);
    GetFullPathNameW(dst, MAX_PATH, dstFull, NULL);
    size_t srcLen = wcslen(srcFull);
    size_t dstLen = wcslen(dstFull);
    if (_wcsnicmp(srcFull, dstFull, srcLen) == 0) {
        if (dstLen > srcLen && dstFull[srcLen] == L'\\') return 1;
    }
    if (_wcsnicmp(dstFull, srcFull, dstLen) == 0) {
        if (srcLen > dstLen && srcFull[dstLen] == L'\\') return 1;
    }
    if (_wcsicmp(srcFull, dstFull) == 0) return 1;
    return 0;
}

void CreateDirectoriesRecursive(const wchar_t* path) {
    wchar_t temp[MAX_PATH];
    wcscpy(temp, path);
    size_t len = wcslen(temp);
    if (len == 0) return;
    if (temp[len-1] == L'\\') temp[len-1] = L'\0';
    if (GetFileAttributesW(temp) != INVALID_FILE_ATTRIBUTES) return;
    wchar_t* p = wcsrchr(temp, L'\\');
    if (p) {
        *p = L'\0';
        CreateDirectoriesRecursive(temp);
        *p = L'\\';
    }
    CreateDirectoryW(temp, NULL);
}

DWORD WINAPI CopyThreadProc(LPVOID lpParam) {
    wchar_t* pPaths = (wchar_t*)lpParam;
    wchar_t* srcRoot = pPaths;
    wchar_t* dstRoot = pPaths + MAX_PATH;
    size_t srcLen = wcslen(srcRoot);
    if (srcLen > 0 && srcRoot[srcLen-1] == L'\\') {
        srcRoot[srcLen-1] = L'\0';
    }
    wchar_t folder[MAX_PATH];
    wchar_t* p = wcsrchr(srcRoot, L'\\');
    if (p && *(p+1) != L'\0') {
        wcscpy(folder, p + 1);
    } else if (p) {
        wchar_t temp[MAX_PATH];
        wcscpy(temp, srcRoot);
        size_t len = wcslen(temp);
        if (len > 0 && temp[len-1] == L'\\') {
            temp[len-1] = L'\0';
        }
        wchar_t* p2 = wcsrchr(temp, L'\\');
        if (p2 && *(p2+1) != L'\0') {
            wcscpy(folder, p2 + 1);
        } else {
            wcscpy(folder, temp);
        }
    } else {
        wcscpy(folder, srcRoot);
    }
    wchar_t dstRootFixed[MAX_PATH];
    wcscpy(dstRootFixed, dstRoot);
    size_t len = wcslen(dstRootFixed);
    if (len > 0 && dstRootFixed[len-1] != L'\\') {
        StringCchPrintfW(dstRootFixed, ARRAYSIZE(dstRootFixed), L"%s\\", dstRoot);
    }
    wchar_t finalDst[MAX_PATH];
    StringCchPrintfW(finalDst, ARRAYSIZE(finalDst), L"%s%s\\", dstRootFixed, folder);
    totalBytes = 0;
    totalFiles = 0;
    copiedBytes = 0;
    copiedFiles = 0;
    GetTotalSize(srcRoot, &totalBytes, &totalFiles);
    EnumerateFiles(srcRoot, srcRoot, finalDst);
    ApplyModeConfig(currentMode);
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    if (g_workerThreads > 0) {
        workerThreadCount = g_workerThreads;
    } else {
        workerThreadCount = si.dwNumberOfProcessors * 2;
        if (workerThreadCount < 4) workerThreadCount = 4;
        if (workerThreadCount > MAX_WORKER_THREADS) workerThreadCount = MAX_WORKER_THREADS;
    }
    if (workerThreadCount > totalFiles && totalFiles > 0) {
        workerThreadCount = (totalFiles > 1) ? totalFiles : 1;
    }
    PostMessage(hMainWnd, WM_UPDATE_PROGRESS, 0, 0);
    HANDLE* workers = (HANDLE*)malloc(workerThreadCount * sizeof(HANDLE));
    if (!workers) {
        PostMessage(hMainWnd, WM_THREAD_FINISHED, 0, 0);
        free(pPaths);
        return 0;
    }
    for (int i = 0; i < workerThreadCount; i++) {
        workers[i] = CreateThread(NULL, 0, WorkerThreadProc, NULL, 0, NULL);
        if (!workers[i]) {
            ShowError(hMainWnd, "dialog_error_thread");
        }
    }
    WaitForMultipleObjects(workerThreadCount, workers, TRUE, INFINITE);
    for (int i = 0; i < workerThreadCount; i++) {
        CloseHandle(workers[i]);
    }
    free(workers);
    EnterCriticalSection(&cs);
    int wasStopped = stopRequested;
    LeaveCriticalSection(&cs);
    PostMessage(hMainWnd, WM_THREAD_FINISHED, 0, 0);
    free(pPaths);
    return 0;
}

DWORD WINAPI WorkerThreadProc(LPVOID lpParam) {
    wchar_t src[MAX_PATH];
    wchar_t dst[MAX_PATH];
    while (1) {
        EnterCriticalSection(&cs);
        if (stopRequested) {
            LeaveCriticalSection(&cs);
            break;
        }
        if (!DequeueFile(src, dst)) {
            LeaveCriticalSection(&cs);
            break;
        }
        LeaveCriticalSection(&cs);
        wchar_t* p = wcsrchr(dst, L'\\');
        if (p) {
            wchar_t dir[MAX_PATH];
            memcpy(dir, dst, (p - dst) * sizeof(wchar_t));
            dir[p - dst] = L'\0';
            CreateDirectoriesRecursive(dir);
        }
        if (!CopySingleFile(src, dst)) {
            if (enableVerify) {
                ShowError(hMainWnd, "dialog_error_verify");
                EnqueueFile(src, dst);
            }
            continue;
        }
        if (enableVerify) {
            WIN32_FILE_ATTRIBUTE_DATA srcAttr, dstAttr;
            if (GetFileAttributesExW(src, GetFileExInfoStandard, &srcAttr) &&
                GetFileAttributesExW(dst, GetFileExInfoStandard, &dstAttr)) {
                LARGE_INTEGER srcSize, dstSize;
                srcSize.HighPart = srcAttr.nFileSizeHigh;
                srcSize.LowPart = srcAttr.nFileSizeLow;
                dstSize.HighPart = dstAttr.nFileSizeHigh;
                dstSize.LowPart = dstAttr.nFileSizeLow;
                if (srcSize.QuadPart != dstSize.QuadPart) {
                    ShowError(hMainWnd, "dialog_error_verify");
                    EnqueueFile(src, dst);
                }
            }
        }
    }
    return 0;
}

BOOL CopySingleFile(const wchar_t* src, const wchar_t* dst) {
    EnterCriticalSection(&cs);
    if (stopRequested) {
        LeaveCriticalSection(&cs);
        return FALSE;
    }
    BOOL paused = isPaused;
    LeaveCriticalSection(&cs);

    while (paused) {
        Sleep(50);
        EnterCriticalSection(&cs);
        if (stopRequested) {
            LeaveCriticalSection(&cs);
            return FALSE;
        }
        paused = isPaused;
        LeaveCriticalSection(&cs);
    }

    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesExW(src, GetFileExInfoStandard, &attr)) {
        ShowError(hMainWnd, "dialog_error_read");
        return FALSE;
    }
    LARGE_INTEGER fs;
    fs.HighPart = attr.nFileSizeHigh;
    fs.LowPart = attr.nFileSizeLow;

    if (fs.QuadPart <= LARGE_FILE_THRESHOLD) {
        ULONGLONG fileStart = GetTickCount64();

        BOOL ok = CopyFileExW(src, dst, NULL, NULL, NULL, 0);
        if (!ok) {
            ShowError(hMainWnd, "dialog_error_write_file");
            return FALSE;
        }

        if (speedLimitKB > 0 && fs.QuadPart > 0) {
            ULONGLONG expected = ((ULONGLONG)fs.QuadPart * 1000) / ((ULONGLONG)speedLimitKB * 1024);
            ULONGLONG elapsed = GetTickCount64() - fileStart;
            if (expected > elapsed) {
                Sleep((DWORD)(expected - elapsed));
            }
        }

        EnterCriticalSection(&cs);
        copiedBytes += fs.QuadPart;
        copiedFiles++;
        long long copiedNow = copiedBytes;
        LeaveCriticalSection(&cs);

        if (totalBytes > 0) {
            int pct = (int)((copiedNow * 100) / totalBytes);
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            PostMessage(hMainWnd, WM_UPDATE_PROGRESS, (WPARAM)pct, 0);
        }
        return TRUE;
    }

    DWORD srcFlags = g_srcFlags | FILE_FLAG_SEQUENTIAL_SCAN;
    DWORD dstFlags = g_dstFlags | FILE_FLAG_SEQUENTIAL_SCAN;

    HANDLE hSrc = CreateFileW(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, srcFlags, NULL);
    if (hSrc == INVALID_HANDLE_VALUE) {
        hSrc = CreateFileW(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hSrc == INVALID_HANDLE_VALUE) {
            ShowError(hMainWnd, "dialog_error_read");
            return FALSE;
        }
    }
    HANDLE hDst = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, dstFlags, NULL);
    if (hDst == INVALID_HANDLE_VALUE) {
        hDst = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hDst == INVALID_HANDLE_VALUE) {
            CloseHandle(hSrc);
            ShowError(hMainWnd, "dialog_error_write_file");
            return FALSE;
        }
    }

    size_t bufferSize = g_bufferSize;
    if (bufferSize == 0 || bufferSize > 256 * 1024 * 1024) {
        bufferSize = 32 * 1024 * 1024;
    }
    if ((LONGLONG)bufferSize > fs.QuadPart) {
        bufferSize = (size_t)fs.QuadPart;
        if (bufferSize == 0) bufferSize = 1;
    }

    if (g_preallocate && fs.QuadPart > 1024 * 1024) {
        LARGE_INTEGER dist;
        dist.QuadPart = fs.QuadPart;
        SetFilePointerEx(hDst, dist, NULL, FILE_BEGIN);
        SetEndOfFile(hDst);
        LARGE_INTEGER zero;
        zero.QuadPart = 0;
        SetFilePointerEx(hDst, zero, NULL, FILE_BEGIN);
    }

    char* buffer = (char*)malloc(bufferSize);
    if (!buffer) {
        CloseHandle(hSrc);
        CloseHandle(hDst);
        ShowError(hMainWnd, "dialog_error_memory");
        return FALSE;
    }

    DWORD bytesRead = 0;
    DWORD bytesWritten = 0;
    BOOL success = TRUE;
    ULONGLONG lastCheck = GetTickCount64();
    ULONGLONG lastProgress = 0;
    ULONGLONG totalWritten = 0;
    ULONGLONG blockStart = GetTickCount64();
    ULONGLONG blockWritten = 0;

    while (1) {
        BOOL readOk = ReadFile(hSrc, buffer, (DWORD)bufferSize, &bytesRead, NULL);
        if (!readOk || bytesRead == 0) break;

        DWORD written = 0;
        char* pBuf = buffer;
        while (written < bytesRead) {
            BOOL writeOk = WriteFile(hDst, pBuf, bytesRead - written, &bytesWritten, NULL);
            if (!writeOk || bytesWritten == 0) {
                success = FALSE;
                break;
            }
            pBuf += bytesWritten;
            written += bytesWritten;
            totalWritten += bytesWritten;
            blockWritten += bytesWritten;

            if (speedLimitKB > 0) {
                ULONGLONG now = GetTickCount64();
                ULONGLONG elapsed = now - blockStart;
                ULONGLONG expected = (blockWritten * 1000) / ((ULONGLONG)speedLimitKB * 1024);
                if (expected > elapsed) {
                    Sleep((DWORD)(expected - elapsed));
                }
                blockStart = GetTickCount64();
                blockWritten = 0;
            }
        }
        if (!success) break;

        ULONGLONG now = GetTickCount64();
        if (now - lastCheck >= 100) {
            lastCheck = now;
            EnterCriticalSection(&cs);
            if (stopRequested) {
                LeaveCriticalSection(&cs);
                success = FALSE;
                break;
            }
            paused = isPaused;
            LeaveCriticalSection(&cs);
            while (paused) {
                Sleep(50);
                EnterCriticalSection(&cs);
                if (stopRequested) {
                    LeaveCriticalSection(&cs);
                    success = FALSE;
                    break;
                }
                paused = isPaused;
                LeaveCriticalSection(&cs);
            }
            if (!success) break;
        }

        if (now - lastProgress >= 100) {
            lastProgress = now;
            EnterCriticalSection(&cs);
            copiedBytes += totalWritten;
            totalWritten = 0;
            long long copiedNow = copiedBytes;
            LeaveCriticalSection(&cs);
            if (totalBytes > 0) {
                int pct = (int)((copiedNow * 100) / totalBytes);
                if (pct < 0) pct = 0;
                if (pct > 100) pct = 100;
                PostMessage(hMainWnd, WM_UPDATE_PROGRESS, (WPARAM)pct, 0);
            }
        }
    }

    if (totalWritten > 0) {
        EnterCriticalSection(&cs);
        copiedBytes += totalWritten;
        LeaveCriticalSection(&cs);
    }

    free(buffer);
    CloseHandle(hSrc);
    CloseHandle(hDst);

    if (!success) {
        ShowError(hMainWnd, "dialog_error_write_file");
        return FALSE;
    }

    EnterCriticalSection(&cs);
    copiedFiles++;
    LeaveCriticalSection(&cs);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            InitializeCriticalSection(&cs);
            SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(hWnd, 0, 245, LWA_ALPHA);
            HRGN hrgn = CreateRoundRectRgn(0, 0, 400, 380, 24, 24);
            SetWindowRgn(hWnd, hrgn, TRUE);
            HRESULT d2dHr = InitD2D(hWnd);
            if (FAILED(d2dHr)) {
                wchar_t errorText[128];
                StringCchPrintfW(errorText, ARRAYSIZE(errorText), L"D2D初始化失败\nHRESULT: 0x%08lX", (unsigned long)d2dHr);
                MessageBoxW(hWnd, errorText, L"错误", MB_OK | MB_ICONERROR);
            }
            hMainWnd = hWnd;
            SetTimer(hWnd, 1, 50, NULL);
            SetFocus(hWnd);
            break;
        }
        case WM_TIMER: {
            if (wParam == 1) {
                InvalidateRect(hWnd, &progressRect, FALSE);
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            POINT pt; pt.x = GET_X_LPARAM(lParam); pt.y = GET_Y_LPARAM(lParam);
            if (PtInRect(&btnCopyRect, pt)) {
                btnCopyState = BTN_PRESSED;
                InvalidateRect(hWnd, &btnCopyRect, FALSE);
                PostMessage(hWnd, WM_COMMAND, 3, 0);
                return 0;
            }
            if (PtInRect(&btnPauseRect, pt)) {
                btnPauseState = BTN_PRESSED;
                InvalidateRect(hWnd, &btnPauseRect, FALSE);
                PostMessage(hWnd, WM_COMMAND, 4, 0);
                return 0;
            }
            if (PtInRect(&btnStopRect, pt)) {
                btnStopState = BTN_PRESSED;
                InvalidateRect(hWnd, &btnStopRect, FALSE);
                PostMessage(hWnd, WM_COMMAND, 5, 0);
                return 0;
            }
            if (PtInRect(&btnBrowseSrcRect, pt)) {
                PostMessage(hWnd, WM_COMMAND, 1, 0);
                return 0;
            }
            if (PtInRect(&btnBrowseDstRect, pt)) {
                PostMessage(hWnd, WM_COMMAND, 2, 0);
                return 0;
            }
            if (PtInRect(&btnSettingRect, pt)) {
                PostMessage(hWnd, WM_COMMAND, 6, 0);
                return 0;
            }
            if (PtInRect(&editSrcRect, pt)) {
                srcFocus = TRUE;
                dstFocus = FALSE;
                InvalidateRect(hWnd, &editSrcRect, FALSE);
                InvalidateRect(hWnd, &editDstRect, FALSE);
                return 0;
            }
            if (PtInRect(&editDstRect, pt)) {
                srcFocus = FALSE;
                dstFocus = TRUE;
                InvalidateRect(hWnd, &editSrcRect, FALSE);
                InvalidateRect(hWnd, &editDstRect, FALSE);
                return 0;
            }
            break;
        }
        case WM_LBUTTONUP: {
            if (btnCopyState == BTN_PRESSED) { btnCopyState = BTN_NORMAL; InvalidateRect(hWnd, &btnCopyRect, FALSE); }
            if (btnPauseState == BTN_PRESSED) { btnPauseState = isCopying ? BTN_NORMAL : BTN_DISABLED; InvalidateRect(hWnd, &btnPauseRect, FALSE); }
            if (btnStopState == BTN_PRESSED) { btnStopState = isCopying ? BTN_NORMAL : BTN_DISABLED; InvalidateRect(hWnd, &btnStopRect, FALSE); }
            break;
        }
        case WM_MOUSEMOVE: {
            POINT pt; pt.x = GET_X_LPARAM(lParam); pt.y = GET_Y_LPARAM(lParam);
            ButtonState nc = PtInRect(&btnCopyRect, pt) ? (btnCopyState == BTN_DISABLED ? BTN_DISABLED : BTN_HOVER) : (btnCopyState == BTN_DISABLED ? BTN_DISABLED : BTN_NORMAL);
            if (nc != btnCopyState) { btnCopyState = nc; InvalidateRect(hWnd, &btnCopyRect, FALSE); }
            ButtonState np = PtInRect(&btnPauseRect, pt) ? (btnPauseState == BTN_DISABLED ? BTN_DISABLED : BTN_HOVER) : (btnPauseState == BTN_DISABLED ? BTN_DISABLED : BTN_NORMAL);
            if (np != btnPauseState) { btnPauseState = np; InvalidateRect(hWnd, &btnPauseRect, FALSE); }
            ButtonState ns = PtInRect(&btnStopRect, pt) ? (btnStopState == BTN_DISABLED ? BTN_DISABLED : BTN_HOVER) : (btnStopState == BTN_DISABLED ? BTN_DISABLED : BTN_NORMAL);
            if (ns != btnStopState) { btnStopState = ns; InvalidateRect(hWnd, &btnStopRect, FALSE); }
            if (PtInRect(&btnCopyRect,pt)||PtInRect(&btnPauseRect,pt)||PtInRect(&btnStopRect,pt)||PtInRect(&btnBrowseSrcRect,pt)||PtInRect(&btnBrowseDstRect,pt)||PtInRect(&btnSettingRect,pt)) {
                SetCursor(LoadCursor(NULL, IDC_HAND));
            } else if (PtInRect(&editSrcRect,pt)||PtInRect(&editDstRect,pt)) {
                SetCursor(LoadCursor(NULL, IDC_IBEAM));
            } else {
                SetCursor(LoadCursor(NULL, IDC_ARROW));
            }
            break;
        }
        case WM_KEYDOWN: {
            wchar_t* target = NULL; int* caret = NULL, * len = NULL;
            if (srcFocus) { target = srcPath; caret = &srcCaretPos; len = &srcLen; }
            else if (dstFocus) { target = dstPath; caret = &dstCaretPos; len = &dstLen; }
            else break;
            if (wParam == VK_BACK) {
                if (*caret > 0) {
                    for (int i = *caret - 1; i < *len - 1; i++) { target[i] = target[i + 1]; }
                    (*caret)--; (*len)--; target[*len] = L'\0';
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            } else if (wParam == VK_LEFT) {
                if (*caret > 0) (*caret)--;
            } else if (wParam == VK_RIGHT) {
                if (*caret < *len) (*caret)++;
            } else if (wParam == VK_DELETE) {
                if (*caret < *len) {
                    for (int i = *caret; i < *len - 1; i++) { target[i] = target[i + 1]; }
                    (*len)--; target[*len] = L'\0';
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            break;
        }
        case WM_CHAR: {
            wchar_t ch = (wchar_t)wParam;
            if (ch >= 32 && ch <= 126) {
                wchar_t* target = NULL; int* caret = NULL, * len = NULL;
                if (srcFocus) { target = srcPath; caret = &srcCaretPos; len = &srcLen; }
                else if (dstFocus) { target = dstPath; caret = &dstCaretPos; len = &dstLen; }
                else break;
                if (*len < MAX_PATH - 1) {
                    for (int i = *len; i > *caret; i--) { target[i] = target[i - 1]; }
                    target[*caret] = ch; (*caret)++; (*len)++; target[*len] = L'\0';
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            break;
        }
        case WM_UPDATE_PROGRESS: {
            progressValue = (int)wParam;
            const wchar_t* status = GetText("status_copying");
            StringCchPrintfW(statusText, ARRAYSIZE(statusText), L"%s %d%%", status, progressValue);
            InvalidateRect(hWnd, &statusRect, FALSE);
            InvalidateRect(hWnd, &progressRect, FALSE);
            break;
        }
        case WM_THREAD_FINISHED: {
            EnterCriticalSection(&cs);
            int wasStopped = stopRequested;
            LeaveCriticalSection(&cs);
            if (wasStopped) {
                progressValue = 0; isCopying = 0; stopRequested = 0; isPaused = 0;
                btnCopyState = BTN_NORMAL; btnPauseState = BTN_DISABLED; btnStopState = BTN_DISABLED;
                wcscpy(statusText, GetText("status_stopped"));
                InvalidateRect(hWnd, NULL, FALSE);
                MessageBoxW(hWnd, GetText("dialog_stopped"), L"CopyTool", MB_OK);
            } else {
                progressValue = 100; isCopying = 0; isPaused = 0;
                btnCopyState = BTN_NORMAL; btnPauseState = BTN_DISABLED; btnStopState = BTN_DISABLED;
                const wchar_t* status = GetText("status_done");
                const wchar_t* done = GetText("dialog_done");
                StringCchPrintfW(statusText, ARRAYSIZE(statusText), L"%s %d %s", status, totalFiles, done);
                InvalidateRect(hWnd, NULL, FALSE);
                MessageBoxW(hWnd, done, L"CopyTool", MB_OK);
            }
            if (hCopyThread) { CloseHandle(hCopyThread); hCopyThread = NULL; }
            break;
        }
        case WM_SETTINGS_CLOSED: {
            InvalidateRect(hWnd, NULL, FALSE);
            break;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case 1: {
                    BROWSEINFOW bi = {0};
                    bi.lpszTitle = GetText("label_src");
                    bi.ulFlags = BIF_RETURNONLYFSDIRS;
                    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
                    if (pidl) {
                        wchar_t path[MAX_PATH];
                        if (SHGetPathFromIDListW(pidl, path)) {
                            wcscpy(srcPath, path);
                            srcLen = wcslen(srcPath);
                            InvalidateRect(hWnd, &editSrcRect, FALSE);
                        }
                        CoTaskMemFree(pidl);
                    }
                    break;
                }
                case 2: {
                    BROWSEINFOW bi = {0};
                    bi.lpszTitle = GetText("label_dst");
                    bi.ulFlags = BIF_RETURNONLYFSDIRS;
                    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
                    if (pidl) {
                        wchar_t path[MAX_PATH];
                        if (SHGetPathFromIDListW(pidl, path)) {
                            wcscpy(dstPath, path);
                            dstLen = wcslen(dstPath);
                            InvalidateRect(hWnd, &editDstRect, FALSE);
                        }
                        CoTaskMemFree(pidl);
                    }
                    break;
                }
                case 3: {
                    if (isCopying) break;
                    currentMode = MODE_AUTO;
                    if (!*srcPath || !*dstPath || CheckImpossibleTask(srcPath, dstPath)) {
                        ShowError(hWnd, "dialog_error_unable");
                        break;
                    }
                    DWORD sa = GetFileAttributesW(srcPath);
                    DWORD da = GetFileAttributesW(dstPath);
                    if (sa == INVALID_FILE_ATTRIBUTES || !(sa & FILE_ATTRIBUTE_DIRECTORY)) {
                        ShowError(hWnd, "dialog_error_unable");
                        break;
                    }
                    if (da == INVALID_FILE_ATTRIBUTES) {
                        ShowError(hWnd, "dialog_error_unable");
                        break;
                    }
                    progressValue = 0; isCopying = 1; isPaused = 0; stopRequested = 0;
                    btnCopyState = BTN_DISABLED; btnPauseState = BTN_NORMAL; btnStopState = BTN_NORMAL;
                    wcscpy(statusText, GetText("status_scanning"));
                    InvalidateRect(hWnd, NULL, FALSE);
                    wchar_t* ps = (wchar_t*)malloc(MAX_PATH * 2 * sizeof(wchar_t));
                    if (!ps) {
                        ShowError(hWnd, "dialog_error_memory");
                        isCopying = 0;
                        btnCopyState = BTN_NORMAL; btnPauseState = BTN_DISABLED; btnStopState = BTN_DISABLED;
                        wcscpy(statusText, GetText("status_ready"));
                        InvalidateRect(hWnd, NULL, FALSE);
                        break;
                    }
                    wchar_t* pd = ps + MAX_PATH;
                    wcscpy(ps, srcPath);
                    wcscpy(pd, dstPath);
                    hCopyThread = CreateThread(NULL, 0, CopyThreadProc, (LPVOID)ps, 0, NULL);
                    if (!hCopyThread) {
                        free(ps);
                        ShowError(hWnd, "dialog_error_thread");
                        isCopying = 0;
                        btnCopyState = BTN_NORMAL; btnPauseState = BTN_DISABLED; btnStopState = BTN_DISABLED;
                        wcscpy(statusText, GetText("status_ready"));
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                    break;
                }
                case 4: {
                    if (!isCopying) break;
                    EnterCriticalSection(&cs);
                    isPaused = !isPaused;
                    LeaveCriticalSection(&cs);
                    wcscpy(statusText, isPaused ? GetText("status_paused") : GetText("status_copying"));
                    InvalidateRect(hWnd, &statusRect, FALSE);
                    InvalidateRect(hWnd, &btnPauseRect, FALSE);
                    break;
                }
                case 5: {
                    if (!isCopying) break;
                    EnterCriticalSection(&cs);
                    stopRequested = 1;
                    isPaused = 0;
                    LeaveCriticalSection(&cs);
                    wcscpy(statusText, GetText("status_terminating"));
                    InvalidateRect(hWnd, &statusRect, FALSE);
                    btnPauseState = BTN_DISABLED;
                    btnStopState = BTN_DISABLED;
                    InvalidateRect(hWnd, &btnPauseRect, FALSE);
                    InvalidateRect(hWnd, &btnStopRect, FALSE);
                    break;
                }
                case 6: {
                    ShowSettingsWindow(GetModuleHandle(NULL));
                    break;
                }
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            DrawMainWindow(hWnd);
            EndPaint(hWnd, &ps);
            break;
        }
        case WM_ERASEBKGND: return 1;
        case WM_SIZE: {
            if (pRenderTarget) {
                RECT rc; GetClientRect(hWnd, &rc);
                UINT width = rc.right - rc.left;
                UINT height = rc.bottom - rc.top;
                pRenderTarget->Resize(D2D1::SizeU(width, height));
            }
            break;
        }
        case WM_DESTROY: {
            if (hCopyThread) {
                EnterCriticalSection(&cs);
                stopRequested = 1;
                LeaveCriticalSection(&cs);
                WaitForSingleObject(hCopyThread, 3000);
                CloseHandle(hCopyThread);
                hCopyThread = NULL;
            }
            ClearQueue();
            DeleteCriticalSection(&cs);
            pRenderTarget.Reset();
            pD2DFactory.Reset();
            pWhiteBrush.Reset();
            pTextBrush.Reset();
            pProgressBgBrush.Reset();
            pButtonTextBrush.Reset();
            pDWriteFactory.Reset();
            pTextFormat.Reset();
            pTextFormatBold.Reset();
            pTextFormatTitle.Reset();
            pTextFormatPercent.Reset();
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icex);
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"CopyTool";
    RegisterClassW(&wc);
    hMainWnd = CreateWindowW(L"CopyTool", L"CopyTool",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 380,
        NULL, NULL, hInst, NULL);
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(hMainWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    ShowWindow(hMainWnd, nShow);
    UpdateWindow(hMainWnd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
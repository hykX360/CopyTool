<<<<<<< HEAD
#define _WIN32_IE 0x0600
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600

#include <algorithm>
#include <windows.h>
#include <aclapi.h>
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
#pragma comment(lib, "aclapi.lib")
#pragma comment(lib, "advapi32.lib")

using Microsoft::WRL::ComPtr;

#define WM_UPDATE_PROGRESS (WM_USER + 100)
#define WM_THREAD_FINISHED (WM_USER + 101)
#define WM_SETTINGS_CLOSED (WM_USER + 102)
#define WM_VERIFY_STARTED (WM_USER + 103)

#define MAX_WORKER_THREADS 32
#define LARGE_FILE_THRESHOLD (1 * 1024 * 1024)

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

enum OptimizeStrategy {
    STRATEGY_AUTO,
    STRATEGY_SMALL,
    STRATEGY_LARGE,
    STRATEGY_BALANCED
};

int g_strategy = STRATEGY_AUTO;
size_t g_largeFileThreshold = 1 * 1024 * 1024;
BOOL g_wasAutoStrategy = FALSE;
HWND hStrategyCombo = NULL;
BOOL strategyDropdownOpen = FALSE;
int hoveredStrategyItem = -1;
BOOL langDropdownOpen = FALSE;
int hoveredLangItem = -1;
const wchar_t* GetText(const char* id);

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
    {L"SSD→SSD", L"SSD→SSD", 32, 256*1024*1024, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 10},
    {L"HDD→HDD", L"HDD→HDD", 4, 32*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 4},
    {L"HDD→SSD", L"HDD→SSD", 8, 64*1024*1024, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 7},
    {L"SSD→HDD", L"SSD→HDD", 4, 64*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 6},
    {L"SSD→UFD", L"SSD→UFD", 2, 4*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING, FALSE, 3},
    {L"UFD→SSD", L"UFD→SSD", 4, 16*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 6},
    {L"HDD→UFD", L"HDD→UFD", 2, 4*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING, FALSE, 2},
    {L"UFD→HDD", L"UFD→HDD", 2, 8*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 3},
    {L"UFD→UFD", L"UFD→UFD", 1, 2*1024*1024, 0, FILE_FLAG_NO_BUFFERING, FALSE, 1},
    {L"极速狂飙", L"极速", 32, 256*1024*1024, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 10}
};

typedef struct {
    int threads;
    size_t bufferSize;
    size_t largeThreshold;
    BOOL useNoBuffering;
    BOOL preallocate;
    const wchar_t* name;
} StrategyConfig;

static const StrategyConfig g_strategyConfigs[] = {
    {0, 0, 0, FALSE, FALSE, L"智能检测"},
    {16, 8*1024*1024, 8*1024*1024, FALSE, FALSE, L"小文件优化"},
    {32, 256*1024*1024, 1*1024*1024, TRUE, TRUE, L"大文件优化"},
    {32, 256*1024*1024, 1*1024*1024, TRUE, TRUE, L"平衡模式"}
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
    {"strategy_auto", L"智能检测", L"智慧偵測", L"Auto Detect", L"자동 감지", L"自動検出"},
    {"strategy_small", L"小文件优化", L"小檔案最佳化", L"Small Files", L"작은 파일", L"小ファイル最適化"},
    {"strategy_large", L"大文件优化", L"大檔案最佳化", L"Large Files", L"큰 파일", L"大ファイル最適化"},
    {"strategy_balanced", L"平衡模式", L"平衡模式", L"Balanced", L"균형", L"バランス"},
    {"nav_home", L"主页", L"主頁", L"Home", L"홈", L"ホーム"},
    {"remaining_time", L"还需", L"還需", L"Remaining", L"남음", L"残り"},
    {"scanning_files", L"扫描中... %d 个文件", L"掃描中... %d 個檔案", L"Scanning... %d files", L"스캔중... %d개 파일", L"スキャン中... %d個のファイル"},
    {"error_title", L"错误", L"錯誤", L"Error", L"오류", L"エラー"},
    {"settings_init_error", L"设置窗口初始化失败！", L"設定視窗初始化失敗！", L"Settings init failed!", L"설정 초기화 실패!", L"設定初期化失敗！"},
    {"d2d_init_error", L"D2D初始化失败", L"D2D初始化失敗", L"D2D init failed", L"D2D 초기화 실패", L"D2D初期化失敗"},
    {"speed_unit", L"MB/s", L"MB/s", L"MB/s", L"MB/s", L"MB/s"},
    {"failed_count", L"，%d 个失败", L"，%d 個失敗", L", %d failed", L", %d개 실패", L"、%d個失敗"},
    {"analyzing", L"分析文件分布...", L"分析檔案分布...", L"Analyzing files...", L"파일 분석중...", L"ファイル分析中..."},
    {"status_verifying", L"校验中", L"校驗中", L"Verifying", L"검증중", L"検証中"},
    {"verify_done", L"校验完成", L"校驗完成", L"Verification done", L"검증 완료", L"検証完了"},
    {"verify_failed", L"，%d 个校验失败", L"，%d 個校驗失敗", L", %d verify failed", L", %d개 검증 실패", L"、%d個検証失敗"}
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
int progressValue = 0, isCopying = 0;
volatile int isPaused = 0, stopRequested = 0;
HANDLE hCopyThread = NULL;
CRITICAL_SECTION cs;

int speedLimitKB = 0;
int enableVerify = 0;
int currentMode = MODE_AUTO;

wchar_t srcPath[MAX_PATH] = L"";
wchar_t dstPath[MAX_PATH] = L"";
wchar_t statusText[128] = L"准备就绪";
int srcCaretPos = 0, dstCaretPos = 0;
int srcLen = 0, dstLen = 0;
BOOL srcFocus = FALSE, dstFocus = FALSE;
BOOL srcHover = FALSE, dstHover = FALSE;

ButtonState btnCopyState = BTN_NORMAL, btnPauseState = BTN_DISABLED, btnStopState = BTN_DISABLED;
ButtonState btnBrowseSrcState = BTN_NORMAL, btnBrowseDstState = BTN_NORMAL, btnSettingState = BTN_NORMAL;
ButtonState btnSettingsSaveState = BTN_NORMAL, btnSettingsCancelState = BTN_NORMAL;

RECT btnCopyRect = {100, 310, 210, 352};
RECT btnPauseRect = {220, 310, 330, 352};
RECT btnStopRect = {340, 310, 450, 352};
RECT btnBrowseSrcRect = {478, 104, 518, 134};
RECT btnBrowseDstRect = {478, 168, 518, 198};
RECT btnSettingRect = {420, 18, 465, 48};
RECT editSrcRect = {100, 104, 470, 134};
RECT editDstRect = {100, 168, 470, 198};
RECT statusRect = {100, 266, 320, 286};
RECT progressRect = {100, 226, 518, 256};
RECT strategyComboRect = {398, 264, 518, 288};
RECT navHomeRect = {5, 28, 65, 86};
RECT navSettingsRect = {5, 96, 65, 154};
int currentPage = 0; // 0=主页, 1=设置
int targetPage = 0;
float pageAlpha = 1.0f;
float pageScale = 1.0f;
BOOL isPageTransitioning = FALSE;
int transitionPhase = 0; // 0=退出, 1=进入
ULONGLONG transitionStartTime = 0;
wchar_t currentFileName[MAX_PATH] = L"";

RECT settingsSpeedInputRect = {30, 94, 160, 126};
RECT settingsUnitRect = {170, 94, 230, 126};
RECT settingsSaveRect = {30, 244, 170, 280};
RECT settingsCancelRect = {190, 244, 330, 280};
RECT settingsCloseRect = {358, 8, 380, 30};
RECT settingsVerifyRect = {220, 163, 238, 181};
RECT settingsLangRect = {220, 186, 310, 214};
RECT settingsStrategyRect = {30, 222, 330, 244};

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
int g_prescanSmallCount = 0, g_prescanLargeCount = 0;
long long g_prescanSmallBytes = 0, g_prescanLargeBytes = 0;
BOOL g_countOnly = FALSE;

typedef struct FileNode {
    wchar_t src[MAX_PATH];
    wchar_t dst[MAX_PATH];
    struct FileNode* next;
} FileNode;

FileNode* queueHead = NULL;
FileNode* queueTail = NULL;
FileNode* verifyHead = NULL;  // 校验用文件列表（复制完后遍历）
FileNode* verifyTail = NULL;
BOOL isVerifying = FALSE;
long long verifiedBytes = 0;
int verifiedFiles = 0;
int failedVerifyFiles = 0;
long long totalBytes = 0;
long long copiedBytes = 0;
int totalFiles = 0;
int copiedFiles = 0;
int failedFiles = 0;
#define MAX_FAIL_LIST 256
wchar_t g_failList[MAX_FAIL_LIST][MAX_PATH] = {0};
DWORD g_failErrors[MAX_FAIL_LIST] = {0};
int g_failCount = 0;
int workerThreadCount = 8;
int filesScanned = 0;
int scanDone = 0;
int filesInQueue = 0;
HANDLE hScanThread = NULL;
double currentSpeedMB = 0.0;
ULONGLONG lastSpeedTime = 0;
long long lastSpeedBytes = 0;

// 线程本地复用缓冲区：避免每个大文件都malloc/free 256MB（最大性能瓶颈之一）
static __thread char* tls_copyBuf = NULL;
static __thread size_t tls_copyBufSize = 0;

// 已创建目录缓存：避免同目录下每个文件都重复GetFileAttributesW递归检查
#define MAX_DIR_CACHE 1024
static wchar_t g_dirCache[MAX_DIR_CACHE][MAX_PATH];
static volatile LONG g_dirCacheCount = 0;

static void DirCache_Init() { InterlockedExchange(&g_dirCacheCount, 0); }
static BOOL DirCache_Exists(const wchar_t* dir) {
    int n = g_dirCacheCount;
    for (int i = 0; i < n; i++) {
        if (_wcsicmp(g_dirCache[i], dir) == 0) return TRUE;
    }
    return FALSE;
}
static void DirCache_Add(const wchar_t* dir) {
    int idx = InterlockedIncrement(&g_dirCacheCount) - 1;
    if (idx < MAX_DIR_CACHE) {
        wcsncpy_s(g_dirCache[idx], MAX_PATH, dir, _TRUNCATE);
    }
}

DWORD WINAPI WorkerThreadProc(LPVOID lpParam);
DWORD WINAPI VerifyThreadProc(LPVOID lpParam);
BOOL CopySingleFile(const wchar_t* src, const wchar_t* dst);
BOOL VerifyFileCopy(const wchar_t* src, const wchar_t* dst);
void CreateDirectoriesRecursive(const wchar_t* path);
int CheckImpossibleTask(const wchar_t* src, const wchar_t* dst);



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
    // 叠加优化策略
    if (g_strategy != STRATEGY_AUTO) {
        const StrategyConfig* sc = &g_strategyConfigs[g_strategy];
        if (sc->threads > 0) { g_workerThreads = sc->threads; workerThreadCount = sc->threads; }
        if (sc->bufferSize > 0) g_bufferSize = sc->bufferSize;
        g_largeFileThreshold = sc->largeThreshold;
        g_preallocate = sc->preallocate;
        if (!sc->useNoBuffering) {
            g_srcFlags &= ~FILE_FLAG_NO_BUFFERING;
            g_dstFlags &= ~FILE_FLAG_NO_BUFFERING;
        } else {
            g_srcFlags |= FILE_FLAG_NO_BUFFERING;
            g_dstFlags |= FILE_FLAG_NO_BUFFERING;
        }
    } else {
        g_largeFileThreshold = 1 * 1024 * 1024;
    }
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
        ellipse1.point = D2D1::Point2F(215.0f, 36.0f);
        ellipse1.radiusX = 48.0f;
        ellipse1.radiusY = 48.0f;
        pDC->FillEllipse(&ellipse1, pPurpleBrush.Get());
    }
    ComPtr<ID2D1SolidColorBrush> pTealBrush;
    D2D1_COLOR_F tealColor = {0.20f, 0.83f, 0.60f, 0.30f};
    pDC->CreateSolidColorBrush(&tealColor, NULL, pTealBrush.GetAddressOf());
    if (pTealBrush) {
        D2D1_ELLIPSE ellipse2;
        ellipse2.point = D2D1::Point2F(240.0f, 52.0f);
        ellipse2.radiusX = 30.0f;
        ellipse2.radiusY = 30.0f;
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
            // 创建居中对齐的文字格式
            ComPtr<IDWriteTextFormat> pCenterFormat;
            pDWriteFactory->CreateTextFormat(L"Malgun Gothic", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16, L"", &pCenterFormat);
            if (pCenterFormat) {
                pCenterFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                pCenterFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                D2D1_RECT_F lr = {(float)rc.left,(float)rc.top,(float)rc.right,(float)rc.bottom};
                pDC->DrawText(text, wcslen(text), pCenterFormat.Get(), &lr, pT.Get());
            } else {
                D2D1_RECT_F lr = {(float)rc.left,(float)rc.top,(float)rc.right,(float)rc.bottom};
                pDC->DrawText(text, wcslen(text), pTextFormatBold.Get(), &lr, pT.Get());
            }
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
                MessageBoxW(hWnd, GetText("settings_init_error"), GetText("error_title"), MB_OK);
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

    // Fluent 侧边栏（柔和蓝白灰）
    {
        ComPtr<ID2D1SolidColorBrush> pNavBg;
        D2D1_COLOR_F navBg = {0.92f, 0.95f, 1.0f, 0.92f};
        pRenderTarget->CreateSolidColorBrush(&navBg, NULL, pNavBg.GetAddressOf());
        D2D1_RECT_F navRect = {0, 0, 70, (FLOAT)rect.bottom};
        pRenderTarget->FillRectangle(&navRect, pNavBg.Get());
        // 分隔线
        ComPtr<ID2D1SolidColorBrush> pNavBorder;
        D2D1_COLOR_F navBorder = {0.72f, 0.80f, 0.90f, 0.5f};
        pRenderTarget->CreateSolidColorBrush(&navBorder, NULL, pNavBorder.GetAddressOf());
        D2D1_RECT_F borderLine = {70, 0, 71, (FLOAT)rect.bottom};
        pRenderTarget->FillRectangle(&borderLine, pNavBorder.Get());

        ComPtr<ID2D1SolidColorBrush> pNavText;
        D2D1_COLOR_F navText = {0.22f, 0.30f, 0.48f, 1.0f};
        pRenderTarget->CreateSolidColorBrush(&navText, NULL, pNavText.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> pNavSelected;
        D2D1_COLOR_F navSelected = {0.68f, 0.82f, 1.0f, 0.55f};
        pRenderTarget->CreateSolidColorBrush(&navSelected, NULL, pNavSelected.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> pNavHover;
        D2D1_COLOR_F navHover = {0.82f, 0.89f, 0.98f, 0.7f};
        pRenderTarget->CreateSolidColorBrush(&navHover, NULL, pNavHover.GetAddressOf());

        // 居中文字格式（大图标用）
        ComPtr<IDWriteTextFormat> pNavIconFormat;
        pDWriteFactory->CreateTextFormat(L"Segoe UI Symbol", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 26, L"", &pNavIconFormat);
        if (pNavIconFormat) {
            pNavIconFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            pNavIconFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        // 设置图标用 Segoe MDL2 Assets（Windows原生标准图标）
        ComPtr<IDWriteTextFormat> pNavSettingsFormat;
        pDWriteFactory->CreateTextFormat(L"Segoe MDL2 Assets", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24, L"", &pNavSettingsFormat);
        if (pNavSettingsFormat) {
            pNavSettingsFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            pNavSettingsFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        ComPtr<IDWriteTextFormat> pNavLabelFormat;
        pDWriteFactory->CreateTextFormat(L"Malgun Gothic", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13, L"", &pNavLabelFormat);
        if (pNavLabelFormat) {
            pNavLabelFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            pNavLabelFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // 主页按钮
        {
            D2D1_ROUNDED_RECT btnR = D2D1::RoundedRect(
                D2D1::RectF((FLOAT)navHomeRect.left, (FLOAT)navHomeRect.top,
                             (FLOAT)navHomeRect.right, (FLOAT)navHomeRect.bottom), 8, 8);
            if (currentPage == 0) pRenderTarget->FillRoundedRectangle(&btnR, pNavSelected.Get());
            D2D1_RECT_F iconR = {(FLOAT)navHomeRect.left, (FLOAT)navHomeRect.top,
                                  (FLOAT)navHomeRect.right, (FLOAT)(navHomeRect.top + 36)};
            if (pNavIconFormat) pRenderTarget->DrawText(L"⌂", 1, pNavIconFormat.Get(), &iconR, pNavText.Get());
            D2D1_RECT_F labelR = {(FLOAT)navHomeRect.left, (FLOAT)(navHomeRect.top + 34),
                                   (FLOAT)navHomeRect.right, (FLOAT)navHomeRect.bottom};
            if (pNavLabelFormat) pRenderTarget->DrawText(GetText("nav_home"), wcslen(GetText("nav_home")), pNavLabelFormat.Get(), &labelR, pNavText.Get());
        }
        // 设置按钮
        {
            D2D1_ROUNDED_RECT btnR = D2D1::RoundedRect(
                D2D1::RectF((FLOAT)navSettingsRect.left, (FLOAT)navSettingsRect.top,
                             (FLOAT)navSettingsRect.right, (FLOAT)navSettingsRect.bottom), 8, 8);
            if (currentPage == 1) pRenderTarget->FillRoundedRectangle(&btnR, pNavSelected.Get());
            D2D1_RECT_F iconR = {(FLOAT)navSettingsRect.left, (FLOAT)navSettingsRect.top,
                                  (FLOAT)navSettingsRect.right, (FLOAT)(navSettingsRect.top + 36)};
            if (pNavSettingsFormat) pRenderTarget->DrawText(L"\uE713", 1, pNavSettingsFormat.Get(), &iconR, pNavText.Get());
            D2D1_RECT_F labelR = {(FLOAT)navSettingsRect.left, (FLOAT)(navSettingsRect.top + 34),
                                   (FLOAT)navSettingsRect.right, (FLOAT)navSettingsRect.bottom};
            if (pNavLabelFormat) pRenderTarget->DrawText(GetText("settings_title"), wcslen(GetText("settings_title")), pNavLabelFormat.Get(), &labelR, pNavText.Get());
        }
    }

    // 页面内容层（带淡入淡出+缩放动画，Motrix Next 风格）
    ComPtr<ID2D1Layer> pPageLayer;
    pRenderTarget->CreateLayer(NULL, pPageLayer.GetAddressOf());
    // 缩放变换（以页面中心为原点）
    D2D1_MATRIX_3X2_F pageTransform = D2D1::Matrix3x2F::Scale(
        pageScale, pageScale,
        D2D1::Point2F(305.0f, 214.0f)
    );
    pRenderTarget->SetTransform(pageTransform);
    D2D1_LAYER_PARAMETERS pageLayerParams = D2D1::LayerParameters();
    pageLayerParams.opacity = pageAlpha;
    pRenderTarget->PushLayer(pageLayerParams, pPageLayer.Get());

    if (currentPage == 0) {
    RECT card = {84, 12, 526, 416};
    DrawGlassCardAdvanced(pRenderTarget.Get(), card);

    if (pTextFormatTitle && pTextBrush) {
        WCHAR t[] = L"CopyTool";
        D2D1_RECT_F lr = {90, 18, 230, 54};
        pRenderTarget->DrawText(t, 8, pTextFormatTitle.Get(), &lr, pTextBrush.Get());
    }

    if (pTextFormat && pTextBrush) {
        const wchar_t* srcLabel = GetText("label_src");
        D2D1_RECT_F l1 = {100, 82, 180, 102};
        pRenderTarget->DrawText(srcLabel, wcslen(srcLabel), pTextFormat.Get(), &l1, pTextBrush.Get());

        const wchar_t* dstLabel = GetText("label_dst");
        D2D1_RECT_F l2 = {100, 146, 180, 166};
        pRenderTarget->DrawText(dstLabel, wcslen(dstLabel), pTextFormat.Get(), &l2, pTextBrush.Get());

        D2D1_RECT_F l3 = {100, 264, 320, 284};
        pRenderTarget->DrawText(statusText, wcslen(statusText), pTextFormat.Get(), &l3, pTextBrush.Get());
    }
    DrawEditBoxAdvanced(pRenderTarget.Get(), editSrcRect, srcPath, srcFocus || srcHover);
    DrawEditBoxAdvanced(pRenderTarget.Get(), editDstRect, dstPath, dstFocus || dstHover);

    DrawButtonAdvanced(pRenderTarget.Get(), btnBrowseSrcRect, L"...", btnBrowseSrcState, FALSE);
    DrawButtonAdvanced(pRenderTarget.Get(), btnBrowseDstRect, L"...", btnBrowseDstState, FALSE);
    DrawProgressBarAdvanced(pRenderTarget.Get(), progressRect, progressValue);

    const wchar_t* copyLabel = GetText("btn_copy");
    const wchar_t* pauseLabel = isPaused ? GetText("btn_resume") : GetText("btn_pause");
    const wchar_t* stopLabel = GetText("btn_stop");
    DrawButtonAdvanced(pRenderTarget.Get(), btnCopyRect, copyLabel, btnCopyState, TRUE);
    DrawButtonAdvanced(pRenderTarget.Get(), btnPauseRect, pauseLabel, btnPauseState, FALSE);
    DrawButtonAdvanced(pRenderTarget.Get(), btnStopRect, stopLabel, btnStopState, FALSE);

    // D2D 下拉框：策略选择（最后绘制，确保盖住其他控件）
    {
        const wchar_t* strategyNames[] = {GetText("strategy_auto"), GetText("strategy_small"), GetText("strategy_large"), GetText("strategy_balanced")};
        ComPtr<ID2D1SolidColorBrush> pDropBg;
        D2D1_COLOR_F dropBg = {0.96f, 0.98f, 1.0f, 1.0f};
        pRenderTarget->CreateSolidColorBrush(&dropBg, NULL, pDropBg.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> pDropText;
        D2D1_COLOR_F dropText = {0.18f, 0.22f, 0.32f, 1.0f};
        pRenderTarget->CreateSolidColorBrush(&dropText, NULL, pDropText.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> pDropHover;
        D2D1_COLOR_F dropHover = {0.80f, 0.88f, 1.0f, 1.0f};
        pRenderTarget->CreateSolidColorBrush(&dropHover, NULL, pDropHover.GetAddressOf());
        ComPtr<ID2D1SolidColorBrush> pDropBorder;
        D2D1_COLOR_F dropBorder = {0.60f, 0.72f, 0.90f, 0.9f};
        pRenderTarget->CreateSolidColorBrush(&dropBorder, NULL, pDropBorder.GetAddressOf());
        // 绘制下拉按钮
        D2D1_ROUNDED_RECT btnRect = D2D1::RoundedRect(
            D2D1::RectF((FLOAT)strategyComboRect.left, (FLOAT)strategyComboRect.top,
                         (FLOAT)strategyComboRect.right, (FLOAT)strategyComboRect.bottom), 6, 6);
        pRenderTarget->FillRoundedRectangle(&btnRect, pDropBg.Get());
        pRenderTarget->DrawRoundedRectangle(&btnRect, pDropBorder.Get(), 1);
        // 策略名称（垂直居中，水平左对齐）
        ComPtr<IDWriteTextFormat> pDropCenterFmt;
        pDWriteFactory->CreateTextFormat(L"Malgun Gothic", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14, L"", &pDropCenterFmt);
        if (pDropCenterFmt) {
            pDropCenterFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            pDropCenterFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        D2D1_RECT_F textR = {(FLOAT)(strategyComboRect.left + 10), (FLOAT)strategyComboRect.top,
                             (FLOAT)(strategyComboRect.right - 25), (FLOAT)strategyComboRect.bottom};
        pRenderTarget->DrawText(strategyNames[g_strategy], wcslen(strategyNames[g_strategy]),
                                 pDropCenterFmt ? pDropCenterFmt.Get() : pTextFormat.Get(), &textR, pDropText.Get());
        // 下拉箭头
        D2D1_RECT_F arrowR = {(FLOAT)(strategyComboRect.right - 20), (FLOAT)(strategyComboRect.top + 6),
                               (FLOAT)(strategyComboRect.right - 5), (FLOAT)(strategyComboRect.bottom - 6)};
        pRenderTarget->DrawText(L"▼", 1, pTextFormat.Get(), &arrowR, pDropText.Get());
        // 下拉列表（展开时盖住下方控件）
        if (strategyDropdownOpen) {
            int itemH = 30;
            int listH = itemH * 4;
            D2D1_ROUNDED_RECT listRect = D2D1::RoundedRect(
                D2D1::RectF((FLOAT)strategyComboRect.left, (FLOAT)(strategyComboRect.bottom + 2),
                             (FLOAT)strategyComboRect.right, (FLOAT)(strategyComboRect.bottom + 2 + listH)), 6, 6);
            pRenderTarget->FillRoundedRectangle(&listRect, pDropBg.Get());
            pRenderTarget->DrawRoundedRectangle(&listRect, pDropBorder.Get(), 1);
            for (int i = 0; i < 4; i++) {
                D2D1_RECT_F itemR = {(FLOAT)(strategyComboRect.left + 2),
                                      (FLOAT)(strategyComboRect.bottom + 4 + i * itemH),
                                      (FLOAT)(strategyComboRect.right - 2),
                                      (FLOAT)(strategyComboRect.bottom + 4 + (i + 1) * itemH)};
                if (i == hoveredStrategyItem || i == g_strategy) {
                    pRenderTarget->FillRectangle(&itemR, pDropHover.Get());
                }
                D2D1_RECT_F itemTextR = {(FLOAT)(strategyComboRect.left + 12),
                                          (FLOAT)(strategyComboRect.bottom + 4 + i * itemH),
                                          (FLOAT)(strategyComboRect.right - 10),
                                          (FLOAT)(strategyComboRect.bottom + 4 + (i + 1) * itemH)};
                pRenderTarget->DrawText(strategyNames[i], wcslen(strategyNames[i]),
                                         pDropCenterFmt ? pDropCenterFmt.Get() : pTextFormat.Get(), &itemTextR, pDropText.Get());
            }
        }
    }
    } else if (currentPage == 1) {
        // 设置页面
        RECT card = {84, 12, 526, 416};
        DrawGlassCardAdvanced(pRenderTarget.Get(), card);

        const wchar_t* title = GetText("settings_title");
        if (pTextFormatTitle && pTextBrush) {
            D2D1_RECT_F lr = {100, 20, 300, 56};
            pRenderTarget->DrawText(title, wcslen(title), pTextFormatTitle.Get(), &lr, pTextBrush.Get());
        }

        const wchar_t* speedLabel = GetText("settings_speed_limit");
        if (pTextFormatSmall && pTextBrush) {
            D2D1_RECT_F lr = {100, 72, 350, 94};
            pRenderTarget->DrawText(speedLabel, wcslen(speedLabel), pTextFormatSmall.Get(), &lr, pTextBrush.Get());
        }

        RECT speedInput = {100, 98, 260, 130};
        RECT unitBtn = {270, 98, 330, 130};
        DrawEditBoxAdvanced(pRenderTarget.Get(), speedInput, speedInputText, speedFocus);
        DrawButtonAdvanced(pRenderTarget.Get(), unitBtn, units[selectedUnit], BTN_NORMAL, FALSE);

        const wchar_t* hint = GetText("settings_hint_zero");
        if (pTextFormatSmall && pTextBrush) {
            D2D1_RECT_F lr = {100, 136, 250, 156};
            pRenderTarget->DrawText(hint, wcslen(hint), pTextFormatSmall.Get(), &lr, pTextBrush.Get());
        }

        const wchar_t* verifyLabel = GetText("settings_verify");
        if (pTextFormatSmall && pTextBrush) {
            D2D1_RECT_F lr = {100, 168, 300, 188};
            pRenderTarget->DrawText(verifyLabel, wcslen(verifyLabel), pTextFormatSmall.Get(), &lr, pTextBrush.Get());
        }
        RECT verifyCheck = {290, 169, 308, 187};
        DrawCheckBox(pRenderTarget.Get(), verifyCheck, enableVerify);

        const wchar_t* langLabel = GetText("settings_language");
        if (pTextFormatSmall && pTextBrush) {
            D2D1_RECT_F lr = {100, 198, 300, 218};
            pRenderTarget->DrawText(langLabel, wcslen(langLabel), pTextFormatSmall.Get(), &lr, pTextBrush.Get());
        }
        RECT langBtn = {100, 222, 260, 254};
        // 语言下拉框
        {
            ComPtr<ID2D1SolidColorBrush> pDropBg;
            D2D1_COLOR_F dropBg = {0.96f, 0.98f, 1.0f, 0.97f};
            pRenderTarget->CreateSolidColorBrush(&dropBg, NULL, pDropBg.GetAddressOf());
            ComPtr<ID2D1SolidColorBrush> pDropText;
            D2D1_COLOR_F dropText = {0.20f, 0.25f, 0.35f, 1.0f};
            pRenderTarget->CreateSolidColorBrush(&dropText, NULL, pDropText.GetAddressOf());
            ComPtr<ID2D1SolidColorBrush> pDropHover;
            D2D1_COLOR_F dropHover = {0.75f, 0.85f, 1.0f, 1.0f};
            pRenderTarget->CreateSolidColorBrush(&dropHover, NULL, pDropHover.GetAddressOf());
            ComPtr<ID2D1SolidColorBrush> pDropBorder;
            D2D1_COLOR_F dropBorder = {0.65f, 0.75f, 0.90f, 0.8f};
            pRenderTarget->CreateSolidColorBrush(&dropBorder, NULL, pDropBorder.GetAddressOf());
            // 下拉按钮
            D2D1_ROUNDED_RECT btnR = D2D1::RoundedRect(
                D2D1::RectF((FLOAT)langBtn.left, (FLOAT)langBtn.top, (FLOAT)langBtn.right, (FLOAT)langBtn.bottom), 8, 8);
            pRenderTarget->FillRoundedRectangle(&btnR, pDropBg.Get());
            pRenderTarget->DrawRoundedRectangle(&btnR, pDropBorder.Get(), 1);
            // 当前语言（居中）
            ComPtr<IDWriteTextFormat> pCenterFmt;
            pDWriteFactory->CreateTextFormat(L"Malgun Gothic", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15, L"", &pCenterFmt);
            if (pCenterFmt) {
                pCenterFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                pCenterFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                D2D1_RECT_F textR = {(FLOAT)(langBtn.left + 10), (FLOAT)langBtn.top, (FLOAT)(langBtn.right - 25), (FLOAT)langBtn.bottom};
                pRenderTarget->DrawText(langNames[currentLang], wcslen(langNames[currentLang]), pCenterFmt.Get(), &textR, pDropText.Get());
            }
            // 下拉箭头
            D2D1_RECT_F arrowR = {(FLOAT)(langBtn.right - 20), (FLOAT)(langBtn.top + 8), (FLOAT)(langBtn.right - 5), (FLOAT)(langBtn.bottom - 8)};
            pRenderTarget->DrawText(L"▼", 1, pTextFormat.Get(), &arrowR, pDropText.Get());
            // 下拉列表
            if (langDropdownOpen) {
                int itemH = 32;
                int listH = itemH * 5;
                D2D1_ROUNDED_RECT listRect = D2D1::RoundedRect(
                    D2D1::RectF((FLOAT)langBtn.left, (FLOAT)(langBtn.bottom + 2), (FLOAT)langBtn.right, (FLOAT)(langBtn.bottom + 2 + listH)), 8, 8);
                pRenderTarget->FillRoundedRectangle(&listRect, pDropBg.Get());
                pRenderTarget->DrawRoundedRectangle(&listRect, pDropBorder.Get(), 1);
                for (int i = 0; i < 5; i++) {
                    D2D1_RECT_F itemR = {(FLOAT)(langBtn.left + 2), (FLOAT)(langBtn.bottom + 4 + i * itemH),
                                          (FLOAT)(langBtn.right - 2), (FLOAT)(langBtn.bottom + 4 + (i + 1) * itemH)};
                    if (i == hoveredLangItem || i == currentLang) {
                        pRenderTarget->FillRectangle(&itemR, pDropHover.Get());
                    }
                    D2D1_RECT_F itemTextR = {(FLOAT)(langBtn.left + 10), (FLOAT)(langBtn.bottom + 4 + i * itemH),
                                              (FLOAT)(langBtn.right - 10), (FLOAT)(langBtn.bottom + 4 + (i + 1) * itemH)};
                    if (pCenterFmt) pRenderTarget->DrawText(langNames[i], wcslen(langNames[i]), pCenterFmt.Get(), &itemTextR, pDropText.Get());
                }
            }
        }

        const wchar_t* saveLabel = GetText("settings_save");
        RECT saveBtn = {100, 300, 240, 340};
        DrawButtonAdvanced(pRenderTarget.Get(), saveBtn, saveLabel, btnSettingsSaveState, TRUE);

        // 语言下拉列表最后绘制，确保不被保存按钮遮挡
        if (langDropdownOpen) {
            RECT langBtn = {100, 222, 260, 254};
            int itemH = 32;
            int listH = itemH * 5;
            ComPtr<ID2D1SolidColorBrush> pDropBg;
            D2D1_COLOR_F dropBg = {0.96f, 0.98f, 1.0f, 0.97f};
            pRenderTarget->CreateSolidColorBrush(&dropBg, NULL, pDropBg.GetAddressOf());
            ComPtr<ID2D1SolidColorBrush> pDropText;
            D2D1_COLOR_F dropText = {0.20f, 0.25f, 0.35f, 1.0f};
            pRenderTarget->CreateSolidColorBrush(&dropText, NULL, pDropText.GetAddressOf());
            ComPtr<ID2D1SolidColorBrush> pDropHover;
            D2D1_COLOR_F dropHover = {0.75f, 0.85f, 1.0f, 1.0f};
            pRenderTarget->CreateSolidColorBrush(&dropHover, NULL, pDropHover.GetAddressOf());
            ComPtr<ID2D1SolidColorBrush> pDropBorder;
            D2D1_COLOR_F dropBorder = {0.65f, 0.75f, 0.90f, 0.8f};
            pRenderTarget->CreateSolidColorBrush(&dropBorder, NULL, pDropBorder.GetAddressOf());

            D2D1_ROUNDED_RECT listRect = D2D1::RoundedRect(
                D2D1::RectF((FLOAT)langBtn.left, (FLOAT)(langBtn.bottom + 2),
                             (FLOAT)langBtn.right, (FLOAT)(langBtn.bottom + 2 + listH)), 8, 8);
            pRenderTarget->FillRoundedRectangle(&listRect, pDropBg.Get());
            pRenderTarget->DrawRoundedRectangle(&listRect, pDropBorder.Get(), 1);

            ComPtr<IDWriteTextFormat> pCenterFmt;
            pDWriteFactory->CreateTextFormat(L"Malgun Gothic", NULL, DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15, L"", &pCenterFmt);
            if (pCenterFmt) {
                pCenterFmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                pCenterFmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }

            for (int i = 0; i < 5; i++) {
                D2D1_RECT_F itemR = {(FLOAT)(langBtn.left + 2),
                                      (FLOAT)(langBtn.bottom + 4 + i * itemH),
                                      (FLOAT)(langBtn.right - 2),
                                      (FLOAT)(langBtn.bottom + 4 + (i + 1) * itemH)};
                if (i == hoveredLangItem || i == currentLang) {
                    pRenderTarget->FillRectangle(&itemR, pDropHover.Get());
                }
                D2D1_RECT_F itemTextR = {(FLOAT)(langBtn.left + 10),
                                          (FLOAT)(langBtn.bottom + 4 + i * itemH),
                                          (FLOAT)(langBtn.right - 10),
                                          (FLOAT)(langBtn.bottom + 4 + (i + 1) * itemH)};
                if (pCenterFmt) pRenderTarget->DrawText(langNames[i], wcslen(langNames[i]),
                    pCenterFmt.Get(), &itemTextR, pDropText.Get());
            }
        }
    }

    pRenderTarget->PopLayer();
    pRenderTarget->SetTransform(D2D1::IdentityMatrix());
    pRenderTarget->EndDraw();
}

void ScanDirectory(const wchar_t* path, const wchar_t* baseSrc, const wchar_t* baseDst) {
    wchar_t sp[MAX_PATH];
    WIN32_FIND_DATAW fd;
    StringCchPrintfW(sp, ARRAYSIZE(sp), L"%s\\*", path);
    // FIND_FIRST_EX_LARGE_FETCH: 大缓冲区批量读取目录，大目录扫描快约2倍（FastCopy同款）
    HANDLE h = FindFirstFileExW(sp, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (stopRequested) break;
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == L'\0' ||
            (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0'))) continue;
        wchar_t src[MAX_PATH], dst[MAX_PATH];
        StringCchPrintfW(src, ARRAYSIZE(src), L"%s\\%s", path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // 跳过reparse point（junction/符号链接），防止无限循环（FastCopy同款）
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            ScanDirectory(src, baseSrc, baseDst);
        } else {
            wchar_t rel[MAX_PATH];
            const wchar_t* relStart = src + wcslen(baseSrc);
            while (*relStart == L'\\') relStart++;
            wcscpy(rel, relStart);
            wchar_t baseDstFixed[MAX_PATH];
            wcscpy(baseDstFixed, baseDst);
            size_t len = wcslen(baseDstFixed);
            if (len > 0 && baseDstFixed[len-1] != L'\\') {
                StringCchPrintfW(baseDstFixed, ARRAYSIZE(baseDstFixed), L"%s\\", baseDst);
            }
            StringCchPrintfW(dst, ARRAYSIZE(dst), L"%s%s", baseDstFixed, rel);
            long long fsize = (long long)fd.nFileSizeHigh << 32 | fd.nFileSizeLow;
            if (g_countOnly) {
                if (fsize < 1024 * 1024) { g_prescanSmallCount++; g_prescanSmallBytes += fsize; }
                else { g_prescanLargeCount++; g_prescanLargeBytes += fsize; }
            } else {
                EnterCriticalSection(&cs);
                if (!stopRequested) {
                    EnqueueFile(src, dst);
                    // 同时加入校验列表（复制完后统一校验）
                    if (enableVerify) {
                        FileNode* vnode = (FileNode*)malloc(sizeof(FileNode));
                        if (vnode) {
                            wcscpy(vnode->src, src);
                            wcscpy(vnode->dst, dst);
                            vnode->next = NULL;
                            if (verifyTail) { verifyTail->next = vnode; verifyTail = vnode; }
                            else { verifyHead = verifyTail = vnode; }
                        }
                    }
                    filesInQueue++;
                    filesScanned++;
                    totalBytes += fsize;
                    totalFiles++;
                }
                LeaveCriticalSection(&cs);
            }
        }
    } while (FindNextFileW(h, &fd) && !stopRequested);
    FindClose(h);
}

DWORD WINAPI ScanThreadProc(LPVOID lpParam) {
    {
        typedef BOOL (WINAPI *SetThreadIoPriority_t)(HANDLE, int);
        SetThreadIoPriority_t p = (SetThreadIoPriority_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadIoPriority");
        if (p) p(GetCurrentThread(), 3);
    }
    wchar_t* pPaths = (wchar_t*)lpParam;
    wchar_t* srcRoot = pPaths;
    wchar_t* dstRoot = pPaths + MAX_PATH;
    size_t srcLen = wcslen(srcRoot);
    if (srcLen > 0 && srcRoot[srcLen-1] == L'\\') srcRoot[srcLen-1] = L'\0';
    wchar_t folder[MAX_PATH];
    wchar_t* p = wcsrchr(srcRoot, L'\\');
    if (p && *(p+1) != L'\0') wcscpy(folder, p + 1);
    else wcscpy(folder, srcRoot);
    wchar_t dstRootFixed[MAX_PATH];
    wcscpy(dstRootFixed, dstRoot);
    size_t len = wcslen(dstRootFixed);
    if (len > 0 && dstRootFixed[len-1] != L'\\') {
        StringCchPrintfW(dstRootFixed, ARRAYSIZE(dstRootFixed), L"%s\\", dstRoot);
    }
    wchar_t finalDst[MAX_PATH];
    StringCchPrintfW(finalDst, ARRAYSIZE(finalDst), L"%s%s\\", dstRootFixed, folder);
    ScanDirectory(srcRoot, srcRoot, finalDst);
    scanDone = 1;
    return 0;
}

// 驱动器类型
enum DiskType { DISK_UNKNOWN = 0, DISK_SSD, DISK_HDD, DISK_USB, DISK_NETWORK };

// 检测驱动器类型：SSD/HDD/U盘/网络
DiskType DetectDriveType(const wchar_t* path) {
    wchar_t root[8] = {0};
    root[0] = path[0]; root[1] = L':'; root[2] = L'\\'; root[3] = L'\0';
    UINT dt = GetDriveTypeW(root);
    if (dt == DRIVE_REMOTE) return DISK_NETWORK;
    if (dt == DRIVE_REMOVABLE) return DISK_USB;
    if (dt != DRIVE_FIXED) return DISK_UNKNOWN;
    wchar_t volPath[16];
    StringCchPrintfW(volPath, 16, L"\\\\.\\%c:", path[0]);
    HANDLE hVol = CreateFileW(volPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hVol == INVALID_HANDLE_VALUE) return DISK_HDD;
    struct { STORAGE_PROPERTY_QUERY q; DEVICE_TRIM_DESCRIPTOR d; } buf;
    memset(&buf, 0, sizeof(buf));
    buf.q.PropertyId = (STORAGE_PROPERTY_ID)8;
    buf.q.QueryType = PropertyStandardQuery;
    DWORD returned = 0;
    BOOL ok = DeviceIoControl(hVol, IOCTL_STORAGE_QUERY_PROPERTY,
        &buf.q, sizeof(buf.q), &buf.d, sizeof(buf.d), &returned, NULL);
    CloseHandle(hVol);
    if (ok && buf.d.TrimEnabled) return DISK_SSD;
    return DISK_HDD;
}

DWORD WINAPI CopyThreadProc(LPVOID lpParam) {
    wchar_t* pPaths = (wchar_t*)lpParam;
    totalBytes = 0; totalFiles = 0; copiedBytes = 0; copiedFiles = 0; failedFiles = 0;
    g_failCount = 0; memset(g_failList, 0, sizeof(g_failList)); memset(g_failErrors, 0, sizeof(g_failErrors));
    isVerifying = FALSE; verifiedBytes = 0; verifiedFiles = 0; failedVerifyFiles = 0;
    DirCache_Init();  // 清空目录缓存
    // 清空旧校验列表
    while (verifyHead) { FileNode* tmp = verifyHead; verifyHead = verifyHead->next; free(tmp); }
    verifyHead = verifyTail = NULL;
    filesScanned = 0; filesInQueue = 0; scanDone = 0;
    ClearQueue();
    // 智能检测模式：先预扫描统计文件大小分布，再决定策略
    if (g_strategy == STRATEGY_AUTO) {
        wchar_t* srcRoot = pPaths;
        wchar_t* dstRoot = pPaths + MAX_PATH;
        size_t srcLen = wcslen(srcRoot);
        if (srcLen > 0 && srcRoot[srcLen-1] == L'\\') srcRoot[srcLen-1] = L'\0';
        wchar_t folder[MAX_PATH];
        wchar_t* p = wcsrchr(srcRoot, L'\\');
        if (p && *(p+1) != L'\0') wcscpy(folder, p + 1);
        else wcscpy(folder, srcRoot);
        wchar_t dstRootFixed[MAX_PATH];
        wcscpy(dstRootFixed, dstRoot);
        size_t len = wcslen(dstRootFixed);
        if (len > 0 && dstRootFixed[len-1] != L'\\') {
            StringCchPrintfW(dstRootFixed, ARRAYSIZE(dstRootFixed), L"%s\\", dstRoot);
        }
        wchar_t finalDst[MAX_PATH];
        StringCchPrintfW(finalDst, ARRAYSIZE(finalDst), L"%s%s\\", dstRootFixed, folder);
        g_prescanSmallCount = 0; g_prescanLargeCount = 0;
        g_prescanSmallBytes = 0; g_prescanLargeBytes = 0;
        g_countOnly = TRUE;
        ScanDirectory(srcRoot, srcRoot, finalDst);
        g_countOnly = FALSE;
        int totalCnt = g_prescanSmallCount + g_prescanLargeCount;
        long long totalPrescanBytes = g_prescanSmallBytes + g_prescanLargeBytes;
        if (totalCnt > 0 && totalPrescanBytes > 0) {
            double smallCntRatio = (double)g_prescanSmallCount / totalCnt;
            double smallByteRatio = (double)g_prescanSmallBytes / totalPrescanBytes;
            double largeCntRatio = (double)g_prescanLargeCount / totalCnt;
            double largeByteRatio = (double)g_prescanLargeBytes / totalPrescanBytes;
            if (smallCntRatio > 0.7 && smallByteRatio < 0.3) g_strategy = STRATEGY_SMALL;
            else if (largeCntRatio > 0.3 && largeByteRatio > 0.7) g_strategy = STRATEGY_LARGE;
            else g_strategy = STRATEGY_BALANCED;
        } else {
            g_strategy = STRATEGY_BALANCED;
        }
        g_wasAutoStrategy = TRUE;
    }
    ApplyModeConfig(currentMode);
    // 同盘检测：源和目标在同一物理磁盘时减少线程，避免寻道抖动（FastCopy同款策略）
    wchar_t srcVol[MAX_PATH] = L"", dstVol[MAX_PATH] = L"";
    wcsncpy_s(srcVol, MAX_PATH, pPaths, 3); srcVol[2] = L'\\'; srcVol[3] = L'\0';
    wcsncpy_s(dstVol, MAX_PATH, pPaths + MAX_PATH, 3); dstVol[2] = L'\\'; dstVol[3] = L'\0';
    DWORD srcSerial = 0, dstSerial = 0;
    GetVolumeInformationW(srcVol, NULL, 0, &srcSerial, NULL, NULL, NULL, 0);
    GetVolumeInformationW(dstVol, NULL, 0, &dstSerial, NULL, NULL, NULL, 0);
    BOOL sameDisk = (srcSerial != 0 && srcSerial == dstSerial);
    // 驱动器类型检测与自动调优（学习FastCopy按介质类型选择策略）
    DiskType srcType = DetectDriveType(pPaths);
    DiskType dstType = DetectDriveType(pPaths + MAX_PATH);
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int workerCount = g_workerThreads > 0 ? g_workerThreads : si.dwNumberOfProcessors * 2;
    size_t tunedBuffer = g_bufferSize;
    if (sameDisk) {
        // 同盘：最少线程，避免磁头来回寻道
        if (workerCount > 4) workerCount = 4;
    } else if (srcType == DISK_NETWORK || dstType == DISK_NETWORK) {
        // 网络盘：中等线程，FastCopy建议网络I/O单元1MB
        if (workerCount > 8) workerCount = 8;
        if (tunedBuffer == 0 || tunedBuffer > 64 * 1024 * 1024) tunedBuffer = 64 * 1024 * 1024;
    } else if (srcType == DISK_USB || dstType == DISK_USB) {
        // U盘/移动硬盘：较少线程，中等缓冲
        if (workerCount > 8) workerCount = 8;
        if (tunedBuffer == 0 || tunedBuffer > 64 * 1024 * 1024) tunedBuffer = 64 * 1024 * 1024;
    } else if (srcType == DISK_HDD || dstType == DISK_HDD) {
        // 涉及HDD：限制线程数减少寻道，大缓冲保证顺序传输
        if (workerCount > 16) workerCount = 16;
    }
    // SSD→SSD：保持32线程+大缓冲，不限制
    if (workerCount < 2) workerCount = 2;
    if (workerCount > MAX_WORKER_THREADS) workerCount = MAX_WORKER_THREADS;
    g_bufferSize = tunedBuffer;
    HANDLE* workers = (HANDLE*)malloc(workerCount * sizeof(HANDLE));
    if (!workers) {
        PostMessage(hMainWnd, WM_THREAD_FINISHED, 0, 0);
        free(pPaths);
        return 0;
    }
    for (int i = 0; i < workerCount; i++) {
        workers[i] = CreateThread(NULL, 0, WorkerThreadProc, NULL, 0, NULL);
        if (!workers[i]) ShowError(hMainWnd, "dialog_error_thread");
    }
    hScanThread = CreateThread(NULL, 0, ScanThreadProc, pPaths, 0, NULL);
    if (!hScanThread) {
        ShowError(hMainWnd, "dialog_error_thread");
        free(workers);
        free(pPaths);
        return 0;
    }
    WaitForSingleObject(hScanThread, INFINITE);
    CloseHandle(hScanThread);
    hScanThread = NULL;
    WaitForMultipleObjects(workerCount, workers, TRUE, INFINITE);
    for (int i = 0; i < workerCount; i++) CloseHandle(workers[i]);
    free(workers);
    EnterCriticalSection(&cs);
    int wasStopped = stopRequested;
    LeaveCriticalSection(&cs);

    // 校验阶段：所有文件复制完后统一校验（多线程）
    if (enableVerify && !wasStopped && verifyHead) {
        isVerifying = TRUE;
        verifiedBytes = 0;
        verifiedFiles = 0;
        failedVerifyFiles = 0;
        copiedBytes = 0;
        progressValue = 0;
        PostMessage(hMainWnd, WM_VERIFY_STARTED, 0, 0);
        // 用4线程校验（读操作，不需要太多线程）
        int verifyThreads = 4;
        HANDLE verifyHandles[4];
        for (int i = 0; i < verifyThreads; i++) {
            verifyHandles[i] = CreateThread(NULL, 0, VerifyThreadProc, NULL, 0, NULL);
        }
        WaitForMultipleObjects(verifyThreads, verifyHandles, TRUE, INFINITE);
        for (int i = 0; i < verifyThreads; i++) CloseHandle(verifyHandles[i]);
        isVerifying = FALSE;
        verifyHead = verifyTail = NULL; // 线程内已free
    }

    PostMessage(hMainWnd, WM_THREAD_FINISHED, 0, 0);
    free(pPaths);
    return 0;
}

DWORD WINAPI WorkerThreadProc(LPVOID lpParam) {
    {
        typedef BOOL (WINAPI *SetThreadIoPriority_t)(HANDLE, int);
        SetThreadIoPriority_t p = (SetThreadIoPriority_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadIoPriority");
        if (p) p(GetCurrentThread(), 4);
    }
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    wchar_t src[MAX_PATH];
    wchar_t dst[MAX_PATH];
    while (1) {
        EnterCriticalSection(&cs);
        if (stopRequested) { LeaveCriticalSection(&cs); break; }
        if (DequeueFile(src, dst)) {
            filesInQueue--;
            LeaveCriticalSection(&cs);
            // 更新当前文件名（只取文件名部分）
            {
                const wchar_t* fname = wcsrchr(src, L'\\');
                if (fname) fname++; else fname = src;
                wcscpy(currentFileName, fname);
            }
            wchar_t* p = wcsrchr(dst, L'\\');
            if (p) {
                wchar_t dir[MAX_PATH];
                memcpy(dir, dst, (p - dst) * sizeof(wchar_t));
                dir[p - dst] = L'\0';
                CreateDirectoriesRecursive(dir);
            }
            // 复制（失败自动重试3次，应对瞬时错误：文件占用、杀毒软件干扰等）
            BOOL copyOk = FALSE;
            DWORD lastErr = 0;
            for (int retry = 0; retry < 3; retry++) {
                if (stopRequested) break;
                copyOk = CopySingleFile(src, dst);
                if (copyOk) break;
                lastErr = GetLastError();
                if (retry < 2) Sleep(200);
            }
            if (!copyOk) {
                // 错误跳过：不终止，不弹窗，只计数+记录
                InterlockedIncrement((LONG volatile*)&failedFiles);
                int idx = InterlockedIncrement((LONG volatile*)&g_failCount) - 1;
                if (idx < MAX_FAIL_LIST) {
                    wcscpy_s(g_failList[idx], MAX_PATH, src);
                    g_failErrors[idx] = lastErr;
                }
                continue;
            }
            InterlockedIncrement((LONG volatile*)&copiedFiles);
        } else {
            if (scanDone) { LeaveCriticalSection(&cs); break; }
            else { LeaveCriticalSection(&cs); Sleep(1); }
        }
    }
    return 0;
}

// 校验文件完整性：对比大小 + 逐字节比对
BOOL VerifyFileCopy(const wchar_t* src, const wchar_t* dst) {
    WIN32_FILE_ATTRIBUTE_DATA srcAttr, dstAttr;
    if (!GetFileAttributesExW(src, GetFileExInfoStandard, &srcAttr)) return FALSE;
    if (!GetFileAttributesExW(dst, GetFileExInfoStandard, &dstAttr)) return FALSE;
    // 1. 大小对比
    if (srcAttr.nFileSizeHigh != dstAttr.nFileSizeHigh || srcAttr.nFileSizeLow != dstAttr.nFileSizeLow) return FALSE;
    LARGE_INTEGER fs;
    fs.HighPart = srcAttr.nFileSizeHigh;
    fs.LowPart = srcAttr.nFileSizeLow;
    if (fs.QuadPart == 0) return TRUE; // 空文件无需比对内容
    // 2. 逐字节比对（用1MB缓冲）
    HANDLE hS = CreateFileW(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hS == INVALID_HANDLE_VALUE) return FALSE;
    HANDLE hD = CreateFileW(dst, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hD == INVALID_HANDLE_VALUE) { CloseHandle(hS); return FALSE; }
    const DWORD bufSize = 1024 * 1024;
    char* bufS = (char*)malloc(bufSize);
    char* bufD = (char*)malloc(bufSize);
    if (!bufS || !bufD) {
        free(bufS); free(bufD);
        CloseHandle(hS); CloseHandle(hD);
        return FALSE;
    }
    BOOL ok = TRUE;
    while (1) {
        DWORD brS = 0, brD = 0;
        if (!ReadFile(hS, bufS, bufSize, &brS, NULL)) { ok = FALSE; break; }
        if (!ReadFile(hD, bufD, bufSize, &brD, NULL)) { ok = FALSE; break; }
        if (brS != brD) { ok = FALSE; break; }
        if (brS == 0) break;
        if (memcmp(bufS, bufD, brS) != 0) { ok = FALSE; break; }
        if (stopRequested) { ok = FALSE; break; }
    }
    free(bufS); free(bufD);
    CloseHandle(hS); CloseHandle(hD);
    return ok;
}

// 强制打开源文件读取（学习FastCopy ForceCreateFileW）：
// 1. 正常打开（FILE_SHARE_READ，最快）
// 2. 共享冲突时用FILE_SHARE_READ|WRITE|DELETE重试
// 3. ACCESS_DENIED时清除只读属性+重置ACL重试
// 4. 最后尝试无NO_BUFFERING标志打开
HANDLE ForceCreateFileRead(const wchar_t* path, DWORD flags) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, flags, NULL);
    if (h != INVALID_HANDLE_VALUE) return h;
    DWORD err = GetLastError();
    // 共享冲突：放宽共享模式重试
    if (err == ERROR_SHARING_VIOLATION) {
        h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, flags, NULL);
        if (h != INVALID_HANDLE_VALUE) return h;
        err = GetLastError();
    }
    // 权限错误：清只读→重置ACL
    if (err == ERROR_ACCESS_DENIED) {
        SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
        h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, flags, NULL);
        if (h != INVALID_HANDLE_VALUE) return h;
        PACL emptyAcl = (PACL)LocalAlloc(LPTR, 256);
        if (emptyAcl) {
            if (InitializeAcl(emptyAcl, 256, ACL_REVISION)) {
                SetNamedSecurityInfoW((wchar_t*)path, SE_FILE_OBJECT,
                    DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION,
                    NULL, NULL, emptyAcl, NULL);
            }
            LocalFree(emptyAcl);
        }
        h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, flags, NULL);
        if (h != INVALID_HANDLE_VALUE) return h;
    }
    // 最后尝试：去掉NO_BUFFERING等特殊标志
    return CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

// 强制创建/打开目标文件写入（学习FastCopy）
HANDLE ForceCreateFileWrite(const wchar_t* path, DWORD flags) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, flags, NULL);
    if (h != INVALID_HANDLE_VALUE) return h;
    DWORD err = GetLastError();
    if (err == ERROR_ACCESS_DENIED) {
        SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
        h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, flags, NULL);
        if (h != INVALID_HANDLE_VALUE) return h;
        PACL emptyAcl = (PACL)LocalAlloc(LPTR, 256);
        if (emptyAcl) {
            if (InitializeAcl(emptyAcl, 256, ACL_REVISION)) {
                SetNamedSecurityInfoW((wchar_t*)path, SE_FILE_OBJECT,
                    DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION,
                    NULL, NULL, emptyAcl, NULL);
            }
            LocalFree(emptyAcl);
        }
        h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, flags, NULL);
        if (h != INVALID_HANDLE_VALUE) return h;
    }
    if (err == ERROR_SHARING_VIOLATION) {
        h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, flags, NULL);
        if (h != INVALID_HANDLE_VALUE) return h;
    }
    // 回退：不带特殊标志
    return CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

BOOL CopySingleFile(const wchar_t* src, const wchar_t* dst) {
    WIN32_FILE_ATTRIBUTE_DATA srcAttr, dstAttr;
    BOOL dstExists = GetFileAttributesExW(dst, GetFileExInfoStandard, &dstAttr);
    if (GetFileAttributesExW(src, GetFileExInfoStandard, &srcAttr)) {
        LARGE_INTEGER srcSize;
        srcSize.HighPart = srcAttr.nFileSizeHigh;
        srcSize.LowPart = srcAttr.nFileSizeLow;
        if (dstExists) {
            LARGE_INTEGER dstSize;
            dstSize.HighPart = dstAttr.nFileSizeHigh;
            dstSize.LowPart = dstAttr.nFileSizeLow;
            // 同时比较大小和修改时间（Robocopy同款），防止误跳过预分配的空壳文件
            BOOL sameSize = (dstSize.QuadPart == srcSize.QuadPart);
            BOOL sameTime = (dstAttr.ftLastWriteTime.dwHighDateTime == srcAttr.ftLastWriteTime.dwHighDateTime &&
                             dstAttr.ftLastWriteTime.dwLowDateTime == srcAttr.ftLastWriteTime.dwLowDateTime);
            if (sameSize && sameTime) {
                InterlockedAdd64((LONG64 volatile*)&copiedBytes, srcSize.QuadPart);
                return TRUE;
            }
        }
    }

    // 0字节文件：直接创建空文件，不走读写循环
    if (srcAttr.nFileSizeHigh == 0 && srcAttr.nFileSizeLow == 0) {
        HANDLE hEmpty = ForceCreateFileWrite(dst, FILE_ATTRIBUTE_NORMAL);
        if (hEmpty == INVALID_HANDLE_VALUE) return FALSE;
        // 保留时间戳和属性
        FILETIME ftC, ftA, ftW;
        HANDLE hSrc0 = ForceCreateFileRead(src, FILE_ATTRIBUTE_NORMAL);
        if (hSrc0 != INVALID_HANDLE_VALUE) {
            if (GetFileTime(hSrc0, &ftC, &ftA, &ftW))
                SetFileTime(hEmpty, &ftC, &ftA, &ftW);
            CloseHandle(hSrc0);
        }
        SetFileAttributesW(dst, srcAttr.dwFileAttributes);
        CloseHandle(hEmpty);
        return TRUE;
    }

    // 大文件：分块 + NO_BUFFERING 超越系统复制
    if (srcAttr.nFileSizeHigh > 0 || srcAttr.nFileSizeLow > (DWORD)g_largeFileThreshold) {
        LARGE_INTEGER fs;
        fs.HighPart = srcAttr.nFileSizeHigh;
        fs.LowPart = srcAttr.nFileSizeLow;
        DWORD srcFlags = g_srcFlags | FILE_FLAG_SEQUENTIAL_SCAN;
        DWORD dstFlags = g_dstFlags | FILE_FLAG_SEQUENTIAL_SCAN;
        HANDLE hSrc = ForceCreateFileRead(src, srcFlags);
        if (hSrc == INVALID_HANDLE_VALUE) return FALSE;
        HANDLE hDstLarge = ForceCreateFileWrite(dst, dstFlags);
        if (hDstLarge == INVALID_HANDLE_VALUE) { CloseHandle(hSrc); return FALSE; }
        size_t bufferSize = g_bufferSize;
        if (bufferSize == 0 || bufferSize > 256 * 1024 * 1024) bufferSize = 32 * 1024 * 1024;
        if ((LONGLONG)bufferSize > fs.QuadPart) bufferSize = (size_t)fs.QuadPart;
        if (bufferSize == 0) bufferSize = 1;
        bufferSize = (bufferSize / 4096) * 4096;
        if (bufferSize == 0) bufferSize = 4096;
        if (g_preallocate && fs.QuadPart > 1024 * 1024) {
            LARGE_INTEGER dist; dist.QuadPart = fs.QuadPart;
            SetFilePointerEx(hDstLarge, dist, NULL, FILE_BEGIN);
            SetEndOfFile(hDstLarge);
            SetFileValidData(hDstLarge, fs.QuadPart);
            LARGE_INTEGER zero; zero.QuadPart = 0;
            SetFilePointerEx(hDstLarge, zero, NULL, FILE_BEGIN);
        }
        // 线程本地复用缓冲区：只分配一次，后续文件直接复用（省去每文件256MB malloc/free）
        if (!tls_copyBuf || tls_copyBufSize < bufferSize) {
            if (tls_copyBuf) _aligned_free(tls_copyBuf);
            tls_copyBuf = (char*)_aligned_malloc(bufferSize, 4096);
            tls_copyBufSize = tls_copyBuf ? bufferSize : 0;
        }
        char* buffer = tls_copyBuf;
        if (!buffer) { CloseHandle(hSrc); CloseHandle(hDstLarge); return FALSE; }
        BOOL needTruncate = FALSE;
        BOOL success = TRUE;
        ULONGLONG totalWritten = 0;
        DWORD bytesRead = 0, bytesWritten = 0;
        const BOOL noSpeedLimit = (speedLimitKB == 0);

        while (1) {
            if (!ReadFile(hSrc, buffer, (DWORD)bufferSize, &bytesRead, NULL) || bytesRead == 0) break;
            DWORD writeSize = bytesRead;
            // NO_BUFFERING 最后一块补齐到4096倍数
            if ((g_dstFlags & FILE_FLAG_NO_BUFFERING) && (bytesRead % 4096 != 0)) {
                DWORD padded = ((bytesRead + 4095) & ~4095);
                memset(buffer + bytesRead, 0, padded - bytesRead);
                writeSize = padded;
                needTruncate = TRUE;
            }
            // 完整对齐块：一次WriteFile搞定（NO_BUFFERING下不会partial write）
            if (writeSize == (DWORD)bufferSize) {
                if (!WriteFile(hDstLarge, buffer, writeSize, &bytesWritten, NULL) || bytesWritten != writeSize) {
                    DWORD wErr = GetLastError();
                    if (wErr == ERROR_DISK_FULL) {
                        BOOL recovered = FALSE;
                        for (int dr = 0; dr < 5; dr++) {
                            Sleep(3000);
                            if (stopRequested) break;
                            if (WriteFile(hDstLarge, buffer, writeSize, &bytesWritten, NULL) && bytesWritten == writeSize) {
                                recovered = TRUE; break;
                            }
                        }
                        if (!recovered) { success = FALSE; break; }
                    } else { success = FALSE; break; }
                }
                totalWritten += writeSize;
            } else {
                // 最后一块：可能partial write，用循环
                DWORD written = 0;
                char* pBuf = buffer;
                while (written < writeSize) {
                    if (!WriteFile(hDstLarge, pBuf, writeSize - written, &bytesWritten, NULL) || bytesWritten == 0) {
                        DWORD wErr = GetLastError();
                        if (wErr == ERROR_DISK_FULL) {
                            BOOL recovered = FALSE;
                            for (int dr = 0; dr < 5; dr++) {
                                Sleep(3000);
                                if (stopRequested) break;
                                if (WriteFile(hDstLarge, pBuf, writeSize - written, &bytesWritten, NULL) && bytesWritten > 0) {
                                    recovered = TRUE; break;
                                }
                            }
                            if (!recovered) { success = FALSE; break; }
                        } else { success = FALSE; break; }
                    }
                    pBuf += bytesWritten;
                    written += bytesWritten;
                    totalWritten += bytesWritten;
                }
            }
            if (!success) break;
            if (stopRequested) { success = FALSE; break; }
            while (isPaused) {
                Sleep(50);
                if (stopRequested) { success = FALSE; break; }
            }
            if (!success) break;
            // 速度限制（仅在启用时检查，不污染热路径）
            if (!noSpeedLimit) {
                static __thread ULONGLONG winStart = 0;
                static __thread LONG64 winBytes = 0;
                ULONGLONG nowMs = GetTickCount64();
                if (winStart == 0 || nowMs - winStart >= 1000) { winStart = nowMs; winBytes = 0; }
                winBytes += (LONG64)writeSize;
                LONG64 limitBytes = (LONG64)speedLimitKB * 1024;
                if (winBytes > limitBytes) {
                    DWORD sleepMs = (DWORD)(1000 - (nowMs - winStart));
                    if (sleepMs > 0 && sleepMs < 1000) Sleep(sleepMs);
                    winStart = GetTickCount64(); winBytes = 0;
                }
            }
            InterlockedAdd64((LONG64 volatile*)&copiedBytes, (LONG64)totalWritten);
            totalWritten = 0;
        }
        // 截断到真实文件大小（去掉补齐的零）
        if (needTruncate && success) {
            SetFilePointerEx(hDstLarge, fs, NULL, FILE_BEGIN);
            SetEndOfFile(hDstLarge);
        }
        if (!success) {
            CloseHandle(hSrc);
            CloseHandle(hDstLarge);
            DeleteFileW(dst);
            return FALSE;
        }
        // 保留源文件时间戳（必须在 CloseHandle 之前）
        FILETIME ftCreate, ftAccess, ftWrite;
        if (GetFileTime(hSrc, &ftCreate, &ftAccess, &ftWrite)) {
            SetFileTime(hDstLarge, &ftCreate, &ftAccess, &ftWrite);
        }
        // 保留文件属性
        SetFileAttributesW(dst, srcAttr.dwFileAttributes);
        CloseHandle(hSrc);
        CloseHandle(hDstLarge);
        return TRUE;
    }

    // 小文件：系统级复制最快
    BOOL result = CopyFileExW(src, dst, NULL, NULL, NULL, 0);
    if (!result && GetLastError() == ERROR_ACCESS_DENIED) {
        // 目标可能是只读文件，清除属性后重试（FastCopy同款）
        SetFileAttributesW(dst, FILE_ATTRIBUTE_NORMAL);
        result = CopyFileExW(src, dst, NULL, NULL, NULL, 0);
    }
    if (!result) return FALSE;
    long long fsize = (long long)srcAttr.nFileSizeHigh << 32 | srcAttr.nFileSizeLow;
    InterlockedAdd64((LONG64 volatile*)&copiedBytes, fsize);
    return TRUE;
}

// 多线程校验工作线程
DWORD WINAPI VerifyThreadProc(LPVOID) {
    while (1) {
        if (stopRequested) break;
        EnterCriticalSection(&cs);
        FileNode* cur = verifyHead;
        if (cur) verifyHead = verifyHead->next;
        LeaveCriticalSection(&cs);
        if (!cur) break;
        long long fsize = 0;
        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (GetFileAttributesExW(cur->src, GetFileExInfoStandard, &attr)) {
            fsize = (long long)attr.nFileSizeHigh << 32 | attr.nFileSizeLow;
        }
        if (VerifyFileCopy(cur->src, cur->dst)) {
            InterlockedIncrement((LONG volatile*)&verifiedFiles);
        } else {
            InterlockedIncrement((LONG volatile*)&failedVerifyFiles);
            int idx = InterlockedIncrement((LONG volatile*)&g_failCount) - 1;
            if (idx < MAX_FAIL_LIST) {
                wcscpy_s(g_failList[idx], MAX_PATH, cur->src);
                g_failErrors[idx] = 999;
            }
        }
        InterlockedAdd64((LONG64 volatile*)&verifiedBytes, fsize);
        free(cur);
    }
    return 0;
}

void CreateDirectoriesRecursive(const wchar_t* path) {
    wchar_t temp[MAX_PATH];
    wcscpy(temp, path);
    size_t len = wcslen(temp);
    if (len == 0) return;
    if (temp[len-1] == L'\\') temp[len-1] = L'\0';
    if (DirCache_Exists(temp)) return;  // 缓存命中，跳过
    if (GetFileAttributesW(temp) != INVALID_FILE_ATTRIBUTES) {
        DirCache_Add(temp);
        return;
    }
    wchar_t* p = wcsrchr(temp, L'\\');
    if (p) {
        *p = L'\0';
        CreateDirectoriesRecursive(temp);
        *p = L'\\';
    }
    CreateDirectoryW(temp, NULL);
    DirCache_Add(temp);
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

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            InitializeCriticalSection(&cs);
            SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(hWnd, 0, 245, LWA_ALPHA);
            HRGN hrgn = CreateRoundRectRgn(0, 0, 540, 460, 24, 24);
            SetWindowRgn(hWnd, hrgn, TRUE);
            HRESULT d2dHr = InitD2D(hWnd);
            if (FAILED(d2dHr)) {
                wchar_t errorText[128];
                StringCchPrintfW(errorText, ARRAYSIZE(errorText), L"%s\nHRESULT: 0x%08lX", GetText("d2d_init_error"), (unsigned long)d2dHr);
                MessageBoxW(hWnd, errorText, GetText("error_title"), MB_OK | MB_ICONERROR);
            }
            hMainWnd = hWnd;
            DragAcceptFiles(hWnd, TRUE);
            SetTimer(hWnd, 1, 200, NULL);
            SetFocus(hWnd);
            break;
        }
        case WM_TIMER: {
            if (wParam == 1) {
                // 页面过渡动画（Motrix Next 风格：非对称时长+贝塞尔曲线+缩放）
                if (isPageTransitioning) {
                    ULONGLONG now = GetTickCount64();
                    float elapsed = (float)(now - transitionStartTime);
                    if (transitionPhase == 0) {
                        // 退出阶段：150ms，加速曲线 cubic-bezier(0.3, 0, 0.8, 0.15)
                        float t = elapsed / 150.0f; if (t > 1.0f) t = 1.0f;
                        float ease = t * t * (3.0f - 2.0f * t); // smoothstep 近似加速
                        pageAlpha = 1.0f - ease;
                        pageScale = 1.0f - 0.02f * ease;
                        if (t >= 1.0f) {
                            currentPage = targetPage;
                            transitionPhase = 1;
                            transitionStartTime = now;
                        }
                    } else {
                        // 进入阶段：200ms，减速曲线 cubic-bezier(0.2, 0, 0, 1)
                        float t = elapsed / 200.0f; if (t > 1.0f) t = 1.0f;
                        float ease = 1.0f - (1.0f - t) * (1.0f - t); // 减速曲线
                        pageAlpha = ease;
                        pageScale = 0.98f + 0.02f * ease;
                        if (t >= 1.0f) {
                            pageAlpha = 1.0f;
                            pageScale = 1.0f;
                            isPageTransitioning = FALSE;
                            SetTimer(hWnd, 1, 200, NULL); // 恢复正常刷新率
                        }
                    }
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                if (isCopying) {
                    ULONGLONG now = GetTickCount64();
                    long long curBytes = isVerifying ? verifiedBytes : copiedBytes;
                    if (lastSpeedTime == 0) { lastSpeedTime = now; lastSpeedBytes = curBytes; }
                    if (now - lastSpeedTime >= 1000) {
                        double deltaMB = (double)(curBytes - lastSpeedBytes) / (1024.0 * 1024.0);
                        double deltaSec = (double)(now - lastSpeedTime) / 1000.0;
                        if (deltaMB > 10.0) {
                            currentSpeedMB = deltaSec > 0 ? deltaMB / deltaSec : 0;
                        }
                        lastSpeedTime = now;
                        lastSpeedBytes = curBytes;
                    }
                    if (!scanDone) {
                        progressValue = 0;
                        if (g_countOnly) {
                            StringCchPrintfW(statusText, ARRAYSIZE(statusText), L"%s", GetText("analyzing"));
                        } else {
                            StringCchPrintfW(statusText, ARRAYSIZE(statusText), GetText("scanning_files"), filesScanned);
                        }
                    } else {
                        long long progressBytes = isVerifying ? verifiedBytes : copiedBytes;
                        if (totalBytes > 0) {
                            int pct = (int)((progressBytes * 100) / totalBytes);
                            if (pct < 0) pct = 0;
                            if (pct > 100) pct = 100;
                            if (isVerifying) {
                                progressValue = pct;
                            } else {
                                static int lastProgress = 0;
                                if (pct == 0) lastProgress = 0;
                                if (pct < lastProgress) pct = lastProgress;
                                else lastProgress = pct;
                                progressValue = pct;
                            }
                        }
                        const wchar_t* status = isVerifying ? GetText("status_verifying") : GetText("status_copying");
                        // 剩余时间估算
                        wchar_t remainStr[32] = L"";
                        if (currentSpeedMB > 0.1 && totalBytes > progressBytes) {
                            double remainBytes = (double)(totalBytes - progressBytes);
                            double remainSec = remainBytes / (currentSpeedMB * 1024.0 * 1024.0);
                            if (remainSec < 3600) {
                                int mins = (int)(remainSec / 60);
                                int secs = (int)(remainSec - mins * 60);
                                StringCchPrintfW(remainStr, ARRAYSIZE(remainStr), L"  %s %d:%02d", GetText("remaining_time"), mins, secs);
                            } else {
                                int hours = (int)(remainSec / 3600);
                                int mins = (int)((remainSec - hours * 3600) / 60);
                                StringCchPrintfW(remainStr, ARRAYSIZE(remainStr), L"  %s %d:%02d:%02d", GetText("remaining_time"), hours, mins, (int)(remainSec - hours*3600 - mins*60));
                            }
                        }
                        StringCchPrintfW(statusText, ARRAYSIZE(statusText), L"%s %d%%  %.1f %s%s", status, progressValue, currentSpeedMB, GetText("speed_unit"), remainStr);
                    }
                    InvalidateRect(hWnd, &statusRect, FALSE);
                    InvalidateRect(hWnd, &progressRect, FALSE);
                }
            }
            break;
        }
        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            wchar_t path[MAX_PATH];
            
            POINT pt;
            DragQueryPoint(hDrop, &pt);
            
            if (DragQueryFileW(hDrop, 0, path, MAX_PATH) > 0) {
                DWORD attrs = GetFileAttributesW(path);
                if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                    if (PtInRect(&editSrcRect, pt)) {
                        wcscpy(srcPath, path);
                        srcLen = wcslen(srcPath);
                    } else if (PtInRect(&editDstRect, pt)) {
                        wcscpy(dstPath, path);
                        dstLen = wcslen(dstPath);
                    }
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            DragFinish(hDrop);
            break;
        }
        case WM_LBUTTONDOWN: {
            POINT pt; pt.x = GET_X_LPARAM(lParam); pt.y = GET_Y_LPARAM(lParam);
            // 侧边栏导航（带柔和过渡动画）
            if (PtInRect(&navHomeRect, pt) && currentPage != 0 && !isPageTransitioning) {
                targetPage = 0;
                isPageTransitioning = TRUE;
                transitionPhase = 0;
                transitionStartTime = GetTickCount64();
                SetTimer(hWnd, 1, 16, NULL); // 60fps 动画
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (PtInRect(&navSettingsRect, pt) && currentPage != 1 && !isPageTransitioning) {
                targetPage = 1;
                isPageTransitioning = TRUE;
                transitionPhase = 0;
                transitionStartTime = GetTickCount64();
                SetTimer(hWnd, 1, 16, NULL); // 60fps 动画
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            // 设置页面点击处理
            if (currentPage == 1) {
                RECT speedInput = {100, 98, 260, 130};
                RECT unitBtn = {270, 98, 330, 130};
                RECT verifyCheck = {290, 169, 308, 187};
                RECT langBtn = {100, 222, 260, 254};
                RECT saveBtn = {100, 300, 240, 340};
                if (PtInRect(&verifyCheck, pt)) {
                    enableVerify = !enableVerify;
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                // 语言下拉框
                if (langDropdownOpen) {
                    int itemH = 32;
                    int listTop = langBtn.bottom + 4;
                    BOOL clickedItem = FALSE;
                    for (int i = 0; i < 5; i++) {
                        RECT itemR = {langBtn.left + 2, listTop + i * itemH, langBtn.right - 2, listTop + (i + 1) * itemH};
                        if (PtInRect(&itemR, pt)) {
                            currentLang = i;
                            langDropdownOpen = FALSE;
                            hoveredLangItem = -1;
                            if (!isCopying) wcscpy(statusText, GetText("status_ready"));
                            InvalidateRect(hWnd, NULL, FALSE);
                            clickedItem = TRUE;
                            break;
                        }
                    }
                    if (clickedItem) return 0;
                    // 点击下拉按钮或外部，关闭
                    langDropdownOpen = FALSE;
                    hoveredLangItem = -1;
                    InvalidateRect(hWnd, NULL, FALSE);
                    if (PtInRect(&langBtn, pt)) return 0;
                } else {
                    if (PtInRect(&langBtn, pt)) {
                        langDropdownOpen = TRUE;
                        InvalidateRect(hWnd, NULL, FALSE);
                        return 0;
                    }
                }
                if (PtInRect(&speedInput, pt)) {
                    speedFocus = TRUE;
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                speedFocus = FALSE;
                if (PtInRect(&unitBtn, pt)) {
                    selectedUnit = (selectedUnit + 1) % 3;
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                if (PtInRect(&saveBtn, pt)) {
                    int val = _wtoi(speedInputText);
                    if (val < 0) val = 0;
                    if (val > 10240) val = 10240;
                    speedLimitKB = val * unitMultipliers[selectedUnit];
                    if (speedLimitKB < 0) speedLimitKB = 0;
                    if (speedLimitKB > 10 * 1024 * 1024) speedLimitKB = 10 * 1024 * 1024;
                    // 保存后切回主页
                    targetPage = 0;
                    isPageTransitioning = TRUE;
                    transitionPhase = 0;
                    transitionStartTime = GetTickCount64();
                    SetTimer(hWnd, 1, 16, NULL); // 60fps 动画
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            // D2D 下拉框处理
            if (strategyDropdownOpen) {
                int itemH = 30;
                int listTop = strategyComboRect.bottom + 4;
                BOOL clickedItem = FALSE;
                for (int i = 0; i < 4; i++) {
                    RECT itemR = {strategyComboRect.left + 2, listTop + i * itemH,
                                  strategyComboRect.right - 2, listTop + (i + 1) * itemH};
                    if (PtInRect(&itemR, pt)) {
                        g_strategy = i;
                        strategyDropdownOpen = FALSE;
                        hoveredStrategyItem = -1;
                        InvalidateRect(hWnd, NULL, FALSE);
                        clickedItem = TRUE;
                        break;
                    }
                }
                if (clickedItem) return 0;
                // 点击下拉按钮或外部，关闭下拉
                strategyDropdownOpen = FALSE;
                hoveredStrategyItem = -1;
                InvalidateRect(hWnd, NULL, FALSE);
                if (PtInRect(&strategyComboRect, pt)) return 0;
            } else {
                if (PtInRect(&strategyComboRect, pt) && !isCopying) {
                    strategyDropdownOpen = TRUE;
                    InvalidateRect(hWnd, NULL, FALSE);
                    return 0;
                }
            }
            if (PtInRect(&btnCopyRect, pt)) { PostMessage(hWnd, WM_COMMAND, 3, 0); return 0; }
            if (PtInRect(&btnPauseRect, pt)) { PostMessage(hWnd, WM_COMMAND, 4, 0); return 0; }
            if (PtInRect(&btnStopRect, pt)) { PostMessage(hWnd, WM_COMMAND, 5, 0); return 0; }
            if (PtInRect(&btnBrowseSrcRect, pt)) { PostMessage(hWnd, WM_COMMAND, 1, 0); return 0; }
            if (PtInRect(&btnBrowseDstRect, pt)) { PostMessage(hWnd, WM_COMMAND, 2, 0); return 0; }
            if (PtInRect(&editSrcRect, pt)) {
                srcFocus = TRUE; dstFocus = FALSE;
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (PtInRect(&editDstRect, pt)) {
                srcFocus = FALSE; dstFocus = TRUE;
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            break;
        }
        case WM_MOUSEMOVE: {
            POINT pt; pt.x = GET_X_LPARAM(lParam); pt.y = GET_Y_LPARAM(lParam);
            
            // D2D 下拉框悬停
            if (strategyDropdownOpen) {
                int itemH = 30;
                int listTop = strategyComboRect.bottom + 4;
                int newHover = -1;
                for (int i = 0; i < 4; i++) {
                    RECT itemR = {strategyComboRect.left + 2, listTop + i * itemH,
                                  strategyComboRect.right - 2, listTop + (i + 1) * itemH};
                    if (PtInRect(&itemR, pt)) { newHover = i; break; }
                }
                if (newHover != hoveredStrategyItem) {
                    hoveredStrategyItem = newHover;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return 0;
            }
            // 语言下拉框悬停
            if (langDropdownOpen) {
                RECT langBtn = {100, 222, 260, 254};
                int itemH = 32;
                int listTop = langBtn.bottom + 4;
                int newHover = -1;
                for (int i = 0; i < 5; i++) {
                    RECT itemR = {langBtn.left + 2, listTop + i * itemH, langBtn.right - 2, listTop + (i + 1) * itemH};
                    if (PtInRect(&itemR, pt)) { newHover = i; break; }
                }
                if (newHover != hoveredLangItem) {
                    hoveredLangItem = newHover;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return 0;
            }
            
            // 检测输入框悬停
            BOOL newSrcHover = PtInRect(&editSrcRect, pt);
            BOOL newDstHover = PtInRect(&editDstRect, pt);
            if (newSrcHover != srcHover || newDstHover != dstHover) {
                srcHover = newSrcHover;
                dstHover = newDstHover;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            
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
                    for (int i = *caret - 1; i < *len - 1; i++) target[i] = target[i + 1];
                    (*caret)--; (*len)--; target[*len] = L'\0';
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            } else if (wParam == VK_LEFT) { if (*caret > 0) (*caret)--; }
            else if (wParam == VK_RIGHT) { if (*caret < *len) (*caret)++; }
            else if (wParam == VK_DELETE) {
                if (*caret < *len) {
                    for (int i = *caret; i < *len - 1; i++) target[i] = target[i + 1];
                    (*len)--; target[*len] = L'\0';
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            break;
        }
        case WM_CHAR: {
            wchar_t ch = (wchar_t)wParam;
            // 设置页速度输入
            if (currentPage == 1 && speedFocus) {
                if (ch >= L'0' && ch <= L'9') {
                    int len = (int)wcslen(speedInputText);
                    if (len < 10) {
                        speedInputText[len] = ch;
                        speedInputText[len+1] = L'\0';
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                } else if (ch == 8) { // backspace
                    int len = (int)wcslen(speedInputText);
                    if (len > 0) {
                        speedInputText[len-1] = L'\0';
                        InvalidateRect(hWnd, NULL, FALSE);
                    }
                }
                return 0;
            }
            if (ch >= 32 && ch <= 126) {
                wchar_t* target = NULL; int* caret = NULL, * len = NULL;
                if (srcFocus) { target = srcPath; caret = &srcCaretPos; len = &srcLen; }
                else if (dstFocus) { target = dstPath; caret = &dstCaretPos; len = &dstLen; }
                else break;
                if (*len < MAX_PATH - 1) {
                    for (int i = *len; i > *caret; i--) target[i] = target[i - 1];
                    target[*caret] = ch; (*caret)++; (*len)++; target[*len] = L'\0';
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            break;
        }
        case WM_THREAD_FINISHED: {
            if (g_wasAutoStrategy) { g_strategy = STRATEGY_AUTO; g_wasAutoStrategy = FALSE; }
            if (hStrategyCombo) {
                EnableWindow(hStrategyCombo, TRUE);
                SendMessageW(hStrategyCombo, CB_SETCURSEL, g_strategy, 0);
            }
            EnterCriticalSection(&cs);
            int wasStopped = stopRequested;
            LeaveCriticalSection(&cs);
            if (wasStopped) {
                progressValue = 0; isCopying = 0; stopRequested = 0; isPaused = 0;
                btnCopyState = BTN_NORMAL; btnPauseState = BTN_DISABLED; btnStopState = BTN_DISABLED;
                wcscpy(statusText, GetText("status_stopped"));
                InvalidateRect(hWnd, NULL, FALSE);
                MessageBoxW(NULL, GetText("dialog_stopped"), L"CopyTool", MB_OK);
            } else {
                progressValue = 100; isCopying = 0; isPaused = 0;
                btnCopyState = BTN_NORMAL; btnPauseState = BTN_DISABLED; btnStopState = BTN_DISABLED;
                const wchar_t* status = GetText("status_done");
                const wchar_t* done = GetText("dialog_done");
                // 构建状态文字
                wchar_t verifyStr[128] = L"";
                if (enableVerify && (verifiedFiles > 0 || failedVerifyFiles > 0)) {
                    if (failedVerifyFiles > 0) {
                        StringCchPrintfW(verifyStr, ARRAYSIZE(verifyStr), GetText("verify_failed"), failedVerifyFiles);
                    } else {
                        wcscpy(verifyStr, L" ✓");
                    }
                }
                if (failedFiles > 0 || failedVerifyFiles > 0) {
                    wchar_t failedStr[64] = L"";
                    if (failedFiles > 0) StringCchPrintfW(failedStr, ARRAYSIZE(failedStr), GetText("failed_count"), failedFiles);
                    StringCchPrintfW(statusText, ARRAYSIZE(statusText), L"%s %d %s%s%s", status, totalFiles, done, failedStr, verifyStr);
                    // 构建失败文件详情
                    wchar_t detailMsg[4096];
                    detailMsg[0] = L'\0';
                    int offset = 0;
                    int totalFail = failedFiles + failedVerifyFiles;
                    StringCchPrintfW(detailMsg + offset, ARRAYSIZE(detailMsg) - offset, L"%s (%d):\r\n\r\n", GetText("dialog_done"), totalFail);
                    offset = (int)wcslen(detailMsg);
                    int showCount = g_failCount < 10 ? g_failCount : 10;
                    for (int i = 0; i < showCount; i++) {
                        StringCchPrintfW(detailMsg + offset, ARRAYSIZE(detailMsg) - offset, L"[%d] %s  (错误码: %lu)\r\n", i + 1, g_failList[i], g_failErrors[i]);
                        offset = (int)wcslen(detailMsg);
                    }
                    if (g_failCount > 10) {
                        StringCchPrintfW(detailMsg + offset, ARRAYSIZE(detailMsg) - offset, L"...还有 %d 个文件失败", g_failCount - 10);
                        offset = (int)wcslen(detailMsg);
                    }
                    // 写完整失败日志到目标目录
                    wchar_t logPath[MAX_PATH];
                    StringCchPrintfW(logPath, ARRAYSIZE(logPath), L"%s\\CopyTool_失败日志.txt", dstPath);
                    HANDLE hLog = CreateFileW(logPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hLog != INVALID_HANDLE_VALUE) {
                        // 写 UTF-16 LE BOM
                        WORD bom = 0xFEFF;
                        DWORD written;
                        WriteFile(hLog, &bom, 2, &written, NULL);
                        wchar_t header[256];
                        StringCchPrintfW(header, ARRAYSIZE(header), L"CopyTool 复制失败日志\r\n共 %d 个文件失败\r\n\r\n", g_failCount);
                        WriteFile(hLog, header, (DWORD)(wcslen(header) * sizeof(wchar_t)), &written, NULL);
                        for (int i = 0; i < g_failCount; i++) {
                            wchar_t errText[256] = L"";
                            if (g_failErrors[i] == 999) {
                                wcscpy(errText, L"校验失败：源文件与目标文件内容不一致");
                            } else if (g_failErrors[i] != 0) {
                                FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                    NULL, g_failErrors[i], 0, errText, 256, NULL);
                                size_t tl = wcslen(errText);
                                while (tl > 0 && (errText[tl-1] == L'\r' || errText[tl-1] == L'\n')) errText[--tl] = L'\0';
                            } else {
                                wcscpy(errText, L"未知错误");
                            }
                            wchar_t line[MAX_PATH + 320];
                            StringCchPrintfW(line, ARRAYSIZE(line), L"[%d] 错误码:%lu  %s\r\n    %s\r\n", i + 1, g_failErrors[i], g_failList[i], errText);
                            WriteFile(hLog, line, (DWORD)(wcslen(line) * sizeof(wchar_t)), &written, NULL);
                        }
                        CloseHandle(hLog);
                    }
                    StringCchPrintfW(detailMsg + offset, ARRAYSIZE(detailMsg) - offset, L"\r\n完整列表已保存到: %s", logPath);
                    MessageBoxW(NULL, detailMsg, L"CopyTool", MB_OK | MB_ICONWARNING);
                } else {
                    StringCchPrintfW(statusText, ARRAYSIZE(statusText), L"%s %d %s%s", status, totalFiles, done, verifyStr);
                    if (enableVerify && verifiedFiles > 0) {
                        wchar_t verifyMsg[256];
                        StringCchPrintfW(verifyMsg, ARRAYSIZE(verifyMsg), L"%s\n%s: %d/%d", done, GetText("verify_done"), verifiedFiles, totalFiles);
                        MessageBoxW(NULL, verifyMsg, L"CopyTool", MB_OK);
                    } else {
                        MessageBoxW(NULL, done, L"CopyTool", MB_OK);
                    }
                }
                InvalidateRect(hWnd, NULL, FALSE);
            }
            if (hCopyThread) { CloseHandle(hCopyThread); hCopyThread = NULL; }
            break;
        }
        case WM_VERIFY_STARTED: {
            progressValue = 0;
            copiedBytes = 0;
            currentSpeedMB = 0;
            lastSpeedTime = 0;
            lastSpeedBytes = 0;
            wcscpy(statusText, GetText("status_verifying"));
            InvalidateRect(hWnd, NULL, FALSE);
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
                    scanDone = 0;
                    currentSpeedMB = 0.0; lastSpeedTime = 0; lastSpeedBytes = 0;
                    btnCopyState = BTN_DISABLED; btnPauseState = BTN_NORMAL; btnStopState = BTN_NORMAL;
                    if (hStrategyCombo) EnableWindow(hStrategyCombo, FALSE);
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
                pRenderTarget->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top));
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

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmd, int nShow) {
    // ===== 命令行模式：CopyTool.exe "源" "目标" --console =====
    if (lpCmd && wcslen(lpCmd) > 0) {
        if (wcsstr(lpCmd, L"--console")) {
            wchar_t src[MAX_PATH] = L"";
            wchar_t dst[MAX_PATH] = L"";
            
            wchar_t* p = lpCmd;
            while (*p == L' ') p++;
            if (*p == L'"') {
                p++;
                int i = 0;
                while (*p && *p != L'"' && i < MAX_PATH - 1) { src[i++] = *p; p++; }
                src[i] = L'\0';
                p++;
            }
            while (*p == L' ') p++;
            if (*p == L'"') {
                p++;
                int i = 0;
                while (*p && *p != L'"' && i < MAX_PATH - 1) { dst[i++] = *p; p++; }
                dst[i] = L'\0';
            }
            
            if (src[0] && dst[0]) {
                InitializeCriticalSection(&cs);
                totalBytes = 0; totalFiles = 0; copiedBytes = 0; copiedFiles = 0; failedFiles = 0;
                filesScanned = 0; filesInQueue = 0; scanDone = 0;
                ClearQueue();
                
                // 扫描（同步调用）
                wchar_t scanPaths[MAX_PATH * 2];
                wcscpy(scanPaths, src);
                wcscpy(scanPaths + MAX_PATH, dst);
                ScanThreadProc((LPVOID)scanPaths);
                
                // 复制
                ApplyModeConfig(MODE_AUTO);
                SYSTEM_INFO si;
                GetSystemInfo(&si);
                int workerCount = si.dwNumberOfProcessors * 2;
                if (workerCount < 4) workerCount = 4;
                if (workerCount > MAX_WORKER_THREADS) workerCount = MAX_WORKER_THREADS;
                
                HANDLE* workers = (HANDLE*)malloc(workerCount * sizeof(HANDLE));
                for (int i = 0; i < workerCount; i++) {
                    workers[i] = CreateThread(NULL, 0, WorkerThreadProc, NULL, 0, NULL);
                }
                
                // 输出进度
                while (1) {
                    BOOL allDone = TRUE;
                    for (int i = 0; i < workerCount; i++) {
                        if (WaitForSingleObject(workers[i], 0) != WAIT_OBJECT_0) {
                            allDone = FALSE;
                            break;
                        }
                    }
                    if (allDone) break;
                    
                    if (totalBytes > 0) {
                        int pct = (int)((copiedBytes * 100) / totalBytes);
                        printf("%d\n", pct);
                        fflush(stdout);
                    }
                    Sleep(100);
                }
                
                for (int i = 0; i < workerCount; i++) CloseHandle(workers[i]);
                free(workers);
                DeleteCriticalSection(&cs);
                
                printf("DONE\n");
                fflush(stdout);
                return 0;
            }
        }
    }
    
    // ===== GUI 模式 =====
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    
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
        CW_USEDEFAULT, CW_USEDEFAULT, 540, 460,
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
=======
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
>>>>>>> 5b0a4414d1c038e1b73e68a52372b014a3db9321
}
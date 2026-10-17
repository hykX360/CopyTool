#define _WIN32_IE 0x0600
#define WINVER 0x0600
#define _WIN32_WINNT 0x0600
#define COPYCORE_EXPORTS

#include <windows.h>
#include <aclapi.h>
#include <strsafe.h>
#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <string.h>

#include "CopyCore.h"

#define MAX_WORKER_THREADS 32

// ------------------------------------------------------------
// 枚举 / 配置（与原 CopyTool.cpp 完全一致）
// ------------------------------------------------------------
enum CopyMode {
    MODE_AUTO, MODE_BALANCED, MODE_SSD_SSD, MODE_HDD_HDD, MODE_HDD_SSD,
    MODE_SSD_HDD, MODE_SSD_UFD, MODE_UFD_SSD, MODE_HDD_UFD, MODE_UFD_HDD,
    MODE_UFD_UFD, MODE_EXTREME
};

enum OptimizeStrategy {
    STRATEGY_AUTO, STRATEGY_SMALL, STRATEGY_LARGE, STRATEGY_BALANCED
};

enum DiskType { DISK_UNKNOWN = 0, DISK_SSD, DISK_HDD, DISK_USB, DISK_NETWORK };

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
    {L"均衡模式", L"均衡", 32, 32*1024*1024, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 5},
    {L"SSD→SSD", L"SSD→SSD", 32, 32*1024*1024, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 10},
    {L"HDD→HDD", L"HDD→HDD", 4, 32*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 4},
    {L"HDD→SSD", L"HDD→SSD", 16, 32*1024*1024, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 7},
    {L"SSD→HDD", L"SSD→HDD", 8, 32*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 6},
    {L"SSD→UFD", L"SSD→UFD", 2, 4*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING, FALSE, 3},
    {L"UFD→SSD", L"UFD→SSD", 4, 16*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 6},
    {L"HDD→UFD", L"HDD→UFD", 2, 4*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING, FALSE, 2},
    {L"UFD→HDD", L"UFD→HDD", 2, 8*1024*1024, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 3},
    {L"UFD→UFD", L"UFD→UFD", 1, 2*1024*1024, 0, FILE_FLAG_NO_BUFFERING, FALSE, 1},
    {L"极速狂飙", L"极速", 32, 32*1024*1024, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, TRUE, 10}
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
    {16, 16*1024*1024, 4*1024*1024, TRUE, TRUE, L"小文件优化"},
    {32, 64*1024*1024, 1*1024*1024, TRUE, TRUE, L"大文件优化"},
    {32, 32*1024*1024, 4*1024*1024, TRUE, TRUE, L"平衡模式"}
};

// ------------------------------------------------------------
// 多语言（与原表一致）
// ------------------------------------------------------------
static int currentLang = 0;
static const wchar_t* langNames[] = {L"简体中文", L"繁體中文", L"English", L"한국어", L"日本語"};

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
    {"settings_saved", L"已保存！", L"已儲存！", L"Saved!", L"저장됨!", L"保存しました"},
    {"settings_hint_zero", L"0 = 不限速", L"0 = 不限速", L"0 = Unlimited", L"0 = 무제한", L"0 = 無制限"},
    {"dialog_done", L"全部复制完成", L"全部複製完成", L"All done!", L"모두 완료!", L"完了しました"},
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
    {"dialog_error_thread", L"系统资源不足", L"系統資源不足", L"Not enough system resources", L"시스템 리소스가 부족", L"システムリソース가 부족"},
    {"dialog_error_memory", L"内存不足", L"記憶體不足", L"Not enough memory", L"메모리 부족", L"メモリが不足しています"},
    {"dialog_ok", L"确定", L"確定", L"OK", L"확인", L"OK"},
    {"strategy_auto", L"智能检测", L"智慧偵測", L"Auto Detect", L"자동 감지", L"自動検出"},
    {"strategy_small", L"小文件优化", L"小檔案最佳化", L"Small Files", L"작은 파일", L"小ファイル最適化"},
    {"strategy_large", L"大文件优化", L"大檔案最佳化", L"Large Files", L"큰 파일", L"大ファイル最適化"},
    {"strategy_balanced", L"平衡模式", L"平衡模式", L"Balanced", L"균형", L"バランス"},
    {"nav_home", L"主页", L"主頁", L"Home", L"홈", L"ホーム"},
    {"remaining_time", L"还需", L"還需", L"Remaining", L"남음", L"残り"},
    {"scanning_files", L"扫描中... %d 个文件", L"掃描中... %d 個檔案", L"Scanning... %d files", L"스캔중... %d개의 파일", L"スキャン中... %d個のファイル"},
    {"error_title", L"错误", L"錯誤", L"Error", L"오류", L"エラー"},
    {"settings_init_error", L"设置窗口初始化失败！", L"設定視窗初始化失敗！", L"Settings init failed!", L"설정 초기화 실패!", L"設定初期化失敗！"},
    {"d2d_init_error", L"D2D初始化失败", L"D2D初始化失败", L"D2D init failed", L"D2D 초기화 실패", L"D2D初期化失敗"},
    {"speed_unit", L"MB/s", L"MB/s", L"MB/s", L"MB/s", L"MB/s"},
    {"failed_count", L"%d 个失败", L"%d 個失敗", L", %d failed", L", %d개 실패", L"%d個失敗"},
    {"analyzing", L"分析文件分布...", L"分析檔案分布...", L"Analyzing files...", L"파일 분석 중...", L"ファイル分析中..."},
    {"status_verifying", L"校验中", L"校驗中", L"Verifying", L"검증중", L"検証中"},
    {"verify_done", L"校验完成", L"校驗完成", L"Verification done", L"검증 완료", L"検証完了"},
    {"verify_failed", L"%d 个校验失败", L"%d 個校驗失敗", L", %d verify failed", L", %d개 검증 실패", L"%d個検証失敗"}
};

static const wchar_t* GetText(const char* id) {
    int count = sizeof(g_LangTable) / sizeof(LangEntry);
    for (int i = 0; i < count; i++) {
        if (strcmp(g_LangTable[i].id, id) == 0) {
            const wchar_t* texts[] = {
                g_LangTable[i].zhCN, g_LangTable[i].zhTW, g_LangTable[i].enUS,
                g_LangTable[i].koKR, g_LangTable[i].jaJP
            };
            return texts[currentLang];
        }
    }
    return L"";
}

// ------------------------------------------------------------
// 全局状态
// ------------------------------------------------------------
static int g_strategy = STRATEGY_AUTO;
static size_t g_largeFileThreshold = 1 * 1024 * 1024;
static BOOL g_wasAutoStrategy = FALSE;

static CRITICAL_SECTION cs;
static BOOL g_csInited = FALSE;

static int progressValue = 0, isCopying = 0;
static int lastProgress = 0;
static volatile int isPaused = 0, stopRequested = 0;
static HANDLE hCopyThread = NULL;
static HANDLE hWakeEvent = NULL;

static int speedLimitKB = 0;
static int enableVerify = 0;
static int currentMode = MODE_AUTO;

static wchar_t srcPath[MAX_PATH] = L"";
static wchar_t dstPath[MAX_PATH] = L"";
static wchar_t statusText[128] = L"准备就绪";

static int g_workerThreads = 8;
static size_t g_bufferSize = 32 * 1024 * 1024;
static DWORD g_srcFlags = FILE_FLAG_SEQUENTIAL_SCAN;
static DWORD g_dstFlags = FILE_FLAG_SEQUENTIAL_SCAN;
static BOOL g_preallocate = TRUE;
static int g_prescanSmallCount = 0, g_prescanLargeCount = 0;
static long long g_prescanSmallBytes = 0, g_prescanLargeBytes = 0;
static BOOL g_countOnly = FALSE;
static HANDLE hLargeFileSem = NULL;
static HANDLE hStopEvent = NULL;
static BOOL g_destIsFresh = FALSE;

typedef struct FileNode {
    wchar_t src[MAX_PATH];
    wchar_t dst[MAX_PATH];
    long long fsize;
    DWORD fattrs;
    FILETIME fwriteTime;
    struct FileNode* next;
} FileNode;

static FileNode* queueHead = NULL;
static FileNode* queueTail = NULL;
static FileNode* verifyHead = NULL;
static FileNode* verifyTail = NULL;
static BOOL isVerifying = FALSE;
static long long verifiedBytes = 0;
static int verifiedFiles = 0;
static int failedVerifyFiles = 0;
static long long totalBytes = 0;
static long long copiedBytes = 0;
static int totalFiles = 0;
static int copiedFiles = 0;
static int failedFiles = 0;
#define MAX_FAIL_LIST 256
static wchar_t g_failList[MAX_FAIL_LIST][MAX_PATH] = {0};
static DWORD g_failErrors[MAX_FAIL_LIST] = {0};
static int g_failCount = 0;

typedef BOOL (WINAPI *SetThreadIoPriority_t)(HANDLE, int);
static SetThreadIoPriority_t g_pSetThreadIoPriority = NULL;
static BOOL g_ioPriorityInited = FALSE;
static void InitIoPriority() {
    if (!g_ioPriorityInited) {
        g_pSetThreadIoPriority = (SetThreadIoPriority_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadIoPriority");
        g_ioPriorityInited = TRUE;
    }
}
static int filesScanned = 0;
static int scanDone = 0;
static HANDLE hScanThread = NULL;
static HANDLE g_workerHandles[64] = {0};
static int g_workerHandleCount = 0;
static double currentSpeedMB = 0.0;
static ULONGLONG lastSpeedTime = 0;
static long long lastSpeedBytes = 0;

static __thread char* tls_copyBuf = NULL;
static __thread size_t tls_copyBufSize = 0;

// 手动复位：扫描线程结束时置位，worker 空闲等待必须同时等它，
// 否则空目录/文件数少于线程数时 worker 会永久挂在 hWakeEvent 上。
static HANDLE hScanDoneEvent = NULL;

// 全局聚合限速器（漏桶，跨所有 worker 线程，cs 保护）。
// 旧实现每线程各自一个 1 秒窗口，N 线程实际超速 N 倍。
static ULONGLONG g_limLastTick = 0;
static long long g_limDebtBytes = 0;
static void RateLimitWait(long long bytes) {
    if (speedLimitKB <= 0 || bytes <= 0) return;
    const long long limitBytesPerSec = (long long)speedLimitKB * 1024;
    for (;;) {
        ULONGLONG now = GetTickCount64();
        long long sleepMs = 0;
        EnterCriticalSection(&cs);
        if (stopRequested) { LeaveCriticalSection(&cs); return; }
        if (g_limLastTick == 0) g_limLastTick = now;
        long long elapsed = (long long)(now - g_limLastTick);
        long long drained = (elapsed * limitBytesPerSec) / 1000;
        g_limDebtBytes -= drained;
        if (g_limDebtBytes < 0) g_limDebtBytes = 0;
        g_limLastTick = now;
        // 桶已排空（debt==0）时直接放行——单块大于 1 秒额度（如 32MB 块配 30MB/s）
        // 也必须能通过，否则会陷入永远等不到额度的死循环。
        if (g_limDebtBytes == 0 || g_limDebtBytes + bytes <= limitBytesPerSec) {
            g_limDebtBytes += bytes;
            LeaveCriticalSection(&cs);
            return;
        }
        // 桶满：按超出量计算需要漏出的时间，让出后重试
        long long excess = g_limDebtBytes + bytes - limitBytesPerSec;
        sleepMs = (excess * 1000) / limitBytesPerSec;
        if (sleepMs < 1) sleepMs = 1;
        if (sleepMs > 100) sleepMs = 100;
        LeaveCriticalSection(&cs);
        Sleep((DWORD)sleepMs);
        // 限速等待期间同时响应暂停
        while (isPaused && !stopRequested) Sleep(10);
    }
}

// 大文件缓冲池：按大文件并发上限预分配，总量封顶 256MB；
// 小文件走 CopyFileEx 不占缓冲；分配失败时回退每线程 TLS 缓冲。
static unsigned char* g_bufPool = NULL;
static size_t g_bufPoolSlotSize = 0;
static int g_bufPoolCount = 0;
static int g_bufFreeSlots[64] = {0};
static int g_bufFreeCount = 0;
static int BufPoolAcquire(void) {
    if (!g_bufPool) return -1;
    EnterCriticalSection(&cs);
    int slot = g_bufFreeSlots[--g_bufFreeCount];
    LeaveCriticalSection(&cs);
    return slot;
}
static void BufPoolRelease(int slot) {
    if (slot < 0) return;
    EnterCriticalSection(&cs);
    g_bufFreeSlots[g_bufFreeCount++] = slot;
    LeaveCriticalSection(&cs);
}

// ------------------------------------------------------------
// 目录创建缓存
// ------------------------------------------------------------
#define DIR_CACHE_BUCKETS 4096
static wchar_t g_dirCache[DIR_CACHE_BUCKETS][MAX_PATH];
static volatile LONG g_dirCacheUsed[DIR_CACHE_BUCKETS];

static inline unsigned DirCache_Hash(const wchar_t* s) {
    unsigned h = 2166136261u;
    while (*s) { h ^= (unsigned)(*s++ | 0x20); h *= 16777619u; }
    return h & (DIR_CACHE_BUCKETS - 1);
}
static void DirCache_Init() { memset((void*)g_dirCacheUsed, 0, sizeof(g_dirCacheUsed)); }
static BOOL DirCache_Exists(const wchar_t* dir) {
    unsigned idx = DirCache_Hash(dir);
    for (int probe = 0; probe < 8; probe++) {
        unsigned i = (idx + probe) & (DIR_CACHE_BUCKETS - 1);
        if (!g_dirCacheUsed[i]) return FALSE;
        if (_wcsicmp(g_dirCache[i], dir) == 0) return TRUE;
    }
    return FALSE;
}
static void DirCache_Add(const wchar_t* dir) {
    unsigned idx = DirCache_Hash(dir);
    for (int probe = 0; probe < 8; probe++) {
        unsigned i = (idx + probe) & (DIR_CACHE_BUCKETS - 1);
        if (InterlockedCompareExchange(&g_dirCacheUsed[i], 1, 0) == 0) {
            wcsncpy_s(g_dirCache[i], MAX_PATH, dir, _TRUNCATE);
            return;
        }
        if (_wcsicmp(g_dirCache[i], dir) == 0) return;
    }
}

// ------------------------------------------------------------
// 队列
// ------------------------------------------------------------
static void EnqueueFileNode(FileNode* node) {
    node->next = NULL;
    if (queueTail) { queueTail->next = node; queueTail = node; }
    else { queueHead = queueTail = node; }
}

static FileNode* DequeueNode() {
    FileNode* node = queueHead;
    if (node) {
        queueHead = node->next;
        if (!queueHead) queueTail = NULL;
    }
    return node;
}

static void ClearQueue() {
    FileNode* node;
    while ((node = DequeueNode()) != NULL) free(node);
}

static void ClearVerifyQueue() {
    while (verifyHead) {
        FileNode* tmp = verifyHead;
        verifyHead = verifyHead->next;
        free(tmp);
    }
    verifyHead = verifyTail = NULL;
}

// ------------------------------------------------------------
// 前向声明
// ------------------------------------------------------------
DWORD WINAPI WorkerThreadProc(LPVOID lpParam);
DWORD WINAPI VerifyThreadProc(LPVOID lpParam);
BOOL CopySingleFile(const wchar_t* src, const wchar_t* dst, long long fsize, DWORD fattrs, const FILETIME* fwriteTime);
BOOL VerifyFileCopy(const wchar_t* src, const wchar_t* dst);
void CreateDirectoriesRecursive(const wchar_t* path);
int CheckImpossibleTask(const wchar_t* src, const wchar_t* dst);

// 原 UI 层 ShowError：DLL 无窗口，错误已通过失败列表/返回值体现
static void ShowError(const wchar_t* /*id*/) {}

// ------------------------------------------------------------
// 磁盘类型识别 / 模式选择
// ------------------------------------------------------------
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

int SelectBestMode(const wchar_t* src, const wchar_t* dst) {
    if (currentMode != MODE_AUTO) return currentMode;
    DiskType srcType = DetectDriveType(src);
    DiskType dstType = DetectDriveType(dst);
    if (srcType == DISK_SSD && dstType == DISK_SSD) return MODE_SSD_SSD;
    if (srcType == DISK_HDD && dstType == DISK_HDD) return MODE_HDD_HDD;
    if (srcType == DISK_HDD && dstType == DISK_SSD) return MODE_HDD_SSD;
    if (srcType == DISK_SSD && dstType == DISK_HDD) return MODE_SSD_HDD;
    if (srcType == DISK_SSD && dstType == DISK_USB) return MODE_SSD_UFD;
    if (srcType == DISK_USB && dstType == DISK_SSD) return MODE_UFD_SSD;
    if (srcType == DISK_HDD && dstType == DISK_USB) return MODE_HDD_UFD;
    if (srcType == DISK_USB && dstType == DISK_HDD) return MODE_UFD_HDD;
    if (srcType == DISK_USB && dstType == DISK_USB) return MODE_UFD_UFD;
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
    if (g_strategy != STRATEGY_AUTO) {
        const StrategyConfig* sc = &g_strategyConfigs[g_strategy];
        if (sc->threads > 0) g_workerThreads = sc->threads;
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

// ------------------------------------------------------------
// 扫描
// ------------------------------------------------------------
void ScanDirectory(const wchar_t* path, const wchar_t* baseSrc, size_t baseSrcLen, const wchar_t* baseDst) {
    wchar_t sp[MAX_PATH];
    WIN32_FIND_DATAW fd;
    StringCchPrintfW(sp, ARRAYSIZE(sp), L"%s\\*", path);
    HANDLE h = FindFirstFileExW(sp, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (stopRequested) break;
        if (g_countOnly && (g_prescanSmallCount + g_prescanLargeCount) >= 3000) break;
        if (fd.cFileName[0] == L'.' && (fd.cFileName[1] == L'\0' || (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0'))) continue;
        wchar_t src[MAX_PATH], dst[MAX_PATH];
        StringCchPrintfW(src, ARRAYSIZE(src), L"%s\\%s", path, fd.cFileName);
        const wchar_t* relStart = src + baseSrcLen;
        while (*relStart == L'\\') relStart++;
        StringCchPrintfW(dst, ARRAYSIZE(dst), L"%s%s", baseDst, relStart);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            if (!g_countOnly) CreateDirectoryW(dst, NULL);
            if (!(g_countOnly && (g_prescanSmallCount + g_prescanLargeCount) >= 3000))
                ScanDirectory(src, baseSrc, baseSrcLen, baseDst);
        } else {
            long long fsize = (long long)fd.nFileSizeHigh << 32 | fd.nFileSizeLow;
            if (g_countOnly) {
                if (fsize < 1024 * 1024) { g_prescanSmallCount++; g_prescanSmallBytes += fsize; }
                else { g_prescanLargeCount++; g_prescanLargeBytes += fsize; }
            } else {
                FileNode* node = (FileNode*)malloc(sizeof(FileNode));
                FileNode* vnode = NULL;
                if (node) {
                    wcscpy(node->src, src); wcscpy(node->dst, dst);
                    node->fsize = fsize; node->fattrs = fd.dwFileAttributes;
                    node->fwriteTime = fd.ftLastWriteTime;
                }
                if (enableVerify) {
                    vnode = (FileNode*)malloc(sizeof(FileNode));
                    if (vnode) {
                        wcscpy(vnode->src, src); wcscpy(vnode->dst, dst);
                        vnode->fsize = fsize; vnode->fattrs = fd.dwFileAttributes;
                        vnode->fwriteTime = fd.ftLastWriteTime;
                    }
                }
                EnterCriticalSection(&cs);
                if (!stopRequested && node) {
                    EnqueueFileNode(node);
                    node = NULL;
                    if (vnode) {
                        vnode->next = NULL;
                        if (verifyTail) { verifyTail->next = vnode; verifyTail = vnode; }
                        else { verifyHead = verifyTail = vnode; }
                        vnode = NULL;
                    }
                    filesScanned++; totalBytes += fsize; totalFiles++;
                }
                LeaveCriticalSection(&cs);
                if (hWakeEvent) SetEvent(hWakeEvent);
                if (node) free(node);
                if (vnode) free(vnode);
            }
        }
    } while (FindNextFileW(h, &fd) && !stopRequested);
    FindClose(h);
}

DWORD WINAPI ScanThreadProc(LPVOID lpParam) {
    InitIoPriority();
    if (g_pSetThreadIoPriority) g_pSetThreadIoPriority(GetCurrentThread(), 3);
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
    g_destIsFresh = CreateDirectoryW(finalDst, NULL);
    if (!g_destIsFresh && GetLastError() != ERROR_ALREADY_EXISTS) g_destIsFresh = FALSE;
    ScanDirectory(srcRoot, srcRoot, wcslen(srcRoot), finalDst);
    EnterCriticalSection(&cs);
    scanDone = 1;
    LeaveCriticalSection(&cs);
    // 广播扫描结束：让所有在 hWakeEvent 上等待的 worker 重新检查队列并退出。
    if (hScanDoneEvent) SetEvent(hScanDoneEvent);
    return 0;
}

// ------------------------------------------------------------
// 复制主流程
// ------------------------------------------------------------
DWORD WINAPI CopyThreadProc(LPVOID lpParam) {
    wchar_t* pPaths = (wchar_t*)lpParam;
    totalBytes = 0; totalFiles = 0; copiedBytes = 0; copiedFiles = 0; failedFiles = 0;
    g_failCount = 0; memset(g_failList, 0, sizeof(g_failList)); memset(g_failErrors, 0, sizeof(g_failErrors));
    isVerifying = FALSE; verifiedBytes = 0; verifiedFiles = 0; failedVerifyFiles = 0;
    DirCache_Init();
    ClearVerifyQueue();
    filesScanned = 0; scanDone = 0;
    ClearQueue();
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
        ScanDirectory(srcRoot, srcRoot, wcslen(srcRoot), finalDst);
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
        } else { g_strategy = STRATEGY_BALANCED; }
        g_wasAutoStrategy = TRUE;
    }
    ApplyModeConfig(currentMode);
    wchar_t srcVol[MAX_PATH] = L"", dstVol[MAX_PATH] = L"";
    wcsncpy_s(srcVol, MAX_PATH, pPaths, 3); srcVol[2] = L'\\'; srcVol[3] = L'\0';
    wcsncpy_s(dstVol, MAX_PATH, pPaths + MAX_PATH, 3); dstVol[2] = L'\\'; dstVol[3] = L'\0';
    DWORD srcSerial = 0, dstSerial = 0;
    GetVolumeInformationW(srcVol, NULL, 0, &srcSerial, NULL, NULL, NULL, 0);
    GetVolumeInformationW(dstVol, NULL, 0, &dstSerial, NULL, NULL, NULL, 0);
    BOOL sameDisk = (srcSerial != 0 && srcSerial == dstSerial);
    DiskType srcType = DetectDriveType(pPaths);
    DiskType dstType = DetectDriveType(pPaths + MAX_PATH);
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int workerCount = g_workerThreads > 0 ? g_workerThreads : si.dwNumberOfProcessors * 2;
    size_t tunedBuffer = g_bufferSize;
    
if (sameDisk) {
    if (srcType == DISK_HDD || dstType == DISK_HDD) {
        if (workerCount > 4) workerCount = 4;
    } else {
        if (workerCount > 16) workerCount = 16;
    }
}
    else if (srcType == DISK_NETWORK || dstType == DISK_NETWORK) {
        if (workerCount > 8) workerCount = 8;
        if (tunedBuffer == 0 || tunedBuffer > 64 * 1024 * 1024) tunedBuffer = 64 * 1024 * 1024;
    } else if (srcType == DISK_USB || dstType == DISK_USB) {
        if (workerCount > 8) workerCount = 8;
        if (tunedBuffer == 0 || tunedBuffer > 64 * 1024 * 1024) tunedBuffer = 64 * 1024 * 1024;
    } else if (srcType == DISK_HDD || dstType == DISK_HDD) {
        if (workerCount > 16) workerCount = 16;
    }
    if (workerCount < 2) workerCount = 2;
    if (workerCount > MAX_WORKER_THREADS) workerCount = MAX_WORKER_THREADS;
    g_bufferSize = tunedBuffer;
    int largeConc = 8;
    if (sameDisk) largeConc = 2;
    else if (srcType == DISK_HDD || dstType == DISK_HDD) largeConc = 4;
    else if (srcType == DISK_USB || dstType == DISK_USB) largeConc = 2;
    else if (srcType == DISK_NETWORK || dstType == DISK_NETWORK) largeConc = 4;
    if (hLargeFileSem) CloseHandle(hLargeFileSem);
    hLargeFileSem = CreateSemaphoreW(NULL, largeConc, largeConc, NULL);
    if (hStopEvent) CloseHandle(hStopEvent);
    hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (hScanDoneEvent) CloseHandle(hScanDoneEvent);
    hScanDoneEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    // 重置全局限速器（漏桶）
    g_limLastTick = 0;
    g_limDebtBytes = 0;

    // 大文件缓冲池：槽位数=大文件并发上限，总量封顶 256MB，64KB 对齐。
    // 失败则 g_bufPool 保持 NULL，复制时回退每线程 TLS 缓冲。
    if (g_bufPool) { VirtualFree(g_bufPool, 0, MEM_RELEASE); g_bufPool = NULL; }
    g_bufPool = NULL; g_bufPoolCount = 0; g_bufPoolSlotSize = 0; g_bufFreeCount = 0;
    {
        size_t slotSize = g_bufferSize;
        const size_t POOL_CAP = 256ULL * 1024 * 1024;
        if ((unsigned long long)slotSize * (unsigned)largeConc > POOL_CAP) {
            slotSize = (size_t)(POOL_CAP / (unsigned)largeConc);
            slotSize = (slotSize / (64 * 1024)) * (64 * 1024);
            if (slotSize < 64 * 1024) slotSize = 64 * 1024;
        }
        size_t total = slotSize * (size_t)largeConc;
        unsigned char* pool = (unsigned char*)VirtualAlloc(NULL, total, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (pool) {
            g_bufPool = pool;
            g_bufPoolSlotSize = slotSize;
            g_bufPoolCount = largeConc;
            for (int i = 0; i < largeConc; i++) g_bufFreeSlots[i] = largeConc - 1 - i;
            g_bufFreeCount = largeConc;
        }
    }

    HANDLE* workers = (HANDLE*)malloc(workerCount * sizeof(HANDLE));
    if (!workers) {
        isCopying = 0;
        free(pPaths);
        return 0;
    }
    hWakeEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    g_workerHandleCount = workerCount;
    for (int i = 0; i < workerCount; i++) {
        workers[i] = CreateThread(NULL, 0, WorkerThreadProc, NULL, 0, NULL);
        g_workerHandles[i] = workers[i];
        if (!workers[i]) ShowError(L"dialog_error_thread");
    }
    hScanThread = CreateThread(NULL, 0, ScanThreadProc, pPaths, 0, NULL);
    if (!hScanThread) {
        ShowError(L"dialog_error_thread");
        stopRequested = 1;
        if (hWakeEvent) { for (int i = 0; i < workerCount; i++) SetEvent(hWakeEvent); }
        WaitForMultipleObjects(workerCount, workers, TRUE, 5000);
        for (int i = 0; i < workerCount; i++) if (workers[i]) CloseHandle(workers[i]);
        free(workers);
        g_workerHandleCount = 0;
        memset(g_workerHandles, 0, sizeof(g_workerHandles));
        if (hWakeEvent) { CloseHandle(hWakeEvent); hWakeEvent = NULL; }
        if (hLargeFileSem) { CloseHandle(hLargeFileSem); hLargeFileSem = NULL; }
        if (hStopEvent) { CloseHandle(hStopEvent); hStopEvent = NULL; }
        if (hScanDoneEvent) { CloseHandle(hScanDoneEvent); hScanDoneEvent = NULL; }
        if (g_bufPool) { VirtualFree(g_bufPool, 0, MEM_RELEASE); g_bufPool = NULL; }
        g_bufPoolCount = 0; g_bufPoolSlotSize = 0; g_bufFreeCount = 0;
        isCopying = 0;
        free(pPaths);
        return 0;
    }

    if (hWakeEvent) { for (int i = 0; i < workerCount; i++) SetEvent(hWakeEvent); }
    WaitForMultipleObjects(workerCount, workers, TRUE, INFINITE);
    for (int i = 0; i < workerCount; i++) CloseHandle(workers[i]);
    free(workers);
    g_workerHandleCount = 0;
    memset(g_workerHandles, 0, sizeof(g_workerHandles));
    if (hWakeEvent) { CloseHandle(hWakeEvent); hWakeEvent = NULL; }
    if (hLargeFileSem) { CloseHandle(hLargeFileSem); hLargeFileSem = NULL; }
    if (hStopEvent) { CloseHandle(hStopEvent); hStopEvent = NULL; }
    if (hScanDoneEvent) { CloseHandle(hScanDoneEvent); hScanDoneEvent = NULL; }
    if (hScanThread) {
        WaitForSingleObject(hScanThread, INFINITE);
        CloseHandle(hScanThread);
        hScanThread = NULL;
    }
    if (g_bufPool) { VirtualFree(g_bufPool, 0, MEM_RELEASE); g_bufPool = NULL; }
    g_bufPoolCount = 0; g_bufPoolSlotSize = 0; g_bufFreeCount = 0;
    EnterCriticalSection(&cs);
    int wasStopped = stopRequested;
    LeaveCriticalSection(&cs);

    if (enableVerify && !wasStopped && verifyHead) {
        isVerifying = TRUE;
        verifiedBytes = 0; verifiedFiles = 0; failedVerifyFiles = 0;
        copiedBytes = 0; lastProgress = 0; progressValue = 0;
        currentSpeedMB = 0.0; lastSpeedTime = 0; lastSpeedBytes = 0;
        StringCchCopyW(statusText, ARRAYSIZE(statusText), GetText("status_verifying"));
        int verifyThreads = 4;
        HANDLE verifyHandles[4];
        for (int i = 0; i < verifyThreads; i++) {
            verifyHandles[i] = CreateThread(NULL, 0, VerifyThreadProc, NULL, 0, NULL);
        }
        WaitForMultipleObjects(verifyThreads, verifyHandles, TRUE, INFINITE);
        for (int i = 0; i < verifyThreads; i++) CloseHandle(verifyHandles[i]);
        isVerifying = FALSE;
        verifyHead = verifyTail = NULL;
    } else {
        ClearVerifyQueue();
    }

    // —— 原 WM_THREAD_FINISHED 的内核部分 ——
    if (g_wasAutoStrategy) { g_strategy = STRATEGY_AUTO; g_wasAutoStrategy = FALSE; }
    EnterCriticalSection(&cs);
    isPaused = 0;
    LeaveCriticalSection(&cs);
    if (wasStopped) {
        progressValue = 0; isCopying = 0; stopRequested = 0;
        StringCchCopyW(statusText, ARRAYSIZE(statusText), GetText("status_stopped"));
    } else {
        progressValue = 100; isCopying = 0;
        if (failedVerifyFiles > 0) failedFiles += failedVerifyFiles;
        const wchar_t* done = GetText("dialog_done");
        const wchar_t* stat = GetText("status_done");
        wchar_t failStr[64] = L"";
        if (failedFiles > 0) StringCchPrintfW(failStr, ARRAYSIZE(failStr), GetText("failed_count"), failedFiles);
        wchar_t verifyStr[128] = L"";
        if (enableVerify && failedVerifyFiles == 0 && verifiedFiles > 0) {
            StringCchPrintfW(verifyStr, ARRAYSIZE(verifyStr), L"  %s %d/%d", GetText("verify_done"), verifiedFiles, totalFiles);
        }
        StringCchPrintfW(statusText, ARRAYSIZE(statusText), L"%s %d %s%s%s",
                         stat, totalFiles, done, failStr, verifyStr);
    }
    free(pPaths);
    return 0;
}

DWORD WINAPI WorkerThreadProc(LPVOID lpParam) {
    (void)lpParam;
    InitIoPriority();
    if (g_pSetThreadIoPriority) g_pSetThreadIoPriority(GetCurrentThread(), 4);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    wchar_t src[MAX_PATH], dst[MAX_PATH];
    while (1) {
        EnterCriticalSection(&cs);
        if (stopRequested) { LeaveCriticalSection(&cs); break; }
        FileNode* node = DequeueNode();
        BOOL done = (!node && scanDone);
        LeaveCriticalSection(&cs);
        if (done) break;
        if (!node) {
            // 必须同时等待"扫描结束"和"停止"事件，否则空树/文件数少于线程数
            // 或用户停止时，worker 会永久挂在自动复位的 hWakeEvent 上。
            HANDLE waits[3] = { hWakeEvent, hScanDoneEvent, hStopEvent };
            int n = 0;
            if (hWakeEvent) waits[n++] = hWakeEvent;
            if (hScanDoneEvent) waits[n++] = hScanDoneEvent;
            if (hStopEvent) waits[n++] = hStopEvent;
            if (n > 0) WaitForMultipleObjects(n, waits, FALSE, INFINITE);
            else Sleep(1);
            continue;
        }
        // 暂停门：小文件走 CopyFileEx 期间不可中断，必须在复制前响应暂停
        while (isPaused && !stopRequested) Sleep(10);
        if (stopRequested) { free(node); break; }
        wcscpy(src, node->src); wcscpy(dst, node->dst);
        long long nodeFsize = node->fsize;
        DWORD nodeFattrs = node->fattrs;
        FILETIME nodeFtime = node->fwriteTime;
        free(node);
        BOOL copyOk = FALSE;
        DWORD lastErr = 0;
        for (int retry = 0; retry < 3; retry++) {
            if (stopRequested) break;
            copyOk = CopySingleFile(src, dst, nodeFsize, nodeFattrs, &nodeFtime);
            if (copyOk) break;
            lastErr = GetLastError();
            if (retry < 2 && !stopRequested) {
                for (int s = 0; s < 20 && !stopRequested; s++) Sleep(10);
            }
        }
        if (!copyOk) {
            if (stopRequested) break;
            InterlockedIncrement((LONG volatile*)&failedFiles);
            int idx = InterlockedIncrement((LONG volatile*)&g_failCount) - 1;
            if (idx < MAX_FAIL_LIST) {
                wcscpy_s(g_failList[idx], MAX_PATH, src);
                g_failErrors[idx] = lastErr;
            }

            continue;
        }
        InterlockedIncrement((LONG volatile*)&copiedFiles);
    }
    if (tls_copyBuf) { _aligned_free(tls_copyBuf); tls_copyBuf = NULL; tls_copyBufSize = 0; }
    return 0;
}

BOOL VerifyFileCopy(const wchar_t* src, const wchar_t* dst) {
    WIN32_FILE_ATTRIBUTE_DATA srcAttr, dstAttr;
    if (!GetFileAttributesExW(src, GetFileExInfoStandard, &srcAttr)) return FALSE;
    if (!GetFileAttributesExW(dst, GetFileExInfoStandard, &dstAttr)) return FALSE;
    if (srcAttr.nFileSizeHigh != dstAttr.nFileSizeHigh || srcAttr.nFileSizeLow != dstAttr.nFileSizeLow) {

        return FALSE;
    }
    LARGE_INTEGER fs;
    fs.HighPart = srcAttr.nFileSizeHigh;
    fs.LowPart = srcAttr.nFileSizeLow;
    if (fs.QuadPart == 0) return TRUE;

    HANDLE hS = INVALID_HANDLE_VALUE;
    HANDLE hD = INVALID_HANDLE_VALUE;
    int retry = 0;
    // 源文件通常不会被杀毒软件锁，但也加个重试
    while (retry < 100) {
        hS = CreateFileW(src, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (hS != INVALID_HANDLE_VALUE) break;
        Sleep(20);
        retry++;
    }
    if (hS == INVALID_HANDLE_VALUE) {

        return FALSE;
    }

    retry = 0;
    // 目标文件刚写完，极可能被杀毒软件锁定，最多等 2 秒
    while (retry < 100) {
        hD = CreateFileW(dst, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (hD != INVALID_HANDLE_VALUE) break;
        Sleep(20);
        retry++;
    }
    if (hD == INVALID_HANDLE_VALUE) {

        CloseHandle(hS);
        return FALSE;
    }

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
        if (brS != brD) {

            ok = FALSE; break;
        }
        if (brS == 0) break;
        if (memcmp(bufS, bufD, brS) != 0) {

            ok = FALSE; break;
        }
        if (stopRequested) { ok = FALSE; break; }
    }
    free(bufS); free(bufD);
    CloseHandle(hS); CloseHandle(hD);
    return ok;
}

HANDLE ForceCreateFileRead(const wchar_t* path, DWORD flags) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, flags, NULL);
    if (h != INVALID_HANDLE_VALUE) return h;
    DWORD err = GetLastError();
    if (err == ERROR_SHARING_VIOLATION) {
        h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, flags, NULL);
        if (h != INVALID_HANDLE_VALUE) return h;
        err = GetLastError();
    }
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
    return CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

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
    return CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

BOOL CopySingleFile(const wchar_t* src, const wchar_t* dst, long long fsize, DWORD fattrs, const FILETIME* fwriteTime) {
    WIN32_FILE_ATTRIBUTE_DATA dstAttr;
    memset(&dstAttr, 0, sizeof(dstAttr));
    LARGE_INTEGER srcSize;
    srcSize.QuadPart = fsize;

    // 1. 断点跳过逻辑
    if (!g_destIsFresh) {
        BOOL dstExists = GetFileAttributesExW(dst, GetFileExInfoStandard, &dstAttr);
        if (dstExists && fwriteTime) {
            LARGE_INTEGER dstSize;
            dstSize.HighPart = dstAttr.nFileSizeHigh;
            dstSize.LowPart = dstAttr.nFileSizeLow;
            BOOL sameSize = (dstSize.QuadPart == srcSize.QuadPart);
            BOOL sameTime = (dstAttr.ftLastWriteTime.dwHighDateTime == fwriteTime->dwHighDateTime &&
                             dstAttr.ftLastWriteTime.dwLowDateTime == fwriteTime->dwLowDateTime);
            if (sameSize && sameTime) {
                InterlockedAdd64((LONG64 volatile*)&copiedBytes, srcSize.QuadPart);
                return TRUE;
            }
        }
    }

    // 2. 全局限速（按文件大小粗粒度）
    if (speedLimitKB != 0) {
        RateLimitWait(fsize);
        if (stopRequested) return FALSE;
    }

    // 3. 统一使用 CopyFileExW（内核态复制，保证文件大小绝对正确）
    BOOL result = CopyFileExW(src, dst, NULL, NULL, NULL, 0);
    if (!result && GetLastError() == ERROR_ACCESS_DENIED) {
        SetFileAttributesW(dst, FILE_ATTRIBUTE_NORMAL);
        result = CopyFileExW(src, dst, NULL, NULL, NULL, 0);
    }
    if (!result) return FALSE;

    // 4. 恢复文件属性
    if (fattrs != INVALID_FILE_ATTRIBUTES) {
        SetFileAttributesW(dst, fattrs);
    }

    // 5. 强制落盘（校验开启时，防止读到的只是内存缓存）
    if (enableVerify) {
        HANDLE hFlush = CreateFileW(dst, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFlush == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED) {
            SetFileAttributesW(dst, FILE_ATTRIBUTE_NORMAL);
            hFlush = CreateFileW(dst, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        }
        if (hFlush != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(hFlush);
            CloseHandle(hFlush);
            if (fattrs != INVALID_FILE_ATTRIBUTES) SetFileAttributesW(dst, fattrs);
        }
    }

    InterlockedAdd64((LONG64 volatile*)&copiedBytes, fsize);
    return TRUE;
}

DWORD WINAPI VerifyThreadProc(LPVOID lpParam) {
    (void)lpParam;
    while (1) {
        if (stopRequested) break;
        EnterCriticalSection(&cs);
        FileNode* cur = verifyHead;
        if (cur) verifyHead = verifyHead->next;
        LeaveCriticalSection(&cs);
        if (!cur) break;
        long long fsize = cur->fsize;
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
    if (DirCache_Exists(temp)) return;
    if (GetFileAttributesW(temp) != INVALID_FILE_ATTRIBUTES) {
        DirCache_Add(temp);
        return;
    }
    wchar_t* p = wcsrchr(temp, L'\\');
    if (p) { *p = L'\0'; CreateDirectoriesRecursive(temp); *p = L'\\'; }
    CreateDirectoryW(temp, NULL);
    DirCache_Add(temp);
}

int CheckImpossibleTask(const wchar_t* src, const wchar_t* dst) {
    wchar_t srcFull[MAX_PATH], dstFull[MAX_PATH];
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

// ============================================================
// 导出 C 接口
// ============================================================
BOOL CopyCore_Init(void) {
    if (!g_csInited) {
        InitializeCriticalSectionAndSpinCount(&cs, 4000);
        g_csInited = TRUE;
    }
    isCopying = 0; isPaused = 0; stopRequested = 0;
    StringCchCopyW(statusText, ARRAYSIZE(statusText), GetText("status_ready"));
    return TRUE;
}

void CopyCore_Shutdown(void) {
    if (hCopyThread || isCopying) {
        EnterCriticalSection(&cs);
        stopRequested = 1; isPaused = 0;
        LeaveCriticalSection(&cs);
        if (hStopEvent) SetEvent(hStopEvent);
        if (hScanDoneEvent) SetEvent(hScanDoneEvent);
        if (hWakeEvent) { for (int i = 0; i < 32; i++) SetEvent(hWakeEvent); }
        if (hLargeFileSem) { for (int i = 0; i < 32; i++) ReleaseSemaphore(hLargeFileSem, 1, NULL); }
        for (int i = 0; i < g_workerHandleCount; i++) {
            if (g_workerHandles[i]) CancelSynchronousIo(g_workerHandles[i]);
        }
        if (hScanThread) CancelSynchronousIo(hScanThread);
        if (hCopyThread) {
            WaitForSingleObject(hCopyThread, INFINITE);
            CloseHandle(hCopyThread);
            hCopyThread = NULL;
        }
    }
    ClearQueue();
    ClearVerifyQueue();
    if (g_csInited) {
        DeleteCriticalSection(&cs);
        g_csInited = FALSE;
    }
}

BOOL CopyCore_Start(const wchar_t* src, const wchar_t* dst) {
    if (isCopying) return FALSE;
    if (!src || !dst || !*src || !*dst || CheckImpossibleTask(src, dst)) {
        StringCchCopyW(statusText, ARRAYSIZE(statusText), GetText("dialog_error_unable"));
        return FALSE;
    }
    DWORD sa = GetFileAttributesW(src);
    DWORD da = GetFileAttributesW(dst);
    if (sa == INVALID_FILE_ATTRIBUTES || !(sa & FILE_ATTRIBUTE_DIRECTORY)) {
        StringCchCopyW(statusText, ARRAYSIZE(statusText), GetText("dialog_error_unable"));
        return FALSE;
    }
    if (da == INVALID_FILE_ATTRIBUTES) {
        StringCchCopyW(statusText, ARRAYSIZE(statusText), GetText("dialog_error_unable"));
        return FALSE;
    }
    if (!g_csInited) CopyCore_Init();
    if (hCopyThread) { CloseHandle(hCopyThread); hCopyThread = NULL; }

    StringCchCopyW(srcPath, ARRAYSIZE(srcPath), src);
    StringCchCopyW(dstPath, ARRAYSIZE(dstPath), dst);

    progressValue = 0; lastProgress = 0; isCopying = 1; isPaused = 0; stopRequested = 0;
    if (hStopEvent) ResetEvent(hStopEvent);
    scanDone = 0;
    currentSpeedMB = 0.0; lastSpeedTime = 0; lastSpeedBytes = 0;
    StringCchCopyW(statusText, ARRAYSIZE(statusText), GetText("status_scanning"));

    wchar_t* ps = (wchar_t*)malloc(MAX_PATH * 2 * sizeof(wchar_t));
    if (!ps) {
        isCopying = 0;
        StringCchCopyW(statusText, ARRAYSIZE(statusText), GetText("dialog_error_memory"));
        return FALSE;
    }
    wchar_t* pd = ps + MAX_PATH;
    wcscpy(ps, src); wcscpy(pd, dst);
    hCopyThread = CreateThread(NULL, 0, CopyThreadProc, (LPVOID)ps, 0, NULL);
    if (!hCopyThread) {
        free(ps);
        isCopying = 0;
        StringCchCopyW(statusText, ARRAYSIZE(statusText), GetText("dialog_error_thread"));
        return FALSE;
    }
    return TRUE;
}

void CopyCore_Pause(void) {
    if (!isCopying) return;
    EnterCriticalSection(&cs);
    isPaused = 1;
    LeaveCriticalSection(&cs);
    StringCchCopyW(statusText, ARRAYSIZE(statusText), GetText("status_paused"));
}

void CopyCore_Resume(void) {
    if (!isCopying) return;
    EnterCriticalSection(&cs);
    isPaused = 0;
    LeaveCriticalSection(&cs);
}

void CopyCore_Stop(void) {
    if (!isCopying) return;
    EnterCriticalSection(&cs);
    stopRequested = 1; isPaused = 0;
    LeaveCriticalSection(&cs);
    if (hStopEvent) SetEvent(hStopEvent);
    if (hScanDoneEvent) SetEvent(hScanDoneEvent);
    if (hWakeEvent) { for (int i = 0; i < 32; i++) SetEvent(hWakeEvent); }
    for (int i = 0; i < g_workerHandleCount; i++) {
        if (g_workerHandles[i]) CancelSynchronousIo(g_workerHandles[i]);
    }
    if (hScanThread) CancelSynchronousIo(hScanThread);
    StringCchCopyW(statusText, ARRAYSIZE(statusText), GetText("status_terminating"));
}

void CopyCore_TogglePause(void) {
    if (!isCopying) return;
    EnterCriticalSection(&cs);
    isPaused = !isPaused;
    int nowPaused = isPaused;
    LeaveCriticalSection(&cs);
    if (nowPaused) StringCchCopyW(statusText, ARRAYSIZE(statusText), GetText("status_paused"));
}

void CopyCore_GetStatus(
    int* out_isCopying, int* out_isPaused, int* out_progressPct, double* out_speedMB,
    long long* out_copiedBytes, long long* out_totalBytes,
    int* out_copiedFiles, int* out_totalFiles, int* out_failedFiles,
    int* out_filesScanned, int* out_scanDone,
    wchar_t* out_statusText, int out_statusBufLen)
{
    if (isCopying) {
        // 与原 WM_TIMER(wParam==1) 完全相同的状态计算
        ULONGLONG now = GetTickCount64();
        long long curBytes = isVerifying ? verifiedBytes : copiedBytes;
        if (lastSpeedTime == 0) { lastSpeedTime = now; lastSpeedBytes = curBytes; }
        if (now - lastSpeedTime >= 1000) {
            double deltaMB = (double)(curBytes - lastSpeedBytes) / (1024.0 * 1024.0);
            double deltaSec = (double)(now - lastSpeedTime) / 1000.0;
            // 旧阈值 10MB/s 导致 U 盘/小文件场景速度永远显示 0；
            // 改为始终更新 + EMA 平滑（0.6 旧值/0.4 新值），慢盘也能显示。
            double inst = deltaSec > 0 ? deltaMB / deltaSec : 0.0;
            currentSpeedMB = (currentSpeedMB <= 0.0) ? inst : (currentSpeedMB * 0.6 + inst * 0.4);
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
                if (isVerifying) progressValue = pct;
                else {
                    if (pct < lastProgress) pct = lastProgress;
                    else lastProgress = pct;
                    progressValue = pct;
                }
            }
            const wchar_t* status = isVerifying ? GetText("status_verifying") : GetText("status_copying");
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
            StringCchPrintfW(statusText, ARRAYSIZE(statusText), L"%s %d%%  %.1f %s%s",
                status, progressValue, currentSpeedMB, GetText("speed_unit"), remainStr);
        }
    }

    if (out_isCopying)    *out_isCopying = isCopying;
    if (out_isPaused)     *out_isPaused = isPaused ? 1 : 0;
    if (out_progressPct)  *out_progressPct = progressValue;
    if (out_speedMB)      *out_speedMB = currentSpeedMB;
    if (out_copiedBytes)  *out_copiedBytes = isVerifying ? verifiedBytes : copiedBytes;
    if (out_totalBytes)   *out_totalBytes = totalBytes;
    if (out_copiedFiles)  *out_copiedFiles = isVerifying ? verifiedFiles : copiedFiles;
    if (out_totalFiles)   *out_totalFiles = totalFiles;
    if (out_failedFiles)  *out_failedFiles = failedFiles;
    if (out_filesScanned) *out_filesScanned = filesScanned;
    if (out_scanDone)     *out_scanDone = scanDone ? 1 : 0;
    if (out_statusText && out_statusBufLen > 0) {
        StringCchCopyW(out_statusText, (size_t)out_statusBufLen, statusText);
    }
}

int CopyCore_GetFailList(wchar_t* out_buf, int bufLen) {
    if (!out_buf || bufLen <= 0) return 0;
    out_buf[0] = L'\0';
    int used = 0;

    for (int i = 0; i < g_failCount && i < MAX_FAIL_LIST; i++) {
        wchar_t line[MAX_PATH + 32];
        int n = StringCchPrintfW(line, ARRAYSIZE(line), L"%lu|%s\n",
                                 (unsigned long)g_failErrors[i], g_failList[i]);
        if (n < 0) continue;
        if (used + n + 1 >= bufLen) break;
        wcscpy_s(out_buf + used, (size_t)(bufLen - used), line);
        used += n;
    }
    return used;
}

void CopyCore_SetSpeedLimit(int kbPerSec) {
    if (kbPerSec < 0) kbPerSec = 0;
    if (kbPerSec > 10 * 1024 * 1024) kbPerSec = 10 * 1024 * 1024;
    speedLimitKB = kbPerSec;
}
void CopyCore_SetVerify(int enable) { enableVerify = enable ? 1 : 0; }
void CopyCore_SetMode(int mode) { if (mode >= 0 && mode <= MODE_EXTREME) currentMode = mode; }
void CopyCore_SetStrategy(int strategy) { if (strategy >= 0 && strategy <= STRATEGY_BALANCED) g_strategy = strategy; }
void CopyCore_SetLanguage(int lang) { if (lang >= 0 && lang < 5) currentLang = lang; }
void CopyCore_SetSrcPath(const wchar_t* src) {
    if (!src) return;
    if (g_csInited) EnterCriticalSection(&cs);
    StringCchCopyW(srcPath, ARRAYSIZE(srcPath), src);
    if (g_csInited) LeaveCriticalSection(&cs);
}
void CopyCore_SetDstPath(const wchar_t* dst) {
    if (!dst) return;
    if (g_csInited) EnterCriticalSection(&cs);
    StringCchCopyW(dstPath, ARRAYSIZE(dstPath), dst);
    if (g_csInited) LeaveCriticalSection(&cs);
}

int  CopyCore_GetSpeedLimit(void) { return speedLimitKB; }
int  CopyCore_GetVerify(void) { return enableVerify; }
int  CopyCore_GetMode(void) { return currentMode; }
int  CopyCore_GetStrategy(void) { return g_strategy; }
int  CopyCore_GetLanguage(void) { return currentLang; }
void CopyCore_GetSrcPath(wchar_t* buf, int len) {
    if (buf && len > 0) StringCchCopyW(buf, (size_t)len, srcPath);
}
void CopyCore_GetDstPath(wchar_t* buf, int len) {
    if (buf && len > 0) StringCchCopyW(buf, (size_t)len, dstPath);
}

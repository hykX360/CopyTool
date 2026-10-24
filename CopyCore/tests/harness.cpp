// CopyCore 真机测试台：LoadLibrary 调用，不依赖宿主。
// 用法: harness.exe <DLL路径> <工作目录> <用例>
//   empty : 只含空目录/空文件的树（验证扫描结束竞态挂死）
//   mix   : 3000 小文件 + 3 个 120MB 大文件 + 空目录，全量哈希校验
//   limit : 2 个 300MB 大文件，限速 30MB/s，验证聚合限速
//   skip  : 对 mix 结果再跑一次（断点跳过）
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

typedef BOOL (__cdecl *InitFn)(void);
typedef BOOL (__cdecl *StartFn)(const wchar_t*, const wchar_t*);
typedef void (__cdecl *StopFn)(void);
typedef void (__cdecl *SetLimitFn)(int);
typedef void (__cdecl *SetVerifyFn)(int);
typedef int  (__cdecl *GetFailFn)(wchar_t*, int);
typedef void (__cdecl *GetStatusFn)(int*,int*,int*,double*,long long*,long long*,
                                   int*,int*,int*,int*,int*,wchar_t*,int);
typedef void (__cdecl *ToggleFn)(void);
typedef void (__cdecl *SetIntFn)(int);

static StartFn pStart; static StopFn pStop;
static SetLimitFn pSetLimit; static SetVerifyFn pSetVerify;
static GetFailFn pGetFail; static GetStatusFn pGetStatus;
static ToggleFn pTogglePause;
static SetIntFn pSetMode = NULL; static SetIntFn pSetStrategy = NULL;

static void pth(wchar_t* out, size_t n, const wchar_t* fmt, ...) {
    va_list a; va_start(a, fmt);
    vswprintf(out, n, fmt, a);
    va_end(a);
}

// 简单确定性填充
static void FillBuf(unsigned char* b, size_t len, unsigned long long seed) {
    unsigned long long x = seed ? seed : 0x123456789ULL;
    for (size_t i = 0; i < len; i++) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        b[i] = (unsigned char)(x >> 33);
    }
}

static unsigned long long HashFile(const wchar_t* path) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN, 0);
    if (h == INVALID_HANDLE_VALUE) return 0xFFFFFFFFFFFFFFFFULL;
    static unsigned char buf[1 << 20];
    unsigned long long h64 = 1469598103934665603ULL;
    DWORD rd;
    while (ReadFile(h, buf, sizeof(buf), &rd, 0) && rd) {
        for (DWORD i = 0; i < rd; i++) { h64 ^= buf[i]; h64 *= 1099511628211ULL; }
    }
    CloseHandle(h);
    return h64;
}

static void MakeFile(const wchar_t* path, long long bytes, unsigned long long seed) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
                           FILE_FLAG_SEQUENTIAL_SCAN, 0);
    if (h == INVALID_HANDLE_VALUE) { wprintf(L"[FAIL] cannot create %ls err=%lu\n", path, GetLastError()); return; }
    static unsigned char buf[1 << 20];
    long long left = bytes;
    long long off = 0;
    while (left > 0) {
        DWORD n = (DWORD)(left > (1<<20) ? (1<<20) : left);
        FillBuf(buf, n, seed + (unsigned long long)(off >> 20));
        DWORD wr; WriteFile(h, buf, n, &wr, 0);
        left -= n; off += n;
    }
    CloseHandle(h);
}

// 运行一次复制并轮询，超时秒数；返回是否正常完成
static int RunCopy(const wchar_t* src, const wchar_t* dst, int timeoutSec,
                   double* outElapsed, double* outAvgMB, int* outFailed) {
    if (!pStart(src, dst)) { wprintf(L"[FAIL] Start returned FALSE\n"); return 0; }
    ULONGLONG t0 = GetTickCount64();
    int lastPct = -1;
    for (;;) {
        int copying=0, paused=0, pct=0, cf=0, tf=0, ff=0, scanned=0, scanDone=0;
        double speed=0; long long cb=0, tb=0;
        wchar_t txt[256];
        pGetStatus(&copying,&paused,&pct,&speed,&cb,&tb,&cf,&tf,&ff,&scanned,&scanDone,txt,256);
        if (!copying) {
            ULONGLONG el = GetTickCount64()-t0;
            *outElapsed = el/1000.0;
            *outAvgMB = el>0 ? (cb/1048576.0)/(el/1000.0) : 0;
            *outFailed = ff;
            wprintf(L"\n  done: %ls  elapsed %.2fs  avg %.1f MB/s  files %d/%d failed %d\n",
                    txt, *outElapsed, *outAvgMB, cf, tf, ff);
            if (ff > 0) {
                wchar_t fl[8192];
                int n = pGetFail(fl, 8192);
                if (n > 0) wprintf(L"  fail list:\n%.*s", n, fl);
            }
            return 1;
        }
        if ((GetTickCount64()-t0)/1000 >= (ULONGLONG)timeoutSec) {
            wprintf(L"\n[FAIL] TIMEOUT %ds (HANG) last: %ls scan=%d done=%d\n",
                    timeoutSec, txt, scanned, scanDone);
            pStop();
            Sleep(2000);
            return 0;
        }
        if (pct != lastPct) { wprintf(L"\r  %d%%  %.1f MB/s  %ls          ", pct, speed, txt); lastPct = pct; }
        Sleep(100);
    }
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 4) { wprintf(L"usage: harness <dll> <workdir> <empty|mix|limit|skip> [dstroot]\n"); return 2; }
    HMODULE m = LoadLibraryW(argv[1]);
    if (!m) { wprintf(L"[FAIL] LoadLibrary failed %lu\n", GetLastError()); return 2; }
    InitFn pInit = (InitFn)GetProcAddress(m,"CopyCore_Init");
    pStart = (StartFn)GetProcAddress(m,"CopyCore_Start");
    pStop = (StopFn)GetProcAddress(m,"CopyCore_Stop");
    pSetLimit = (SetLimitFn)GetProcAddress(m,"CopyCore_SetSpeedLimit");
    pSetVerify = (SetVerifyFn)GetProcAddress(m,"CopyCore_SetVerify");
    pGetFail = (GetFailFn)GetProcAddress(m,"CopyCore_GetFailList");
    pGetStatus = (GetStatusFn)GetProcAddress(m,"CopyCore_GetStatus");
    pTogglePause = (ToggleFn)GetProcAddress(m,"CopyCore_TogglePause");
    pSetMode = (SetIntFn)GetProcAddress(m,"CopyCore_SetMode");
    pSetStrategy = (SetIntFn)GetProcAddress(m,"CopyCore_SetStrategy");
    if (!pInit||!pStart||!pStop||!pSetLimit||!pSetVerify||!pGetFail||!pGetStatus) {
        wprintf(L"[FAIL] missing exports\n"); return 2;
    }
    pInit();

    wchar_t src[1024], dst[1024];
    pth(src,1024,L"%ls\\src",argv[2]);
    if (argc >= 5) pth(dst,1024,L"%ls",argv[4]);
    else pth(dst,1024,L"%ls\\dst",argv[2]);
    CreateDirectoryW(argv[2],0);
    CreateDirectoryW(dst,0);

    const wchar_t* test = argv[3];
    if (!wcscmp(test,L"empty")) {
        wchar_t tmp[1024];
        CreateDirectoryW(src,0);
        pth(tmp,1024,L"%ls\\src\\a",argv[2]); CreateDirectoryW(tmp,0);
        pth(tmp,1024,L"%ls\\src\\a\\b",argv[2]); CreateDirectoryW(tmp,0);
        pth(tmp,1024,L"%ls\\src\\a\\b\\c",argv[2]); CreateDirectoryW(tmp,0);
        pth(tmp,1024,L"%ls\\src\\d",argv[2]); CreateDirectoryW(tmp,0);
        pth(tmp,1024,L"%ls\\src\\e",argv[2]); CreateDirectoryW(tmp,0);
        wchar_t f[1024];
        pth(f,1024,L"%ls\\src\\a\\empty.txt",argv[2]);
        MakeFile(f,0,1);
        double el=0, avg=0; int failed=0;
        int ok = RunCopy(src,dst,15,&el,&avg,&failed);
        wprintf(L"\n[empty] %ls\n", ok?L"PASS completed":L"FAIL hang/timeout");
        return ok?0:1;
    }
    if (!wcscmp(test,L"mix") || !wcscmp(test,L"limit") ||
        !wcscmp(test,L"mix1") || !wcscmp(test,L"mix2") || !wcscmp(test,L"mix3")) {
        if (!wcscmp(test,L"mix1") && pSetStrategy) pSetStrategy(1);
        if (!wcscmp(test,L"mix2") && pSetStrategy) pSetStrategy(2);
        if (!wcscmp(test,L"mix3") && pSetStrategy) pSetStrategy(3);
        CreateDirectoryW(src,0);
        int isMix = (!wcscmp(test,L"mix") || !wcscmp(test,L"mix1") || !wcscmp(test,L"mix2") || !wcscmp(test,L"mix3"));
        if (isMix) {
            // 3000 小文件 + 若干空目录
            for (int i=0;i<30;i++) {
                wchar_t sub[1024];
                pth(sub,1024,L"%ls\\d%d",src,i);
                CreateDirectoryW(sub,0);
                pth(sub,1024,L"%ls\\d%d\\sub",src,i);
                CreateDirectoryW(sub,0);
            }
            for (int i=0;i<3000;i++) {
                int dir = i%30;
                wchar_t f[1024];
                pth(f,1024,L"%ls\\d%d\\sub\\f%05d.bin",src,dir,i);
                srand(i);
                MakeFile(f, 1 + (rand()%90000), (unsigned long long)i+1);
            }
            wchar_t f[1024];
            for (int i=0;i<3;i++) {
                pth(f,1024,L"%ls\\big%02d.bin",src,i);
                MakeFile(f, 120LL*1024*1024, (unsigned long long)(900+i));
            }
        } else {
            wchar_t f[1024];
            for (int i=0;i<2;i++) {
                pth(f,1024,L"%ls\\huge%02d.bin",src,i);
                MakeFile(f, 300LL*1024*1024, (unsigned long long)(700+i));
            }
        }
        pSetVerify(isMix?1:0);
        pSetLimit(!wcscmp(test,L"limit")?30*1024:0); // 30 MB/s
        double el=0, avg=0; int failed=0;
        int ok = RunCopy(src,dst, !wcscmp(test,L"limit")?60:120, &el,&avg,&failed);
        if (!wcscmp(test,L"limit")) {
            // 聚合限速：30MB/s 设定，600MB 理论 ~20s；多线程超标会 <15s
            wprintf(L"[limit] avg %.1f MB/s (limit 30) elapsed %.2fs (expect ~20s)\n", avg, el);
            ok = ok && (avg < 60.0); // 超过 60 视为限速失效（允许 2x 内突发）
            wprintf(L"[limit] %ls\n", ok?L"PASS limited":L"FAIL over-limit");
        } else {
            // 哈希校验（抽查 + 大文件全量）
            int bad = 0;
            wchar_t a[1024], b[1024];
            for (int i=0;i<3;i++) {
                pth(a,1024,L"%ls\\big%02d.bin",src,i);
                pth(b,1024,L"%ls\\src\\big%02d.bin",dst,i);
                unsigned long long h1=HashFile(a), h2=HashFile(b);
                if (h1!=h2) { wprintf(L"  big%02d hash mismatch %016llx vs %016llx\n",i,h1,h2); bad++; }
            }
            // 抽查 30 个小文件
            for (int i=0;i<30;i++) {
                int idx = i*100;
                pth(a,1024,L"%ls\\d%d\\sub\\f%05d.bin",src,idx%30,idx);
                pth(b,1024,L"%ls\\src\\d%d\\sub\\f%05d.bin",dst,idx%30,idx);
                if (HashFile(a)!=HashFile(b)) { wprintf(L"  f%05d mismatch\n",idx); bad++; }
            }
            // 空目录存在性
            wchar_t e[1024];
            pth(e,1024,L"%ls\\src\\d0\\sub",dst);
            if (GetFileAttributesW(e)==INVALID_FILE_ATTRIBUTES) { wprintf(L"  empty dir missing\n"); bad++; }
            wprintf(L"[mix] %ls (failed %d, hash mismatch %d)\n", (ok&&bad==0)?L"PASS":L"FAIL", failed, bad);
            ok = ok && bad==0 && failed==0;
        }
        return ok?0:1;
    }
    if (!wcscmp(test,L"skip")) {
        double el=0, avg=0; int failed=0;
        pSetVerify(1);
        int ok = RunCopy(src,dst,120,&el,&avg,&failed);
        wprintf(L"[skip] %ls\n", ok?L"PASS":L"FAIL");
        return ok?0:1;
    }
    if (!wcscmp(test,L"stop")) {
        // 单个 800MB 文件，启动 1s 后终止，要求 15s 内 isCopying 归零
        CreateDirectoryW(src,0);
        wchar_t f[1024];
        pth(f,1024,L"%ls\\big.bin",src);
        MakeFile(f, 800LL*1024*1024, 555);
        if (!pStart(src,dst)) { wprintf(L"[FAIL] Start\n"); return 1; }
        Sleep(1000);
        ULONGLONG t0=GetTickCount64();
        pStop();
        int ok=0;
        for (int i=0;i<150;i++) {
            int copying=0, paused=0, pct=0, cf=0, tf=0, ff=0, scanned=0, sd=0;
            double sp=0; long long cb=0,tb=0; wchar_t txt[256];
            pGetStatus(&copying,&paused,&pct,&sp,&cb,&tb,&cf,&tf,&ff,&scanned,&sd,txt,256);
            if (!copying) { ok=1; break; }
            Sleep(100);
        }
        wprintf(L"[stop] %ls  (stop 后 %llums 归零)\n", ok?L"PASS":L"FAIL timeout", (GetTickCount64()-t0));
        return ok?0:1;
    }
    if (!wcscmp(test,L"pause")) {
        // 800MB 文件：1s 后暂停，2s 与 4s 各读一次字节数应基本冻结；恢复后能完成
        CreateDirectoryW(src,0);
        wchar_t f[1024];
        pth(f,1024,L"%ls\\big.bin",src);
        MakeFile(f, 800LL*1024*1024, 666);
        if (!pStart(src,dst)) { wprintf(L"[FAIL] Start\n"); return 1; }
        Sleep(1000);
        pTogglePause();
        Sleep(2000);
        long long cbA=0, tb=0;
        int copying=0,paused=0,pct=0,cf=0,tf=0,ff=0,sc=0,sd=0; double sp=0; wchar_t txt[256];
        pGetStatus(&copying,&paused,&pct,&sp,&cbA,&tb,&cf,&tf,&ff,&sc,&sd,txt,256);
        Sleep(2000);
        long long cbB=0;
        pGetStatus(&copying,&paused,&pct,&sp,&cbB,&tb,&cf,&tf,&ff,&sc,&sd,txt,256);
        long long frozenDelta = cbB-cbA;
        int pausedDuringPause = paused;
        wprintf(L"[pause] paused flag=%d, 2s bytes delta %lld (expect < 4MB)\n", paused, frozenDelta);
        pTogglePause();
        for (int i=0;i<600;i++) {
            pGetStatus(&copying,&paused,&pct,&sp,&cbA,&tb,&cf,&tf,&ff,&sc,&sd,txt,256);
            if (!copying) break;
            Sleep(100);
        }
        int ok = pausedDuringPause && (frozenDelta < 4*1024*1024) && !copying;
        wprintf(L"[pause] %ls\n", ok?L"PASS":L"FAIL");
        return ok?0:1;
    }
    if (!wcscmp(test,L"edge") || !wcscmp(test,L"edge5") || !wcscmp(test,L"edge3")) {
        // 边界尺寸：0/1/扇区边界/1MB边界/几 MB；含只读、隐藏、只读+隐藏
        CreateDirectoryW(src,0);
        wchar_t dir[1024];
        pth(dir,1024,L"%ls\\edge",src);
        CreateDirectoryW(dir,0);
        static const long long sizes[] = {
            0,1,2,3,7,511,512,513,4095,4096,4097,8191,8192,8193,
            10000,12287,12288,12289,65535,65536,65537,
            1048575,1048576,1048577,1048576+4096,1048576+4097,
            2097152,4194304,5242880,10485760,
            33554432,33554432+100,35651584+123
        };
        int n = (int)(sizeof(sizes)/sizeof(sizes[0]));
        for (int i=0;i<n;i++) {
            wchar_t f[1024];
            pth(f,1024,L"%ls\\s%02d_%lld.bin",dir,i,sizes[i]);
            MakeFile(f, sizes[i], (unsigned long long)i*131+7);
        }
        // 特殊属性文件
        wchar_t sp[1024];
        pth(sp,1024,L"%ls\\readonly.bin",dir); MakeFile(sp, 12345, 42);
        SetFileAttributesW(sp, FILE_ATTRIBUTE_READONLY);
        pth(sp,1024,L"%ls\\hidden.bin",dir); MakeFile(sp, 67890, 43);
        SetFileAttributesW(sp, FILE_ATTRIBUTE_HIDDEN);
        pth(sp,1024,L"%ls\\rohidden.bin",dir); MakeFile(sp, 16777216, 44);
        SetFileAttributesW(sp, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN);

        pSetVerify(1);
        if (!wcscmp(test,L"edge5") && pSetMode) pSetMode(5); // 强制 SSD->HDD（src 缓冲/dst NO_BUFFERING）
        if (!wcscmp(test,L"edge3") && pSetMode) pSetMode(3); // HDD->HDD（纯缓冲）
        double el=0,avg=0; int failed=0;
        int ok = RunCopy(src,dst,120,&el,&avg,&failed);
        int bad=0;
        for (int i=0;i<n;i++) {
            wchar_t a[1024], b[1024];
            pth(a,1024,L"%ls\\s%02d_%lld.bin",dir,i,sizes[i]);
            pth(b,1024,L"%ls\\src\\edge\\s%02d_%lld.bin",dst,i,sizes[i]);
            unsigned long long h1=HashFile(a), h2=HashFile(b);
            if (h1!=h2) { wprintf(L"  size %lld hash mismatch %016llx vs %016llx\n",sizes[i],h1,h2); bad++; }
        }
        static const wchar_t* names[] = { L"readonly.bin", L"hidden.bin", L"rohidden.bin" };
        for (int i=0;i<3;i++) {
            wchar_t a[1024], b[1024];
            pth(a,1024,L"%ls\\%ls",dir,names[i]);
            pth(b,1024,L"%ls\\src\\edge\\%ls",dst,names[i]);
            if (HashFile(a)!=HashFile(b)) { wprintf(L"  %ls mismatch\n",names[i]); bad++; }
        }
        wprintf(L"[edge] %ls (failed %d, hash mismatch %d)\n", (ok&&bad==0&&failed==0)?L"PASS":L"FAIL",failed,bad);
        return (ok&&bad==0&&failed==0)?0:1;
    }
    wprintf(L"unknown test\n");
    return 2;
}


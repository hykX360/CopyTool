// CopyCore 真机基准：LoadLibrary 调 DLL，复制任意真实目录，测时间。
// 用法: bench.exe <dll> <srcDir> <dstRoot> [verify 0|1]
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

typedef BOOL (__cdecl *InitFn)(void);
typedef BOOL (__cdecl *StartFn)(const wchar_t*, const wchar_t*);
typedef void (__cdecl *StopFn)(void);
typedef void (__cdecl *SetVerifyFn)(int);
typedef int  (__cdecl *GetFailFn)(wchar_t*, int);
typedef void (__cdecl *GetStatusFn)(int*,int*,int*,double*,long long*,long long*,
                                   int*,int*,int*,int*,int*,wchar_t*,int);

int wmain(int argc, wchar_t** argv) {
    if (argc < 4) { wprintf(L"usage: bench <dll> <src> <dstroot> [verify]\n"); return 2; }
    HMODULE m = LoadLibraryW(argv[1]);
    if (!m) { wprintf(L"LoadLibrary failed %lu\n", GetLastError()); return 2; }
    InitFn pInit = (InitFn)GetProcAddress(m,"CopyCore_Init");
    StartFn pStart = (StartFn)GetProcAddress(m,"CopyCore_Start");
    StopFn pStop = (StopFn)GetProcAddress(m,"CopyCore_Stop");
    SetVerifyFn pSetVerify = (SetVerifyFn)GetProcAddress(m,"CopyCore_SetVerify");
    GetFailFn pGetFail = (GetFailFn)GetProcAddress(m,"CopyCore_GetFailList");
    GetStatusFn pGetStatus = (GetStatusFn)GetProcAddress(m,"CopyCore_GetStatus");
    if (!pInit||!pStart||!pStop||!pSetVerify||!pGetFail||!pGetStatus) { wprintf(L"missing exports\n"); return 2; }
    pInit();
    pSetVerify(argc>=5 ? _wtoi(argv[4]) : 0);

    if (!pStart(argv[2], argv[3])) { wprintf(L"Start FALSE\n"); return 2; }
    ULONGLONG t0 = GetTickCount64();
    int lastPct = -1;
    for (;;) {
        int copying=0,paused=0,pct=0,cf=0,tf=0,ff=0,sc=0,sd=0;
        double speed=0; long long cb=0,tb=0; wchar_t txt[256];
        pGetStatus(&copying,&paused,&pct,&speed,&cb,&tb,&cf,&tf,&ff,&sc,&sd,txt,256);
        if (!copying) {
            ULONGLONG el = GetTickCount64()-t0;
            double sec = el/1000.0;
            wprintf(L"elapsed=%.2fs  files=%d/%d failed=%d  bytes=%lld/%lld  avg=%.1f MB/s\n",
                    sec, cf, tf, ff, cb, tb, sec>0?(cb/1048576.0)/sec:0);
            wprintf(L"status: %ls\n", txt);
            if (ff>0) {
                wchar_t fl[4096]; int n = pGetFail(fl,4096);
                if (n>0) wprintf(L"fails:\n%.*s", n, fl);
            }
            return ff==0?0:1;
        }
        if ((GetTickCount64()-t0) > 120000) {
            wprintf(L"TIMEOUT 120s  %ls\n", txt);
            pStop(); return 1;
        }
        if (pct != lastPct) { wprintf(L"\r  %d%% %.1f MB/s        ", pct, speed); lastPct=pct; }
        Sleep(100);
    }
}

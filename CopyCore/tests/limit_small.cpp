// 小文件限速验证：480 个 256KB 文件（共 120MB），限速 30MB/s，预期 ~4s
#include <windows.h>
#include <stdio.h>
typedef BOOL (__cdecl *InitFn)(void);
typedef BOOL (__cdecl *StartFn)(const wchar_t*, const wchar_t*);
typedef void (__cdecl *SetLimitFn)(int);
typedef void (__cdecl *GetStatusFn)(int*,int*,int*,double*,long long*,long long*,
                                   int*,int*,int*,int*,int*,wchar_t*,int);
int wmain(int argc, wchar_t** argv) {
    if (argc < 4) { wprintf(L"usage: limit_small <dll> <workdir> <limitKB>\n"); return 2; }
    HMODULE m = LoadLibraryW(argv[1]);
    if (!m) { wprintf(L"LoadLibrary failed %lu\n", GetLastError()); return 2; }
    auto pInit = (InitFn)GetProcAddress(m,"CopyCore_Init");
    auto pStart = (StartFn)GetProcAddress(m,"CopyCore_Start");
    auto pSetLimit = (SetLimitFn)GetProcAddress(m,"CopyCore_SetSpeedLimit");
    auto pGetStatus = (GetStatusFn)GetProcAddress(m,"CopyCore_GetStatus");
    pInit();
    wchar_t src[1024], dst[1024];
    CreateDirectoryW(argv[2],NULL);
    swprintf(src,1024,L"%ls\\src",argv[2]);
    swprintf(dst,1024,L"%ls\\dst",argv[2]);
    CreateDirectoryW(src,NULL);
    unsigned char buf[262144];
    for (int i=0;i<(int)sizeof(buf);i++) buf[i]=(unsigned char)(i*7+3);
    for (int i=0;i<480;i++){
        wchar_t f[1100];
        swprintf(f,1100,L"%ls\\s%03d.bin",src,i);
        HANDLE h=CreateFileW(f,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,0,NULL);
        DWORD wr; WriteFile(h,buf,sizeof(buf),&wr,NULL); CloseHandle(h);
    }
    CreateDirectoryW(dst,NULL);
    int lim = _wtoi(argv[3]);
    pSetLimit(lim);
    ULONGLONG t0=GetTickCount64();
    if (!pStart(src,dst)) { wprintf(L"Start FALSE\n"); return 1; }
    for (;;) {
        int copying=1,paused=0,files=0,total=0,failed=0; double sp=0; long long cb=0,tb=0;
        pGetStatus(&copying,&paused,&files,&sp,&cb,&tb,&files,&total,&failed,NULL,NULL,NULL,0);
        if (!copying){
            double el=(GetTickCount64()-t0)/1000.0;
            double avg = tb>0 ? (double)tb/1048576.0/el : 0;
            wprintf(L"files=%d/%d failed=%d elapsed=%.2fs avg=%.1f MB/s (limit %d)\n",
                    files,total,failed,el,avg,lim/1024);
            // 120MB/30MB/s 理论 4s；允许 3.0~6.0s，且不得有失败
            int pass = (failed==0 && files==480 && el>=3.0 && el<=6.0) ? 1 : 0;
            wprintf(L"[limit_small] %s\n", pass?L"PASS":L"FAIL");
            return pass?0:1;
        }
        Sleep(100);
    }
}

#pragma once
#include <windows.h>

#ifdef COPYCORE_EXPORTS
#define COPYCORE_API __declspec(dllexport)
#else
#define COPYCORE_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

    COPYCORE_API BOOL CopyCore_Init(void);
    COPYCORE_API void CopyCore_Shutdown(void);

    COPYCORE_API BOOL CopyCore_Start(const wchar_t* src, const wchar_t* dst);
    COPYCORE_API void CopyCore_Pause(void);
    COPYCORE_API void CopyCore_Resume(void);
    COPYCORE_API void CopyCore_Stop(void);
    COPYCORE_API void CopyCore_TogglePause(void);

    COPYCORE_API void CopyCore_GetStatus(
        int*        out_isCopying,
        int*        out_isPaused,
        int*        out_progressPct,
        double*     out_speedMB,
        long long*  out_copiedBytes,
        long long*  out_totalBytes,
        int*        out_copiedFiles,
        int*        out_totalFiles,
        int*        out_failedFiles,
        int*        out_filesScanned,
        int*        out_scanDone,
        wchar_t*    out_statusText,
        int         out_statusBufLen
    );

    /* 失败项打包为 "错误码|路径\n错误码|路径\n..."，返回写入的字符数（不含结尾 \0） */
    COPYCORE_API int CopyCore_GetFailList(wchar_t* out_buf, int bufLen);

    COPYCORE_API void CopyCore_SetSpeedLimit(int kbPerSec);
    COPYCORE_API void CopyCore_SetVerify(int enable);
    COPYCORE_API void CopyCore_SetMode(int mode);
    COPYCORE_API void CopyCore_SetStrategy(int strategy);
    COPYCORE_API void CopyCore_SetLanguage(int lang);
    COPYCORE_API void CopyCore_SetSrcPath(const wchar_t* src);
    COPYCORE_API void CopyCore_SetDstPath(const wchar_t* dst);

    COPYCORE_API int  CopyCore_GetSpeedLimit(void);
    COPYCORE_API int  CopyCore_GetVerify(void);
    COPYCORE_API int  CopyCore_GetMode(void);
    COPYCORE_API int  CopyCore_GetStrategy(void);
    COPYCORE_API int  CopyCore_GetLanguage(void);
    COPYCORE_API void CopyCore_GetSrcPath(wchar_t* buf, int len);
    COPYCORE_API void CopyCore_GetDstPath(wchar_t* buf, int len);

#ifdef __cplusplus
}
#endif

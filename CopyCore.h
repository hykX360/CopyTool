#ifndef COPYCORE_H
#define COPYCORE_H

#ifdef BUILD_DLL
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

EXPORT void InitCopyEngine(void);
EXPORT void StartCopy(const char* src, const char* dst);
EXPORT void PauseCopy(void);
EXPORT void ResumeCopy(void);
EXPORT void StopCopy(void);
EXPORT int  GetProgress(void);
EXPORT const char* GetStatus(void);
EXPORT int  IsCopying(void);

#ifdef __cplusplus
}
#endif

#endif
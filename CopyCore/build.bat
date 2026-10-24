@echo off
setlocal

set MINGW_BIN=C:\mingw64\bin
set GCC=%MINGW_BIN%\gcc.exe
set GPP=%MINGW_BIN%\g++.exe

cd /d "%~dp0"

if not exist "CopyCore.cpp" (
    echo ERROR: CopyCore.cpp not found in %CD%
    exit /b 1
)
if not exist "rpmalloc.c" (
    echo ERROR: rpmalloc.c not found in %CD%
    exit /b 1
)

if not exist "x64\Release" mkdir "x64\Release"
if not exist "obj" mkdir "obj"

set PATH=%MINGW_BIN%;%PATH%

echo [1/2] Compiling rpmalloc.c (with ENABLE_OVERRIDE)...
"%GCC%" -c -O2 -DENABLE_OVERRIDE=1 -I. rpmalloc.c -o obj\rpmalloc.o
if errorlevel 1 exit /b 1

echo [2/2] Compiling CopyCore.cpp and linking...
"%GPP%" -shared -O2 -std=c++14 -DENABLE_OVERRIDE=1 -I. ^
  CopyCore.cpp obj\rpmalloc.o ^
  -o x64\Release\CopyCore.dll ^
  -ladvapi32 -lkernel32 -static-libgcc -static-libstdc++ ^
  -Wl,--out-implib,x64\Release\libCopyCore.a

set RC=%ERRORLEVEL%
echo build exit=%RC%
endlocal & exit /b %RC%
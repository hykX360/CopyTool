@echo off
REM CopyCore.dll 构建脚本（MinGW x64）
setlocal

set MINGW_BIN=C:\mingw64\bin
set GPP=%MINGW_BIN%\g++.exe
set SRC=%~dp0CopyCore.cpp
set OUTDIR=%~dp0x64\Release

if not exist "%GPP%" (
    echo ERROR: %GPP% not found.
    exit /b 1
)
if not exist "%SRC%" (
    echo ERROR: %SRC% not found.
    exit /b 1
)
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

set PATH=%MINGW_BIN%;%PATH%

"%GPP%" -shared -O2 -std=c++14 "%SRC%" -o "%OUTDIR%\CopyCore.dll" ^
  -ladvapi32 -lkernel32 -static-libgcc -static-libstdc++ ^
  -Wl,--out-implib,"%OUTDIR%\libCopyCore.a"

set RC=%ERRORLEVEL%
echo build exit=%RC%
endlocal & exit /b %RC%
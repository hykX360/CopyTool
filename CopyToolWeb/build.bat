@echo off
setlocal

REM 自动结束可能残留的进程，避免链接器覆盖文件时出现 Permission denied
taskkill /F /IM CopyToolWeb.exe >nul 2>&1
timeout /t 1 /nobreak >nul

REM 配置 MinGW 工具链路径
REM windres 内部调用 gcc 进行预处理，必须将 mingw64\bin 加入 PATH
set PATH=C:\mingw64\bin;%PATH%
set GPP=C:\mingw64\bin\c++.exe
set WINDRES=C:\mingw64\bin\windres.exe

if not exist "%GPP%" set GPP=c++.exe
if not exist "%WINDRES%" set WINDRES=windres.exe

if not exist bin mkdir bin

echo [1/3] Compiling resources...
"%WINDRES%" -O coff -i resource.rc -o resource.o
if errorlevel 1 ( echo RESOURCE BUILD FAILED & exit /b 1 )

echo [2/3] Compiling main executable...
"%GPP%" -O2 -std=c++17 -municode -mwindows -DUNICODE -D_UNICODE ^
  -Iinclude main.cpp resource.o -o bin\CopyToolWeb.exe ^
  -lole32 -loleaut32 -luuid -lshlwapi -ldwmapi -luser32 -lgdi32 -lcomctl32 -lshell32 ^
  -static-libgcc -static-libstdc++
if errorlevel 1 ( echo BUILD FAILED & exit /b 1 )

echo [3/3] Deploying dependencies...
copy /Y "..\CopyCore\x64\Release\CopyCore.dll" "bin\CopyCore.dll" >nul
copy /Y "sdk\pkg\runtimes\win-x64\native\WebView2Loader.dll" "bin\WebView2Loader.dll" >nul

echo.
echo === BUILD OK ===
echo --- WebUI resource sizes (compare with host_log) ---
for %%F in (WebUI\index.html WebUI\css\styles.css WebUI\ui.js WebUI\renderer.js) do (
    if exist "%%F" echo   %%~nxF: %%~zF bytes
)
echo ===================================================
dir /b bin
endlocal
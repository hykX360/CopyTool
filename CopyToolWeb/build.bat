@echo off
setlocal

REM 自动结束可能残留的进程，避免链接器覆盖文件时出现 Permission denied
taskkill /F /IM CopyToolWeb.exe >nul 2>&1
timeout /t 1 /nobreak >nul

REM 配置 MinGW 工具链路径
set PATH=C:\mingw64\bin;%PATH%
set GPP=C:\mingw64\bin\c++.exe
set WINDRES=C:\mingw64\bin\windres.exe

if not exist "%GPP%" set GPP=c++.exe
if not exist "%WINDRES%" set WINDRES=windres.exe

if not exist bin mkdir bin

echo [1/4] Compiling manifest resource...
"%WINDRES%" -O coff -i resource.rc -o resource.o
if errorlevel 1 ( echo RESOURCE BUILD FAILED & exit /b 1 )

echo [2/4] Compiling icon resource...
REM 检查 E:\CopyTool 下的 icon.rc 是否存在，存在则编译并链接
set ICON_OBJ=
if exist "E:\CopyTool\icon.rc" (
    "%WINDRES%" -O coff -i "E:\CopyTool\icon.rc" -o "icon.o"
    if errorlevel 1 ( echo ICON BUILD FAILED & exit /b 1 )
    set ICON_OBJ=icon.o
    echo   Icon compiled successfully.
) else (
    echo   [WARNING] E:\CopyTool\icon.rc not found, skipping icon.
)

echo [3/4] Compiling main executable...
"%GPP%" -O2 -std=c++17 -municode -mwindows -DUNICODE -D_UNICODE ^
  -Iinclude main.cpp resource.o %ICON_OBJ% -o bin\CopyToolWeb.exe ^
  -lole32 -loleaut32 -luuid -lshlwapi -ldwmapi -luser32 -lgdi32 -lcomctl32 -lshell32 ^
  -static-libgcc -static-libstdc++
if errorlevel 1 ( echo BUILD FAILED & exit /b 1 )

echo [4/4] Deploying dependencies...
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
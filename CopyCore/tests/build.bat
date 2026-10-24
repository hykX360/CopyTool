@echo off
rem CopyCore 真机测试工具构建脚本
rem 产物: bench.exe / harness.exe / limit_small.exe（*.exe 已被 .gitignore 忽略，不入库）
setlocal
set "CXX=g++"
where %CXX% >nul 2>&1
if errorlevel 1 (
    if exist "C:\mingw64\bin\g++.exe" (
        set "CXX=C:\mingw64\bin\g++.exe"
    ) else (
        echo [ERROR] g++ not found in PATH and C:\mingw64\bin\g++.exe missing
        exit /b 1
    )
)
echo [1/3] bench.exe
"%CXX%" -O2 -std=c++14 -municode bench.cpp -o bench.exe
if errorlevel 1 goto :err
echo [2/3] harness.exe
"%CXX%" -O2 -std=c++14 -municode harness.cpp -o harness.exe
if errorlevel 1 goto :err
echo [3/3] limit_small.exe
"%CXX%" -O2 -std=c++14 -municode limit_small.cpp -o limit_small.exe
if errorlevel 1 goto :err
echo Build OK.
exit /b 0
:err
echo Build FAILED.
exit /b 1
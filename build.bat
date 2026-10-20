@echo off
setlocal

set "CONFIG=Debug"

if /I "%~1"=="release" set "CONFIG=Release"

if /I not "%~1"=="debug" if /I not "%~1"=="release" if not "%~1"=="" (
    echo Usage: build.bat [debug^|release]
    exit /b 1
)

cd /d "%~dp0"

cmake -S . -B "build\%CONFIG%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 exit /b %errorlevel%

cmake --build "build\%CONFIG%" --parallel
exit /b %errorlevel%

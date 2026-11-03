@echo off
setlocal

set "CONFIG=Debug"

if /I "%~1"=="release" set "CONFIG=Release"

if /I not "%~1"=="debug" if /I not "%~1"=="release" if not "%~1"=="" (
    echo Usage: build.bat [debug^|release]
    exit /b 1
)

cd /d "%~dp0"

echo ==^> Configuration (%CONFIG%) : le premier lancement telecharge raylib, EnTT et Catch2, ca peut prendre quelques minutes
cmake -S . -B "build\%CONFIG%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 exit /b %errorlevel%

echo ==^> Compilation
cmake --build "build\%CONFIG%" --parallel
if errorlevel 1 exit /b %errorlevel%

echo ==^> Build %CONFIG% termine
exit /b 0

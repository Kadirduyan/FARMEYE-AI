@echo off
setlocal EnableExtensions
title FarmEye AI UI Build

REM Prefer cmd-only; call vcvars then cmake
REM VS 18 = Visual Studio 2026; also try 2022 Community/BuildTools
set "VCVARS="
if exist "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" (
  set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
  set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
  set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
  set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
  for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
  )
)
if not defined VCVARS (
  echo [HATA] vcvars64.bat bulunamadi
  echo Visual Studio Installer ^> Modify ^> "Desktop development with C++" yukleyin.
  pause
  exit /b 1
)
echo Using: %VCVARS%

call "%VCVARS%"
cd /d "%~dp0"

echo === CMake configure ===
cmake -B build-ui -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 ^
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
  -DFARMEYE_BUILD_ENGINE=ON -DFARMEYE_BUILD_UI=ON
if errorlevel 1 (
  echo [HATA] cmake configure
  pause
  exit /b 1
)

echo === Build ===
cmake --build build-ui --config Release
if errorlevel 1 (
  echo [HATA] build
  pause
  exit /b 1
)

echo.
echo OK: %~dp0build-ui\FarmEyeAI.exe
if exist "%~dp0build-ui\FarmEyeAI.exe" (
  echo Calistirmak icin Enter...
  pause
  start "" "%~dp0build-ui\FarmEyeAI.exe"
) else (
  pause
)
endlocal

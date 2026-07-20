@echo off
setlocal EnableExtensions
cd /d C:\FarmEye\FARMEYE-AI

set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
  echo NO_VCVARS
  exit /b 1
)

call "%VCVARS%"
if errorlevel 1 exit /b 1

echo === TOOLS ===
where cmake
where ninja
where cl

set "QTPRE=C:/Qt/6.8.3/msvc2022_64"
if not exist "C:\Qt\6.8.3\msvc2022_64\bin\qmake.exe" (
  echo NO_QT
  exit /b 1
)

echo === CMAKE NINJA ===
cmake -B build-ui -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=%QTPRE% -DFARMEYE_BUILD_ENGINE=ON -DFARMEYE_BUILD_UI=ON
if errorlevel 1 (
  echo === CMAKE VS ===
  cmake -B build-ui-vs -G "Visual Studio 18 2026" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=%QTPRE% -DFARMEYE_BUILD_ENGINE=ON -DFARMEYE_BUILD_UI=ON
  if errorlevel 1 (
    cmake -B build-ui-vs -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=%QTPRE% -DFARMEYE_BUILD_ENGINE=ON -DFARMEYE_BUILD_UI=ON
  )
  cmake --build build-ui-vs --config Release -j 8
  set "CAND1=C:\FarmEye\FARMEYE-AI\build-ui-vs\Release\FarmEyeAI.exe"
  set "CAND2=C:\FarmEye\FARMEYE-AI\build-ui-vs\FarmEyeAI.exe"
) else (
  cmake --build build-ui --config Release -j 8
  set "CAND1=C:\FarmEye\FARMEYE-AI\build-ui\FarmEyeAI.exe"
  set "CAND2=C:\FarmEye\FARMEYE-AI\build-ui\Release\FarmEyeAI.exe"
)

echo === LOOK FOR EXE ===
if exist "%CAND1%" (
  echo START %CAND1%
  start "" "%CAND1%"
  exit /b 0
)
if exist "%CAND2%" (
  echo START %CAND2%
  start "" "%CAND2%"
  exit /b 0
)
echo NO_EXE
dir /s /b C:\FarmEye\FARMEYE-AI\FarmEyeAI.exe 2>nul
dir /s /b C:\FarmEye\FARMEYE-AI\build-ui\*.exe 2>nul
dir /s /b C:\FarmEye\FARMEYE-AI\build-ui-vs\*.exe 2>nul
exit /b 2

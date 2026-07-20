@echo off
setlocal
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
  echo vcvars missing
  exit /b 1
)
call "%VCVARS%"
cd /d C:\FarmEye\FARMEYE-AI
cmake -B build-ui -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 -DFARMEYE_BUILD_ENGINE=ON -DFARMEYE_BUILD_UI=ON
if errorlevel 1 exit /b 1
cmake --build build-ui --config Release
exit /b %ERRORLEVEL%

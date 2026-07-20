@echo off
cd /d C:\FarmEye\FARMEYE-AI
taskkill /F /IM FarmEyeAI.exe >nul 2>&1
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cmake -B build-ui -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 -DFARMEYE_BUILD_ENGINE=ON -DFARMEYE_BUILD_UI=ON
if errorlevel 1 exit /b 1
cmake --build build-ui --config Release
if errorlevel 1 exit /b 1
call deploy_and_run.bat

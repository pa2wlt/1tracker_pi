:: Build script for appveyor, https://www.appveyor.com
:: Builds one version linked against wxWidgets 3.2

@echo off
setlocal enabledelayedexpansion

set "SCRIPTDIR=%~dp0"
set "GIT_HOME=C:\Program Files\Git"

:: %CONFIGURATION% comes from appveyor.yml, set a default if invoked elsewise.
if "%CONFIGURATION%" == "" set "CONFIGURATION=RelWithDebInfo"

call %SCRIPTDIR%..\buildwin\win_deps.bat
if errorlevel 1 (echo ERROR: win_deps.bat failed & exit /b 1)
echo USING wxWidgets_LIB_DIR: !wxWidgets_LIB_DIR!
echo USING wxWidgets_ROOT_DIR: !wxWidgets_ROOT_DIR!

if not defined VCINSTALLDIR (
  for /f "tokens=* USEBACKQ" %%p in (
    `"%programfiles(x86)%\Microsoft Visual Studio\Installer\vswhere" ^
    -latest -property installationPath`
  ) do call "%%p\Common7\Tools\vsDevCmd.bat"
)

if exist build (rmdir /s /q build)
mkdir build && cd build

set "VCPKG_TOOLCHAIN="
if exist "C:\Tools\vcpkg\scripts\buildsystems\vcpkg.cmake" (
  set "VCPKG_TOOLCHAIN=-DCMAKE_TOOLCHAIN_FILE=C:/Tools/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x86-windows"
)

cmake -A Win32 -G "Visual Studio 17 2022" ^
    -DCMAKE_GENERATOR_PLATFORM=Win32 ^
    -DCMAKE_BUILD_TYPE=%CONFIGURATION% ^
    -DwxWidgets_LIB_DIR=!wxWidgets_LIB_DIR! ^
    -DwxWidgets_ROOT_DIR=!wxWidgets_ROOT_DIR! ^
    !VCPKG_TOOLCHAIN! ^
    -DOCPN_TARGET_TUPLE=msvc-wx32;10;x86_64 ^
    ..
if errorlevel 1 (echo ERROR: cmake configure failed & exit /b 1)

cmake --build . --target tarball --config %CONFIGURATION%
if errorlevel 1 (echo ERROR: cmake --build tarball failed & exit /b 1)

:: Display dependencies debug info — failure here must not mask success above,
:: but we only expect it to work if the build produced the DLL.
echo import glob; import subprocess > ldd.py
echo lib = glob.glob("app/*/plugins/*.dll")[0] >> ldd.py
echo subprocess.call(['dumpbin', '/dependents', lib], shell=True) >> ldd.py
python ldd.py

echo Uploading artifact
call upload.bat
if errorlevel 1 (echo ERROR: upload.bat failed & exit /b 1)

echo Pushing updates to catalog
python %SCRIPTDIR%..\ci\git-push
if errorlevel 1 (echo ERROR: git-push failed & exit /b 1)
cd ..

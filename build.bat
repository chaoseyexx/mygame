@echo off
setlocal
cd /d "%~dp0"
set "PATH=C:\Program Files\CMake\bin;C:\Users\chaos\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;C:\Users\chaos\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20260908-ucrt-x86_64\bin;%PATH%"
echo Recompiling Mercenary Custom Engine...
cmake --build build --config Release
if %ERRORLEVEL% equ 0 (
    copy /y "build\MercenaryGame.exe" "MercenaryGame.exe" >nul
    copy /y "build\MercenaryServer.exe" "MercenaryServer.exe" >nul
    copy /y "build\MercenaryEditor.exe" "MercenaryEditor.exe" >nul
    echo [SUCCESS] Build complete and binaries synchronized! Run run.bat to launch Mercenary.
) else (
    echo [ERROR] Build failed.
)
pause

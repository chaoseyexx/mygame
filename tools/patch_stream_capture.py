import re

# 1. Patch src/main.cpp
with open('src/main.cpp', 'r', encoding='utf-8') as f:
    main_code = f.read()

main_target = """#ifdef _WIN32
#include <windows.h>
#endif"""

main_repl = """#ifdef _WIN32
#include <windows.h>
// Request high-performance discrete GPU on hybrid / dual-GPU laptops (NVIDIA Optimus & AMD PowerXpress).
// Fixes OpenGL black screen when capturing via Discord Game Share, OBS Game Capture, or virtual camera.
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif"""

if main_target in main_code:
    main_code = main_code.replace(main_target, main_repl, 1)
    with open('src/main.cpp', 'w', encoding='utf-8', newline='') as f:
        f.write(main_code)
    print("Patched src/main.cpp successfully")
else:
    print("Target not found in src/main.cpp")

# 2. Patch src/engine/Window.cpp
with open('src/engine/Window.cpp', 'r', encoding='utf-8') as f:
    win_code = f.read()

# Update init styles: set WS_EX_APPWINDOW and retain WS_SYSMENU | WS_MINIMIZEBOX for FullscreenWindowed
win_target1 = """#ifdef _WIN32
    HWND nativeHwnd = glfwGetWin32Window(m_window);
    if (nativeHwnd) {
        if (m_displayMode == WindowDisplayMode::FullscreenWindowed) {
            LONG_PTR style = GetWindowLongPtr(nativeHwnd, GWL_STYLE);
            style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
            style |= WS_POPUP | WS_VISIBLE;
            SetWindowLongPtr(nativeHwnd, GWL_STYLE, style);
            SetWindowPos(nativeHwnd, HWND_TOP, 0, 0, monW, monH, SWP_SHOWWINDOW | SWP_FRAMECHANGED | SWP_NOACTIVATE);
        }
        BOOL disableTransitions = TRUE;
        DwmSetWindowAttribute(nativeHwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &disableTransitions, sizeof(disableTransitions));
    }
#endif"""

win_repl1 = """#ifdef _WIN32
    HWND nativeHwnd = glfwGetWin32Window(m_window);
    if (nativeHwnd) {
        // Ensure WS_EX_APPWINDOW is set so Discord, OBS, Windows Graphics Capture (WGC),
        // and the Windows taskbar/DWM recognize the game as a top-level streamable application.
        LONG_PTR exStyle = GetWindowLongPtr(nativeHwnd, GWL_EXSTYLE);
        exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
        exStyle |= WS_EX_APPWINDOW;
        SetWindowLongPtr(nativeHwnd, GWL_EXSTYLE, exStyle);

        if (m_displayMode == WindowDisplayMode::FullscreenWindowed) {
            // Retain WS_SYSMENU and WS_MINIMIZEBOX so Discord application enumeration
            // and OBS window capture recognize this as a valid application window rather than a popup tooltip.
            LONG_PTR style = GetWindowLongPtr(nativeHwnd, GWL_STYLE);
            style &= ~(WS_CAPTION | WS_THICKFRAME);
            style |= WS_POPUP | WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX;
            SetWindowLongPtr(nativeHwnd, GWL_STYLE, style);
            SetWindowPos(nativeHwnd, HWND_TOP, 0, 0, monW, monH, SWP_SHOWWINDOW | SWP_FRAMECHANGED | SWP_NOACTIVATE);
        }
    }
#endif"""

if win_target1 in win_code:
    win_code = win_code.replace(win_target1, win_repl1, 1)
    print("Patched Window.cpp target 1 (init styles)")
else:
    print("Window.cpp target 1 not found")

# Update setDisplayMode styles
win_target2 = """#ifdef _WIN32
        HWND curHwnd = glfwGetWin32Window(m_window);
        if (curHwnd) {
            LONG_PTR style = GetWindowLongPtr(curHwnd, GWL_STYLE);
            style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
            style |= WS_POPUP | WS_VISIBLE;
            SetWindowLongPtr(curHwnd, GWL_STYLE, style);
            SetWindowPos(curHwnd, HWND_TOP, 0, 0, monW, monH, SWP_SHOWWINDOW | SWP_FRAMECHANGED | SWP_NOACTIVATE);
        }
#endif"""

win_repl2 = """#ifdef _WIN32
        HWND curHwnd = glfwGetWin32Window(m_window);
        if (curHwnd) {
            LONG_PTR exStyle = GetWindowLongPtr(curHwnd, GWL_EXSTYLE);
            exStyle |= WS_EX_APPWINDOW;
            SetWindowLongPtr(curHwnd, GWL_EXSTYLE, exStyle);

            LONG_PTR style = GetWindowLongPtr(curHwnd, GWL_STYLE);
            style &= ~(WS_CAPTION | WS_THICKFRAME);
            style |= WS_POPUP | WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX;
            SetWindowLongPtr(curHwnd, GWL_STYLE, style);
            SetWindowPos(curHwnd, HWND_TOP, 0, 0, monW, monH, SWP_SHOWWINDOW | SWP_FRAMECHANGED | SWP_NOACTIVATE);
        }
#endif"""

if win_target2 in win_code:
    win_code = win_code.replace(win_target2, win_repl2, 1)
    print("Patched Window.cpp target 2 (setDisplayMode)")
else:
    print("Window.cpp target 2 not found")

# Update setFullscreen / toggleFullscreen
win_target3 = """void Window::setFullscreen(bool fullscreen) {
    setDisplayMode(fullscreen ? WindowDisplayMode::ExclusiveFullscreen : WindowDisplayMode::FullscreenWindowed);
}

void Window::toggleFullscreen() {
    if (m_displayMode == WindowDisplayMode::FullscreenWindowed) {
        setDisplayMode(WindowDisplayMode::ExclusiveFullscreen);
    } else {
        setDisplayMode(WindowDisplayMode::FullscreenWindowed);
    }
}"""

win_repl3 = """void Window::setFullscreen(bool fullscreen) {
    setDisplayMode(fullscreen ? WindowDisplayMode::FullscreenWindowed : WindowDisplayMode::Windowed);
}

void Window::toggleFullscreen() {
    if (m_displayMode == WindowDisplayMode::FullscreenWindowed || m_displayMode == WindowDisplayMode::ExclusiveFullscreen) {
        setDisplayMode(WindowDisplayMode::Windowed);
    } else {
        setDisplayMode(WindowDisplayMode::FullscreenWindowed);
    }
}"""

if win_target3 in win_code:
    win_code = win_code.replace(win_target3, win_repl3, 1)
    print("Patched Window.cpp target 3 (setFullscreen / toggleFullscreen)")
else:
    print("Window.cpp target 3 not found")

with open('src/engine/Window.cpp', 'w', encoding='utf-8', newline='') as f:
    f.write(win_code)

# 3. Patch src/game/GameApp.cpp
with open('src/game/GameApp.cpp', 'r', encoding='utf-8') as f:
    game_code = f.read()

game_target = """    config.displayMode = isCaptureOrWindowedTest ? WindowDisplayMode::Windowed : 
                         (cfgFullscreen ? WindowDisplayMode::ExclusiveFullscreen : WindowDisplayMode::FullscreenWindowed);"""

game_repl = """    config.displayMode = isCaptureOrWindowedTest ? WindowDisplayMode::Windowed : 
                         (cfgFullscreen ? WindowDisplayMode::FullscreenWindowed : WindowDisplayMode::Windowed);"""

if game_target in game_code:
    game_code = game_code.replace(game_target, game_repl, 1)
    with open('src/game/GameApp.cpp', 'w', encoding='utf-8', newline='') as f:
        f.write(game_code)
    print("Patched GameApp.cpp displayMode default successfully")
else:
    print("GameApp.cpp displayMode target not found")

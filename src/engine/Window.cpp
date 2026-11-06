#include "Window.hpp"
#include <iostream>
#include <algorithm>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#endif

namespace Mercenary {

Window::Window() {}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}

bool Window::init(const WindowConfig& config) {
    if (!glfwInit()) {
        std::cerr << "[Engine Error] Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // 4x MSAA antialiasing
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // STRICTLY NON-RESIZABLE BY PLAYER (Never allow arbitrary dragging/scaling)

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* vidMode = primaryMonitor ? glfwGetVideoMode(primaryMonitor) : nullptr;
    int monW = vidMode ? vidMode->width : 1920;
    int monH = vidMode ? vidMode->height : 1080;
    int refreshRate = vidMode ? vidMode->refreshRate : 60;

    m_displayMode = config.displayMode;

    if (m_displayMode == WindowDisplayMode::ExclusiveFullscreen) {
        glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_TRUE);
        m_width = monW;
        m_height = monH;
        m_window = glfwCreateWindow(monW, monH, config.title.c_str(), primaryMonitor, nullptr);
    } else if (m_displayMode == WindowDisplayMode::FullscreenWindowed) {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
        m_width = monW;
        m_height = monH;
        m_window = glfwCreateWindow(monW, monH, config.title.c_str(), nullptr, nullptr);
        if (m_window) {
            glfwSetWindowPos(m_window, 0, 0);
        }
    } else { // Windowed (Fixed resolution, e.g. for developer / automated capture tests)
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        m_width = (config.width > 0) ? config.width : 1600;
        m_height = (config.height > 0) ? config.height : 900;
        m_window = glfwCreateWindow(m_width, m_height, config.title.c_str(), nullptr, nullptr);
        if (m_window) {
            int posX = std::max(0, (monW - m_width) / 2);
            int posY = std::max(0, (monH - m_height) / 2);
            glfwSetWindowPos(m_window, posX, posY);
            glfwSetWindowSizeLimits(m_window, m_width, m_height, m_width, m_height); // Lock size!
        }
    }

    if (!m_window) {
        std::cerr << "[Engine Error] Failed to create GLFW window in requested mode. Trying fallback windowed.\n";
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        m_width = 1600;
        m_height = 900;
        m_window = glfwCreateWindow(m_width, m_height, config.title.c_str(), nullptr, nullptr);
        m_displayMode = WindowDisplayMode::Windowed;
        if (!m_window) {
            std::cerr << "[Engine Error] Failed to create GLFW window fallback\n";
            glfwTerminate();
            return false;
        }
    }

    // Force non-resizable attribute on the native window so Windows borders cannot be grabbed
    glfwSetWindowAttrib(m_window, GLFW_RESIZABLE, GLFW_FALSE);

#ifdef _WIN32
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
#endif

    // Query true initial framebuffer resolution in pixels (handles high-DPI scaling & display offsets)
    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
    if (fbWidth > 0 && fbHeight > 0) {
        m_width = fbWidth;
        m_height = fbHeight;
    }
    m_windowedWidth = m_width;
    m_windowedHeight = m_height;
    glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);

    glfwMakeContextCurrent(m_window);
    m_vsync = config.vsync;
    glfwSwapInterval(m_vsync ? 1 : 0);

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);
    glfwSetCursorPosCallback(m_window, cursorPosCallback);
    glfwSetScrollCallback(m_window, scrollCallback);

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "[Engine Error] Failed to initialize GLAD (OpenGL 4.5)\n";
        return false;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_MULTISAMPLE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glViewport(0, 0, m_width, m_height);

    double mx, my;
    glfwGetCursorPos(m_window, &mx, &my);
    m_mousePos = glm::vec2((float)mx, (float)my);
    m_lastMousePos = m_mousePos;

    return true;
}

void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_width = width;
        self->m_height = height;
        glViewport(0, 0, width, height);
    }
}

void Window::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        if (self->m_firstMouse) {
            self->m_mousePos = glm::vec2((float)xpos, (float)ypos);
            self->m_firstMouse = false;
        }
        glm::vec2 delta = glm::vec2((float)xpos, (float)ypos) - self->m_mousePos;
        delta.x = std::clamp(delta.x, -250.0f, 250.0f);
        delta.y = std::clamp(delta.y, -250.0f, 250.0f);
        self->m_mouseDelta += delta;
        self->m_mousePos = glm::vec2((float)xpos, (float)ypos);
    }
}

void Window::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->m_scrollDelta += (float)yoffset;
    }
}

void Window::pollEvents() {
    m_mouseDelta = glm::vec2(0.0f);
    m_scrollDelta = 0.0f;
    glfwPollEvents();

    if (m_window) {
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(m_window, &fbW, &fbH);
        if (fbW > 0 && fbH > 0 && (fbW != m_width || fbH != m_height)) {
            m_width = fbW;
            m_height = fbH;
            glViewport(0, 0, m_width, m_height);
        }
    }
}

void Window::setDisplayMode(WindowDisplayMode mode) {
    if (!m_window) return;
    if (m_displayMode == mode) return;

    m_displayMode = mode;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* vidMode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    int monW = vidMode ? vidMode->width : 1920;
    int monH = vidMode ? vidMode->height : 1080;
    int refreshRate = vidMode ? vidMode->refreshRate : 60;

    if (m_displayMode == WindowDisplayMode::ExclusiveFullscreen) {
        glfwSetWindowMonitor(m_window, monitor, 0, 0, monW, monH, refreshRate);
    } else if (m_displayMode == WindowDisplayMode::FullscreenWindowed) {
        glfwSetWindowMonitor(m_window, nullptr, 0, 0, monW, monH, 0);
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowAttrib(m_window, GLFW_RESIZABLE, GLFW_FALSE);
        glfwSetWindowPos(m_window, 0, 0);
        glfwSetWindowSize(m_window, monW, monH);
#ifdef _WIN32
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
#endif
    } else { // Fixed Windowed
        int winW = 1600;
        int winH = 900;
        if (monW <= winW || monH <= winH) {
            winW = 1280;
            winH = 720;
        }
        int posX = (monW - winW) / 2;
        int posY = (monH - winH) / 2;
        glfwSetWindowMonitor(m_window, nullptr, posX, posY, winW, winH, 0);
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
        glfwSetWindowAttrib(m_window, GLFW_RESIZABLE, GLFW_FALSE);
        glfwSetWindowSizeLimits(m_window, winW, winH, winW, winH);
    }

    glfwSetWindowAttrib(m_window, GLFW_RESIZABLE, GLFW_FALSE);

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
    if (fbWidth > 0 && fbHeight > 0) {
        m_width = fbWidth;
        m_height = fbHeight;
        glViewport(0, 0, m_width, m_height);
    }
}

void Window::setFullscreen(bool fullscreen) {
    setDisplayMode(fullscreen ? WindowDisplayMode::FullscreenWindowed : WindowDisplayMode::Windowed);
}

void Window::toggleFullscreen() {
    if (m_displayMode == WindowDisplayMode::FullscreenWindowed || m_displayMode == WindowDisplayMode::ExclusiveFullscreen) {
        setDisplayMode(WindowDisplayMode::Windowed);
    } else {
        setDisplayMode(WindowDisplayMode::FullscreenWindowed);
    }
}

void Window::setResolution(int width, int height) {
    if (!m_window || width <= 0 || height <= 0) return;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* vidMode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    int monW = vidMode ? vidMode->width : 1920;
    int monH = vidMode ? vidMode->height : 1080;
    int refreshRate = vidMode ? vidMode->refreshRate : 60;

    if (m_displayMode == WindowDisplayMode::ExclusiveFullscreen) {
        glfwSetWindowMonitor(m_window, monitor, 0, 0, width, height, refreshRate);
    } else if (m_displayMode == WindowDisplayMode::FullscreenWindowed) {
        // Borderless windowed matches monitor bounds
    } else { // Windowed
        glfwSetWindowSizeLimits(m_window, GLFW_DONT_CARE, GLFW_DONT_CARE, GLFW_DONT_CARE, GLFW_DONT_CARE);
        glfwSetWindowSize(m_window, width, height);
        glfwSetWindowSizeLimits(m_window, width, height, width, height);
        int posX = std::max(0, (monW - width) / 2);
        int posY = std::max(0, (monH - height) / 2);
        glfwSetWindowPos(m_window, posX, posY);
    }

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    if (fbW > 0 && fbH > 0) {
        m_width = fbW;
        m_height = fbH;
        glViewport(0, 0, m_width, m_height);
    }
}

void Window::swapBuffers() {
    glfwSwapBuffers(m_window);
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Window::close() {
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

bool Window::isKeyPressed(int key) const {
    return glfwGetKey(m_window, key) == GLFW_PRESS;
}

bool Window::isMouseButtonPressed(int button) const {
    return glfwGetMouseButton(m_window, button) == GLFW_PRESS;
}

} // namespace Mercenary

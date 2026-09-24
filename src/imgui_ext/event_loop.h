#pragma once

#include <GLFW/glfw3.h>
#include <atomic>
#include <chrono>

namespace ImGuiExt {

class EventLoop {
public:
    static EventLoop& Instance() {
        static EventLoop instance;
        return instance;
    }

    void Init(GLFWwindow* window) {
        m_window = window;
        m_reactive_mode = true;
        m_repaint_frames_left = 3;

        // Chain GLFW input callbacks so every event requests repaint
        m_prev_cursor_pos = glfwSetCursorPosCallback(window, CursorPosCallback);
        m_prev_mouse_button = glfwSetMouseButtonCallback(window, MouseButtonCallback);
        m_prev_scroll = glfwSetScrollCallback(window, ScrollCallback);
        m_prev_key = glfwSetKeyCallback(window, KeyCallback);
        m_prev_char = glfwSetCharCallback(window, CharCallback);
        m_prev_window_size = glfwSetWindowSizeCallback(window, WindowSizeCallback);
        m_prev_window_refresh = glfwSetWindowRefreshCallback(window, WindowRefreshCallback);
        m_prev_window_focus = glfwSetWindowFocusCallback(window, WindowFocusCallback);
    }

    void SetReactiveMode(bool reactive) {
        m_reactive_mode = reactive;
        if (!reactive) {
            m_repaint_frames_left = 1;
        } else {
            m_repaint_frames_left = 3;
        }
    }

    bool IsReactiveMode() const {
        return m_reactive_mode;
    }

    void RequestRepaint(int frames = 3) {
        int current = m_repaint_frames_left.load();
        while (current < frames && !m_repaint_frames_left.compare_exchange_weak(current, frames)) {}
        if (m_window) {
            glfwPostEmptyEvent();
        }
    }

    int GetRepaintFramesLeft() const {
        return m_repaint_frames_left.load();
    }

    // Called at the start of each frame iteration
    bool StepBeforeWait(bool want_text_input, bool has_active_animations, bool force_short_timeout = false) {
        if (!m_reactive_mode) {
            glfwPollEvents();
            return true;
        }

        // If we have pending repaint frames, render immediately without sleeping
        if (m_repaint_frames_left.load() > 0) {
            glfwPollEvents();
            return true;
        }

        // If text input is active (cursor blink) or animations are running, sleep with short timeout
        double timeout = 0.50; // default 500ms safety timeout
        if (want_text_input) {
            // ImGui cursor blink period is ~0.8s (0.4s on, 0.4s off).
            // A 100ms timeout provides smooth, responsive cursor blinking without high CPU usage.
            timeout = 0.10;
            m_repaint_frames_left.store(1);
        } else if (has_active_animations) {
            timeout = 0.016; // 60 FPS for active animations
            m_repaint_frames_left.store(1);
        } else if (force_short_timeout) {
            timeout = 0.050; // 50ms polling for benchmark time checking without spinning
        }

        glfwWaitEventsTimeout(timeout);

        return m_repaint_frames_left.load() > 0;
    }

    // Called after a frame is rendered and presented
    void StepAfterRender() {
        if (m_reactive_mode) {
            int current = m_repaint_frames_left.load();
            if (current > 0) {
                m_repaint_frames_left.fetch_sub(1);
            }
        }
    }

private:
    EventLoop() = default;

    GLFWwindow* m_window = nullptr;
    bool m_reactive_mode = true;
    std::atomic<int> m_repaint_frames_left{3};

    // Chained callbacks
    GLFWcursorposfun m_prev_cursor_pos = nullptr;
    GLFWmousebuttonfun m_prev_mouse_button = nullptr;
    GLFWscrollfun m_prev_scroll = nullptr;
    GLFWkeyfun m_prev_key = nullptr;
    GLFWcharfun m_prev_char = nullptr;
    GLFWwindowsizefun m_prev_window_size = nullptr;
    GLFWwindowrefreshfun m_prev_window_refresh = nullptr;
    GLFWwindowfocusfun m_prev_window_focus = nullptr;

    static void CursorPosCallback(GLFWwindow* w, double x, double y) {
        Instance().RequestRepaint(3);
        if (Instance().m_prev_cursor_pos) Instance().m_prev_cursor_pos(w, x, y);
    }

    static void MouseButtonCallback(GLFWwindow* w, int b, int a, int m) {
        Instance().RequestRepaint(3);
        if (Instance().m_prev_mouse_button) Instance().m_prev_mouse_button(w, b, a, m);
    }

    static void ScrollCallback(GLFWwindow* w, double x, double y) {
        Instance().RequestRepaint(3);
        if (Instance().m_prev_scroll) Instance().m_prev_scroll(w, x, y);
    }

    static void KeyCallback(GLFWwindow* w, int k, int s, int a, int m) {
        Instance().RequestRepaint(3);
        if (Instance().m_prev_key) Instance().m_prev_key(w, k, s, a, m);
    }

    static void CharCallback(GLFWwindow* w, unsigned int c) {
        Instance().RequestRepaint(3);
        if (Instance().m_prev_char) Instance().m_prev_char(w, c);
    }

    static void WindowSizeCallback(GLFWwindow* w, int width, int height) {
        Instance().RequestRepaint(3);
        if (Instance().m_prev_window_size) Instance().m_prev_window_size(w, width, height);
    }

    static void WindowRefreshCallback(GLFWwindow* w) {
        Instance().RequestRepaint(3);
        if (Instance().m_prev_window_refresh) Instance().m_prev_window_refresh(w);
    }

    static void WindowFocusCallback(GLFWwindow* w, int focused) {
        Instance().RequestRepaint(3);
        if (Instance().m_prev_window_focus) Instance().m_prev_window_focus(w, focused);
    }
};

// Global public API
inline void InitEventLoop(GLFWwindow* window) { EventLoop::Instance().Init(window); }
inline void RequestRepaint(int frames = 3) { EventLoop::Instance().RequestRepaint(frames); }
inline void SetReactiveMode(bool reactive) { EventLoop::Instance().SetReactiveMode(reactive); }
inline bool IsReactiveMode() { return EventLoop::Instance().IsReactiveMode(); }

} // namespace ImGuiExt

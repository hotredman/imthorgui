#pragma once

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <algorithm>

namespace ImGuiExt {

class EventLoop {
public:
    static EventLoop& Instance() {
        static EventLoop instance;
        return instance;
    }

    void Init(SDL_Window* window) {
        m_window = window;
        m_reactive_mode = true;
        m_repaint_frames_left = 3;
        m_should_close = false;
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
            bool expected = false;
            if (m_user_event_pending.compare_exchange_strong(expected, true)) {
                SDL_Event event;
                SDL_zero(event);
                event.type = SDL_EVENT_USER;
                SDL_PushEvent(&event);
            }
        }
    }

    int GetRepaintFramesLeft() const {
        return m_repaint_frames_left.load();
    }

    bool ShouldClose() const {
        return m_should_close;
    }

    void SetShouldClose(bool close = true) {
        m_should_close = close;
    }

    // Called at the start of each frame iteration
    bool StepBeforeWait(bool want_text_input, bool has_active_animations, bool force_short_timeout = false) {
        if (m_should_close) return false;

        if (!m_reactive_mode) {
            // Continuous mode: drain all pending events immediately and render every frame
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                DispatchEvent(event);
            }
            return !m_should_close;
        }

        // Keep rendering while mouse button is held down (dragging sliders, scrolling, etc.)
        if (ImGui::GetCurrentContext()) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseDown[0] || io.MouseDown[1] || io.MouseDown[2]) {
                int current = m_repaint_frames_left.load();
                while (current < 2 && !m_repaint_frames_left.compare_exchange_weak(current, 2)) {}
            }
        }

        // If we have pending repaint frames, drain all events non-blockingly and render immediately
        if (m_repaint_frames_left.load() > 0) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                DispatchEvent(event);
            }
            return !m_should_close;
        }

        // Calculate sleep timeout in milliseconds
        // ImGui cursor blink period is ~0.8s (0.4s on, 0.4s off).
        // 100ms provides smooth cursor blinking without high CPU usage.
        Sint32 timeout_ms = 500; // default 500ms safety timeout
        if (want_text_input) {
            timeout_ms = 100;
            m_repaint_frames_left.store(1);
        } else if (has_active_animations) {
            timeout_ms = 16; // 60 FPS for active animations
            m_repaint_frames_left.store(1);
        } else if (force_short_timeout) {
            timeout_ms = 50; // 50ms polling for benchmark
        }

        // Wait for an event with timeout (0% CPU idle)
        SDL_Event event;
        bool got_event = SDL_WaitEventTimeout(&event, timeout_ms);
        if (got_event) {
            DispatchEvent(event);
            // Drain remaining events in queue
            while (SDL_PollEvent(&event)) {
                DispatchEvent(event);
            }
        }

        return !m_should_close && (m_repaint_frames_left.load() > 0);
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

    void DispatchEvent(const SDL_Event& event) {
        if (event.type == SDL_EVENT_QUIT) {
            m_should_close = true;
            return;
        }
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            if (!m_window || event.window.windowID == SDL_GetWindowID(m_window)) {
                m_should_close = true;
                return;
            }
        }

        // Any user interaction (mouse, touch, key, drop, resize) wakes up rendering
        if (event.type != SDL_EVENT_USER && event.type != SDL_EVENT_POLL_SENTINEL) {
            int current = m_repaint_frames_left.load();
            while (current < 3 && !m_repaint_frames_left.compare_exchange_weak(current, 3)) {}
        } else if (event.type == SDL_EVENT_USER) {
            m_user_event_pending.store(false);
        }

        ImGui_ImplSDL3_ProcessEvent(&event);
    }

    SDL_Window* m_window = nullptr;
    bool m_reactive_mode = true;
    bool m_should_close = false;
    std::atomic<int> m_repaint_frames_left{3};
    std::atomic<bool> m_user_event_pending{false};
};

// Global public API
inline void InitEventLoop(SDL_Window* window) { EventLoop::Instance().Init(window); }
inline void RequestRepaint(int frames = 3) { EventLoop::Instance().RequestRepaint(frames); }
inline void SetReactiveMode(bool reactive) { EventLoop::Instance().SetReactiveMode(reactive); }
inline bool IsReactiveMode() { return EventLoop::Instance().IsReactiveMode(); }

} // namespace ImGuiExt

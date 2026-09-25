#pragma once

#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cmath>
#include <algorithm>

#include "imgui.h"
#include "imgui_ext/event_loop.h"
#include "imgui_ext/lttb.h"

namespace ImGuiExt {

enum class SignalType {
    ECG_Cardiac = 0,
    MultiHarmonic = 1,
    SquarePulse = 2,
    ChirpSweep = 3,
    NoiseAM = 4
};

class SignalSource {
public:
    SignalSource() {
        m_raw_buffer.resize(m_buffer_capacity, 0.0f);
        m_display_snapshot.resize(m_buffer_capacity, 0.0f);
    }

    ~SignalSource() {
        Stop();
    }

    void Start(int sample_rate_hz = 50000, int target_fps = 60) {
        if (m_running.load()) return;
        m_running.store(true);
        m_paused.store(false);
        m_sample_rate = sample_rate_hz;
        m_target_fps = target_fps;
        m_worker = std::thread(&SignalSource::WorkerThread, this);
    }

    void Stop() {
        if (!m_running.load()) return;
        m_running.store(false);
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }

    void SetPaused(bool paused) {
        m_paused.store(paused);
    }

    bool IsPaused() const {
        return m_paused.load();
    }

    void SetSignalType(SignalType type) {
        m_signal_type.store((int)type);
    }

    SignalType GetSignalType() const {
        return (SignalType)m_signal_type.load();
    }

    void SetFrequency(float freq_hz) {
        m_freq.store(freq_hz);
    }

    float GetFrequency() const {
        return m_freq.load();
    }

    void SetNoiseLevel(float noise) {
        m_noise.store(noise);
    }

    float GetNoiseLevel() const {
        return m_noise.load();
    }

    void SetBufferCapacity(size_t points) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer_capacity = (std::max)((size_t)1000, (std::min)((size_t)500000, points));
        m_raw_buffer.resize(m_buffer_capacity, 0.0f);
        m_display_snapshot.resize(m_buffer_capacity, 0.0f);
        m_write_idx = 0;
    }

    size_t GetBufferCapacity() const {
        return m_buffer_capacity;
    }

    void SetTargetFPS(int fps) {
        m_target_fps.store((std::max)(10, (std::min)(120, fps)));
    }

    int GetTargetFPS() const {
        return m_target_fps.load();
    }

    // Fetches current snapshot for rendering (non-blocking or fast lock)
    size_t GetSnapshot(std::vector<float>& out_data, float& out_vpp, float& out_rms) {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t n = m_buffer_capacity;
        if (out_data.size() != n) {
            out_data.resize(n);
        }

        // Copy in circular order (oldest to newest)
        size_t head = m_write_idx;
        size_t part1 = n - head;
        if (part1 > 0) {
            std::copy(m_raw_buffer.begin() + head, m_raw_buffer.end(), out_data.begin());
        }
        if (head > 0) {
            std::copy(m_raw_buffer.begin(), m_raw_buffer.begin() + head, out_data.begin() + part1);
        }

        out_vpp = m_cached_vpp;
        out_rms = m_cached_rms;
        return n;
    }

private:
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::atomic<int> m_signal_type{(int)SignalType::ECG_Cardiac};
    std::atomic<float> m_freq{5.0f};
    std::atomic<float> m_noise{0.02f};
    std::atomic<int> m_target_fps{60};
    int m_sample_rate = 50000;

    size_t m_buffer_capacity = 100000;
    std::vector<float> m_raw_buffer;
    std::vector<float> m_display_snapshot;
    size_t m_write_idx = 0;
    std::mutex m_mutex;
    std::thread m_worker;

    float m_cached_vpp = 0.0f;
    float m_cached_rms = 0.0f;

    // ECG cardiac waveform generator (P-Q-R-S-T complex)
    static float GenerateECG(double phase) {
        // phase in [0, 1)
        phase = phase - floor(phase);
        float y = 0.0f;

        // P wave (at phase 0.15)
        float dp = (float)(phase - 0.15);
        y += 0.25f * expf(-dp * dp / 0.0006f);

        // Q drop (at phase 0.24)
        float dq = (float)(phase - 0.24);
        y -= 0.20f * expf(-dq * dq / 0.0001f);

        // R sharp peak (at phase 0.27) - critical for testing peak retention!
        float dr = (float)(phase - 0.27);
        y += 1.80f * expf(-dr * dr / 0.00012f);

        // S drop (at phase 0.30)
        float ds = (float)(phase - 0.30);
        y -= 0.45f * expf(-ds * ds / 0.00015f);

        // T wave (at phase 0.48)
        float dt = (float)(phase - 0.48);
        y += 0.40f * expf(-dt * dt / 0.0035f);

        // Isoelectric baseline
        return y - 0.2f;
    }

    void WorkerThread() {
        double phase = 0.0;
        auto last_repaint_time = std::chrono::steady_clock::now();

        // Simple high-throughput PRNG for noise
        uint32_t rng_state = 123456789;
        auto fast_rand = [&]() -> float {
            rng_state ^= rng_state << 13;
            rng_state ^= rng_state >> 17;
            rng_state ^= rng_state << 5;
            return ((float)(rng_state & 0xFFFF) / 32768.0f) - 1.0f;
        };

        while (m_running.load()) {
            if (m_paused.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }

            int target_fps = m_target_fps.load();
            int chunk_size = m_sample_rate / target_fps;
            if (chunk_size < 10) chunk_size = 10;

            SignalType type = (SignalType)m_signal_type.load();
            float f = m_freq.load();
            float noise_amp = m_noise.load();
            double dt = 1.0 / (double)m_sample_rate;

            float min_v = 1e9f, max_v = -1e9f;
            double sum_sq = 0.0;

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                size_t cap = m_buffer_capacity;

                for (int i = 0; i < chunk_size; ++i) {
                    phase += (double)f * dt;
                    if (phase >= 1000.0) phase -= 1000.0;

                    float val = 0.0f;
                    switch (type) {
                        case SignalType::ECG_Cardiac:
                            val = GenerateECG(phase);
                            break;
                        case SignalType::MultiHarmonic:
                            val = sinf((float)(phase * 6.2831853)) +
                                  0.45f * sinf((float)(phase * 6.2831853 * 3.0)) +
                                  0.25f * sinf((float)(phase * 6.2831853 * 7.0));
                            break;
                        case SignalType::SquarePulse: {
                            double frac = phase - floor(phase);
                            val = (frac < 0.5) ? 1.0f : -1.0f;
                            // Band-limiting ringing
                            val += 0.2f * sinf((float)(frac * 6.2831853 * 20.0)) * expf((float)(-frac * 10.0));
                            break;
                        }
                        case SignalType::ChirpSweep: {
                            double sweep_f = 2.0 + 30.0 * (0.5 + 0.5 * sin(phase * 0.5));
                            val = sinf((float)(phase * sweep_f * 6.2831853));
                            break;
                        }
                        case SignalType::NoiseAM: {
                            float carrier = sinf((float)(phase * 6.2831853));
                            float mod = 0.5f + 0.5f * sinf((float)(phase * 6.2831853 * 0.15));
                            val = carrier * mod;
                            break;
                        }
                    }

                    if (noise_amp > 0.0f) {
                        val += fast_rand() * noise_amp;
                    }

                    m_raw_buffer[m_write_idx] = val;
                    m_write_idx = (m_write_idx + 1) % cap;

                    if (val < min_v) min_v = val;
                    if (val > max_v) max_v = val;
                    sum_sq += (double)val * (double)val;
                }

                m_cached_vpp = (max_v - min_v);
                m_cached_rms = (float)sqrt(sum_sq / chunk_size);
            }

            // Trigger UI update exactly when chunk is ready
            auto now = std::chrono::steady_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(now - last_repaint_time).count();
            double target_interval_ms = 1000.0 / (double)target_fps;

            if (elapsed_ms >= target_interval_ms) {
                last_repaint_time = now;
                ImGuiExt::RequestRepaint(1);
            }

            // Sleep remainder to pace the stream
            double remaining_ms = target_interval_ms - elapsed_ms;
            if (remaining_ms > 1.0) {
                std::this_thread::sleep_for(std::chrono::milliseconds((int)remaining_ms));
            }
        }
    }
};

class OscilloscopeWidget {
public:
    OscilloscopeWidget() {
        m_signal.Start(50000, 60);
    }

    ~OscilloscopeWidget() {
        m_signal.Stop();
    }

    SignalSource& GetSignal() { return m_signal; }

    void RenderUI() {
        ImGui::Begin("Real-Time Oscilloscope & Signal Monitor", nullptr);

        // Header metrics & status
        bool paused = m_signal.IsPaused();
        if (paused) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[PAUSED: UI in 0% CPU Sleep Mode]");
        } else {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[STREAMING: %d kS/s @ %d FPS]",
                               (int)(m_signal.GetBufferCapacity() / 1000), m_signal.GetTargetFPS());
        }

        ImGui::SameLine();
        if (ImGui::Button(paused ? "  Resume Stream  " : "  Pause Stream  ")) {
            m_signal.SetPaused(!paused);
        }

        ImGui::Separator();

        // 1. Oscilloscope Screen Area
        ImVec2 avail_size = ImGui::GetContentRegionAvail();
        float plot_height = (std::max)(220.0f, avail_size.y - 170.0f);
        ImVec2 screen_size(avail_size.x, plot_height);
        ImVec2 screen_pos = ImGui::GetCursorScreenPos();

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Background screen (phosphor CRT dark green/black)
        ImU32 bg_col = 0xFF0D1410;
        ImU32 border_col = 0xFF1E3A28;
        dl->AddRectFilled(screen_pos, ImVec2(screen_pos.x + screen_size.x, screen_pos.y + screen_size.y), bg_col, 6.0f);
        dl->AddRect(screen_pos, ImVec2(screen_pos.x + screen_size.x, screen_pos.y + screen_size.y), border_col, 6.0f, 0, 1.5f);

        // Grid (8 vertical, 6 horizontal divisions)
        ImU32 grid_col = 0xFF14291B;
        ImU32 center_grid_col = 0xFF1F402B;

        int num_div_x = 10;
        int num_div_y = 6;
        for (int i = 1; i < num_div_x; ++i) {
            float gx = screen_pos.x + (screen_size.x / (float)num_div_x) * (float)i;
            ImU32 c = (i == num_div_x / 2) ? center_grid_col : grid_col;
            dl->AddLine(ImVec2(gx, screen_pos.y), ImVec2(gx, screen_pos.y + screen_size.y), c, 1.0f);
        }
        for (int j = 1; j < num_div_y; ++j) {
            float gy = screen_pos.y + (screen_size.y / (float)num_div_y) * (float)j;
            ImU32 c = (j == num_div_y / 2) ? center_grid_col : grid_col;
            dl->AddLine(ImVec2(screen_pos.x, gy), ImVec2(screen_pos.x + screen_size.x, gy), c, 1.0f);
        }

        // Center reticle tick marks
        float mid_y = screen_pos.y + screen_size.y * 0.5f;
        for (int i = 0; i <= num_div_x * 5; ++i) {
            float tick_x = screen_pos.x + (screen_size.x / (float)(num_div_x * 5)) * (float)i;
            float tick_h = (i % 5 == 0) ? 6.0f : 3.0f;
            dl->AddLine(ImVec2(tick_x, mid_y - tick_h), ImVec2(tick_x, mid_y + tick_h), center_grid_col, 1.0f);
        }

        // Fetch waveform snapshot
        float vpp = 0.0f, rms = 0.0f;
        size_t raw_count = m_signal.GetSnapshot(m_snapshot_cache, vpp, rms);

        // Allocate screen points for downsampled signal
        size_t target_pixels = (size_t)(std::max)(100.0f, screen_size.x);
        if (m_downsampled_points.size() < target_pixels) {
            m_downsampled_points.resize(target_pixels);
        }

        // Measure LTTB downsampling time
        auto t0 = std::chrono::high_resolution_clock::now();
        size_t rendered_pts = 0;

        if (m_use_lttb) {
            rendered_pts = LTTB::Downsample(m_snapshot_cache.data(), raw_count,
                                            m_downsampled_points.data(), target_pixels,
                                            screen_pos, screen_size,
                                            m_y_min, m_y_max);
        } else {
            // Naive decimation for comparison
            rendered_pts = target_pixels;
            double step = (double)raw_count / (double)target_pixels;
            float y_range = (m_y_max - m_y_min) > 1e-5f ? (m_y_max - m_y_min) : 1.0f;
            for (size_t i = 0; i < target_pixels; ++i) {
                size_t src_idx = (std::min)((size_t)(i * step), raw_count - 1);
                float val = m_snapshot_cache[src_idx];
                float norm_y = (val - m_y_min) / y_range;
                norm_y = (std::max)(0.0f, (std::min)(1.0f, norm_y));
                m_downsampled_points[i] = ImVec2(screen_pos.x + (float)i,
                                                 screen_pos.y + screen_size.y * (1.0f - norm_y));
            }
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double lttb_duration_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        // Draw waveform via AddPolyline (intercepted as StrokePolyline by ThorVG!)
        if (rendered_pts > 1) {
            // Glow effect: subtle wider line behind
            ImU32 glow_col = 0x3300FF66;
            dl->AddPolyline(m_downsampled_points.data(), (int)rendered_pts, glow_col, 0, m_line_thickness + 2.5f);

            // Main sharp trace (phosphor green or cyan)
            ImU32 trace_col = m_cyan_trace ? 0xFF00E5FF : 0xFF33FF77;
            dl->AddPolyline(m_downsampled_points.data(), (int)rendered_pts, trace_col, 0, m_line_thickness);
        }

        // On-screen OSD Overlay
        char osd_buf[128];
        snprintf(osd_buf, sizeof(osd_buf), "Points: %zu -> %zu (%s: %.2f us) | Vpp: %.2f V | RMS: %.2f V",
                 raw_count, rendered_pts, m_use_lttb ? "LTTB" : "Decimate", lttb_duration_us, vpp, rms);
        dl->AddText(ImVec2(screen_pos.x + 10.0f, screen_pos.y + 8.0f), 0xDDFFFFFF, osd_buf);

        ImGui::Dummy(screen_size);

        // 2. Oscilloscope Control Panel
        ImGui::Separator();
        ImGui::Columns(2, "ScopeControls", false);

        // Left Column: Signal parameters
        const char* signal_names[] = { "ECG Cardiac Pulse (Sharp QRS)", "Multi-Harmonic Wave", "Square Pulse with Ringing", "Chirp Sweep", "AM Carrier Modulated" };
        int current_type = (int)m_signal.GetSignalType();
        if (ImGui::Combo("Signal Waveform", &current_type, signal_names, IM_ARRAYSIZE(signal_names))) {
            m_signal.SetSignalType((SignalType)current_type);
        }

        float freq = m_signal.GetFrequency();
        if (ImGui::SliderFloat("Signal Frequency (Hz)", &freq, 0.5f, 50.0f, "%.1f Hz")) {
            m_signal.SetFrequency(freq);
        }

        float noise = m_signal.GetNoiseLevel();
        if (ImGui::SliderFloat("Noise Floor", &noise, 0.0f, 0.20f, "%.2f")) {
            m_signal.SetNoiseLevel(noise);
        }

        ImGui::NextColumn();

        // Right Column: Downsampling & Display parameters
        ImGui::Checkbox("Enable LTTB Downsampling", &m_use_lttb);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Largest-Triangle-Three-Buckets:\nPreserves visual peaks and valleys when reducing\n100,000+ points to screen resolution without lag.");
        }

        ImGui::SameLine();
        ImGui::Checkbox("Cyan Phosphor", &m_cyan_trace);

        int cap = (int)(m_signal.GetBufferCapacity() / 1000);
        if (ImGui::SliderInt("Buffer Size (kSamples)", &cap, 10, 250, "%d kPts")) {
            m_signal.SetBufferCapacity((size_t)cap * 1000);
        }

        ImGui::SliderFloat("Trace Thickness", &m_line_thickness, 1.0f, 4.0f, "%.1f px");

        ImGui::Columns(1);
        ImGui::End();
    }

private:
    SignalSource m_signal;
    std::vector<float> m_snapshot_cache;
    std::vector<ImVec2> m_downsampled_points;

    bool m_use_lttb = true;
    bool m_cyan_trace = false;
    float m_line_thickness = 1.8f;
    float m_y_min = -2.2f;
    float m_y_max = 2.2f;
};

} // namespace ImGuiExt

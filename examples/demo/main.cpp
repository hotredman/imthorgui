#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <iomanip>
#include <cmath>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#endif

#include <GL/gl.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "imgui_ext/event_loop.h"
#include "imgui_ext/frame_dedup.h"
#include "imgui_ext/recorder.h"
#include "imgui_ext/renderer.h"

// OpenGL typedefs for GL_TIME_ELAPSED queries
#define GL_TIME_ELAPSED 0x88BF
#define GL_QUERY_RESULT 0x8866
typedef unsigned __int64 GLuint64;
typedef void (APIENTRY *PFNGLGENQUERIESPROC) (GLsizei n, GLuint *ids);
typedef void (APIENTRY *PFNGLDELETEQUERIESPROC) (GLsizei n, const GLuint *ids);
typedef void (APIENTRY *PFNGLBEGINQUERYPROC) (GLenum target, GLuint id);
typedef void (APIENTRY *PFNGLENDQUERYPROC) (GLenum target);
typedef void (APIENTRY *PFNGLGETQUERYOBJECTUI64VPROC) (GLuint id, GLenum pname, GLuint64 *params);
typedef void (APIENTRY *PFNGLGETQUERYOBJECTUIVPROC) (GLuint id, GLenum pname, GLuint *params);

static PFNGLGENQUERIESPROC glGenQueriesPtr = nullptr;
static PFNGLDELETEQUERIESPROC glDeleteQueriesPtr = nullptr;
static PFNGLBEGINQUERYPROC glBeginQueryPtr = nullptr;
static PFNGLENDQUERYPROC glEndQueryPtr = nullptr;
static PFNGLGETQUERYOBJECTUI64VPROC glGetQueryObjectui64vPtr = nullptr;
static PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuivPtr = nullptr;

static void InitGLQueries() {
    glGenQueriesPtr = (PFNGLGENQUERIESPROC)glfwGetProcAddress("glGenQueries");
    glDeleteQueriesPtr = (PFNGLDELETEQUERIESPROC)glfwGetProcAddress("glDeleteQueries");
    glBeginQueryPtr = (PFNGLBEGINQUERYPROC)glfwGetProcAddress("glBeginQuery");
    glEndQueryPtr = (PFNGLENDQUERYPROC)glfwGetProcAddress("glEndQuery");
    glGetQueryObjectui64vPtr = (PFNGLGETQUERYOBJECTUI64VPROC)glfwGetProcAddress("glGetQueryObjectui64v");
    glGetQueryObjectuivPtr = (PFNGLGETQUERYOBJECTUIVPROC)glfwGetProcAddress("glGetQueryObjectuiv");
}

struct PerformanceMetrics {
    double cpu_usage_percent = 0.0;
    double ram_working_set_mb = 0.0;
    double ram_peak_working_set_mb = 0.0;
    double cpu_work_time_ms = 0.0;
    double cpu_frame_time_ms = 0.0;
    double gpu_frame_time_ms = 0.0;
    double fps = 0.0;

#ifdef _WIN32
    ULARGE_INTEGER last_kernel_time{0};
    ULARGE_INTEGER last_user_time{0};
    std::chrono::steady_clock::time_point last_cpu_check_time{};
    int num_processors = 1;
#endif

    void Init() {
#ifdef _WIN32
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        num_processors = (int)sys_info.dwNumberOfProcessors;

        FILETIME ftCreation, ftExit, ftKernel, ftUser;
        GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser);
        last_kernel_time.LowPart = ftKernel.dwLowDateTime;
        last_kernel_time.HighPart = ftKernel.dwHighDateTime;
        last_user_time.LowPart = ftUser.dwLowDateTime;
        last_user_time.HighPart = ftUser.dwHighDateTime;
        last_cpu_check_time = std::chrono::steady_clock::now();
#endif
    }

    void Update(double frame_time_ms, double work_time_ms, double gpu_time_ms, double current_fps) {
        cpu_frame_time_ms = frame_time_ms;
        cpu_work_time_ms = work_time_ms;
        gpu_frame_time_ms = gpu_time_ms;
        fps = current_fps;

#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            ram_working_set_mb = (double)pmc.WorkingSetSize / (1024.0 * 1024.0);
            ram_peak_working_set_mb = (double)pmc.PeakWorkingSetSize / (1024.0 * 1024.0);
        }

        auto now = std::chrono::steady_clock::now();
        double elapsed_sec = std::chrono::duration<double>(now - last_cpu_check_time).count();
        if (elapsed_sec >= 0.5) {
            FILETIME ftCreation, ftExit, ftKernel, ftUser;
            if (GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser)) {
                ULARGE_INTEGER cur_kernel, cur_user;
                cur_kernel.LowPart = ftKernel.dwLowDateTime;
                cur_kernel.HighPart = ftKernel.dwHighDateTime;
                cur_user.LowPart = ftUser.dwLowDateTime;
                cur_user.HighPart = ftUser.dwHighDateTime;

                ULONGLONG kernel_diff = cur_kernel.QuadPart - last_kernel_time.QuadPart;
                ULONGLONG user_diff = cur_user.QuadPart - last_user_time.QuadPart;
                ULONGLONG total_diff = kernel_diff + user_diff;

                double proc_time_sec = (double)total_diff / 10000000.0;
                cpu_usage_percent = (proc_time_sec / (elapsed_sec * num_processors)) * 100.0;

                last_kernel_time = cur_kernel;
                last_user_time = cur_user;
                last_cpu_check_time = now;
            }
        }
#endif
    }
};

struct BenchmarkSample {
    bool is_render = false;
    double cpu_usage = 0.0;
    double ram_mb = 0.0;
    double cpu_work_time_ms = 0.0;
    double cpu_total_time_ms = 0.0;
    double gpu_frame_time_ms = 0.0;
    double fps = 0.0;
};

struct BenchmarkResult {
    std::string name;
    size_t frames_rendered = 0;
    double duration_sec = 0.0;
    double avg_fps = 0.0;
    double avg_worktime_ms = 0.0;
    double max_worktime_ms = 0.0;
    double total_frame_time_ms = 0.0;
    double thread_cpu_time_ms = 0.0;
    double thread_cpu_percent = 0.0;
    double process_cpu_percent = 0.0;
    double avg_gpu_time_ms = 0.0;
    double ram_mb = 0.0;
};

struct BenchmarkSession {
    bool running = false;
    std::string name;
    double duration_sec = 5.0;
    std::chrono::steady_clock::time_point start_time;
    std::vector<BenchmarkSample> samples;
    std::vector<BenchmarkResult> all_results;

#ifdef _WIN32
    FILETIME start_kernel_time{0, 0};
    FILETIME start_user_time{0, 0};
#endif

    void Start(const std::string& session_name, double duration) {
        name = session_name;
        duration_sec = duration;
        samples.clear();
        samples.reserve((size_t)(duration * 200));
#ifdef _WIN32
        FILETIME ftCreation, ftExit;
        GetThreadTimes(GetCurrentThread(), &ftCreation, &ftExit, &start_kernel_time, &start_user_time);
#endif
        start_time = std::chrono::steady_clock::now();
        running = true;
        std::cout << "\n>>> Starting benchmark: " << name << " (" << duration_sec << "s) <<<\n";
    }

    void RecordRender(const PerformanceMetrics& m, double cpu_work_time_ms) {
        if (!running) return;
        samples.push_back({true, m.cpu_usage_percent, m.ram_working_set_mb, cpu_work_time_ms, m.cpu_frame_time_ms, m.gpu_frame_time_ms, m.fps});
        CheckElapsed();
    }

    void RecordIdle(const PerformanceMetrics& m) {
        if (!running) return;
        samples.push_back({false, m.cpu_usage_percent, m.ram_working_set_mb, 0.0, m.cpu_frame_time_ms, 0.0, 0.0});
        CheckElapsed();
    }

    void CheckElapsed() {
        if (!running) return;
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time).count();
        if (elapsed >= duration_sec) {
            Finish();
        }
    }

    void Finish() {
        running = false;
        if (samples.empty()) return;

        auto now = std::chrono::steady_clock::now();
        double actual_duration = std::chrono::duration<double>(now - start_time).count();

        double thread_cpu_time_ms = 0.0;
        double thread_cpu_percent = 0.0;
#ifdef _WIN32
        FILETIME ftCreation, ftExit, cur_kernel, cur_user;
        GetThreadTimes(GetCurrentThread(), &ftCreation, &ftExit, &cur_kernel, &cur_user);
        ULARGE_INTEGER sk, su, ck, cu;
        sk.LowPart = start_kernel_time.dwLowDateTime; sk.HighPart = start_kernel_time.dwHighDateTime;
        su.LowPart = start_user_time.dwLowDateTime;   su.HighPart = start_user_time.dwHighDateTime;
        ck.LowPart = cur_kernel.dwLowDateTime;         ck.HighPart = cur_kernel.dwHighDateTime;
        cu.LowPart = cur_user.dwLowDateTime;           cu.HighPart = cur_user.dwHighDateTime;
        ULONGLONG thread_time_100ns = (ck.QuadPart - sk.QuadPart) + (cu.QuadPart - su.QuadPart);
        thread_cpu_time_ms = (double)thread_time_100ns / 10000.0;
        thread_cpu_percent = (thread_cpu_time_ms / (actual_duration * 1000.0)) * 100.0;
#endif

        size_t rendered_frames = 0;
        double sum_cpu = 0, sum_ram = 0, sum_worktime = 0, sum_totaltime = 0, sum_gputime = 0;
        double max_worktime = 0;
        for (const auto& s : samples) {
            sum_cpu += s.cpu_usage;
            sum_ram += s.ram_mb;
            sum_totaltime += s.cpu_total_time_ms;
            if (s.is_render) {
                rendered_frames++;
                sum_worktime += s.cpu_work_time_ms;
                sum_gputime += s.gpu_frame_time_ms;
                if (s.cpu_work_time_ms > max_worktime) max_worktime = s.cpu_work_time_ms;
            }
        }
        size_t n = samples.size();
        double avg_fps = (double)rendered_frames / actual_duration;
        double avg_worktime = (rendered_frames > 0) ? (sum_worktime / rendered_frames) : 0.0;
        double avg_gputime = (rendered_frames > 0) ? (sum_gputime / rendered_frames) : 0.0;
        double avg_totaltime = (n > 0) ? (sum_totaltime / n) : 0.0;
        double avg_cpu = (n > 0) ? (sum_cpu / n) : 0.0;
        double avg_ram = (n > 0) ? (sum_ram / n) : 0.0;

        BenchmarkResult res;
        res.name = name;
        res.frames_rendered = rendered_frames;
        res.duration_sec = actual_duration;
        res.avg_fps = avg_fps;
        res.avg_worktime_ms = avg_worktime;
        res.max_worktime_ms = max_worktime;
        res.total_frame_time_ms = avg_totaltime;
        res.thread_cpu_time_ms = thread_cpu_time_ms;
        res.thread_cpu_percent = thread_cpu_percent;
        res.process_cpu_percent = avg_cpu;
        res.avg_gpu_time_ms = avg_gputime;
        res.ram_mb = avg_ram;
        all_results.push_back(res);

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n=========================================\n";
        std::cout << " BENCHMARK RESULTS: " << name << "\n";
        std::cout << " Frames Rendered:   " << rendered_frames << " frames in " << actual_duration << "s\n";
        std::cout << " Average FPS:       " << avg_fps << "\n";
        std::cout << " CPU Work Time:     " << avg_worktime << " ms/frame (Max: " << max_worktime << " ms)\n";
        std::cout << " Total Frame Time:  " << avg_totaltime << " ms/frame\n";
        std::cout << " Thread CPU Time:   " << thread_cpu_time_ms << " ms total (" << thread_cpu_percent << "% of 1 core)\n";
        std::cout << " Process CPU Usage: " << avg_cpu << " % (All cores)\n";
        std::cout << " GPU Frame Time:    " << avg_gputime << " ms\n";
        std::cout << " RAM Working Set:   " << avg_ram << " MB\n";
        std::cout << "=========================================\n" << std::endl;
    }

    void PrintSummaryMarkdown() {
        if (all_results.empty()) return;
        std::cout << "\n### Comparative Benchmark Summary\n\n";
        std::cout << "| Режим / Сценарий | Кадров за 5с | FPS | CPU Work (мс/кадр) | Время потока CPU | Загрузка 1 ядра | GPU Time | RAM |\n";
        std::cout << "| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |\n";
        for (const auto& r : all_results) {
            std::cout << "| " << r.name << " | **" << r.frames_rendered << "** | "
                      << std::fixed << std::setprecision(1) << r.avg_fps << " | "
                      << std::fixed << std::setprecision(2) << r.avg_worktime_ms << " мс | "
                      << std::fixed << std::setprecision(1) << r.thread_cpu_time_ms << " мс | "
                      << std::fixed << std::setprecision(2) << r.thread_cpu_percent << " % | "
                      << std::fixed << std::setprecision(2) << r.avg_gpu_time_ms << " мс | "
                      << std::fixed << std::setprecision(1) << r.ram_mb << " МБ |\n";
        }
        std::cout << "\n" << std::endl;
    }
};

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main(int argc, char** argv) {
    bool auto_benchmark = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--benchmark") {
            auto_benchmark = true;
        }
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(monitor);
    if (main_scale <= 0.0f) main_scale = 1.0f;

    int mon_x = 0, mon_y = 0, mon_w = 1280, mon_h = 720;
    glfwGetMonitorWorkarea(monitor, &mon_x, &mon_y, &mon_w, &mon_h);

    // Compute comfortable window dimensions fitting within monitor work area
    int base_w = (std::min)(mon_w - 40, (int)(1360 * (main_scale > 1.25f ? 1.0f : main_scale)));
    int base_h = (std::min)(mon_h - 60, (int)(800 * (main_scale > 1.25f ? 1.0f : main_scale)));
    if (base_w < 1024) base_w = (std::min)(1024, mon_w - 20);
    if (base_h < 640)  base_h = (std::min)(640, mon_h - 40);

    GLFWwindow* window = glfwCreateWindow(base_w, base_h, "ImGui Vector Backend - Stage 0 & 1 Demo", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwSetWindowPos(window, mon_x + (mon_w - base_w) / 2, mon_y + (mon_h - base_h) / 2);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync (60Hz) as standard baseline

    float window_scale = ImGui_ImplGlfw_GetContentScaleForWindow(window);
    if (window_scale > 0.0f) main_scale = window_scale;

    std::cout << "[Demo] Monitor Content Scale: " << main_scale << ", Window Size: " << base_w << "x" << base_h << "\n";

    InitGLQueries();

    GLuint gl_query = 0;
    if (glGenQueriesPtr) {
        glGenQueriesPtr(1, &gl_query);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // Scale UI style according to monitor DPI
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);

    // Load clean TrueType font supporting Cyrillic + Latin glyph ranges
    const char* font_path = "C:/Windows/Fonts/segoeui.ttf";
    float font_size = 19.0f * main_scale;
    ImFontConfig cfg;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = false;
    ImFont* font = io.Fonts->AddFontFromFileTTF(font_path, font_size, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    if (!font) {
        font_path = "C:/Windows/Fonts/arial.ttf";
        font = io.Fonts->AddFontFromFileTTF(font_path, font_size, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Initialize Event Loop manager
    ImGuiExt::InitEventLoop(window);

    ImGuiExt::IRenderer* vector_renderer = ImGuiExt::CreateThorVGRenderer();
    int init_fb_w = 0, init_fb_h = 0;
    glfwGetFramebufferSize(window, &init_fb_w, &init_fb_h);
    vector_renderer->Init(init_fb_w, init_fb_h);
    if (font) {
        vector_renderer->LoadFontFile(font_path);
    }
    bool use_vector_backend = true;
    ImGuiExt::SetVectorInterception(use_vector_backend);

    PerformanceMetrics metrics;
    metrics.Init();
    BenchmarkSession bench;

    int auto_bench_stage = auto_benchmark ? 1 : 0;
    auto bench_stage_timer = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(window)) {
        auto frame_start = std::chrono::steady_clock::now();

        // Automated benchmark sequencer
        if (auto_benchmark) {
            auto now = std::chrono::steady_clock::now();
            double stage_elapsed = std::chrono::duration<double>(now - bench_stage_timer).count();

            if (auto_bench_stage == 1 && stage_elapsed > 1.0) {
                // Stage 0: Continuous Idle
                ImGuiExt::SetReactiveMode(false);
                bench.Start("Stage 0: Continuous - Idle (5s)", 5.0);
                auto_bench_stage = 2;
            } else if (auto_bench_stage == 2 && !bench.running) {
                // Stage 0: Continuous Active
                bench.Start("Stage 0: Continuous - Mouse Motion (5s)", 5.0);
                bench_stage_timer = std::chrono::steady_clock::now();
                auto_bench_stage = 3;
            } else if (auto_bench_stage == 3) {
                double t = std::chrono::duration<double>(now - bench_stage_timer).count() * 4.0;
                double mx = (640.0 + 300.0 * std::sin(t)) * main_scale;
                double my = (360.0 + 200.0 * std::cos(t)) * main_scale;
                glfwSetCursorPos(window, mx, my);
                ImGuiExt::RequestRepaint(3);
                if (!bench.running) {
                    // Transition to Stage 1
                    ImGuiExt::SetReactiveMode(true);
                    bench.Start("Stage 1: Reactive - Idle (5s)", 5.0);
                    bench_stage_timer = std::chrono::steady_clock::now();
                    auto_bench_stage = 4;
                }
            } else if (auto_bench_stage == 4 && !bench.running) {
                // Stage 1: Reactive Active
                bench.Start("Stage 1: Reactive - Mouse Motion (5s)", 5.0);
                bench_stage_timer = std::chrono::steady_clock::now();
                auto_bench_stage = 5;
            } else if (auto_bench_stage == 5) {
                double t = std::chrono::duration<double>(now - bench_stage_timer).count() * 4.0;
                double mx = (640.0 + 300.0 * std::sin(t)) * main_scale;
                double my = (360.0 + 200.0 * std::cos(t)) * main_scale;
                glfwSetCursorPos(window, mx, my);
                ImGuiExt::RequestRepaint(3);
                if (!bench.running) {
                    // Transition to Stage 2 (Reactive + Dedup)
                    ImGuiExt::SetReactiveMode(true);
                    ImGuiExt::SetFrameDeduplication(true);
                    bench.Start("Stage 2: Reactive+Dedup - Idle (5s)", 5.0);
                    bench_stage_timer = std::chrono::steady_clock::now();
                    auto_bench_stage = 6;
                }
            } else if (auto_bench_stage == 6 && !bench.running) {
                bench.Start("Stage 2: Reactive+Dedup - Mouse Motion (5s)", 5.0);
                bench_stage_timer = std::chrono::steady_clock::now();
                auto_bench_stage = 7;
            } else if (auto_bench_stage == 7) {
                double t = std::chrono::duration<double>(now - bench_stage_timer).count() * 4.0;
                double mx = (640.0 + 300.0 * std::sin(t)) * main_scale;
                double my = (360.0 + 200.0 * std::cos(t)) * main_scale;
                glfwSetCursorPos(window, mx, my);
                ImGuiExt::RequestRepaint(3);
                if (!bench.running) {
                    // Transition to Stage 3 (ThorVG Vector Backend - Idle)
                    use_vector_backend = true;
                    ImGuiExt::SetVectorInterception(true);
                    bench.Start("Stage 3: ThorVG Vector - Idle (5s)", 5.0);
                    bench_stage_timer = std::chrono::steady_clock::now();
                    auto_bench_stage = 8;
                }
            } else if (auto_bench_stage == 8 && !bench.running) {
                // Stage 3 (ThorVG Vector Backend - Mouse Motion)
                bench.Start("Stage 3: ThorVG Vector - Mouse Motion (5s)", 5.0);
                bench_stage_timer = std::chrono::steady_clock::now();
                auto_bench_stage = 9;
            } else if (auto_bench_stage == 9) {
                double t = std::chrono::duration<double>(now - bench_stage_timer).count() * 4.0;
                double mx = (640.0 + 300.0 * std::sin(t)) * main_scale;
                double my = (360.0 + 200.0 * std::cos(t)) * main_scale;
                glfwSetCursorPos(window, mx, my);
                ImGuiExt::RequestRepaint(3);
                if (!bench.running) {
                    auto_bench_stage = 10;
                    std::cout << "All automated benchmarks complete!\n";
                    bench.PrintSummaryMarkdown();
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
            }
        }

        // Wait / Poll events according to event loop configuration
        bool want_text = io.WantTextInput;
        bool should_render = ImGuiExt::EventLoop::Instance().StepBeforeWait(want_text, false, bench.running);

        if (!should_render) {
            auto now = std::chrono::steady_clock::now();
            double frame_time_ms = std::chrono::duration<double, std::milli>(now - frame_start).count();
            metrics.Update(frame_time_ms, 0.0, 0.0, 0.0);
            bench.RecordIdle(metrics);
            continue;
        }

        auto cpu_work_start = std::chrono::steady_clock::now();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Responsive side-by-side layout
        float pad = 12.0f * main_scale;
        float total_w = io.DisplaySize.x;
        float total_h = io.DisplaySize.y;

        float hud_w = (std::min)(560.0f * main_scale, (total_w - pad * 3.0f) * 0.44f);
        float hud_h = total_h - pad * 2.0f;
        float demo_x = pad + hud_w + pad;
        float demo_w = total_w - demo_x - pad;
        float demo_h = total_h - pad * 2.0f;

        // 1. Show standard ImGui Demo
        ImGui::SetNextWindowPos(ImVec2(demo_x, pad), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(demo_w, demo_h), ImGuiCond_FirstUseEver);
        ImGui::ShowDemoWindow();

        // 2. Metrics & Benchmark HUD
        {
            ImGui::SetNextWindowPos(ImVec2(pad, pad), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(hud_w, hud_h), ImGuiCond_FirstUseEver);
            ImGui::Begin("ImGui Vector Backend - Controls & Metrics", nullptr);
            ImGui::Text("Dear ImGui %s", IMGUI_VERSION);
            ImGui::Separator();

            bool reactive = ImGuiExt::IsReactiveMode();
            if (ImGui::Checkbox("Reactive Event-Driven Loop (Stage 1)", &reactive)) {
                ImGuiExt::SetReactiveMode(reactive);
            }
            if (reactive) {
                ImGui::SameLine();
                ImGui::TextDisabled("(pending repaints: %d)", ImGuiExt::EventLoop::Instance().GetRepaintFramesLeft());
            }

            bool dedup = ImGuiExt::IsFrameDeduplicationEnabled();
            if (ImGui::Checkbox("Frame Deduplication (Stage 2)", &dedup)) {
                ImGuiExt::SetFrameDeduplication(dedup);
            }
            if (dedup) {
                ImGui::SameLine();
                ImGui::TextDisabled("(skipped %llu frames)", ImGuiExt::FrameDeduplicator::Instance().GetTotalFramesSkipped());
            }

            ImGui::Separator();
            if (ImGui::Checkbox("ThorVG Vector Backend (Stage 3)", &use_vector_backend)) {
                ImGuiExt::SetVectorInterception(use_vector_backend);
            }
            if (use_vector_backend) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "[ACTIVE: Vector Primitives]");

                size_t total_cmds = 0, vector_cmds = 0, fallback_cmds = 0;
                for (const auto& kv : ImGuiExt::Recorder::Instance().GetStreams()) {
                    total_cmds += kv.second.commands.size();
                    for (const auto& c : kv.second.commands) {
                        if (c.type == ImGuiExt::CmdType::FallbackMesh) fallback_cmds++;
                        else vector_cmds++;
                    }
                }
                ImGui::Text("Recorded: %zu commands (%zu vector, %zu fallback mesh)", total_cmds, vector_cmds, fallback_cmds);
            } else {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "[Stock OpenGL3 Triangles]");
            }

            if (ImGui::Button("Simulate Texture Update")) {
                ImGuiExt::NotifyTextureUpdated();
            }

            ImGui::Separator();
            ImGui::Text("FPS: %.1f (Total Frame: %.2f ms)", metrics.fps, metrics.cpu_frame_time_ms);
            ImGui::Text("CPU Work Time: %.2f ms / frame", metrics.cpu_work_time_ms);
            ImGui::Text("GPU Frame Time: %.2f ms", metrics.gpu_frame_time_ms);
            ImGui::Text("CPU Usage (Process): %.2f %%", metrics.cpu_usage_percent);
            ImGui::Text("RAM Working Set: %.2f MB (Peak: %.2f MB)", metrics.ram_working_set_mb, metrics.ram_peak_working_set_mb);

            ImGui::Separator();
            if (bench.running) {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Benchmarking '%s'... (%.1fs left)",
                    bench.name.c_str(),
                    bench.duration_sec - std::chrono::duration<double>(std::chrono::steady_clock::now() - bench.start_time).count());
            } else {
                if (ImGui::Button("Run 5s Idle Benchmark")) {
                    std::string label = reactive ? (dedup ? "Stage 2: Reactive+Dedup - Idle (5s)" : "Stage 1: Reactive - Idle (5s)") : "Stage 0: Continuous - Idle (5s)";
                    bench.Start(label, 5.0);
                }
                ImGui::SameLine();
                if (ImGui::Button("Run 5s Active Benchmark")) {
                    std::string label = reactive ? (dedup ? "Stage 2: Reactive+Dedup - Mouse Motion (5s)" : "Stage 1: Reactive - Mouse Motion (5s)") : "Stage 0: Continuous - Mouse Motion (5s)";
                    bench.Start(label, 5.0);
                }
                if (ImGui::Button("Run 5s Vector Benchmark")) {
                    use_vector_backend = true;
                    ImGuiExt::SetVectorInterception(true);
                    bench.Start("Stage 3: ThorVG Vector - Mouse Motion (5s)", 5.0);
                }
                ImGui::Separator();
                if (ImGui::Button("Reset Layout to Default")) {
                    ImGui::SetWindowPos("ImGui Vector Backend - Controls & Metrics", ImVec2(pad, pad));
                    ImGui::SetWindowSize("ImGui Vector Backend - Controls & Metrics", ImVec2(hud_w, hud_h));
                    ImGui::SetWindowPos("Dear ImGui Demo", ImVec2(demo_x, pad));
                    ImGui::SetWindowSize("Dear ImGui Demo", ImVec2(demo_w, demo_h));
                }
            }
            ImGui::End();
        }

        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();

        // Stage 2: Deduplication check
        bool frame_changed = ImGuiExt::FrameDeduplicator::Instance().ShouldRenderFrame(draw_data);
        if (!frame_changed) {
            // Identical frame: skip glClear, RenderDrawData, and glfwSwapBuffers
            auto cpu_work_end = std::chrono::steady_clock::now();
            double cpu_work_time_ms = std::chrono::duration<double, std::milli>(cpu_work_end - cpu_work_start).count();
            auto frame_end = std::chrono::steady_clock::now();
            double frame_time_ms = std::chrono::duration<double, std::milli>(frame_end - frame_start).count();

            metrics.Update(frame_time_ms, cpu_work_time_ms, 0.0, io.Framerate);
            bench.RecordRender(metrics, cpu_work_time_ms);

            ImGuiExt::EventLoop::Instance().StepAfterRender();
            continue;
        }

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (glBeginQueryPtr && gl_query) {
            glBeginQueryPtr(GL_TIME_ELAPSED, gl_query);
        }

        if (use_vector_backend) {
            ImGuiExt::Recorder::Instance().EndFrame();
            vector_renderer->Resize(display_w, display_h);
            vector_renderer->RenderDrawData(draw_data);
            vector_renderer->PresentGL();
        } else {
            ImGui_ImplOpenGL3_RenderDrawData(draw_data);
        }

        if (glEndQueryPtr && gl_query) {
            glEndQueryPtr(GL_TIME_ELAPSED);
        }

        auto cpu_work_end = std::chrono::steady_clock::now();
        double cpu_work_time_ms = std::chrono::duration<double, std::milli>(cpu_work_end - cpu_work_start).count();

        glfwSwapBuffers(window);

        // Read back GPU query
        double gpu_time_ms = 0.0;
        if (glGetQueryObjectui64vPtr && gl_query) {
            GLuint64 timeElapsedNs = 0;
            glGetQueryObjectui64vPtr(gl_query, GL_QUERY_RESULT, &timeElapsedNs);
            gpu_time_ms = (double)timeElapsedNs / 1000000.0;
        }

        auto frame_end = std::chrono::steady_clock::now();
        double frame_time_ms = std::chrono::duration<double, std::milli>(frame_end - frame_start).count();

        metrics.Update(frame_time_ms, cpu_work_time_ms, gpu_time_ms, io.Framerate);
        bench.RecordRender(metrics, cpu_work_time_ms);

        ImGuiExt::EventLoop::Instance().StepAfterRender();
    }

    if (glDeleteQueriesPtr && gl_query) {
        glDeleteQueriesPtr(1, &gl_query);
    }

    if (vector_renderer) {
        vector_renderer->Shutdown();
        delete vector_renderer;
        vector_renderer = nullptr;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

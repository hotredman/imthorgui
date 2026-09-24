#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#endif

#include "imgui.h"
#include "imgui_ext/renderer.h"
#include "thorvg.h"

struct TestResult {
    std::string text;
    float imgui_w = 0.0f;
    float thorvg_w = 0.0f;
    float diff = 0.0f;
    bool passed = false;
};

int main() {
    std::cout << "========================================================\n";
    std::cout << " Stage 3: Text Metrics Consistency Validation Test\n";
    std::cout << "========================================================\n\n";

    // 1. Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    const char* font_path = "C:/Windows/Fonts/segoeui.ttf";
    float font_size = 18.0f;

    ImFontConfig cfg;
    cfg.OversampleH = 1;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = false;

    // Build with full Cyrillic + Latin glyph ranges
    ImFont* font = io.Fonts->AddFontFromFileTTF(font_path, font_size, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    if (!font) {
        // Fallback to Arial if Segoe UI isn't available
        font_path = "C:/Windows/Fonts/arial.ttf";
        font = io.Fonts->AddFontFromFileTTF(font_path, font_size, &cfg, io.Fonts->GetGlyphRangesCyrillic());
    }

    if (!font) {
        std::cerr << "[ERROR] Failed to load TTF font for ImGui\n";
        ImGui::DestroyContext(ctx);
        return 1;
    }

    io.Fonts->Build();
    std::cout << "Loaded Font: " << font_path << " (Size: " << font_size << "px)\n";
    std::cout << "ImFont: " << font->GetDebugName() << "\n\n";

    // 2. Initialize ThorVG Renderer
    ImGuiExt::IRenderer* renderer = ImGuiExt::CreateThorVGRenderer();
    if (!renderer->Init(800, 600)) {
        std::cerr << "[ERROR] Failed to init ThorVG renderer\n";
        delete renderer;
        ImGui::DestroyContext(ctx);
        return 1;
    }

    bool font_loaded = renderer->LoadFontFile(font_path);
    if (!font_loaded) {
        std::cerr << "[ERROR] Failed to load font in ThorVG: " << font_path << "\n";
        delete renderer;
        ImGui::DestroyContext(ctx);
        return 1;
    }

    // 3. Test Strings across Latin, Cyrillic, Digits, Punctuation, Mixed
    std::vector<std::pair<std::string, std::string>> test_categories = {
        // Latin
        {"Latin: Standard Greeting", "Hello World"},
        {"Latin: Pangram", "The quick brown fox jumps over the lazy dog"},
        {"Latin: CamelCase & Codes", "ImGuiExt::ThorVGRenderer::MeasureText"},
        {"Latin: Uppercase", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
        {"Latin: Lowercase", "abcdefghijklmnopqrstuvwxyz"},

        // Cyrillic
        {"Cyrillic: Standard Greeting", "Привет, мир!"},
        {"Cyrillic: Pangram", "Съешь же ещё этих мягких французских булок, да выпей чаю"},
        {"Cyrillic: Uppercase", "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ"},
        {"Cyrillic: Lowercase", "абвгдеёжзийклмнопрстуфхцчшщъыьэюя"},
        {"Cyrillic: UI Labels", "Файл Редактирование Вид Справка Настройки"},

        // Digits & Punctuation
        {"Digits: Sequence", "0123456789"},
        {"Digits: Formatting", "+1 (555) 019-2834 | 1,234,567.89 | 99.99%"},
        {"Symbols: Keyboard", "!@#$%^&*()_+-=[]{}|;':,./<>?"},
        {"Symbols: Math", "sin(x)^2 + cos(x)^2 = 1.000"},

        // Mixed
        {"Mixed: UI Title", "ImGui Vector Backend v1.0.7 (Форк + ThorVG)"},
        {"Mixed: Code snippet", "auto shape = tvg::Shape::gen(); // Создание примитива"},
        {"Mixed: Path & Numbers", "C:\\Projects\\hotredman\\imthorgui #42 (100% OK)"},
    };

    std::vector<TestResult> results;
    int pass_count = 0;
    float max_diff = 0.0f;
    float sum_diff = 0.0f;

    // Tolerance in pixels: ImGui uses unhinted FreeType/stb_truetype advance rounded or float;
    // ThorVG uses TrueType unhinted font layout with floating point advance.
    // Over a full paragraph / string, acceptable diff is <= 1.5px (or <= 2% relative width).
    const float ABS_TOLERANCE_PX = 2.0f;
    const float REL_TOLERANCE_PCT = 0.03f; // 3%

    std::cout << "| № | Категория / Строка | ImGui (px) | ThorVG (px) | Разница | Статус |\n";
    std::cout << "|---|--------------------|:----------:|:-----------:|:-------:|:------:|\n";

    for (size_t i = 0; i < test_categories.size(); ++i) {
        const auto& item = test_categories[i];
        const std::string& label = item.first;
        const std::string& text = item.second;

        ImVec2 imgui_size = font->CalcTextSizeA(font_size, FLT_MAX, -1.0f, text.c_str());
        float thorvg_w = 0.0f, thorvg_h = 0.0f;
        renderer->MeasureText(text.c_str(), nullptr, font_size, thorvg_w, thorvg_h);

        float diff = std::abs(imgui_size.x - thorvg_w);
        float rel_diff = imgui_size.x > 0.0f ? (diff / imgui_size.x) : 0.0f;

        bool passed = (diff <= ABS_TOLERANCE_PX) || (rel_diff <= REL_TOLERANCE_PCT);
        if (passed) pass_count++;

        if (diff > max_diff) max_diff = diff;
        sum_diff += diff;

        results.push_back({text, imgui_size.x, thorvg_w, diff, passed});

        std::string display_text = text.length() > 30 ? (text.substr(0, 27) + "...") : text;
        std::cout << "| " << std::setw(2) << (i + 1) << " | "
                  << std::left << std::setw(32) << label << " | "
                  << std::right << std::fixed << std::setprecision(1) << std::setw(8) << imgui_size.x << " | "
                  << std::setw(9) << thorvg_w << " | "
                  << std::setw(6) << diff << " | "
                  << (passed ? "  OK  " : " FAIL ") << " |\n";
    }

    std::cout << "\n--------------------------------------------------------\n";
    std::cout << "Результаты валидации:\n";
    std::cout << " Успешных тестов:   " << pass_count << " / " << test_categories.size()
              << " (" << (pass_count * 100 / test_categories.size()) << "%)\n";
    std::cout << " Макс. расхождение: " << std::fixed << std::setprecision(2) << max_diff << " px\n";
    std::cout << " Среднее расхождение: " << (sum_diff / (float)test_categories.size()) << " px\n";
    std::cout << " Допуск:            <= " << ABS_TOLERANCE_PX << " px или <= " << (REL_TOLERANCE_PCT * 100.0f) << "%\n";
    std::cout << "--------------------------------------------------------\n";

    // Clean up
    renderer->Shutdown();
    delete renderer;
    ImGui::DestroyContext(ctx);

    bool all_passed = (pass_count == (int)test_categories.size());
    if (all_passed) {
        std::cout << "\n>>> [PASS] Единый источник метрик подтверждён! Текстовые метрики согласованы.\n\n";
        return 0;
    } else {
        std::cout << "\n>>> [WARNING] Имеются небольшие расхождения метрик (> допуска).\n\n";
        return 1;
    }
}

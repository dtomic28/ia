#include <cstdint>
#include <cstdio>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#include "lcg.hpp"

struct GeneratorSettings {
    uint64_t m{1ull << 32};
    uint64_t a{69069};
    uint64_t b{1};
    uint64_t seed{0};
    uint64_t low{1};
    uint64_t high{100};
    int count{10000};
};

static std::vector<double> generate(const GeneratorSettings& s) {
    LCG lcg{s.m, s.a, s.b, s.seed};
    std::vector<double> values;
    values.reserve(s.count);
    for (int i = 0; i < s.count; i++)
        values.push_back(static_cast<double>(lcg.getRandom(s.low, s.high)));
    return values;
}

static void drawSettings(GeneratorSettings& s, std::vector<double>& values) {
    const ImU64 one = 1;

    ImGui::SeparatorText("LCG parameters");
    ImGui::InputScalar("m", ImGuiDataType_U64, &s.m);
    ImGui::InputScalar("a", ImGuiDataType_U64, &s.a);
    ImGui::InputScalar("b", ImGuiDataType_U64, &s.b);
    ImGui::InputScalar("seed", ImGuiDataType_U64, &s.seed);

    ImGui::SeparatorText("Output range");
    ImGui::InputScalar("low", ImGuiDataType_U64, &s.low, &one);
    ImGui::InputScalar("high", ImGuiDataType_U64, &s.high, &one);
    ImGui::SliderInt("count", &s.count, 1, 1000000, "%d", ImGuiSliderFlags_Logarithmic);

    const bool valid = s.m > 0 && s.low <= s.high;
    if (!valid)
        ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "m must be > 0 and low <= high");

    ImGui::BeginDisabled(!valid);
    if (ImGui::Button("Generate"))
        values = generate(s);
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Super-Duper defaults"))
        s = GeneratorSettings{};
}

static void drawValues(const std::vector<double>& values) {
    ImGui::SeparatorText("First values");
    if (ImGui::BeginTable("values", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, {0, 200})) {
        ImGui::TableSetupColumn("i");
        ImGui::TableSetupColumn("value");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(values.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", i);
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", values[i]);
            }
        }
        ImGui::EndTable();
    }
}

static void drawPlots(const GeneratorSettings& s, const std::vector<double>& values) {
    if (values.empty()) {
        ImGui::TextDisabled("Press Generate to produce values.");
        return;
    }

    const int count = static_cast<int>(values.size());
    const double range = static_cast<double>(s.high - s.low + 1);
    const int bins = range <= 200 ? static_cast<int>(range) : 100;

    if (ImPlot::BeginPlot("Histogram", {-1, 300})) {
        ImPlot::SetupAxes("value", "count", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        ImPlot::PlotHistogram("values", values.data(), count, bins, 1.0,
                              ImPlotRange(static_cast<double>(s.low) - 0.5, static_cast<double>(s.high) + 0.5));
        ImPlot::EndPlot();
    }

    // Consecutive pairs (x_n, x_n+1) reveal the lattice structure of an LCG
    if (count > 1 && ImPlot::BeginPlot("Consecutive pairs", {-1, -1}, ImPlotFlags_Equal)) {
        ImPlot::SetupAxes("x[n]", "x[n+1]", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        ImPlotSpec spec;
        spec.Marker = ImPlotMarker_Circle;
        spec.MarkerSize = 1.5f;
        ImPlot::PlotScatter("pairs", values.data(), values.data() + 1, count - 1, spec);
        ImPlot::EndPlot();
    }
}

int main() {
    glfwSetErrorCallback([](int error, const char* description) {
        std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
    });
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    GLFWwindow* window = glfwCreateWindow(1280, 800, "vaja01 - LCG", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    GeneratorSettings settings;
    std::vector<double> values = generate(settings);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
            glfwWaitEvents();
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Single window filling the whole viewport
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("LCG", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

        if (ImGui::BeginTable("layout", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("controls", ImGuiTableColumnFlags_WidthFixed, 340);
            ImGui::TableSetupColumn("plots", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextColumn();
            drawSettings(settings, values);
            drawValues(values);
            ImGui::TableNextColumn();
            drawPlots(settings, values);
            ImGui::EndTable();
        }

        ImGui::End();

        ImGui::Render();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

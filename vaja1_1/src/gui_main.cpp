#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

#include "lcg.h"
#include "prime_generators.h"

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

    if (ImPlot::BeginPlot("Histogram", {-1, -1})) {
        ImPlot::SetupAxes("value", "count", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        ImPlot::PlotHistogram("values", values.data(), count, bins, 1.0,
                              ImPlotRange(static_cast<double>(s.low) - 0.5, static_cast<double>(s.high) + 0.5));
        ImPlot::EndPlot();
    }
}

struct GeneratedPrime {
    const char* method;
    int bits;
    uint64_t prime;
    double microseconds;
};

struct PrimeState {
    int bits{16};
    uint64_t s{defaultMillerRabinRounds};
    uint64_t seed{0};
    std::vector<GeneratedPrime> generated;

    uint64_t testNumber{97};
    bool tested{false};
    uint64_t testedNumber{0};
    bool naiveResult{false};
    MRResult millerRabinResult{MRResult::composite};
    double naiveMicroseconds{0};
    double millerRabinMicroseconds{0};
};

template <typename Function>
static double measureMicroseconds(Function&& function) {
    const auto start = std::chrono::steady_clock::now();
    function();
    const auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count();
}

static std::string toBinary(uint64_t value) {
    if (value == 0)
        return "0";
    std::string binary;
    while (value > 0) {
        binary.insert(binary.begin(), static_cast<char>('0' + (value & 1)));
        value >>= 1;
    }
    return binary;
}

static const char* toString(MRResult result) {
    switch (result) {
        case MRResult::prime: return "prime";
        case MRResult::composite: return "composite";
        case MRResult::probablyPrime: return "probably prime";
    }
    return "";
}

static void drawPrimeControls(PrimeState& state) {
    const ImU64 one = 1;

    ImGui::SeparatorText("Super-Duper generator");
    ImGui::InputScalar("seed", ImGuiDataType_U64, &state.seed);
    if (ImGui::Button("Reset generator"))
        SuperDuper = LCG{1ull << 32, 69069, 1, state.seed};
    ImGui::SetItemTooltip("Restarts Super-Duper LCG(2^32, 69069, 1, seed).\n"
                          "Reset before each method to get the same starting number.");

    ImGui::SeparatorText("Miller-Rabin");
    ImGui::InputScalar("s", ImGuiDataType_U64, &state.s, &one);
    if (state.s < 1)
        state.s = 1;

    ImGui::SeparatorText("Generate prime");
    ImGui::SliderInt("bits", &state.bits, 2, 32, "%d", ImGuiSliderFlags_AlwaysClamp);
    ImGui::TextDisabled("range: [%llu, %llu]",
                        static_cast<unsigned long long>(minNBit(state.bits)),
                        static_cast<unsigned long long>(maxNBit(state.bits)));

    const auto bits = static_cast<uint8_t>(state.bits);
    if (ImGui::Button("Naive")) {
        uint64_t prime = 0;
        const double time = measureMicroseconds([&] { prime = generatePrimeNaive(bits); });
        state.generated.push_back({"naive", state.bits, prime, time});
    }
    ImGui::SameLine();
    if (ImGui::Button("Miller-Rabin")) {
        uint64_t prime = 0;
        const double time = measureMicroseconds([&] { prime = generatePrimeMillerRabin(bits, state.s); });
        state.generated.push_back({"Miller-Rabin", state.bits, prime, time});
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        state.generated.clear();

    ImGui::SeparatorText("Test number");
    ImGui::InputScalar("number", ImGuiDataType_U64, &state.testNumber);

    const bool valid = state.testNumber <= maxNBit(32);
    if (!valid)
        ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "number must fit in 32 bits");

    ImGui::BeginDisabled(!valid);
    if (ImGui::Button("Test with both methods")) {
        state.tested = true;
        state.testedNumber = state.testNumber;
        state.naiveMicroseconds = measureMicroseconds([&] { state.naiveResult = isPrimeNaive(state.testNumber); });
        state.millerRabinMicroseconds = measureMicroseconds([&] {
            state.millerRabinResult = millerRabinTest(state.testNumber, state.s);
        });
    }
    ImGui::EndDisabled();

    if (state.tested) {
        ImGui::Text("%llu = %s", static_cast<unsigned long long>(state.testedNumber),
                    toBinary(state.testedNumber).c_str());
        ImGui::Text("naive:        %s (%.2f us)", state.naiveResult ? "prime" : "composite", state.naiveMicroseconds);
        ImGui::Text("Miller-Rabin: %s (%.2f us)", toString(state.millerRabinResult), state.millerRabinMicroseconds);
    }
}

static void drawGeneratedPrimes(const PrimeState& state) {
    ImGui::SeparatorText("Generated primes");
    if (ImGui::BeginTable("primes", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                          {0, -FLT_MIN})) {
        ImGui::TableSetupColumn("method");
        ImGui::TableSetupColumn("bits");
        ImGui::TableSetupColumn("prime");
        ImGui::TableSetupColumn("binary");
        ImGui::TableSetupColumn("time [us]");
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        // Newest first
        for (auto it = state.generated.rbegin(); it != state.generated.rend(); ++it) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(it->method);
            ImGui::TableNextColumn();
            ImGui::Text("%d", it->bits);
            ImGui::TableNextColumn();
            ImGui::Text("%llu", static_cast<unsigned long long>(it->prime));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(toBinary(it->prime).c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", it->microseconds);
        }
        ImGui::EndTable();
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
    GLFWwindow* window = glfwCreateWindow(1280, 800, "vaja1_1 - LCG & primes", nullptr, nullptr);
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
    PrimeState primeState;

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

        if (ImGui::BeginTabBar("tabs")) {
            if (ImGui::BeginTabItem("Random numbers")) {
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
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Primes")) {
                if (ImGui::BeginTable("primeLayout", 2, ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("controls", ImGuiTableColumnFlags_WidthFixed, 340);
                    ImGui::TableSetupColumn("results", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableNextColumn();
                    drawPrimeControls(primeState);
                    ImGui::TableNextColumn();
                    drawGeneratedPrimes(primeState);
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
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

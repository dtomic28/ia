#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <nfd.h>

#include "file_crypto.h"
#include "keygen.h"
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

inline constexpr unsigned naiveMaxBits = 32;

struct GeneratedPrime {
    const char* method;
    int bits;
    // Converting 2048-bit numbers every frame is slow, so the text is made once
    std::string decimal;
    std::string binary;
    double microseconds;
};

struct PrimeState {
    int bits{16};
    uint64_t s{defaultMillerRabinRounds};
    uint64_t seed{0};
    std::vector<GeneratedPrime> generated;

    char testNumber[700]{"97"};    // 2048 bits is 617 decimal digits
    bool tested{false};
    cpp_int testedNumber{0};
    bool naiveTested{false};
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

static std::string toBinary(const cpp_int& value) {
    if (value == 0)
        return "0";
    std::string binary;
    for (int bit = static_cast<int>(msb(value)); bit >= 0; bit--)
        binary += bit_test(value, bit) ? '1' : '0';
    return binary;
}

// Parses a non-negative decimal number of any size
static std::optional<cpp_int> parseDecimal(const char* text) {
    if (*text == '\0' || std::strspn(text, "0123456789") != std::strlen(text))
        return std::nullopt;
    return cpp_int(text);
}

// Parses a non-negative decimal number, fails if it doesn't fit in 2048 bits
static std::optional<cpp_int> parseBig(const char* text) {
    const std::optional<cpp_int> value = parseDecimal(text);
    if (!value || *value > maxNBit(maxPrimeBits))
        return std::nullopt;
    return value;
}

static void addGenerated(PrimeState& state, const char* method, const cpp_int& prime, double microseconds) {
    state.generated.push_back({method, state.bits, prime.str(), toBinary(prime), microseconds});
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
    ImGui::SliderInt("bits", &state.bits, 2, maxPrimeBits, "%d",
                     ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
    ImGui::TextDisabled("range: [2^%d, 2^%d - 1]", state.bits - 1, state.bits);

    const auto bits = static_cast<unsigned>(state.bits);
    // Trial division is hopeless past 32 bits
    ImGui::BeginDisabled(bits > naiveMaxBits);
    if (ImGui::Button("Naive")) {
        cpp_int prime = 0;
        const double time = measureMicroseconds([&] { prime = generatePrimeNaive(bits); });
        addGenerated(state, "naive", prime, time);
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Only up to %u bits", naiveMaxBits);
    ImGui::SameLine();
    if (ImGui::Button("Miller-Rabin")) {
        cpp_int prime = 0;
        const double time = measureMicroseconds([&] { prime = generatePrimeMillerRabin(bits, state.s); });
        addGenerated(state, "Miller-Rabin", prime, time);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        state.generated.clear();

    ImGui::SeparatorText("Test number");
    ImGui::InputTextMultiline("number", state.testNumber, sizeof(state.testNumber), {0, 60},
                              ImGuiInputTextFlags_CharsDecimal);

    const std::optional<cpp_int> number = parseBig(state.testNumber);
    if (!number)
        ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "number must be a decimal that fits in %u bits", maxPrimeBits);

    ImGui::BeginDisabled(!number);
    if (ImGui::Button("Test with both methods")) {
        state.tested = true;
        state.testedNumber = *number;
        state.naiveTested = state.testedNumber <= maxNBit(naiveMaxBits);
        if (state.naiveTested)
            state.naiveMicroseconds = measureMicroseconds([&] { state.naiveResult = isPrimeNaive(state.testedNumber); });
        state.millerRabinMicroseconds = measureMicroseconds([&] {
            state.millerRabinResult = millerRabinTest(state.testedNumber, state.s);
        });
    }
    ImGui::EndDisabled();

    if (state.tested) {
        ImGui::TextWrapped("%s = %s", state.testedNumber.str().c_str(), toBinary(state.testedNumber).c_str());
        if (state.naiveTested)
            ImGui::Text("naive:        %s (%.2f us)", state.naiveResult ? "prime" : "composite", state.naiveMicroseconds);
        else
            ImGui::Text("naive:        skipped, more than %u bits", naiveMaxBits);
        ImGui::Text("Miller-Rabin: %s (%.2f us)", toString(state.millerRabinResult), state.millerRabinMicroseconds);
    }
}

static void drawGeneratedPrimes(const PrimeState& state) {
    ImGui::SeparatorText("Generated primes");
    if (ImGui::BeginTable("primes", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                          {0, -FLT_MIN})) {
        ImGui::TableSetupColumn("method", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("bits", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("prime", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("binary", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("time [us]", ImGuiTableColumnFlags_WidthFixed);
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
            ImGui::TextWrapped("%s", it->decimal.c_str());
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", it->binary.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", it->microseconds);
        }
        ImGui::EndTable();
    }
}

// With 2 bits the only odd prime is 3, so p and q could never differ
inline constexpr int minKeyBits = 3;
inline constexpr size_t pathSize = 1024;

struct GeneratedKeys {
    const char* method;
    uint32_t bits;
    uint32_t modulusBits;
    // Converting big numbers every frame is slow, so the text is made once
    std::string p;
    std::string q;
    std::string n;
    std::string phi;
    std::string e;
    std::string d;
    std::string publicKeyPath;
    std::string privateKeyPath;
    double microseconds;
};

struct NumberResult {
    std::string error;
    std::string message;
    std::string cipher;
    std::string decrypted;
    bool matches{false};
    double encryptMicroseconds{0};
    double decryptMicroseconds{0};
};

struct FileResult {
    const char* operation;
    std::string error;
    std::string inputPath;
    std::string outputPath;
    uint64_t inputBytes{0};
    uint64_t outputBytes{0};
    uint32_t blockBits{0};
    double milliseconds{0};
};

struct RsaState {
    uint64_t seed{0};
    PrimeMethod method{PrimeMethod::millerRabin};
    uint64_t s{defaultMillerRabinRounds};
    int bits{15};

    char publicKeyPath[pathSize]{"pubkey.txt"};
    char privateKeyPath[pathSize]{"privkey.txt"};
    char message[1300]{"42"};    // a 4096-bit n has 1234 decimal digits
    char inputPath[pathSize]{""};
    char encryptedPath[pathSize]{"enc.bin"};
    char decryptedPath[pathSize]{"msg.bin"};

    // Long operations run on a background thread so the window keeps drawing.
    // The results below are only written by that thread and only read once it has finished.
    std::future<void> job;
    const char* jobName{""};
    std::atomic<double> progress{-1.0};    // negative when the job can't report progress

    std::optional<GeneratedKeys> keys;
    std::optional<NumberResult> number;
    std::optional<FileResult> file;
};

static bool isRunning(const RsaState& state) {
    return state.job.valid() && state.job.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
}

static void startJob(RsaState& state, const char* name, bool reportsProgress, std::function<void()> function) {
    state.jobName = name;
    state.progress = reportsProgress ? 0.0 : -1.0;
    state.job = std::async(std::launch::async, std::move(function));
}

static std::string absolutePath(const std::string& path) {
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(path, error);
    return error ? path : absolute.string();
}

// Text field for a path with a "..." button that opens the native file dialog
static void drawPathInput(const char* label, char* path, size_t size, bool save) {
    ImGui::PushID(label);
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    const float buttonWidth = ImGui::GetFrameHeight() * 1.5f;

    ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - buttonWidth - spacing);
    ImGui::InputText("##path", path, size);
    if (ImGui::BeginItemTooltip()) {
        ImGui::TextUnformatted(absolutePath(path).c_str());
        ImGui::EndTooltip();
    }

    ImGui::SameLine(0, spacing);
    if (ImGui::Button("...", {buttonWidth, 0})) {
        // Starts in the working directory, that is where relative paths like enc.bin end up
        const std::string startFolder = std::filesystem::current_path().string();
        const std::string fileName = std::filesystem::path(path).filename().string();
        nfdu8char_t* chosen = nullptr;
        const nfdresult_t result = save ? NFD_SaveDialogU8(&chosen, nullptr, 0, startFolder.c_str(), fileName.c_str())
                                        : NFD_OpenDialogU8(&chosen, nullptr, 0, startFolder.c_str());
        if (result == NFD_OKAY) {
            std::snprintf(path, size, "%s", chosen);
            NFD_FreePathU8(chosen);
        } else if (result == NFD_ERROR) {
            std::fprintf(stderr, "File dialog error: %s\n", NFD_GetError());
        }
    }

    ImGui::SameLine(0, spacing);
    ImGui::TextUnformatted(label);
    ImGui::PopID();
}

static void startKeyGeneration(RsaState& state) {
    const auto bits = static_cast<uint32_t>(state.bits);
    const PrimeMethod method = state.method;
    const uint64_t s = state.s;
    const std::string publicKeyPath = state.publicKeyPath;
    const std::string privateKeyPath = state.privateKeyPath;

    startJob(state, "Generating keys", false, [&state, bits, method, s, publicKeyPath, privateKeyPath] {
        KeyPair keys;
        const double time = measureMicroseconds([&] {
            keys = generateAndSaveKeys(bits, method, s, publicKeyPath, privateKeyPath);
        });

        state.keys = GeneratedKeys{
            method == PrimeMethod::naive ? "naive" : "Miller-Rabin",
            bits,
            static_cast<uint32_t>(msb(keys.publicKey.modulus) + 1),
            keys.p.str(),
            keys.q.str(),
            keys.publicKey.modulus.str(),
            keys.phi.str(),
            keys.publicKey.exponent.str(),
            keys.privateKey.exponent.str(),
            absolutePath(publicKeyPath),
            absolutePath(privateKeyPath),
            time,
        };
    });
}

// Encrypts M with the public key file and decrypts it back with the private key file
static void startNumberTest(RsaState& state) {
    const std::string text = state.message;
    const std::string publicKeyPath = state.publicKeyPath;
    const std::string privateKeyPath = state.privateKeyPath;

    startJob(state, "Encrypting number", false, [&state, text, publicKeyPath, privateKeyPath] {
        NumberResult result;
        const std::optional<Key> publicKey = loadKey(publicKeyPath);
        const std::optional<Key> privateKey = loadKey(privateKeyPath);
        const std::optional<cpp_int> message = parseDecimal(text.c_str());

        if (!publicKey || !privateKey) {
            result.error = "Could not load the key files, generate keys first";
        } else if (!message) {
            result.error = "M must be a non-negative decimal number";
        } else if (*message >= publicKey->modulus) {
            result.error = "M must be smaller than n";
        } else {
            cpp_int cipher;
            cpp_int decrypted;
            result.encryptMicroseconds = measureMicroseconds([&] {
                cipher = modularExponentiation(*message, publicKey->exponent, publicKey->modulus);
            });
            result.decryptMicroseconds = measureMicroseconds([&] {
                decrypted = modularExponentiation(cipher, privateKey->exponent, privateKey->modulus);
            });
            result.message = message->str();
            result.cipher = cipher.str();
            result.decrypted = decrypted.str();
            result.matches = decrypted == *message;
        }
        state.number = result;
    });
}

static void startFileJob(RsaState& state, bool encrypt) {
    const std::string inputPath = encrypt ? state.inputPath : state.encryptedPath;
    const std::string outputPath = encrypt ? state.encryptedPath : state.decryptedPath;
    const std::string keyPath = encrypt ? state.publicKeyPath : state.privateKeyPath;

    startJob(state, encrypt ? "Encrypting file" : "Decrypting file", true,
             [&state, encrypt, inputPath, outputPath, keyPath] {
        FileResult result{encrypt ? "Encrypted" : "Decrypted"};
        result.inputPath = absolutePath(inputPath);
        result.outputPath = absolutePath(outputPath);

        const std::optional<Key> key = loadKey(keyPath);
        std::error_code error;
        if (!key) {
            result.error = "Could not load " + keyPath;
        } else if (!std::filesystem::is_regular_file(inputPath, error)) {
            result.error = "Input file " + result.inputPath + " does not exist";
        } else {
            result.blockBits = static_cast<uint32_t>(msb(key->modulus));
            const auto onProgress = [&state](double fraction) { state.progress = fraction; };

            bool ok = false;
            result.milliseconds = measureMicroseconds([&] {
                ok = encrypt ? encryptFile(inputPath, outputPath, *key, onProgress)
                             : decryptFile(inputPath, outputPath, *key, onProgress);
            }) / 1000.0;

            if (ok) {
                result.inputBytes = std::filesystem::file_size(inputPath, error);
                result.outputBytes = std::filesystem::file_size(outputPath, error);
            } else {
                result.error = "Failed, the details are in the console";
            }
        }
        state.file = result;
    });
}

static void drawRsaControls(RsaState& state) {
    const ImU64 one = 1;
    const bool running = isRunning(state);

    if (running) {
        ImGui::SeparatorText(state.jobName);
        const double progress = state.progress;
        // A negative fraction makes ImGui draw an indeterminate, moving bar
        if (progress >= 0.0)
            ImGui::ProgressBar(static_cast<float>(progress), {-FLT_MIN, 0});
        else
            ImGui::ProgressBar(-1.0f * static_cast<float>(ImGui::GetTime()), {-FLT_MIN, 0}, "Working...");
    }

    ImGui::BeginDisabled(running);

    ImGui::SeparatorText("Super-Duper generator");
    ImGui::InputScalar("seed", ImGuiDataType_U64, &state.seed);
    if (ImGui::Button("Reset generator"))
        SuperDuper = LCG{1ull << 32, 69069, 1, state.seed};
    ImGui::SetItemTooltip("Restarts Super-Duper LCG(2^32, 69069, 1, seed).\n"
                          "The same seed gives the same keys.");

    ImGui::SeparatorText("Key files");
    drawPathInput("public", state.publicKeyPath, pathSize, false);
    drawPathInput("private", state.privateKeyPath, pathSize, false);

    ImGui::SeparatorText("Key generation");
    if (ImGui::RadioButton("naive", state.method == PrimeMethod::naive))
        state.method = PrimeMethod::naive;
    ImGui::SameLine();
    if (ImGui::RadioButton("Miller-Rabin", state.method == PrimeMethod::millerRabin))
        state.method = PrimeMethod::millerRabin;

    ImGui::BeginDisabled(state.method != PrimeMethod::millerRabin);
    ImGui::InputScalar("s", ImGuiDataType_U64, &state.s, &one);
    if (state.s < 1)
        state.s = 1;
    ImGui::EndDisabled();

    // Trial division is hopeless past 32 bits
    const int maxBits = state.method == PrimeMethod::naive ? static_cast<int>(naiveMaxBits) : static_cast<int>(maxPrimeBits);
    state.bits = std::clamp(state.bits, minKeyBits, maxBits);
    ImGui::SliderInt("bits of p, q", &state.bits, minKeyBits, maxBits, "%d",
                     ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
    ImGui::TextDisabled("basic version: up to 15 bits");

    if (ImGui::Button("Generate keys"))
        startKeyGeneration(state);
    ImGui::SetItemTooltip("Saves the public key to the public key file and the private key to the private key file.");

    ImGui::SeparatorText("Encrypt number");
    ImGui::InputTextMultiline("M", state.message, sizeof(state.message), {0, 60}, ImGuiInputTextFlags_CharsDecimal);
    if (ImGui::Button("Encrypt and decrypt"))
        startNumberTest(state);
    ImGui::SetItemTooltip("C = M^e mod n with the public key, then M = C^d mod n with the private key.");

    ImGui::SeparatorText("Files");
    drawPathInput("input", state.inputPath, pathSize, false);
    drawPathInput("encrypted", state.encryptedPath, pathSize, true);
    drawPathInput("decrypted", state.decryptedPath, pathSize, true);

    if (ImGui::Button("Encrypt"))
        startFileJob(state, true);
    ImGui::SetItemTooltip("input -> encrypted, with the public key");
    ImGui::SameLine();
    if (ImGui::Button("Decrypt"))
        startFileJob(state, false);
    ImGui::SetItemTooltip("encrypted -> decrypted, with the private key");

    ImGui::EndDisabled();
}

static void drawNumberRow(const char* name, const std::string& value) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(name);
    ImGui::TableNextColumn();
    ImGui::TextWrapped("%s", value.c_str());
}

static void drawRsaResults(const RsaState& state) {
    if (isRunning(state)) {
        ImGui::TextDisabled("Working...");
        return;
    }

    const ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
    const ImVec4 errorColor{1.0f, 0.4f, 0.4f, 1.0f};

    ImGui::SeparatorText("Keys");
    if (const auto& keys = state.keys) {
        ImGui::Text("%s, %u-bit p and q, n has %u bits, %.2f ms", keys->method, keys->bits, keys->modulusBits,
                    keys->microseconds / 1000.0);
        if (ImGui::BeginTable("keys", 2, tableFlags)) {
            ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
            drawNumberRow("p", keys->p);
            drawNumberRow("q", keys->q);
            drawNumberRow("n = pq", keys->n);
            drawNumberRow("phi(n)", keys->phi);
            drawNumberRow("e", keys->e);
            drawNumberRow("d", keys->d);
            ImGui::EndTable();
        }
        ImGui::TextWrapped("public key (e, n): %s", keys->publicKeyPath.c_str());
        ImGui::TextWrapped("private key (d, n): %s", keys->privateKeyPath.c_str());
    } else {
        ImGui::TextDisabled("Press Generate keys.");
    }

    ImGui::SeparatorText("Number");
    if (const auto& number = state.number) {
        if (!number->error.empty()) {
            ImGui::TextColored(errorColor, "%s", number->error.c_str());
        } else {
            if (ImGui::BeginTable("number", 2, tableFlags)) {
                ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
                drawNumberRow("M", number->message);
                drawNumberRow("C = M^e mod n", number->cipher);
                drawNumberRow("C^d mod n", number->decrypted);
                ImGui::EndTable();
            }
            if (number->matches)
                ImGui::Text("decrypted == M, encrypt %.2f us, decrypt %.2f us", number->encryptMicroseconds,
                            number->decryptMicroseconds);
            else
                ImGui::TextColored(errorColor, "decrypted != M");
        }
    } else {
        ImGui::TextDisabled("Press Encrypt and decrypt.");
    }

    ImGui::SeparatorText("File");
    if (const auto& file = state.file) {
        if (!file->error.empty()) {
            ImGui::TextColored(errorColor, "%s", file->error.c_str());
        } else {
            ImGui::Text("%s in %.2f ms", file->operation, file->milliseconds);
            ImGui::TextWrapped("from: %s (%llu B)", file->inputPath.c_str(),
                               static_cast<unsigned long long>(file->inputBytes));
            ImGui::TextWrapped("to:   %s (%llu B)", file->outputPath.c_str(),
                               static_cast<unsigned long long>(file->outputBytes));
            ImGui::Text("blocks: %u bits -> %u bits", file->blockBits, file->blockBits + 1);
        }
    } else {
        ImGui::TextDisabled("Pick a file and press Encrypt or Decrypt.");
    }
}

int main() {
    glfwSetErrorCallback([](int error, const char* description) {
        std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
    });
    if (!glfwInit())
        return 1;
    if (NFD_Init() != NFD_OKAY)
        std::fprintf(stderr, "File dialogs unavailable: %s\n", NFD_GetError());

#if defined(__APPLE__)
    // macOS only offers GL 3.2+ as a forward-compatible core profile
    const char* glslVersion = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 800, "vaja1 - LCG, primes & RSA", nullptr, nullptr);
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
    ImGui_ImplOpenGL3_Init(glslVersion);
    GeneratorSettings settings;
    std::vector<double> values = generate(settings);
    PrimeState primeState;
    RsaState rsaState;

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
                    // Both tabs draw from SuperDuper, so primes wait while an RSA job uses it
                    ImGui::BeginDisabled(isRunning(rsaState));
                    drawPrimeControls(primeState);
                    ImGui::EndDisabled();
                    ImGui::TableNextColumn();
                    drawGeneratedPrimes(primeState);
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("RSA")) {
                if (ImGui::BeginTable("rsaLayout", 2, ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("controls", ImGuiTableColumnFlags_WidthFixed, 420);
                    ImGui::TableSetupColumn("results", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableNextColumn();
                    drawRsaControls(rsaState);
                    ImGui::TableNextColumn();
                    // Keys with thousands of digits don't fit, so the results scroll on their own
                    if (ImGui::BeginChild("rsaResults")) {
                        drawRsaResults(rsaState);
                    }
                    ImGui::EndChild();
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

    // The job writes into rsaState, so it has to finish before rsaState goes away
    if (rsaState.job.valid())
        rsaState.job.wait();

    NFD_Quit();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

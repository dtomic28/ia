#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>

#include "prime_generators.h"

static bool parseU64(const char* text, uint64_t& out) {
    const char* end = text + std::strlen(text);
    auto [ptr, ec] = std::from_chars(text, end, out);
    return ec == std::errc{} && ptr == end;
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << " <naive|mr> <bits> <s> <repetitions> <seed>\n"
                  << "Generates <repetitions> primes with <bits> bits and prints the average time per prime in microseconds.\n"
                  << "<s> is the number of Miller-Rabin rounds and is ignored by the naive method.\n";
        return 1;
    }

    const std::string_view method = argv[1];
    if (method != "naive" && method != "mr") {
        std::cerr << "Unknown method '" << method << "', expected naive or mr\n";
        return 1;
    }

    const char* names[] = {"bits", "s", "repetitions", "seed"};
    uint64_t args[4];
    for (int i = 0; i < 4; i++) {
        if (!parseU64(argv[i + 2], args[i])) {
            std::cerr << "Invalid value for " << names[i] << ": '" << argv[i + 2] << "'\n";
            return 1;
        }
    }

    const auto [bits, s, repetitions, seed] = args;
    // Trial division is hopeless past 32 bits, Miller-Rabin goes up to the full cpp_int width
    const uint64_t maxBits = method == "naive" ? 32 : maxPrimeBits;
    if (bits < 2 || bits > maxBits) {
        std::cerr << "bits must be between 2 and " << maxBits << " for " << method << "\n";
        return 1;
    }
    if (s < 1 || repetitions < 1) {
        std::cerr << "s and repetitions must be at least 1\n";
        return 1;
    }

    SuperDuper = LCG{1ull << 32, 69069, 1, seed};

    // Summing the low 64 bits of the primes keeps the compiler from optimizing the generation away
    volatile uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (uint64_t i = 0; i < repetitions; i++) {
        const uint2048_t prime = method == "naive" ? generatePrimeNaive(static_cast<unsigned>(bits))
                                                   : generatePrimeMillerRabin(static_cast<unsigned>(bits), s);
        checksum = checksum + static_cast<uint64_t>(prime & UINT64_MAX);
    }
    const auto end = std::chrono::steady_clock::now();

    const double totalMicroseconds = std::chrono::duration<double, std::micro>(end - start).count();
    std::cout << totalMicroseconds / static_cast<double>(repetitions) << "\n";
    return 0;
}

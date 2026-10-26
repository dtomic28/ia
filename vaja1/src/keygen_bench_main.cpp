#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string_view>

#include "keygen.h"

static bool parseU64(const char* text, uint64_t& out) {
    const char* end = text + std::strlen(text);
    auto [ptr, ec] = std::from_chars(text, end, out);
    return ec == std::errc{} && ptr == end;
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << " <naive|mr> <bits> <s> <repetitions> <seed>\n"
                  << "Generates <repetitions> RSA key pairs from <bits>-bit primes p and q and prints\n"
                  << "the average time per key pair in microseconds and the average number of bits of n.\n"
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
    // With 2 bits p and q can never differ, trial division is hopeless past 32 bits
    const uint64_t maxBits = method == "naive" ? 32 : maxPrimeBits;
    if (bits < 3 || bits > maxBits) {
        std::cerr << "bits must be between 3 and " << maxBits << " for " << method << "\n";
        return 1;
    }
    if (s < 1 || repetitions < 1) {
        std::cerr << "s and repetitions must be at least 1\n";
        return 1;
    }

    SuperDuper = LCG{1ull << 32, 69069, 1, seed};
    const PrimeMethod primeMethod = method == "naive" ? PrimeMethod::naive : PrimeMethod::millerRabin;

    // n is usually 2 * bits or 2 * bits - 1 bits long, so the real average is reported for the x axis.
    // Summing the bits also keeps the compiler from optimizing the generation away.
    uint64_t modulusBitsSum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (uint64_t i = 0; i < repetitions; i++) {
        const KeyPair keys = generateKeys(static_cast<uint32_t>(bits), primeMethod, s);
        modulusBitsSum += msb(keys.publicKey.modulus) + 1;
    }
    const auto end = std::chrono::steady_clock::now();

    const double totalMicroseconds = std::chrono::duration<double, std::micro>(end - start).count();
    std::cout << totalMicroseconds / static_cast<double>(repetitions) << " "
              << static_cast<double>(modulusBitsSum) / static_cast<double>(repetitions) << "\n";
    return 0;
}

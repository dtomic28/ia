#include <charconv>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include "lcg.hpp"

static bool parseU64(const char* text, uint64_t& out) {
    const char* end = text + std::strlen(text);
    auto [ptr, ec] = std::from_chars(text, end, out);
    return ec == std::errc{} && ptr == end;
}

int main(int argc, char* argv[]) {
    if (argc != 8) {
        std::cerr << "Usage: " << argv[0] << " <m> <a> <b> <seed> <low> <high> <count>\n"
                  << "Prints <count> numbers from LCG(m, a, b, seed) in [low, high], separated by spaces.\n";
        return 1;
    }

    const char* names[] = {"m", "a", "b", "seed", "low", "high", "count"};
    uint64_t args[7];
    for (int i = 0; i < 7; i++) {
        if (!parseU64(argv[i + 1], args[i])) {
            std::cerr << "Invalid value for " << names[i] << ": '" << argv[i + 1] << "'\n";
            return 1;
        }
    }

    const auto [m, a, b, seed, low, high, count] = args;
    if (m == 0) {
        std::cerr << "m must be greater than 0\n";
        return 1;
    }
    if (low > high) {
        std::cerr << "low must be less than or equal to high\n";
        return 1;
    }

    LCG lcg{m, a, b, seed};
    std::string output;
    output.reserve(count * 8);
    for (uint64_t i = 0; i < count; i++) {
        if (i > 0)
            output += ' ';
        output += std::to_string(lcg.getRandom(low, high));
    }
    output += '\n';

    std::cout << output;
    return 0;
}

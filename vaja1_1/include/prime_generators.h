#pragma once

#include <cmath>
#include <cstdint>

#include "lcg.h"
#include "bin_helpers.h"

inline LCG SuperDuper{1ull << 32, 69069, 1, 0};

inline constexpr uint64_t defaultMillerRabinRounds = 20;

inline bool isPrimeNaive(uint64_t p) {
    if (p < 2)
        return false;
    if (p % 2 == 0)
        return p == 2;

    uint64_t j = 3;
    while (j <= std::sqrt(p)) {
        if (p % j == 0)
            return false;
        j += 2;
    }
    return true;
}

inline uint64_t generatePrimeNaive(uint8_t numOfBits) {
    uint64_t p = SuperDuper.getRandom(minNBit(numOfBits), maxNBit(numOfBits));
    if (p % 2 == 0)
        p++;

    while (!isPrimeNaive(p))
        p += 2;
    return p;
}

inline uint64_t modularExponentiation(uint64_t base, uint64_t exponent, uint64_t modulus) {
    uint64_t result = 1;
    for (int bit = 63; bit >= 0; bit--) {
        result = (result * result) % modulus;
        if ((exponent >> bit) & 1)
            result = (result * base) % modulus;
    }
    return result;
}

enum class MRResult {
    prime,
    composite,
    probablyPrime
};

/**
 * @details Because im having a hard time with these single char variable names im going to write this comment.
 * @param p - Potential number which we test if it's a prime number
 * @param s - number of iterations
 * @return
 */
inline MRResult millerRabinTest(uint64_t p, uint64_t s = defaultMillerRabinRounds) {
    if (p <= 3)
        return MRResult::prime;

    if (p % 2 == 0)
        return MRResult::composite;

    uint64_t d = p - 1;
    uint64_t k = 0;
    while (d % 2 == 0) {
        d /= 2;
        k++;
    }

    for (uint64_t j = 1; j <= s; j++) {
        uint64_t a = SuperDuper.getRandom(2, p - 2);
        uint64_t x = modularExponentiation(a, d, p);

        if (x == 1)
            continue;

        for (uint64_t i = 0; i < k; i++) {
            if (x == p-1)
                break;
            x = (x * x) % p;
        }

        if (x != p-1)
            return MRResult::composite;
    }

    return MRResult::probablyPrime;
}

inline uint64_t generatePrimeMillerRabin(uint8_t numOfBits, uint64_t s = defaultMillerRabinRounds) {
    uint64_t p = SuperDuper.getRandom(minNBit(numOfBits), maxNBit(numOfBits));
    if (p % 2 == 0)
        p++;

    while (millerRabinTest(p, s) == MRResult::composite)
        p += 2;
    return p;
}

#pragma once

#include <cstdint>

#include <boost/multiprecision/cpp_int.hpp>

#include "lcg.h"
#include "bin_helpers.h"

inline LCG SuperDuper{1ull << 32, 69069, 1, 0};

inline constexpr uint64_t defaultMillerRabinRounds = 20;
inline constexpr unsigned maxPrimeBits = 2048;

/**
 * @details Super-Duper only gives 32 bits per step, so enough steps are glued together to cover the range.
 * For ranges up to 32 bits this uses a single step, same as LCG::getRandom.
 */
inline uint2048_t getRandomBig(LCG& lcg, const uint2048_t& low, const uint2048_t& high) {
    const uint2048_t range = high - low + 1;    // wraps to 0 when the range covers all 2048 bits
    const unsigned bits = range == 0 ? 2048 : msb(range) + 1;

    uint2048_t r = 0;
    for (unsigned i = 0; i < bits; i += 32)
        r = (r << 32) | (lcg.calculateNextR() & 0xFFFFFFFF);

    return range == 0 ? low + r : low + r % range;
}

inline bool isPrimeNaive(const uint2048_t& p) {
    if (p < 2)
        return false;
    if (p % 2 == 0)
        return p == 2;

    const uint2048_t limit = sqrt(p);
    uint2048_t j = 3;
    while (j <= limit) {
        if (p % j == 0)
            return false;
        j += 2;
    }
    return true;
}

inline uint2048_t generatePrimeNaive(unsigned numOfBits) {
    uint2048_t p = getRandomBig(SuperDuper, minNBit(numOfBits), maxNBit(numOfBits));
    if (p % 2 == 0)
        p++;

    while (!isPrimeNaive(p))
        p += 2;
    return p;
}

// The product of two 2048-bit numbers needs up to 4096 bits before the modulo
inline uint2048_t multiplyModulo(const uint2048_t& a, const uint2048_t& b, const uint2048_t& modulus) {
    return static_cast<uint2048_t>(uint4096_t(a) * uint4096_t(b) % uint4096_t(modulus));
}

inline uint2048_t modularExponentiation(const uint2048_t& base, const uint2048_t& exponent, const uint2048_t& modulus) {
    uint2048_t result = 1;
    if (exponent == 0)
        return result;

    for (int bit = static_cast<int>(msb(exponent)); bit >= 0; bit--) {
        result = multiplyModulo(result, result, modulus);
        if (bit_test(exponent, bit))
            result = multiplyModulo(result, base, modulus);
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
inline MRResult millerRabinTest(const uint2048_t& p, uint64_t s = defaultMillerRabinRounds) {
    if (p <= 3)
        return MRResult::prime;

    if (p % 2 == 0)
        return MRResult::composite;

    uint2048_t d = p - 1;
    uint64_t k = 0;
    while (d % 2 == 0) {
        d /= 2;
        k++;
    }

    for (uint64_t j = 1; j <= s; j++) {
        uint2048_t a = getRandomBig(SuperDuper, 2, p - 2);
        uint2048_t x = modularExponentiation(a, d, p);

        if (x == 1)
            continue;

        for (uint64_t i = 0; i < k; i++) {
            if (x == p-1)
                break;
            x = multiplyModulo(x, x, p);
        }

        if (x != p-1)
            return MRResult::composite;
    }

    return MRResult::probablyPrime;
}

inline uint2048_t generatePrimeMillerRabin(unsigned numOfBits, uint64_t s = defaultMillerRabinRounds) {
    uint2048_t p = getRandomBig(SuperDuper, minNBit(numOfBits), maxNBit(numOfBits));
    if (p % 2 == 0)
        p++;

    while (millerRabinTest(p, s) == MRResult::composite)
        p += 2;
    return p;
}

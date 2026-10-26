#pragma once

#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "prime_generators.h"

// Finds gcd and coefficients such that first * coefFirst + second * coefSecond = gcd.
// Coefficients can be negative, so they use the signed cpp_int.
inline void extendedEuclid(const cpp_int& first, const cpp_int& second, cpp_int& gcd, cpp_int& coefFirst, cpp_int& coefSecond)
{
    if (second == 0) {
        gcd = first;
        coefFirst = 1;
        coefSecond = 0;
        return;
    }

    cpp_int innerGcd, innerCoefFirst, innerCoefSecond;
    extendedEuclid(second, first % second, innerGcd, innerCoefFirst, innerCoefSecond);

    gcd = innerGcd;
    coefFirst = innerCoefSecond;
    coefSecond = innerCoefFirst - (first / second) * innerCoefSecond;
}

// Mathematical mod: result is always in 0..modulus-1, unlike % which keeps the sign of value
inline cpp_int positiveMod(const cpp_int& value, const cpp_int& modulus)
{
    cpp_int result = value % modulus;
    if (result < 0)
        result += modulus;
    return result;
}

// Solves factor * x = target (mod modulus) and returns the first solution, nothing if there is none.
// For RSA gcd(e, phi) = 1, so this first solution is the only one.
inline std::optional<cpp_int> modularLinearEquationSolver(const cpp_int& factor, const cpp_int& target, const cpp_int& modulus)
{
    cpp_int gcd, coefFactor, coefModulus;
    extendedEuclid(factor, modulus, gcd, coefFactor, coefModulus);

    if (target % gcd != 0)
        return std::nullopt;

    return positiveMod(coefFactor * (target / gcd), modulus);
}

// Writes a key as two lines: exponent, then modulus
inline void saveKey(const std::string& path, const cpp_int& exponent, const cpp_int& modulus)
{
    std::ofstream file(path);
    if (!file) {
        std::cerr << "Could not open " << path << "\n";
        return;
    }
    file << exponent << "\n" << modulus << "\n";
}

inline void generateKeys(std::uint32_t noOfBits)
{
    auto p  = generatePrimeMillerRabin(noOfBits);
    cpp_int q;
    do
    {
        q = generatePrimeMillerRabin(noOfBits);
    } while (q == p);

    cpp_int n = p * q;
    auto phi = (p-1) * (q-1);

    cpp_int gcd, ignore1, ignore2, e;

    while (true)
    {
        e = getRandomBig(SuperDuper,3, maxNBit(noOfBits));
        e |= 1; // Always set to odd number
        extendedEuclid(e, phi, gcd, ignore1, ignore2);
        if (gcd == 1)
            break;
    }

    const cpp_int d{modularLinearEquationSolver(e, 1, phi).value()};

    saveKey("pubkey.txt", e, n);
    saveKey("privkey.txt", d, n);
}

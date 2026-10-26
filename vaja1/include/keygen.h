#pragma once

#include <optional>

#include "io_utils.h"
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

// Which prime generator to use for p and q
enum class PrimeMethod {
    naive,
    millerRabin
};

struct KeyPair
{
    Key publicKey;
    Key privateKey;

    // Secret values the keys were made from, kept only so they can be shown and checked
    cpp_int p;
    cpp_int q;
    cpp_int phi;
};

inline cpp_int generatePrime(std::uint32_t noOfBits, PrimeMethod method, uint64_t s)
{
    return method == PrimeMethod::naive ? generatePrimeNaive(noOfBits) : generatePrimeMillerRabin(noOfBits, s);
}

// noOfBits must be at least 3, with 2 bits the only odd prime is 3 so p and q can never differ
inline KeyPair generateKeys(std::uint32_t noOfBits, PrimeMethod method = PrimeMethod::millerRabin,
                            uint64_t s = defaultMillerRabinRounds)
{
    auto p  = generatePrime(noOfBits, method, s);
    cpp_int q;
    do
    {
        q = generatePrime(noOfBits, method, s);
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

    return {{e, n}, {d, n}, p, q, phi};
}

inline KeyPair generateAndSaveKeys(std::uint32_t noOfBits, PrimeMethod method = PrimeMethod::millerRabin,
                                   uint64_t s = defaultMillerRabinRounds,
                                   const std::string& publicKeyPath = "pubkey.txt",
                                   const std::string& privateKeyPath = "privkey.txt")
{
    const KeyPair keys = generateKeys(noOfBits, method, s);
    saveKey(publicKeyPath, keys.publicKey);
    saveKey(privateKeyPath, keys.privateKey);
    return keys;
}

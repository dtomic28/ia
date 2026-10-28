#pragma once

#include <cstdint>

class LCG {
public:
    LCG(uint64_t m, uint64_t a, uint64_t b, uint64_t r) : m(m), a(a), b(b), r(r)
    {
    };

    uint64_t calculateNextR() {
        return (r = ((a * r + b) % m));
    }

    uint64_t getRandom(uint64_t low, uint64_t high) {
        return low + calculateNextR() % (high - low + 1);
    }

private:
    uint64_t m{0}; //Modulus
    uint64_t a{0}; //Multiplier
    uint64_t b{0}; //Incrament
    uint64_t r{0}; //Seed/State
};

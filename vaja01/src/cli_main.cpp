#include <iostream>

#include "lcg.hpp"

static LCG SuperDuper{1ull << 32, 69069, 1, 0};

int main() {
    for (int i = 0; i < 10; i++)
        std::cout << "Super duper value: " << SuperDuper.getRandom(1, 100) << "\n";
}

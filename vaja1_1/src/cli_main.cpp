#include <iostream>

#include "prime_generators.h"

int main() {
    for (int i = 0; i < 10; i++)
        std::cout << "Super duper value: " << SuperDuper.getRandom(1, 100) << "\n";
}

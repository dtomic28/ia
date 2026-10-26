#include "keygen.h"

int main() {
   auto keys = generateAndSaveKeys(2048);

   for (int i = 0; i<100; i++)
   {
       auto M = getRandomBig(SuperDuper, 0, keys.privateKey.modulus);

       auto c = modularExponentiation(M, keys.publicKey.exponent, keys.publicKey.modulus);
       auto d = modularExponentiation(c, keys.privateKey.exponent, keys.privateKey.modulus);

       if (M != d)
           std::cout << "You fucked up" << std::endl;
   }

}

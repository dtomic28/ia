#pragma once

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#include "io_utils.h"
#include "prime_generators.h"

// Bits stored at the start of an encrypted file for the original file size in bytes
inline constexpr unsigned fileSizeBits = 64;

/**
 * @details The file is read in blocks of floor(log2 n) bits, so every block is smaller than n.
 * Each encrypted block can be anything up to n - 1, so it is written with one bit more.
 * The original size is stored first, so decryption knows where the padding of the last block starts.
 */
inline bool encryptFile(const std::string& inputPath, const std::string& outputPath, const Key& publicKey)
{
    std::ifstream input(inputPath, std::ios::binary | std::ios::ate);
    if (!input) {
        std::cerr << "Could not open " << inputPath << "\n";
        return false;
    }
    const auto fileSize = static_cast<uint64_t>(input.tellg());
    input.seekg(0);

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "Could not open " << outputPath << "\n";
        return false;
    }

    const unsigned blockBits = msb(publicKey.modulus);    // floor(log2 n)
    BitReader reader(input);
    BitWriter writer(output);

    writer.write(fileSize, fileSizeBits);

    const uint64_t totalBits = fileSize * 8;
    for (uint64_t done = 0; done < totalBits; done += blockBits) {
        // The last block can be shorter, it gets padded with zeros at the end
        const auto bits = static_cast<unsigned>(std::min<uint64_t>(blockBits, totalBits - done));
        const cpp_int message = reader.read(bits) << (blockBits - bits);

        writer.write(modularExponentiation(message, publicKey.exponent, publicKey.modulus), blockBits + 1);
    }
    writer.flush();
    return true;
}

inline bool decryptFile(const std::string& inputPath, const std::string& outputPath, const Key& privateKey)
{
    std::ifstream input(inputPath, std::ios::binary);
    if (!input) {
        std::cerr << "Could not open " << inputPath << "\n";
        return false;
    }

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "Could not open " << outputPath << "\n";
        return false;
    }

    const unsigned blockBits = msb(privateKey.modulus);    // floor(log2 n)
    BitReader reader(input);
    BitWriter writer(output);

    const auto fileSize = static_cast<uint64_t>(reader.read(fileSizeBits));
    if (!reader.good()) {
        std::cerr << inputPath << " is not an encrypted file\n";
        return false;
    }

    const uint64_t totalBits = fileSize * 8;
    for (uint64_t done = 0; done < totalBits; done += blockBits) {
        const cpp_int cipher = reader.read(blockBits + 1);
        if (!reader.good()) {
            std::cerr << inputPath << " is shorter than expected\n";
            return false;
        }
        const cpp_int message = modularExponentiation(cipher, privateKey.exponent, privateKey.modulus);

        // Drops the zero padding that was added to the last block
        const auto bits = static_cast<unsigned>(std::min<uint64_t>(blockBits, totalBits - done));
        writer.write(message >> (blockBits - bits), bits);
    }
    writer.flush();
    return true;
}

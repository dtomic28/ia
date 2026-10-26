#pragma once

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "bin_helpers.h"

// Reads a stream as a sequence of bits, most significant bit of each byte first
class BitReader
{
public:
    explicit BitReader(std::istream& in) : in(in) {}

    // Reads count bits into a number, the first bit read becomes the most significant one
    cpp_int read(unsigned count)
    {
        cpp_int value = 0;
        while (count > 0) {
            if (bitsLeft == 0) {
                const int next = in.get();
                if (next == EOF) {
                    reachedEnd = true;
                    return value << count;
                }
                current = static_cast<uint8_t>(next);
                bitsLeft = 8;
            }

            // Takes as many bits from the current byte as possible at once
            const unsigned take = std::min(count, bitsLeft);
            const unsigned chunk = (current >> (bitsLeft - take)) & ((1u << take) - 1);
            value = (value << take) | chunk;
            bitsLeft -= take;
            count -= take;
        }
        return value;
    }

    // False once a read ran past the end of the stream
    bool good() const { return !reachedEnd; }

private:
    std::istream& in;
    uint8_t current{0};
    unsigned bitsLeft{0};
    bool reachedEnd{false};
};

// Writes bits to a stream, filling each byte from its most significant bit
class BitWriter
{
public:
    explicit BitWriter(std::ostream& out) : out(out) {}

    // Writes the lowest count bits of value, most significant first
    void write(const cpp_int& value, unsigned count)
    {
        while (count > 0) {
            const unsigned take = std::min(count, 8 - bitsUsed);
            const auto chunk = static_cast<unsigned>((value >> (count - take)) & ((1u << take) - 1));
            current = static_cast<uint8_t>((current << take) | chunk);
            bitsUsed += take;
            count -= take;

            if (bitsUsed == 8) {
                out.put(static_cast<char>(current));
                current = 0;
                bitsUsed = 0;
            }
        }
    }

    // Pads the last unfinished byte with zeros and writes it
    void flush()
    {
        if (bitsUsed == 0)
            return;
        out.put(static_cast<char>(current << (8 - bitsUsed)));
        current = 0;
        bitsUsed = 0;
    }

private:
    std::ostream& out;
    uint8_t current{0};
    unsigned bitsUsed{0};
};

// Public key is (e, n), private key is (d, n)
struct Key
{
    cpp_int exponent;
    cpp_int modulus;
};

// Writes a key as two lines: exponent, then modulus
inline void saveKey(const std::string& path, const Key& key)
{
    std::ofstream file(path);
    if (!file) {
        std::cerr << "Could not open " << path << "\n";
        return;
    }
    file << key.exponent << "\n" << key.modulus << "\n";
}

// Reads a key written by saveKey, nothing if the file is missing or malformed
inline std::optional<Key> loadKey(const std::string& path)
{
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Could not open " << path << "\n";
        return std::nullopt;
    }

    Key key;
    if (!(file >> key.exponent >> key.modulus)) {
        std::cerr << "Invalid key in " << path << "\n";
        return std::nullopt;
    }
    return key;
}

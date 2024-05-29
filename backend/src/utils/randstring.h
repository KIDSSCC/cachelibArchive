#pragma once

#include <random>
#include <vector>

// Assume CHAR_SYMBOLS is a globally defined constant array of char
const char CHAR_SYMBOLS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
const int CHAR_SYMBOLS_LENGTH = sizeof(CHAR_SYMBOLS) - 1; // Subtract 1 to ignore the null terminator

// Fast masks for bit manipulation
const std::vector<int> FAST_MASKS = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};

inline std::string randomFastString(std::mt19937& rng, size_t length) {
    std::string result(length, ' '); // Initialize the string with the desired length
    int numRounds = length / FAST_MASKS.size();
    size_t i = 0;

    for (int ctr = 0; ctr < numRounds; ++ctr) {
        int rand = rng() % 10000; // Simulating rng.nextInt(10000) from Java
        for (int mask : FAST_MASKS) {
            result[i++] = CHAR_SYMBOLS[(rand | mask) % CHAR_SYMBOLS_LENGTH];
        }
    }

    // Generate characters one by one for the remaining part
    for (; i < length; ++i) {
        result[i] = CHAR_SYMBOLS[rng() % CHAR_SYMBOLS_LENGTH];
    }

    return result;
}

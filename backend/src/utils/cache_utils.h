#pragma once

#include <string>
#include <vector>
#include <numeric>

inline std::string pack_values(std::vector<std::string> values) {
    return std::accumulate(values.begin(), values.end(), std::string(),
        [](const std::string& a, const std::string& b) {
            return a + b;
        });
}

inline std::vector<std::string> unpack_values(const std::string& packed, int max_field_size) {
    std::vector<std::string> values;
    size_t start = 0;
    while (start < packed.length()) {
        values.push_back(packed.substr(start, max_field_size));
        start += max_field_size;
    }
    return values;
}
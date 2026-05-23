#pragma once
#include <string>
#include <unordered_set>
#include <algorithm>
#include <cstdint>

struct AggState {
    __int128 sum = 0;
    int64_t count = 0;
    bool has_minmax = false;
    int64_t min_val = 0;
    int64_t max_val = 0;
    std::string min_str;
    std::string max_str;
    std::unordered_set<std::string> distinct_set;
};

inline std::string int128_to_string(__int128 value) {
    if (value == 0) {
        return "0";
    }

    std::string result;
    const bool negative = value < 0;
    if (negative) {
        value = -value;
    }

    while (value > 0) {
        result.push_back('0' + static_cast<char>(value % 10));
        value /= 10;
    }

    if (negative) {
        result.push_back('-');
    }

    std::ranges::reverse(result);
    return result;
}

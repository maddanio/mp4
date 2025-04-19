#pragma once

#include <cstdlib>

namespace game_on
{
    inline constexpr uint64_t bitshift(uint64_t x, int8_t n)
    {
        return n > 0 ? (x << n) : (x >> (-n));
    }

    template<typename value_t> inline constexpr value_t from_bits(
        const char* data_,
        size_t first_bit,
        size_t num_bits
    )
    {
        // make data unsigned
        auto data = (const unsigned char*)data_;
        // skip useless leading bytes
        size_t first_byte = first_bit / 8;
        first_bit -= 8 * first_byte;
        data += first_byte;
        // calculate some other bit positions
        size_t last_bit = first_bit + num_bits - 1;
        size_t num_bytes = last_bit / 8 + 1;
        size_t trailing_bits = 7 - last_bit % 8;
        // compose result bits in uint64_t
        uint64_t result = 0;
        result |= bitshift(data[0] & (0xff >> first_bit), 8 * (num_bytes - 1) - trailing_bits);
        for (size_t i = 1; i < num_bytes; ++i)
            result |= bitshift(data[i], 8 * (num_bytes - 1 - i) - trailing_bits);
        // finally cast to result type
        return value_t(result);
    }
}

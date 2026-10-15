// Reconstructed from x64 FS15 1.4.2.0, function 0x14028f430.
// See recovered/decompiled/game-focus/ and analysis/rle8-reference.*.
// Domain: 1..8 value bits, stride >= value bits, bounded caller-owned output.
// Unlike the original unchecked routine, invalid input/capacity returns SIZE_MAX.
#include <cstddef>
#include <cstdint>
#include <limits>
#include "api.h"

static constexpr auto failure = (std::numeric_limits<std::size_t>::max)();

extern "C" FS15_CORE_API std::size_t fs15_decode_rle8(
    const std::uint8_t* input, std::size_t input_size,
    unsigned stride_bits, unsigned value_bits,
    std::uint8_t* output, std::size_t output_bytes) noexcept {
    if ((!input && input_size) || !output || !value_bits || value_bits > 8 ||
        stride_bits < value_bits || stride_bits > 32) return failure;
    std::size_t count = 0, cursor = 0;
    std::uint8_t previous = 0;
    bool have_previous_literal = false;
    const unsigned mask = (1u << value_bits) - 1;
    auto write = [&](std::uint8_t value) -> bool {
        if (value > mask || count > failure / stride_bits) return false;
        const auto bit_offset = count * stride_bits;
        if (bit_offset > failure - value_bits ||
            (bit_offset + value_bits - 1) / 8 >= output_bytes) return false;
        for (unsigned bit = 0; bit < value_bits; ++bit) {
            const auto position = bit_offset + bit;
            const auto bit_mask = static_cast<std::uint8_t>(1u << (position & 7));
            auto& byte = output[position >> 3];
            byte = static_cast<std::uint8_t>((byte & ~bit_mask) | (((value >> bit) & 1u) ? bit_mask : 0));
        }
        ++count;
        return true;
    };
    while (cursor < input_size) {
        const auto value = input[cursor++];
        if (!write(value)) return failure;
        if (have_previous_literal && previous == value) {
            if (cursor < input_size) {
                std::size_t repeats = 0;
                std::uint8_t extension;
                do {
                    extension = input[cursor++];
                    if (repeats > failure - extension) return failure;
                    repeats += extension;
                } while (extension == 255 && cursor < input_size);
                for (std::size_t i = 0; i < repeats; ++i) {
                    if (!write(value)) return failure;
                }
                have_previous_literal = false;
            }
        } else {
            have_previous_literal = true;
        }
        previous = value;
    }
    return count;
}

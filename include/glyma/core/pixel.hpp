#pragma once

#include <cstdint>

namespace glyma::core {

struct Pixel {
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};
    std::uint8_t a{0};

    constexpr Pixel() = default;

    constexpr Pixel(
        std::uint8_t red,
        std::uint8_t green,
        std::uint8_t blue,
        std::uint8_t alpha = 255
    ) : r(red), g(green), b(blue), a(alpha) {}

    constexpr bool operator==(const Pixel&) const = default;
};

} // namespace glyma::core
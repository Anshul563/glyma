#pragma once

#include <cstdint>
#include <vector>

#include "glyma/core/pixel.hpp"

namespace glyma::core {

class ImageBuffer {
public:
    ImageBuffer(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;

    [[nodiscard]] Pixel getPixel(
        std::uint32_t x,
        std::uint32_t y
    ) const;

    void setPixel(
        std::uint32_t x,
        std::uint32_t y,
        Pixel pixel
    );

    void clear(Pixel color = Pixel{});

    [[nodiscard]] const std::vector<Pixel>& pixels() const noexcept;

    // Mutable row-major pixel storage, tightly packed (width * height entries).
    // Exposed for bulk interop (for example decoding from an external image
    // library) where per-pixel setPixel calls would be wasteful. The size is
    // always width() * height().
    [[nodiscard]] Pixel* data() noexcept;
    [[nodiscard]] const Pixel* data() const noexcept;

private:
    std::uint32_t width_;
    std::uint32_t height_;
    std::vector<Pixel> pixels_;

    [[nodiscard]] std::size_t indexOf(
        std::uint32_t x,
        std::uint32_t y
    ) const;
};

} // namespace glyma::core
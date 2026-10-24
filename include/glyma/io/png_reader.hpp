#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "glyma/core/image_buffer.hpp"

namespace glyma::core {

class PngReader {
public:
    // Maximum pixel count allowed to avoid excessive allocations
    static constexpr std::uint64_t maxPixelCount = 256ULL * 1024ULL * 1024ULL;

    // Decode a PNG file to RGBA8 ImageBuffer. Throws std::runtime_error or
    // std::invalid_argument on failure.
    [[nodiscard]] static ImageBuffer read(const std::string& path);
};

} // namespace glyma::core

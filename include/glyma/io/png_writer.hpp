#pragma once

#include <string>

#include "glyma/core/image_buffer.hpp"

namespace glyma::core {

class PngWriter {
public:
    // Encode an ImageBuffer to RGBA8 PNG. Throws on failure. Does not modify source.
    static void write(const ImageBuffer& image, const std::string& path);
};

} // namespace glyma::core

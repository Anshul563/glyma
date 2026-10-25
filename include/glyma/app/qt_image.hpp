#pragma once

#include <QImage>

#include "glyma/core/image_buffer.hpp"

namespace glyma::app {

// Convert an engine RGBA8 buffer into an independent QImage (RGBA8888).
// The returned image owns a deep copy of the pixel data and remains valid
// after the source buffer is destroyed.
[[nodiscard]] QImage toQImage(const core::ImageBuffer& buffer);

// Convert a QImage into an engine RGBA8 buffer (straight alpha). The image is
// converted to RGBA8888 first if needed. Throws std::invalid_argument for a
// null or empty image (the engine has no zero-sized buffer).
[[nodiscard]] core::ImageBuffer toImageBuffer(const QImage& image);

} // namespace glyma::app

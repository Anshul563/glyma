#pragma once

#include <QImage>

#include "glyma/core/image_buffer.hpp"

namespace glyma::app {

// Convert an engine RGBA8 buffer into an independent QImage (RGBA8888).
// The returned image owns a deep copy of the pixel data and remains valid
// after the source buffer is destroyed.
[[nodiscard]] QImage toQImage(const core::ImageBuffer& buffer);

} // namespace glyma::app

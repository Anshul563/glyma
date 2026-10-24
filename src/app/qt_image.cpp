#include "glyma/app/qt_image.hpp"

#include <cstring>

namespace glyma::app {

QImage toQImage(const core::ImageBuffer& buffer) {
    static_assert(sizeof(core::Pixel) == 4, "Pixel must be tightly packed RGBA8");

    if (buffer.width() == 0 || buffer.height() == 0) {
        return {};
    }

    QImage image(
        static_cast<int>(buffer.width()),
        static_cast<int>(buffer.height()),
        QImage::Format_RGBA8888
    );
    if (image.isNull()) {
        return {};
    }

    const auto& pixels = buffer.pixels();
    const std::size_t rowBytes = static_cast<std::size_t>(buffer.width()) * 4;
    const auto* source = reinterpret_cast<const unsigned char*>(pixels.data());

    // Copy row by row so Qt's scanline padding (bytesPerLine) is respected.
    for (std::uint32_t y = 0; y < buffer.height(); ++y) {
        std::memcpy(
            image.scanLine(static_cast<int>(y)),
            source + static_cast<std::size_t>(y) * rowBytes,
            rowBytes
        );
    }
    return image;
}

} // namespace glyma::app

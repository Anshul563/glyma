#include "glyma/app/qt_image.hpp"

#include <cstring>
#include <stdexcept>

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

core::ImageBuffer toImageBuffer(const QImage& image) {
    static_assert(sizeof(core::Pixel) == 4, "Pixel must be tightly packed RGBA8");

    if (image.isNull() || image.width() <= 0 || image.height() <= 0) {
        throw std::invalid_argument("Cannot convert an empty image.");
    }

    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.isNull()) {
        throw std::invalid_argument("Failed to convert the image to RGBA8888.");
    }

    core::ImageBuffer buffer(
        static_cast<std::uint32_t>(rgba.width()),
        static_cast<std::uint32_t>(rgba.height())
    );
    const std::size_t rowBytes = static_cast<std::size_t>(rgba.width()) * 4;
    auto* destination = reinterpret_cast<unsigned char*>(buffer.data());

    // Copy row by row so Qt's scanline padding (bytesPerLine) is respected.
    for (int y = 0; y < rgba.height(); ++y) {
        std::memcpy(
            destination + static_cast<std::size_t>(y) * rowBytes,
            rgba.constScanLine(y),
            rowBytes
        );
    }
    return buffer;
}

} // namespace glyma::app

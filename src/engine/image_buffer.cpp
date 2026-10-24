#include "glyma/core/image_buffer.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace glyma::core {

ImageBuffer::ImageBuffer(
    std::uint32_t width,
    std::uint32_t height
) : width_(width), height_(height) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument(
            "Image dimensions must be greater than zero."
        );
    }

    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);

    if (w > pixels_.max_size() / h) {
        throw std::length_error("Image dimensions are too large.");
    }

    pixels_.resize(w * h, Pixel{});
}

std::uint32_t ImageBuffer::width() const noexcept {
    return width_;
}

std::uint32_t ImageBuffer::height() const noexcept {
    return height_;
}

std::size_t ImageBuffer::indexOf(
    std::uint32_t x,
    std::uint32_t y
) const {
    if (x >= width_ || y >= height_) {
        throw std::out_of_range(
            "Pixel coordinates are outside the image."
        );
    }

    return static_cast<std::size_t>(y) * width_ + x;
}

Pixel ImageBuffer::getPixel(
    std::uint32_t x,
    std::uint32_t y
) const {
    return pixels_[indexOf(x, y)];
}

void ImageBuffer::setPixel(
    std::uint32_t x,
    std::uint32_t y,
    Pixel pixel
) {
    pixels_[indexOf(x, y)] = pixel;
}

void ImageBuffer::clear(Pixel color) {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

const std::vector<Pixel>& ImageBuffer::pixels() const noexcept {
    return pixels_;
}

} // namespace glyma::core
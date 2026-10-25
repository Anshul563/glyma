#include "glyma/core/layer.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace glyma::core {
namespace {

std::atomic<LayerId> g_nextLayerId{1};

void validateName(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Layer name cannot be empty.");
    }

    const auto isWhitespace = [](unsigned char c) {
        return std::isspace(c) != 0;
    };

    if (std::all_of(name.begin(), name.end(), isWhitespace)) {
        throw std::invalid_argument("Layer name cannot be whitespace-only.");
    }
}

void validateOpacity(float opacity) {
    if (!std::isfinite(opacity) || opacity < 0.0F || opacity > 1.0F) {
        throw std::invalid_argument(
            "Layer opacity must be finite and between 0.0 and 1.0."
        );
    }
}

} // namespace

Layer::Layer(std::string name, std::uint32_t width, std::uint32_t height)
    : Layer(generateId(), std::move(name), width, height) {}

Layer::Layer(
    LayerId id,
    std::string name,
    std::uint32_t width,
    std::uint32_t height
)
    : id_(id),
      name_(std::move(name)),
      buffer_(width, height) {
    validateName(name_);
    validateOpacity(opacity_);
}

Layer::Layer(std::string name, TextLayerData text)
    : Layer(generateId(), std::move(name), std::move(text)) {}

Layer::Layer(LayerId id, std::string name, TextLayerData text)
    : id_(id),
      name_(std::move(name)),
      buffer_(1, 1) {
    validateName(name_);
    validateOpacity(opacity_);
    text.validate();
    type_ = LayerType::Text;
    text_ = std::move(text);
    if (!text_->autoSize && text_->boxWidth > 0 && text_->boxHeight > 0) {
        buffer_ = ImageBuffer(text_->boxWidth, text_->boxHeight);
    }
}

Layer::Layer(std::string name, ShapeLayerData shape)
    : Layer(generateId(), std::move(name), std::move(shape)) {}

Layer::Layer(LayerId id, std::string name, ShapeLayerData shape)
    : id_(id),
      name_(std::move(name)),
      buffer_(1, 1) {
    validateName(name_);
    validateOpacity(opacity_);
    shape.validate();
    type_ = LayerType::Vector;
    shape_ = std::move(shape);
}

LayerId Layer::id() const noexcept {
    return id_;
}

const std::string& Layer::name() const noexcept {
    return name_;
}

void Layer::setName(std::string name) {
    validateName(name);
    name_ = std::move(name);
}

ImageBuffer& Layer::buffer() noexcept {
    return buffer_;
}

const ImageBuffer& Layer::buffer() const noexcept {
    return buffer_;
}

std::uint32_t Layer::width() const noexcept {
    return buffer_.width();
}

std::uint32_t Layer::height() const noexcept {
    return buffer_.height();
}

LayerType Layer::type() const noexcept {
    return type_;
}

bool Layer::isRaster() const noexcept {
    return type_ == LayerType::Raster;
}

bool Layer::isText() const noexcept {
    return type_ == LayerType::Text;
}

bool Layer::isVector() const noexcept {
    return type_ == LayerType::Vector;
}

const TextLayerData& Layer::text() const {
    if (!text_.has_value()) {
        throw std::logic_error("Layer is not a text layer.");
    }
    return *text_;
}

TextLayerData& Layer::text() {
    if (!text_.has_value()) {
        throw std::logic_error("Layer is not a text layer.");
    }
    return *text_;
}

void Layer::setTextData(TextLayerData data) {
    data.validate();
    type_ = LayerType::Text;
    text_ = std::move(data);
    shape_.reset();
}

const ShapeLayerData& Layer::shape() const {
    if (!shape_.has_value()) {
        throw std::logic_error("Layer is not a vector layer.");
    }
    return *shape_;
}

ShapeLayerData& Layer::shape() {
    if (!shape_.has_value()) {
        throw std::logic_error("Layer is not a vector layer.");
    }
    return *shape_;
}

void Layer::setShapeData(ShapeLayerData data) {
    data.validate();
    type_ = LayerType::Vector;
    shape_ = std::move(data);
    text_.reset();
}

bool Layer::rasterize() noexcept {
    if (type_ == LayerType::Raster) {
        return false;
    }
    type_ = LayerType::Raster;
    text_.reset();
    shape_.reset();
    return true;
}

bool Layer::visible() const noexcept {
    return visible_;
}

void Layer::setVisible(bool visible) noexcept {
    visible_ = visible;
}

float Layer::opacity() const noexcept {
    return opacity_;
}

void Layer::setOpacity(float opacity) {
    validateOpacity(opacity);
    opacity_ = opacity;
}

std::int32_t Layer::x() const noexcept {
    return x_;
}

std::int32_t Layer::y() const noexcept {
    return y_;
}

void Layer::setX(std::int32_t x) noexcept {
    x_ = x;
}

void Layer::setY(std::int32_t y) noexcept {
    y_ = y;
}

void Layer::setPosition(std::int32_t x, std::int32_t y) noexcept {
    x_ = x;
    y_ = y;
}

LayerId Layer::generateId() noexcept {
    return g_nextLayerId.fetch_add(1, std::memory_order_relaxed);
}

} // namespace glyma::core

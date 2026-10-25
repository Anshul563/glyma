#include "glyma/core/document.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <limits>
#include <stdexcept>

#include "glyma/core/compositor.hpp"

namespace glyma::core {
namespace {

std::atomic<Document::Id> g_nextDocumentId{1};

void validateName(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Document name cannot be empty.");
    }
    const auto isWhitespace = [](unsigned char c) {
        return std::isspace(c) != 0;
    };
    if (std::all_of(name.begin(), name.end(), isWhitespace)) {
        throw std::invalid_argument("Document name cannot be whitespace-only.");
    }
}

} // namespace

Document::Document(std::string name, std::uint32_t width, std::uint32_t height)
    : id_(generateId()),
      name_(std::move(name)),
      width_(width),
      height_(height) {
    validateName(name_);
    if (width_ == 0 || height_ == 0) {
        throw std::invalid_argument("Document dimensions must be greater than zero.");
    }
    // Check for potential overflow in pixel count
    const std::uint64_t px = static_cast<std::uint64_t>(width_) * static_cast<std::uint64_t>(height_);
    if (px > maxPixelCount) {
        throw std::invalid_argument("Document dimensions exceed resource limits.");
    }
    if (static_cast<std::uint64_t>(width_) > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) / static_cast<std::uint64_t>(height_)) {
        throw std::length_error("Document dimensions are too large.");
    }
    selection_.setDocumentSize(width_, height_);
}

Document::Id Document::id() const noexcept {
    return id_;
}

const std::string& Document::name() const noexcept {
    return name_;
}

void Document::setName(std::string name) {
    validateName(name);
    name_ = std::move(name);
}

std::uint32_t Document::width() const noexcept {
    return width_;
}

std::uint32_t Document::height() const noexcept {
    return height_;
}

std::uint64_t Document::pixelCount() const noexcept {
    return static_cast<std::uint64_t>(width_) * static_cast<std::uint64_t>(height_);
}

void Document::setCanvasSize(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument(
            "Document dimensions must be greater than zero."
        );
    }
    const std::uint64_t pixels =
        static_cast<std::uint64_t>(width) *
        static_cast<std::uint64_t>(height);
    if (pixels > maxPixelCount) {
        throw std::invalid_argument("Document dimensions exceed resource limits.");
    }

    width_ = width;
    height_ = height;
    selection_.setDocumentSize(width_, height_);
}

bool Document::hasSolidBackground() const noexcept {
    return !transparentBackground_;
}

Pixel Document::backgroundColor() const noexcept {
    return backgroundColor_;
}

void Document::setSolidBackground(Pixel color) {
    transparentBackground_ = false;
    backgroundColor_ = color;
}

void Document::setTransparentBackground() noexcept {
    transparentBackground_ = true;
    backgroundColor_ = Pixel{0, 0, 0, 0};
}

LayerStack& Document::layerStack() noexcept {
    return layerStack_;
}

const LayerStack& Document::layerStack() const noexcept {
    return layerStack_;
}

std::size_t Document::layerCount() const noexcept {
    return layerStack_.size();
}

const core::Selection& Document::selection() const noexcept {
    return selection_;
}

void Document::setSelection(const core::Selection& selection) noexcept {
    selection_ = selection;
    selection_.setDocumentSize(width_, height_);
}

LayerId Document::addCanvasLayer(std::string name) {
    Layer layer(std::move(name), width_, height_);
    return layerStack_.addLayer(std::move(layer));
}

LayerId Document::addLayer(Layer layer) {
    return layerStack_.addLayer(std::move(layer));
}

bool Document::removeLayer(LayerId id) noexcept {
    return layerStack_.removeLayer(id);
}

Layer* Document::findLayer(LayerId id) noexcept {
    return layerStack_.findLayer(id);
}

const Layer* Document::findLayer(LayerId id) const noexcept {
    return layerStack_.findLayer(id);
}

bool Document::moveLayerUp(LayerId id) noexcept {
    return layerStack_.moveLayerUp(id);
}

bool Document::moveLayerDown(LayerId id) noexcept {
    return layerStack_.moveLayerDown(id);
}

ImageBuffer Document::render() const {
    ImageBuffer canvas(width_, height_);
    if (transparentBackground_) {
        canvas.clear(Pixel{0, 0, 0, 0});
    } else {
        canvas.clear(backgroundColor_);
    }

    // Composite layers bottom-to-top
    Compositor::composite(layerStack_, canvas);
    return canvas;
}

Document::Id Document::generateId() noexcept {
    return g_nextDocumentId.fetch_add(1, std::memory_order_relaxed);
}

} // namespace glyma::core

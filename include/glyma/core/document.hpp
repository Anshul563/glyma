#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/layer_stack.hpp"
#include "glyma/core/pixel.hpp"
#include "glyma/core/selection.hpp"

namespace glyma::core {

// A document represents a single editable image project.
// It owns a canvas (fixed dimensions) and a LayerStack.
// Rendering composites the configured background first, then layers bottom-to-top.
class Document {
public:
    using Id = std::uint64_t;

    static constexpr Id invalidId = 0;
    // Conservative limit to avoid excessive allocations; can be raised later.
    static constexpr std::uint64_t maxPixelCount = 64ULL * 1024ULL * 1024ULL;

    // Transparent background by default (Pixel with alpha 0).
    Document(std::string name, std::uint32_t width, std::uint32_t height);

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
    Document(Document&&) = default;
    Document& operator=(Document&&) = default;
    ~Document() = default;

    [[nodiscard]] Id id() const noexcept;

    [[nodiscard]] const std::string& name() const noexcept;
    // Throws std::invalid_argument for empty/whitespace-only names.
    void setName(std::string name);

    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;
    [[nodiscard]] std::uint64_t pixelCount() const noexcept;

    // Replace the canvas dimensions. Layer buffers are not resampled or
    // repositioned; callers that need those effects must use the transform
    // helpers in transform.hpp. Selection geometry is re-clipped to the new
    // bounds. Throws std::invalid_argument for zero or over-limit sizes.
    void setCanvasSize(std::uint32_t width, std::uint32_t height);

    [[nodiscard]] bool hasSolidBackground() const noexcept;
    [[nodiscard]] Pixel backgroundColor() const noexcept;
    // Set a solid background color (alpha is respected by renderer).
    void setSolidBackground(Pixel color);
    // Make background transparent.
    void setTransparentBackground() noexcept;

    // Layer stack access (non-owning view of structure; layers owned by stack)
    [[nodiscard]] LayerStack& layerStack() noexcept;
    [[nodiscard]] const LayerStack& layerStack() const noexcept;

    [[nodiscard]] std::size_t layerCount() const noexcept;

    // The active pixel selection for this document. Empty by default.
    [[nodiscard]] const core::Selection& selection() const noexcept;
    void setSelection(const core::Selection& selection) noexcept;

    // Add a new transparent layer sized to the canvas. Returns layer ID.
    LayerId addCanvasLayer(std::string name);
    // Add an existing layer (must have dimensions compatible? We don't require
    // match; compositor clips to canvas). Returns layer ID.
    LayerId addLayer(Layer layer);

    bool removeLayer(LayerId id) noexcept;
    [[nodiscard]] Layer* findLayer(LayerId id) noexcept;
    [[nodiscard]] const Layer* findLayer(LayerId id) const noexcept;

    bool moveLayerUp(LayerId id) noexcept;
    bool moveLayerDown(LayerId id) noexcept;

    // Render document to an ImageBuffer of canvas dimensions.
    // Background is composited first, then layers bottom-to-top.
    // Source layers are not mutated.
    [[nodiscard]] ImageBuffer render() const;

    [[nodiscard]] static Id generateId() noexcept;

private:
    Id id_;
    std::string name_;
    std::uint32_t width_;
    std::uint32_t height_;
    bool transparentBackground_{true};
    Pixel backgroundColor_{0, 0, 0, 0};
    LayerStack layerStack_;
    core::Selection selection_;
};

} // namespace glyma::core

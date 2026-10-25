#pragma once

#include <cstdint>

#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/text_layer.hpp"

namespace glyma::app {

// Result of rasterizing a text layer's editable payload.
struct TextRenderResult {
    core::ImageBuffer image;
    // Measured box bounds. For auto-sized text these are the natural bounds;
    // for fixed boxes they echo the requested box size.
    std::uint32_t boxWidth{0};
    std::uint32_t boxHeight{0};
};

// Rasterizes Qt-free text metadata (TextLayerData) into an engine ImageBuffer
// using Qt's font engine. Kept out of the core so the engine stays
// Qt-independent. Layout honours line breaks, word wrap, alignment, line and
// letter spacing, and the requested box; text layers use the produced image as
// a regenerable render cache.
class TextRenderer {
public:
    // Render `data` to a new buffer. Throws std::invalid_argument for invalid
    // payloads and std::runtime_error when the image cannot be allocated.
    [[nodiscard]] static TextRenderResult render(const core::TextLayerData& data);

    // Render a text layer in place: replace its buffer with the rasterized
    // image and, for auto-sized text, record the measured box. Returns false
    // when the layer is not a text layer or rendering fails.
    static bool renderInto(core::Layer& layer);

    // Refresh the render cache of every text layer in `document`. Raster layers
    // are left untouched. Returns the number of layers refreshed.
    static std::size_t refreshDocument(core::Document& document);
};

} // namespace glyma::app

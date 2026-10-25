#pragma once

#include <cstddef>

#include <QPainterPath>

#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/shape_layer.hpp"

class QPainter;

namespace glyma::app {

// Result of rasterizing a vector layer's editable payload.
struct ShapeRenderResult {
    core::ImageBuffer image;
    // Visual bounds of the shape in document coordinates, including stroke
    // width and arrowheads. The buffer origin is floor(bounds.x), floor(bounds.y).
    core::ShapeBounds bounds;
};

// Rasterizes Qt-free shape metadata (ShapeLayerData) into an engine ImageBuffer
// using Qt's painting engine. Kept out of the core so the engine stays
// Qt-independent. Vector layers use the produced image as a regenerable render
// cache; the editable geometry remains authoritative.
class ShapeRenderer {
public:
    // Render `shape` to a new buffer. Throws std::invalid_argument for invalid
    // payloads and std::runtime_error when the image cannot be allocated.
    [[nodiscard]] static ShapeRenderResult render(const core::ShapeLayerData& shape);

    // Render a vector layer in place: replace its buffer with the rasterized
    // image and reposition it to the visual-bounds origin. Returns false when
    // the layer is not a vector layer or rendering fails.
    static bool renderInto(core::Layer& layer);

    // Refresh the render cache of every vector layer in `document`. Other layer
    // types are left untouched. Returns the number of layers refreshed.
    static std::size_t refreshDocument(core::Document& document);

    // Build the outline of a shape in document coordinates. Shared with the
    // canvas overlay so previews match the rendered layer exactly.
    [[nodiscard]] static QPainterPath painterPath(const core::ShapeLayerData& shape);

    // Paint fill, stroke, and arrowheads for `shape` in document coordinates.
    // The caller is responsible for any translation/rotation transform.
    static void paintShape(QPainter& painter, const core::ShapeLayerData& shape);
};

} // namespace glyma::app

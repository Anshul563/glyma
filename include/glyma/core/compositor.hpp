#pragma once

#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer_stack.hpp"

namespace glyma::core {

class Compositor {
public:
    // Composite source over destination (same dimensions).
    // Opacity must be between 0.0 (invisible) and 1.0 (fully enabled).
    static void composite(
        const ImageBuffer& source,
        ImageBuffer& destination,
        float opacity = 1.0F
    );

    // Render a LayerStack into destination (the canvas).
    //
    // Layers are processed in bottom-to-top order (index 0 to end),
    // applying source-over compositing for each visible, non-fully-transparent
    // layer. Each layer's opacity and (x,y) offset are respected. Pixels outside
    // the canvas are clipped; layers may extend beyond canvas boundaries.
    // Source layer buffers are not mutated.
    static void composite(
        const LayerStack& stack,
        ImageBuffer& destination
    );

    // Render a sub-rectangle of the canvas into destination. `originX`/`originY`
    // are the canvas coordinates of destination pixel (0, 0); destination is
    // therefore a window onto the full canvas. Layers are still composited
    // bottom-to-top with their own offsets and opacity, and are clipped to the
    // window. This lets callers repaint a dirty region without recompositing
    // the whole document.
    static void compositeRegion(
        const LayerStack& stack,
        ImageBuffer& destination,
        std::int32_t originX,
        std::int32_t originY
    );
};

} // namespace glyma::core

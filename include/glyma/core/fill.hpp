#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "glyma/core/brush.hpp"
#include "glyma/core/command.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/pixel.hpp"

namespace glyma::core {

class Selection;

// Parameters for the bucket fill tool. All values are unpremultiplied RGBA8;
// no color-space conversion is performed.
//
// Tolerance rule: a pixel matches the seed when, for every channel in {r, g,
// b, a}, the absolute difference between the pixel and the seed is at most
// `tolerance`. Alpha is compared like any other channel. Tolerance is an
// integer in [0, 255]; 0 requires an exact channel-wise match.
//
// Fill blending: each filled pixel is replaced by the straight-alpha
// source-over result of compositing `color` over the pixel's current value,
// with `opacity` scaling the fill's alpha. With opacity 1.0 and a fully opaque
// fill color, the pixel becomes exactly `color`. With opacity 0.0 the pixel is
// unchanged.
struct FillSettings {
    static constexpr int maxTolerance = 255;

    Pixel color{0, 0, 0, 255};
    float opacity{1.0F};
    int tolerance{0};

    [[nodiscard]] bool isValid() const noexcept;
    // Throws std::invalid_argument when a parameter is out of range or not
    // finite.
    void validate() const;
};

// Compute the contiguous fill region starting at the seed pixel using an
// iterative, stack-safe scanline flood fill (no recursion). Only pixels that
// match the seed within tolerance and lie within `selection` (every pixel when
// the selection is empty) are filled. Four-connected only; diagonal pixels are
// never treated as connected.
//
// Returns the sparse set of changed pixels with their before/after values,
// suitable for a FillCommand. Empty when the seed is out of bounds, the seed
// is outside an active selection, or no pixel would change. The buffer is
// mutated in place by this function; the caller is responsible for wrapping
// the result in a command so the change is undoable.
std::vector<PixelChange> fillRegion(
    ImageBuffer& buffer,
    int seedX,
    int seedY,
    const FillSettings& settings,
    const Selection& selection
);

// Reversible sparse pixel edit produced by fillRegion(). Redo sets every
// changed pixel to the exact value the fill produced; undo restores the values
// captured before the fill started.
class FillCommand : public Command {
public:
    FillCommand(LayerId layerId, std::vector<PixelChange> changes);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

    [[nodiscard]] std::size_t changeCount() const noexcept;

private:
    bool apply(Document& document, bool useAfter);

    LayerId layerId_;
    std::vector<PixelChange> changes_;
};

} // namespace glyma::core

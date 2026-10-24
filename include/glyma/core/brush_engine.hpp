#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <vector>

#include "glyma/core/brush.hpp"
#include "glyma/core/command.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"

namespace glyma::core {

// Reversible sparse pixel edit produced by BrushStroke::finish(). Redo sets
// every changed pixel to the exact value the stroke produced; undo restores
// the values captured before the stroke started.
class StrokeCommand : public Command {
public:
    StrokeCommand(LayerId layerId, std::vector<PixelChange> changes);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

    [[nodiscard]] std::size_t changeCount() const noexcept;

private:
    bool apply(Document& document, bool useAfter);

    LayerId layerId_;
    std::vector<PixelChange> changes_;
};

// Progressive CPU brush stroke over one layer buffer.
//
// Coverage model: each dab is a disc with a one-pixel linear antialiased edge.
// A pixel's coverage is 1 inside `radius * hardness - 0.5` pixels from the dab
// centre, 0 outside `radius + 0.5`, and linearly interpolated in between. A
// stroke keeps the maximum coverage reached for each pixel and recomputes its
// result from the pixel's original value, so overlapping dabs do not build up
// opacity beyond the configured brush opacity.
//
// Painting applies straight-alpha source-over blending. Erasing scales the
// destination alpha by (1 - opacity * coverage) and preserves RGB for partial
// erasure; fully erased pixels become Pixel{0, 0, 0, 0}.
//
// The caller owns the ImageBuffer and must keep it alive for the lifetime of
// the stroke. The finished command identifies the layer by ID when applied.
class BrushStroke {
public:
    BrushStroke(
        ImageBuffer& buffer,
        LayerId layerId,
        const BrushSettings& settings,
        BrushMode mode
    );
    ~BrushStroke();

    BrushStroke(const BrushStroke&) = delete;
    BrushStroke& operator=(const BrushStroke&) = delete;
    BrushStroke(BrushStroke&&) = delete;
    BrushStroke& operator=(BrushStroke&&) = delete;

    // Paint the initial dab. Returns true if any pixel changed.
    bool start(double x, double y);

    // Extend the stroke to a new position, interpolating dabs so there are no
    // gaps. Returns true if any pixel changed.
    bool lineTo(double x, double y);

    // Restore every pixel touched so far. Returns true if anything was restored.
    bool cancel();

    // Finish the stroke and hand ownership of its sparse changes to a command.
    // Returns nullptr when no pixel changed. The buffer is left painted.
    [[nodiscard]] std::unique_ptr<StrokeCommand> finish();

    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] std::size_t changeCount() const noexcept;
    [[nodiscard]] const BrushSettings& settings() const noexcept;

private:
    struct PendingChange {
        std::size_t index{0};
        Pixel before{};
        Pixel after{};
        double coverage{0.0};
    };

    [[nodiscard]] double coverageAt(
        double x,
        double y,
        std::uint32_t pixelX,
        std::uint32_t pixelY
    ) const noexcept;
    bool stamp(double x, double y);

    ImageBuffer* buffer_{nullptr};
    LayerId layerId_{Layer::invalidId};
    BrushSettings settings_;
    BrushMode mode_{BrushMode::Paint};
    bool active_{true};
    bool hasLast_{false};
    double lastX_{0.0};
    double lastY_{0.0};
    std::vector<PendingChange> changes_;
    std::unordered_map<std::size_t, std::size_t> changePositions_;
};

} // namespace glyma::core

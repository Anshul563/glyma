#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "glyma/core/brush.hpp"
#include "glyma/core/command.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"

namespace glyma::core {

// A reversible raster edit over a single layer.
//
// Two storage strategies are supported so that undo data stays compact:
//
//   * Sparse   - the list of changed pixels with their before/after values.
//                Cheap when only a few pixels change (e.g. a small selection).
//   * Snapshot - full before/after copies of the layer buffer. Cheaper than the
//                sparse form once more than half of the pixels change, and
//                bounded to two buffers regardless of image size.
//
// makePixelEditCommand() diffs the two buffer states and picks the smaller
// representation. The command identifies the layer by ID when applied, so it
// survives layer reordering; it fails cleanly if the layer is gone.
class PixelEditCommand : public Command {
public:
    PixelEditCommand(LayerId layerId, std::vector<PixelChange> changes);

    PixelEditCommand(LayerId layerId, ImageBuffer before, ImageBuffer after);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

    [[nodiscard]] bool usesSnapshot() const noexcept;
    // Number of pixels the command restores or reapplies.
    [[nodiscard]] std::size_t changeCount() const noexcept;

private:
    bool apply(Document& document, bool useAfter);

    LayerId layerId_;
    bool snapshot_{false};
    std::vector<PixelChange> changes_;
    std::unique_ptr<ImageBuffer> before_;
    std::unique_ptr<ImageBuffer> after_;
};

// Diff `before` and `after` (which must share dimensions) and build the most
// compact reversible command. Returns nullptr when the buffers are identical.
// Throws std::invalid_argument when the dimensions differ.
[[nodiscard]] std::unique_ptr<PixelEditCommand> makePixelEditCommand(
    LayerId layerId,
    const ImageBuffer& before,
    const ImageBuffer& after
);

} // namespace glyma::core

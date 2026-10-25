#include "glyma/core/pixel_edit.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "glyma/core/document.hpp"
#include "glyma/core/layer.hpp"

namespace glyma::core {

PixelEditCommand::PixelEditCommand(LayerId layerId, std::vector<PixelChange> changes)
    : layerId_(layerId), changes_(std::move(changes)) {}

PixelEditCommand::PixelEditCommand(
    LayerId layerId,
    ImageBuffer before,
    ImageBuffer after
) : layerId_(layerId),
    snapshot_(true),
    before_(std::make_unique<ImageBuffer>(std::move(before))),
    after_(std::make_unique<ImageBuffer>(std::move(after))) {}

std::string PixelEditCommand::name() const {
    return "Adjust";
}

bool PixelEditCommand::usesSnapshot() const noexcept {
    return snapshot_;
}

std::size_t PixelEditCommand::changeCount() const noexcept {
    if (snapshot_) {
        if (!before_) {
            return 0;
        }
        return static_cast<std::size_t>(before_->width()) * before_->height();
    }
    return changes_.size();
}

bool PixelEditCommand::execute(Document& document) {
    return apply(document, true);
}

bool PixelEditCommand::undo(Document& document) {
    return apply(document, false);
}

bool PixelEditCommand::apply(Document& document, bool useAfter) {
    Layer* layer = document.findLayer(layerId_);
    if (layer == nullptr) {
        return false;
    }

    ImageBuffer& buffer = layer->buffer();

    if (snapshot_) {
        if (!before_ || !after_) {
            return false;
        }
        buffer = useAfter ? *after_ : *before_;
        return true;
    }

    const std::uint32_t width = buffer.width();
    const std::uint32_t height = buffer.height();
    const std::size_t pixelCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    for (const PixelChange& change : changes_) {
        if (change.index >= pixelCount) {
            return false;
        }
    }

    for (const PixelChange& change : changes_) {
        const auto x = static_cast<std::uint32_t>(
            change.index % static_cast<std::size_t>(width)
        );
        const auto y = static_cast<std::uint32_t>(
            change.index / static_cast<std::size_t>(width)
        );
        buffer.setPixel(x, y, useAfter ? change.after : change.before);
    }
    return true;
}

std::unique_ptr<PixelEditCommand> makePixelEditCommand(
    LayerId layerId,
    const ImageBuffer& before,
    const ImageBuffer& after
) {
    if (before.width() != after.width() || before.height() != after.height()) {
        throw std::invalid_argument(
            "Pixel edit buffers must have matching dimensions."
        );
    }

    const std::vector<Pixel>& beforePixels = before.pixels();
    const std::vector<Pixel>& afterPixels = after.pixels();

    std::vector<PixelChange> changes;
    for (std::size_t index = 0; index < beforePixels.size(); ++index) {
        if (beforePixels[index] != afterPixels[index]) {
            changes.push_back(PixelChange{index, beforePixels[index], afterPixels[index]});
        }
    }

    if (changes.empty()) {
        return nullptr;
    }

    const std::size_t sparseBytes = changes.size() * sizeof(PixelChange);
    const std::size_t snapshotBytes = 2 * beforePixels.size() * sizeof(Pixel);

    if (sparseBytes <= snapshotBytes) {
        return std::make_unique<PixelEditCommand>(layerId, std::move(changes));
    }
    return std::make_unique<PixelEditCommand>(layerId, before, after);
}

} // namespace glyma::core

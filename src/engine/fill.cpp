#include "glyma/core/fill.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "glyma/core/document.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/selection.hpp"

namespace glyma::core {
namespace {

std::uint8_t toByte(double value) {
    return static_cast<std::uint8_t>(
        std::clamp(std::lround(value), 0L, 255L)
    );
}

// Per-channel absolute-difference match on unpremultiplied RGBA8. Alpha is
// compared like any other channel; no color-space conversion is performed.
bool channelsWithinTolerance(Pixel a, Pixel b, int tolerance) {
    return std::abs(static_cast<int>(a.r) - static_cast<int>(b.r)) <= tolerance &&
           std::abs(static_cast<int>(a.g) - static_cast<int>(b.g)) <= tolerance &&
           std::abs(static_cast<int>(a.b) - static_cast<int>(b.b)) <= tolerance &&
           std::abs(static_cast<int>(a.a) - static_cast<int>(b.a)) <= tolerance;
}

// Straight-alpha source-over of fillColor over dest, with fillOpacity scaling
// the fill alpha. Mirrors Compositor::sourceOver. With opacity 1.0 and a fully
// opaque fill color the destination is replaced exactly; with opacity 0.0 it is
// unchanged.
Pixel blendFill(Pixel fillColor, float opacity, Pixel dest) {
    const double sa =
        (static_cast<double>(fillColor.a) / 255.0) *
        std::clamp(static_cast<double>(opacity), 0.0, 1.0);
    const double da = static_cast<double>(dest.a) / 255.0;
    const double oa = sa + da * (1.0 - sa);
    if (oa <= 0.0) {
        return Pixel{0, 0, 0, 0};
    }
    const auto blendChannel = [&](std::uint8_t source, std::uint8_t dst) {
        return toByte(
            (static_cast<double>(source) * sa +
             static_cast<double>(dst) * da * (1.0 - sa)) / oa
        );
    };
    return Pixel{
        blendChannel(fillColor.r, dest.r),
        blendChannel(fillColor.g, dest.g),
        blendChannel(fillColor.b, dest.b),
        toByte(oa * 255.0)
    };
}

} // namespace

bool FillSettings::isValid() const noexcept {
    return std::isfinite(opacity) && opacity >= 0.0F && opacity <= 1.0F &&
           tolerance >= 0 && tolerance <= maxTolerance;
}

void FillSettings::validate() const {
    if (!isValid()) {
        throw std::invalid_argument(
            "Fill opacity must be in [0, 1] and tolerance in [0, 255], and "
            "all values finite."
        );
    }
}

std::vector<PixelChange> fillRegion(
    ImageBuffer& buffer,
    int seedX,
    int seedY,
    const FillSettings& settings,
    const Selection& selection
) {
    std::vector<PixelChange> changes;
    settings.validate();

    const std::uint32_t width = buffer.width();
    const std::uint32_t height = buffer.height();
    if (width == 0 || height == 0) {
        return changes;
    }
    if (seedX < 0 || seedY < 0 ||
        static_cast<std::uint32_t>(seedX) >= width ||
        static_cast<std::uint32_t>(seedY) >= height) {
        return changes;
    }
    const std::uint32_t seedUx = static_cast<std::uint32_t>(seedX);
    const std::uint32_t seedUy = static_cast<std::uint32_t>(seedY);

    // When a selection is active, the seed must lie inside it.
    if (!selection.isEmpty() && !selection.pixelSelected(seedUx, seedUy)) {
        return changes;
    }

    const Pixel seedColor = buffer.getPixel(seedUx, seedUy);
    const int tolerance = settings.tolerance;

    const auto matchable = [&](std::uint32_t x, std::uint32_t y) -> bool {
        if (!channelsWithinTolerance(buffer.getPixel(x, y), seedColor, tolerance)) {
            return false;
        }
        return selection.isEmpty() || selection.pixelSelected(x, y);
    };

    if (!matchable(seedUx, seedUy)) {
        return changes;
    }

    const std::size_t pixelCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<bool> visited(pixelCount, false);

    const auto indexOf = [width](std::uint32_t x, std::uint32_t y) -> std::size_t {
        return static_cast<std::size_t>(y) * width + x;
    };

    // Expand the maximal contiguous matchable, unvisited horizontal span
    // containing (x, y) on row y.
    const auto expandRow = [&](
        std::uint32_t y,
        std::uint32_t x,
        std::uint32_t& outLeft,
        std::uint32_t& outRight
    ) -> bool {
        if (!matchable(x, y) || visited[indexOf(x, y)]) {
            return false;
        }
        std::uint32_t left = x;
        while (left > 0 && matchable(left - 1, y) && !visited[indexOf(left - 1, y)]) {
            --left;
        }
        std::uint32_t right = x;
        while (right + 1 < width && matchable(right + 1, y) &&
               !visited[indexOf(right + 1, y)]) {
            ++right;
        }
        outLeft = left;
        outRight = right;
        return true;
    };

    // Fill a horizontal span, recording only pixels that actually change.
    const auto fillSpan = [&](std::uint32_t y, std::uint32_t left, std::uint32_t right) {
        for (std::uint32_t x = left; x <= right; ++x) {
            const std::size_t index = indexOf(x, y);
            visited[index] = true;
            const Pixel before = buffer.getPixel(x, y);
            const Pixel after = blendFill(settings.color, settings.opacity, before);
            if (after != before) {
                changes.push_back(PixelChange{index, before, after});
            }
            buffer.setPixel(x, y, after);
        }
    };

    struct Span {
        int y{0};
        int left{0};
        int right{0};
    };
    std::vector<Span> stack;

    // Examine a single row segment for new matchable spans to fill and enqueue
    // the rows above/below them for further expansion.
    const auto enqueueAdjacent = [&](
        std::uint32_t y,
        std::uint32_t left,
        std::uint32_t right
    ) {
        if (y >= height) {
            return;
        }
        std::uint32_t x = left;
        while (x <= right) {
            if (matchable(x, y) && !visited[indexOf(x, y)]) {
                std::uint32_t spanLeft = 0;
                std::uint32_t spanRight = 0;
                if (expandRow(y, x, spanLeft, spanRight)) {
                    fillSpan(y, spanLeft, spanRight);
                    stack.push_back(Span{
                        static_cast<int>(y) - 1,
                        static_cast<int>(spanLeft),
                        static_cast<int>(spanRight)
                    });
                    stack.push_back(Span{
                        static_cast<int>(y) + 1,
                        static_cast<int>(spanLeft),
                        static_cast<int>(spanRight)
                    });
                }
                x = spanRight + 1;
            } else {
                ++x;
            }
        }
    };

    std::uint32_t seedLeft = 0;
    std::uint32_t seedRight = 0;
    if (!expandRow(seedUy, seedUx, seedLeft, seedRight)) {
        return changes;
    }
    fillSpan(seedUy, seedLeft, seedRight);

    enqueueAdjacent(seedUy - 1, seedLeft, seedRight);
    enqueueAdjacent(seedUy + 1, seedLeft, seedRight);

    while (!stack.empty()) {
        const Span current = stack.back();
        stack.pop_back();
        if (current.y < 0 ||
            static_cast<std::uint32_t>(current.y) >= height) {
            continue;
        }
        enqueueAdjacent(
            static_cast<std::uint32_t>(current.y),
            static_cast<std::uint32_t>(current.left),
            static_cast<std::uint32_t>(current.right)
        );
    }

    return changes;
}

FillCommand::FillCommand(LayerId layerId, std::vector<PixelChange> changes)
    : layerId_(layerId), changes_(std::move(changes)) {}

std::string FillCommand::name() const {
    return "Fill";
}

bool FillCommand::execute(Document& document) {
    return apply(document, true);
}

bool FillCommand::undo(Document& document) {
    return apply(document, false);
}

std::size_t FillCommand::changeCount() const noexcept {
    return changes_.size();
}

bool FillCommand::apply(Document& document, bool useAfter) {
    Layer* layer = document.findLayer(layerId_);
    if (layer == nullptr) {
        return false;
    }

    ImageBuffer& buffer = layer->buffer();
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

} // namespace glyma::core

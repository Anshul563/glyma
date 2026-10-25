#include "glyma/core/brush_engine.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include "glyma/core/document.hpp"

namespace glyma::core {
namespace {

constexpr double minDabSpacing = 0.25;
constexpr double maxCoordinateMagnitude = 1.0e6;
constexpr double maxDabsPerSegment = 65536.0;
constexpr double coverageEpsilon = 1.0e-12;

std::uint8_t toByte(double value) {
    return static_cast<std::uint8_t>(
        std::clamp(std::lround(value), 0L, 255L)
    );
}

bool isUsableCoordinate(double value) noexcept {
    return std::isfinite(value) &&
           std::abs(value) <= maxCoordinateMagnitude;
}

Pixel paintPixel(Pixel destination, Pixel color, double sourceAlpha) {
    const double sa = std::clamp(sourceAlpha, 0.0, 1.0);
    if (sa <= 0.0) {
        return destination;
    }

    const double da = static_cast<double>(destination.a) / 255.0;
    const double oa = sa + da * (1.0 - sa);
    if (oa <= 0.0) {
        return Pixel{0, 0, 0, 0};
    }

    const auto blendChannel = [&](std::uint8_t source, std::uint8_t dst) {
        return toByte(
            (static_cast<double>(source) * sa +
             static_cast<double>(dst) * da * (1.0 - sa)) /
            oa
        );
    };

    return Pixel{
        blendChannel(color.r, destination.r),
        blendChannel(color.g, destination.g),
        blendChannel(color.b, destination.b),
        toByte(oa * 255.0)
    };
}

Pixel erasePixel(Pixel destination, double amount) {
    const double factor = std::clamp(1.0 - amount, 0.0, 1.0);
    const long alpha = std::lround(
        static_cast<double>(destination.a) * factor
    );
    if (alpha <= 0) {
        // Fully transparent pixels use a canonical zero value so no stale RGB
        // survives complete erasure.
        return Pixel{0, 0, 0, 0};
    }
    return Pixel{
        destination.r,
        destination.g,
        destination.b,
        static_cast<std::uint8_t>(alpha)
    };
}

} // namespace

bool BrushSettings::isValid() const noexcept {
    return std::isfinite(diameter) && diameter > 0.0 &&
           diameter <= maxDiameter && std::isfinite(opacity) &&
           opacity >= 0.0F && opacity <= 1.0F && std::isfinite(hardness) &&
           hardness >= 0.0F && hardness <= 1.0F;
}

void BrushSettings::validate() const {
    if (!isValid()) {
        throw std::invalid_argument(
            "Brush diameter must be in (0, 10000], opacity and hardness in "
            "[0, 1], and all values finite."
        );
    }
}

StrokeCommand::StrokeCommand(LayerId layerId, std::vector<PixelChange> changes)
    : layerId_(layerId), changes_(std::move(changes)) {}

std::string StrokeCommand::name() const {
    return "Stroke";
}

bool StrokeCommand::execute(Document& document) {
    return apply(document, true);
}

bool StrokeCommand::undo(Document& document) {
    return apply(document, false);
}

std::size_t StrokeCommand::changeCount() const noexcept {
    return changes_.size();
}

bool StrokeCommand::apply(Document& document, bool useAfter) {
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

BrushStroke::BrushStroke(
    ImageBuffer& buffer,
    LayerId layerId,
    const BrushSettings& settings,
    BrushMode mode
) : buffer_(&buffer),
    layerId_(layerId),
    settings_(settings),
    mode_(mode) {
    settings_.validate();
    if (layerId_ == Layer::invalidId) {
        throw std::invalid_argument("A brush stroke requires a valid layer ID.");
    }
}

BrushStroke::~BrushStroke() {
    if (active_) {
        cancel();
    }
}

bool BrushStroke::start(double x, double y) {
    resetDirty();
    if (!active_ || !isUsableCoordinate(x) || !isUsableCoordinate(y)) {
        return false;
    }
    hasLast_ = true;
    lastX_ = x;
    lastY_ = y;
    return stamp(x, y);
}

bool BrushStroke::lineTo(double x, double y) {
    resetDirty();
    if (!active_ || !hasLast_ || !isUsableCoordinate(x) ||
        !isUsableCoordinate(y)) {
        return false;
    }

    const double dx = x - lastX_;
    const double dy = y - lastY_;
    const double distance = std::hypot(dx, dy);
    if (distance <= 0.0) {
        return false;
    }

    const double radius = settings_.diameter * 0.5;
    const double spacing = std::max(minDabSpacing, radius * 0.25);
    const double stepCount = std::ceil(distance / spacing);
    if (stepCount > maxDabsPerSegment) {
        return false;
    }

    const auto steps = static_cast<std::size_t>(stepCount);
    bool changed = false;
    for (std::size_t step = 1; step <= steps; ++step) {
        const double t = static_cast<double>(step) /
                         static_cast<double>(steps);
        changed = stamp(
            std::lerp(lastX_, x, t),
            std::lerp(lastY_, y, t)
        ) || changed;
    }

    lastX_ = x;
    lastY_ = y;
    return changed;
}

bool BrushStroke::cancel() {
    if (buffer_ == nullptr) {
        active_ = false;
        changes_.clear();
        changePositions_.clear();
        return false;
    }

    const std::uint32_t width = buffer_->width();
    for (const PendingChange& change : changes_) {
        const auto x = static_cast<std::uint32_t>(
            change.index % static_cast<std::size_t>(width)
        );
        const auto y = static_cast<std::uint32_t>(
            change.index / static_cast<std::size_t>(width)
        );
        buffer_->setPixel(x, y, change.before);
    }

    const bool restored = !changes_.empty();
    changes_.clear();
    changePositions_.clear();
    active_ = false;
    hasLast_ = false;
    return restored;
}

std::unique_ptr<StrokeCommand> BrushStroke::finish() {
    if (!active_) {
        return nullptr;
    }
    active_ = false;
    hasLast_ = false;

    if (changes_.empty()) {
        return nullptr;
    }

    std::vector<PixelChange> pixels;
    pixels.reserve(changes_.size());
    for (const PendingChange& change : changes_) {
        pixels.push_back(PixelChange{
            change.index,
            change.before,
            change.after
        });
    }
    changes_.clear();
    changePositions_.clear();
    return std::make_unique<StrokeCommand>(layerId_, std::move(pixels));
}

bool BrushStroke::active() const noexcept {
    return active_;
}

std::size_t BrushStroke::changeCount() const noexcept {
    return changes_.size();
}

const BrushSettings& BrushStroke::settings() const noexcept {
    return settings_;
}

void BrushStroke::resetDirty() noexcept {
    dirtyValid_ = false;
    dirtyMinX_ = 0;
    dirtyMinY_ = 0;
    dirtyMaxX_ = 0;
    dirtyMaxY_ = 0;
}

void BrushStroke::expandDirty(long long pixelX, long long pixelY) noexcept {
    if (!dirtyValid_) {
        dirtyValid_ = true;
        dirtyMinX_ = pixelX;
        dirtyMaxX_ = pixelX;
        dirtyMinY_ = pixelY;
        dirtyMaxY_ = pixelY;
        return;
    }
    dirtyMinX_ = std::min(dirtyMinX_, pixelX);
    dirtyMinY_ = std::min(dirtyMinY_, pixelY);
    dirtyMaxX_ = std::max(dirtyMaxX_, pixelX);
    dirtyMaxY_ = std::max(dirtyMaxY_, pixelY);
}

PixelRegion BrushStroke::lastDirtyRegion() const noexcept {
    if (!dirtyValid_ || buffer_ == nullptr) {
        return {};
    }
    const long long width = static_cast<long long>(buffer_->width());
    const long long height = static_cast<long long>(buffer_->height());
    const long long minX = std::clamp(dirtyMinX_, 0LL, width - 1);
    const long long minY = std::clamp(dirtyMinY_, 0LL, height - 1);
    const long long maxX = std::clamp(dirtyMaxX_, 0LL, width - 1);
    const long long maxY = std::clamp(dirtyMaxY_, 0LL, height - 1);
    if (maxX < minX || maxY < minY) {
        return {};
    }
    return PixelRegion{
        static_cast<std::uint32_t>(minX),
        static_cast<std::uint32_t>(minY),
        static_cast<std::uint32_t>(maxX - minX + 1),
        static_cast<std::uint32_t>(maxY - minY + 1)
    };
}

double BrushStroke::coverageAt(
    double x,
    double y,
    std::uint32_t pixelX,
    std::uint32_t pixelY
) const noexcept {
    const double radius = settings_.diameter * 0.5;
    const double solidRadius = std::max(
        0.0,
        radius * static_cast<double>(settings_.hardness) - 0.5
    );
    const double outerRadius = radius + 0.5;

    const double dx = static_cast<double>(pixelX) + 0.5 - x;
    const double dy = static_cast<double>(pixelY) + 0.5 - y;
    const double distance = std::hypot(dx, dy);

    if (distance <= solidRadius) {
        return 1.0;
    }
    if (distance >= outerRadius) {
        return 0.0;
    }
    return (outerRadius - distance) / (outerRadius - solidRadius);
}

bool BrushStroke::stamp(double x, double y) {
    if (buffer_ == nullptr) {
        return false;
    }

    const double outerRadius = settings_.diameter * 0.5 + 0.5;
    const auto width = static_cast<long long>(buffer_->width());
    const auto height = static_cast<long long>(buffer_->height());

    const long long minX = std::max(
        0LL,
        static_cast<long long>(std::floor(x - outerRadius - 0.5))
    );
    const long long maxX = std::min(
        width - 1,
        static_cast<long long>(std::ceil(x + outerRadius - 0.5))
    );
    const long long minY = std::max(
        0LL,
        static_cast<long long>(std::floor(y - outerRadius - 0.5))
    );
    const long long maxY = std::min(
        height - 1,
        static_cast<long long>(std::ceil(y + outerRadius - 0.5))
    );

    if (minX > maxX || minY > maxY) {
        return false;
    }

    bool changed = false;
    for (long long py = minY; py <= maxY; ++py) {
        for (long long px = minX; px <= maxX; ++px) {
            const auto pixelX = static_cast<std::uint32_t>(px);
            const auto pixelY = static_cast<std::uint32_t>(py);
            const double coverage = coverageAt(x, y, pixelX, pixelY);
            if (coverage <= coverageEpsilon) {
                continue;
            }

            const double amount =
                static_cast<double>(settings_.opacity) * coverage;
            if (amount <= 0.0) {
                continue;
            }

            const std::size_t index =
                static_cast<std::size_t>(pixelY) *
                    static_cast<std::size_t>(buffer_->width()) +
                static_cast<std::size_t>(pixelX);
            const Pixel before = buffer_->getPixel(pixelX, pixelY);

            auto position = changePositions_.find(index);
            if (position != changePositions_.end()) {
                PendingChange& pending = changes_[position->second];
                if (coverage <= pending.coverage + coverageEpsilon) {
                    continue;
                }

                const Pixel after = mode_ == BrushMode::Paint
                    ? paintPixel(
                          pending.before,
                          settings_.color,
                          amount * static_cast<double>(settings_.color.a) /
                              255.0
                      )
                    : erasePixel(pending.before, amount);

                pending.coverage = coverage;
                pending.after = after;
                buffer_->setPixel(pixelX, pixelY, after);
                expandDirty(px, py);
                changed = changed || after != before;
                continue;
            }

            const Pixel after = mode_ == BrushMode::Paint
                ? paintPixel(
                      before,
                      settings_.color,
                      amount * static_cast<double>(settings_.color.a) / 255.0
                  )
                : erasePixel(before, amount);
            if (after == before) {
                continue;
            }

            changePositions_.emplace(index, changes_.size());
            changes_.push_back(PendingChange{index, before, after, coverage});
            buffer_->setPixel(pixelX, pixelY, after);
            expandDirty(px, py);
            changed = true;
        }
    }

    return changed;
}

} // namespace glyma::core

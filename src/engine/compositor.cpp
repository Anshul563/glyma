#include "glyma/core/compositor.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace glyma::core {
namespace {

std::uint8_t toByte(float value) {
    return static_cast<std::uint8_t>(
        std::clamp(std::lround(value), 0L, 255L)
    );
}

Pixel sourceOver(Pixel source, Pixel destination, float opacity) {
    const float sourceAlpha =
        (static_cast<float>(source.a) / 255.0F) * opacity;

    const float destinationAlpha =
        static_cast<float>(destination.a) / 255.0F;

    const float outputAlpha =
        sourceAlpha + destinationAlpha * (1.0F - sourceAlpha);

    if (outputAlpha <= 0.0F) {
        return Pixel{0, 0, 0, 0};
    }

    const auto blendChannel = [&](std::uint8_t sourceChannel,
                                  std::uint8_t destinationChannel) {
        const float sourceContribution =
            static_cast<float>(sourceChannel) * sourceAlpha;

        const float destinationContribution =
            static_cast<float>(destinationChannel) *
            destinationAlpha * (1.0F - sourceAlpha);

        return toByte(
            (sourceContribution + destinationContribution) / outputAlpha
        );
    };

    return Pixel{
        blendChannel(source.r, destination.r),
        blendChannel(source.g, destination.g),
        blendChannel(source.b, destination.b),
        toByte(outputAlpha * 255.0F)
    };
}

} // namespace

void Compositor::composite(
    const ImageBuffer& source,
    ImageBuffer& destination,
    float opacity
) {
    if (!std::isfinite(opacity) || opacity < 0.0F || opacity > 1.0F) {
        throw std::invalid_argument(
            "Compositing opacity must be between 0.0 and 1.0."
        );
    }

    if (source.width() != destination.width() ||
        source.height() != destination.height()) {
        throw std::invalid_argument(
            "Source and destination dimensions must match."
        );
    }

    if (opacity == 0.0F) {
        return;
    }

    for (std::uint32_t y = 0; y < source.height(); ++y) {
        for (std::uint32_t x = 0; x < source.width(); ++x) {
            const Pixel sourcePixel = source.getPixel(x, y);
            const Pixel destinationPixel = destination.getPixel(x, y);

            destination.setPixel(
                x,
                y,
                sourceOver(sourcePixel, destinationPixel, opacity)
            );
        }
    }
}

void Compositor::composite(
    const LayerStack& stack,
    ImageBuffer& destination
) {
    if (stack.empty()) {
        return;
    }

    const std::uint32_t canvasWidth = destination.width();
    const std::uint32_t canvasHeight = destination.height();

    // Process layers bottom-to-top (index 0..end). This matches standard
    // layer stack ordering where index 0 is the bottom-most layer.
    for (const auto& layer : stack.layers()) {
        if (!layer.visible()) {
            continue;
        }

        const float layerOpacity = layer.opacity();
        if (layerOpacity <= 0.0F) {
            continue;
        }
        if (!std::isfinite(layerOpacity)) {
            continue;
        }
        if (layerOpacity > 1.0F) {
            // Layer validation should prevent this, but be defensive.
            continue;
        }

        const std::int32_t offsetX = layer.x();
        const std::int32_t offsetY = layer.y();

        const std::uint32_t layerWidth = layer.width();
        const std::uint32_t layerHeight = layer.height();

        const std::int32_t startX = offsetX;
        const std::int32_t startY = offsetY;
        const std::int32_t endX = offsetX + static_cast<std::int32_t>(layerWidth);
        const std::int32_t endY = offsetY + static_cast<std::int32_t>(layerHeight);

        const std::int32_t clipStartX = std::max(startX, std::int32_t{0});
        const std::int32_t clipStartY = std::max(startY, std::int32_t{0});
        const std::int32_t clipEndX =
            std::min(endX, static_cast<std::int32_t>(canvasWidth));
        const std::int32_t clipEndY =
            std::min(endY, static_cast<std::int32_t>(canvasHeight));

        if (clipStartX >= clipEndX || clipStartY >= clipEndY) {
            continue;
        }

        const auto& sourceBuffer = layer.buffer();
        const float opacity = layerOpacity;

        for (std::int32_t y = clipStartY; y < clipEndY; ++y) {
            for (std::int32_t x = clipStartX; x < clipEndX; ++x) {
                const std::int32_t srcX = x - startX;
                const std::int32_t srcY = y - startY;
                if (srcX < 0 || srcY < 0) {
                    continue;
                }

                const Pixel sourcePixel = sourceBuffer.getPixel(
                    static_cast<std::uint32_t>(srcX),
                    static_cast<std::uint32_t>(srcY)
                );
                const Pixel destinationPixel = destination.getPixel(
                    static_cast<std::uint32_t>(x),
                    static_cast<std::uint32_t>(y)
                );

                destination.setPixel(
                    static_cast<std::uint32_t>(x),
                    static_cast<std::uint32_t>(y),
                    sourceOver(sourcePixel, destinationPixel, opacity)
                );
            }
        }
    }
}

} // namespace glyma::core

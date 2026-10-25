#include "glyma/core/adjustments.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace glyma::core {
namespace {

// Rec.601 luma weights over encoded RGB. Glyma has no color management, so this
// is a simple perceptual weighting rather than a linear-light calculation.
constexpr float lumR = 0.299F;
constexpr float lumG = 0.587F;
constexpr float lumB = 0.114F;

std::uint8_t clampByte(double value) {
    return static_cast<std::uint8_t>(
        std::clamp(std::lround(value), 0L, 255L)
    );
}

float luminance(Pixel pixel) {
    return lumR * static_cast<float>(pixel.r) +
           lumG * static_cast<float>(pixel.g) +
           lumB * static_cast<float>(pixel.b);
}

void requireFiniteRange(float value, float low, float high, const char* name) {
    if (!std::isfinite(value) || value < low || value > high) {
        throw std::invalid_argument(
            std::string(name) + " is out of the supported range."
        );
    }
}

// Iterate every pixel, rewriting only those accepted by `mask`. `op` maps an
// RGB value to a new RGB value; alpha is always preserved. Returns the number
// of pixels whose RGBA value changed.
template <typename Op>
std::size_t applyPerPixel(ImageBuffer& buffer, const SelectionMask& mask, Op op) {
    std::size_t changed = 0;
    for (std::uint32_t y = 0; y < buffer.height(); ++y) {
        for (std::uint32_t x = 0; x < buffer.width(); ++x) {
            if (!mask.includes(x, y)) {
                continue;
            }
            const Pixel before = buffer.getPixel(x, y);
            Pixel after = op(before);
            after.a = before.a;
            if (after != before) {
                buffer.setPixel(x, y, after);
                ++changed;
            }
        }
    }
    return changed;
}

std::vector<float> makeGaussianKernel(float sigma, int& outRadius) {
    const int radius = std::max(1, static_cast<int>(std::ceil(3.0 * sigma)));
    const double twoSigmaSquared = 2.0 * static_cast<double>(sigma) * sigma;
    std::vector<float> kernel(static_cast<std::size_t>(2 * radius + 1));
    double sum = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        const double value = std::exp(-(static_cast<double>(i) * i) / twoSigmaSquared);
        kernel[static_cast<std::size_t>(i + radius)] = static_cast<float>(value);
        sum += value;
    }
    const double inv = sum > 0.0 ? 1.0 / sum : 0.0;
    for (float& value : kernel) {
        value = static_cast<float>(value * inv);
    }
    outRadius = radius;
    return kernel;
}

// Premultiplied Gaussian blur of the entire buffer. Returns a row-major float
// image with four interleaved channels (r, g, b, a) in [0, 255], premultiplied
// by alpha. Boundary reads replicate the nearest edge pixel.
std::vector<float> blurPremultiplied(const ImageBuffer& buffer, float sigma) {
    const std::uint32_t width = buffer.width();
    const std::uint32_t height = buffer.height();

    int radius = 0;
    const std::vector<float> kernel = makeGaussianKernel(sigma, radius);

    const std::size_t pixelCount =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    // Build the premultiplied source once.
    std::vector<float> source(pixelCount * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const Pixel pixel = buffer.getPixel(x, y);
            const float alpha = static_cast<float>(pixel.a);
            const float scale = alpha / 255.0F;
            const std::size_t base =
                (static_cast<std::size_t>(y) * width + x) * 4;
            source[base + 0] = static_cast<float>(pixel.r) * scale;
            source[base + 1] = static_cast<float>(pixel.g) * scale;
            source[base + 2] = static_cast<float>(pixel.b) * scale;
            source[base + 3] = alpha;
        }
    }

    // Horizontal pass into a scratch buffer.
    std::vector<float> horizontal(pixelCount * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            float acc[4] = {0.0F, 0.0F, 0.0F, 0.0F};
            for (int i = -radius; i <= radius; ++i) {
                int sx = static_cast<int>(x) + i;
                sx = std::clamp(sx, 0, static_cast<int>(width) - 1);
                const float weight = kernel[static_cast<std::size_t>(i + radius)];
                const std::size_t base =
                    (static_cast<std::size_t>(y) * width +
                     static_cast<std::uint32_t>(sx)) *
                    4;
                for (int c = 0; c < 4; ++c) {
                    acc[c] += source[base + static_cast<std::size_t>(c)] * weight;
                }
            }
            const std::size_t out =
                (static_cast<std::size_t>(y) * width + x) * 4;
            for (int c = 0; c < 4; ++c) {
                horizontal[out + static_cast<std::size_t>(c)] = acc[c];
            }
        }
    }

    // Vertical pass.
    std::vector<float> result(pixelCount * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            float acc[4] = {0.0F, 0.0F, 0.0F, 0.0F};
            for (int i = -radius; i <= radius; ++i) {
                int sy = static_cast<int>(y) + i;
                sy = std::clamp(sy, 0, static_cast<int>(height) - 1);
                const float weight = kernel[static_cast<std::size_t>(i + radius)];
                const std::size_t base =
                    (static_cast<std::size_t>(sy) * width + x) * 4;
                for (int c = 0; c < 4; ++c) {
                    acc[c] +=
                        horizontal[base + static_cast<std::size_t>(c)] * weight;
                }
            }
            const std::size_t out =
                (static_cast<std::size_t>(y) * width + x) * 4;
            for (int c = 0; c < 4; ++c) {
                result[out + static_cast<std::size_t>(c)] = acc[c];
            }
        }
    }

    return result;
}

Pixel unpremultiply(const float* channels) {
    // Round alpha first so the RGB is scaled by the value actually stored. If
    // the rounded alpha is 0 the pixel is fully transparent and its RGB is
    // discarded, which prevents colored fringes in transparent regions.
    const std::uint8_t alpha = clampByte(static_cast<double>(channels[3]));
    if (alpha == 0) {
        return Pixel{0, 0, 0, 0};
    }
    const float scale = 255.0F / static_cast<float>(alpha);
    return Pixel{
        clampByte(static_cast<double>(channels[0]) * scale),
        clampByte(static_cast<double>(channels[1]) * scale),
        clampByte(static_cast<double>(channels[2]) * scale),
        alpha
    };
}

} // namespace

bool SelectionMask::includes(std::uint32_t x, std::uint32_t y) const noexcept {
    if (selection == nullptr || selection->isEmpty()) {
        return true;
    }
    return selection->contains(
        static_cast<double>(x) + 0.5 + offsetX,
        static_cast<double>(y) + 0.5 + offsetY
    );
}

bool AdjustmentSettings::isDefault() const noexcept {
    return exposure == 0.0F && brightness == 0.0F && contrast == 0.0F &&
           saturation == 0.0F && !grayscale && blurSigma == 0.0F &&
           sharpen == 0.0F;
}

bool AdjustmentSettings::isValid() const noexcept {
    const auto inRange = [](float value, float low, float high) {
        return std::isfinite(value) && value >= low && value <= high;
    };
    return inRange(exposure, -AdjustmentSettings::maxExposure, AdjustmentSettings::maxExposure) &&
           inRange(brightness, -1.0F, 1.0F) &&
           inRange(contrast, -1.0F, 1.0F) &&
           inRange(saturation, -1.0F, 1.0F) &&
           inRange(blurSigma, 0.0F, AdjustmentSettings::maxBlurSigma) &&
           inRange(sharpen, 0.0F, AdjustmentSettings::maxSharpen);
}

void AdjustmentSettings::validate() const {
    if (!isValid()) {
        throw std::invalid_argument(
            "Adjustment parameters are out of range or not finite."
        );
    }
}

std::size_t adjustExposure(
    ImageBuffer& buffer,
    float stops,
    const SelectionMask& mask
) {
    requireFiniteRange(stops, -AdjustmentSettings::maxExposure, AdjustmentSettings::maxExposure, "Exposure");
    if (stops == 0.0F) {
        return 0;
    }
    const double factor = std::exp2(static_cast<double>(stops));
    return applyPerPixel(buffer, mask, [factor](Pixel pixel) {
        return Pixel{
            clampByte(static_cast<double>(pixel.r) * factor),
            clampByte(static_cast<double>(pixel.g) * factor),
            clampByte(static_cast<double>(pixel.b) * factor),
            pixel.a
        };
    });
}

std::size_t adjustBrightness(
    ImageBuffer& buffer,
    float amount,
    const SelectionMask& mask
) {
    requireFiniteRange(amount, -1.0F, 1.0F, "Brightness");
    if (amount == 0.0F) {
        return 0;
    }
    const double offset = static_cast<double>(amount) * 255.0;
    return applyPerPixel(buffer, mask, [offset](Pixel pixel) {
        return Pixel{
            clampByte(static_cast<double>(pixel.r) + offset),
            clampByte(static_cast<double>(pixel.g) + offset),
            clampByte(static_cast<double>(pixel.b) + offset),
            pixel.a
        };
    });
}

std::size_t adjustContrast(
    ImageBuffer& buffer,
    float amount,
    const SelectionMask& mask
) {
    requireFiniteRange(amount, -1.0F, 1.0F, "Contrast");
    if (amount == 0.0F) {
        return 0;
    }
    const double factor = 1.0 + static_cast<double>(amount);
    constexpr double pivot = 127.5;
    return applyPerPixel(buffer, mask, [factor](Pixel pixel) {
        const auto contrast = [factor](std::uint8_t channel) {
            return clampByte((static_cast<double>(channel) - pivot) * factor + pivot);
        };
        return Pixel{
            contrast(pixel.r),
            contrast(pixel.g),
            contrast(pixel.b),
            pixel.a
        };
    });
}

std::size_t adjustSaturation(
    ImageBuffer& buffer,
    float amount,
    const SelectionMask& mask
) {
    requireFiniteRange(amount, -1.0F, 1.0F, "Saturation");
    if (amount == 0.0F) {
        return 0;
    }
    const double factor = 1.0 + static_cast<double>(amount);
    return applyPerPixel(buffer, mask, [factor](Pixel pixel) {
        const double lum = static_cast<double>(luminance(pixel));
        const auto saturate = [lum, factor](std::uint8_t channel) {
            return clampByte(lum + (static_cast<double>(channel) - lum) * factor);
        };
        return Pixel{
            saturate(pixel.r),
            saturate(pixel.g),
            saturate(pixel.b),
            pixel.a
        };
    });
}

std::size_t adjustGrayscale(ImageBuffer& buffer, const SelectionMask& mask) {
    return applyPerPixel(buffer, mask, [](Pixel pixel) {
        const std::uint8_t grey = clampByte(static_cast<double>(luminance(pixel)));
        return Pixel{grey, grey, grey, pixel.a};
    });
}

std::size_t gaussianBlur(
    ImageBuffer& buffer,
    float sigma,
    const SelectionMask& mask
) {
    requireFiniteRange(sigma, 0.0F, AdjustmentSettings::maxBlurSigma, "Blur sigma");
    if (sigma <= 0.0F) {
        return 0;
    }
    const std::vector<float> blurred = blurPremultiplied(buffer, sigma);
    const std::uint32_t width = buffer.width();
    const std::uint32_t height = buffer.height();

    std::size_t changed = 0;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            if (!mask.includes(x, y)) {
                continue;
            }
            const std::size_t base =
                (static_cast<std::size_t>(y) * width + x) * 4;
            const Pixel after = unpremultiply(&blurred[base]);
            const Pixel before = buffer.getPixel(x, y);
            if (after != before) {
                buffer.setPixel(x, y, after);
                ++changed;
            }
        }
    }
    return changed;
}

std::size_t sharpen(
    ImageBuffer& buffer,
    float amount,
    const SelectionMask& mask
) {
    requireFiniteRange(amount, 0.0F, AdjustmentSettings::maxSharpen, "Sharpen amount");
    if (amount <= 0.0F) {
        return 0;
    }
    const std::vector<float> blurred = blurPremultiplied(buffer, 1.0F);
    const std::uint32_t width = buffer.width();
    const std::uint32_t height = buffer.height();

    std::size_t changed = 0;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            if (!mask.includes(x, y)) {
                continue;
            }
            const Pixel before = buffer.getPixel(x, y);
            const float alpha = static_cast<float>(before.a);
            const float scale = alpha / 255.0F;
            const std::size_t base =
                (static_cast<std::size_t>(y) * width + x) * 4;

            // Work on premultiplied channels and clamp to [0, alpha] so the
            // result cannot overshoot or bleed color into transparent pixels.
            const float premul[3] = {
                static_cast<float>(before.r) * scale,
                static_cast<float>(before.g) * scale,
                static_cast<float>(before.b) * scale
            };
            float sharp[3];
            for (int c = 0; c < 3; ++c) {
                const float value =
                    premul[c] + amount * (premul[c] - blurred[base + static_cast<std::size_t>(c)]);
                sharp[c] = std::clamp(value, 0.0F, alpha);
            }

            Pixel after;
            if (alpha <= 0.0F) {
                after = Pixel{0, 0, 0, 0};
            } else {
                const float inverse = 255.0F / alpha;
                after = Pixel{
                    clampByte(static_cast<double>(sharp[0]) * inverse),
                    clampByte(static_cast<double>(sharp[1]) * inverse),
                    clampByte(static_cast<double>(sharp[2]) * inverse),
                    before.a
                };
            }
            if (after != before) {
                buffer.setPixel(x, y, after);
                ++changed;
            }
        }
    }
    return changed;
}

std::unique_ptr<ImageBuffer> processImage(
    const ImageBuffer& source,
    const AdjustmentSettings& settings,
    const SelectionMask& mask
) {
    settings.validate();
    auto result = std::make_unique<ImageBuffer>(source);

    if (settings.exposure != 0.0F) {
        (void)adjustExposure(*result, settings.exposure, mask);
    }
    if (settings.brightness != 0.0F) {
        (void)adjustBrightness(*result, settings.brightness, mask);
    }
    if (settings.contrast != 0.0F) {
        (void)adjustContrast(*result, settings.contrast, mask);
    }
    if (settings.saturation != 0.0F) {
        (void)adjustSaturation(*result, settings.saturation, mask);
    }
    if (settings.grayscale) {
        (void)adjustGrayscale(*result, mask);
    }
    if (settings.blurSigma > 0.0F) {
        (void)gaussianBlur(*result, settings.blurSigma, mask);
    }
    if (settings.sharpen > 0.0F) {
        (void)sharpen(*result, settings.sharpen, mask);
    }
    return result;
}

} // namespace glyma::core

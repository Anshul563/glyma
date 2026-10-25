#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "glyma/core/image_buffer.hpp"
#include "glyma/core/selection.hpp"

namespace glyma::core {

// Coordinate-aware selection test for a single layer buffer.
//
// Selection geometry is expressed in continuous document coordinates, while a
// layer's buffer is addressed in layer-local coordinates. `offsetX`/`offsetY`
// are the layer's position within the document, so buffer pixel (x, y) maps to
// the document point (x + 0.5 + offsetX, y + 0.5 + offsetY). When `selection`
// is null or empty, every pixel is included.
//
// The mask is passed by value to the adjustment functions so a layer-local
// offset can be supplied without translating the selection geometry.
struct SelectionMask {
    const Selection* selection{nullptr};
    double offsetX{0.0};
    double offsetY{0.0};

    [[nodiscard]] bool includes(std::uint32_t x, std::uint32_t y) const noexcept;
};

// Parameters for the adjustment pipeline.
//
// All tonal adjustments operate on encoded (gamma-encoded, sRGB-like) 8-bit
// channels. Glyma is not color-managed, so these operations are approximations
// and no claim of colorimetric accuracy is made. Alpha is preserved exactly by
// every tonal adjustment; fully transparent pixels keep their alpha.
//
// Parameter ranges (enforced by validate()):
//   exposure    stops in [-4, 4]; per-channel gain of 2^stops in encoded space.
//   brightness  [-1, 1]; additive offset of `amount * 255` per RGB channel.
//   contrast    [-1, 1]; scales the distance from the midpoint 127.5 by
//               `(1 + amount)` (factor 0 at -1, 1 at 0, 2 at +1).
//   saturation  [-1, 1]; scales the distance from Rec.601 luminance by
//               `(1 + amount)`; -1 produces grayscale.
//   grayscale   boolean; replaces RGB with Rec.601 luminance.
//   blurSigma   Gaussian sigma in pixels, [0, 50]; 0 disables.
//   sharpen     unsharp strength, [0, 2]; 0 disables; fixed 1 px radius.
struct AdjustmentSettings {
    static constexpr float maxExposure = 4.0F;
    static constexpr float maxBlurSigma = 50.0F;
    static constexpr float maxSharpen = 2.0F;

    float exposure{0.0F};
    float brightness{0.0F};
    float contrast{0.0F};
    float saturation{0.0F};
    bool grayscale{false};
    float blurSigma{0.0F};
    float sharpen{0.0F};

    // True when no field differs from its default (identity operation).
    [[nodiscard]] bool isDefault() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool operator==(const AdjustmentSettings&) const = default;
    // Throws std::invalid_argument when a parameter is out of range or not
    // finite.
    void validate() const;
};

// Individual in-place operations.
//
// Each validates its parameter (throwing std::invalid_argument when out of
// range), returns the number of pixels whose value changed, and preserves
// alpha. Only pixels included by `mask` are written. Spatial filters read the
// whole image (so their kernels see real neighboring pixels) but never write
// outside `mask`; this keeps pixels outside an active selection byte-for-byte
// unchanged.
std::size_t adjustExposure(ImageBuffer& buffer, float stops, const SelectionMask& mask);
std::size_t adjustBrightness(ImageBuffer& buffer, float amount, const SelectionMask& mask);
std::size_t adjustContrast(ImageBuffer& buffer, float amount, const SelectionMask& mask);
std::size_t adjustSaturation(ImageBuffer& buffer, float amount, const SelectionMask& mask);
std::size_t adjustGrayscale(ImageBuffer& buffer, const SelectionMask& mask);

// Separable Gaussian blur (horizontal then vertical pass) using clamped-edge
// boundary handling. The convolution runs on premultiplied alpha internally so
// that colors do not bleed into transparent pixels; the result is
// unpremultiplied again. Fully transparent output pixels are normalized to
// {0, 0, 0, 0}.
std::size_t gaussianBlur(ImageBuffer& buffer, float sigma, const SelectionMask& mask);

// Unsharp mask with a fixed 1 px Gaussian radius. Computed on premultiplied
// alpha and clamped to [0, alpha] so sharpening cannot overshoot or create
// colored fringes around transparent pixels. Alpha is preserved.
std::size_t sharpen(ImageBuffer& buffer, float amount, const SelectionMask& mask);

// Apply the whole pipeline in a fixed order:
// exposure, brightness, contrast, saturation, grayscale, blur, sharpen.
//
// The operation is applied to a copy of `source`, so `source` is never
// modified; callers compare the returned buffer with `source` to build an
// undoable edit. Returns a new buffer of the same dimensions.
[[nodiscard]] std::unique_ptr<ImageBuffer> processImage(
    const ImageBuffer& source,
    const AdjustmentSettings& settings,
    const SelectionMask& mask
);

} // namespace glyma::core

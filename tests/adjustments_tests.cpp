#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#include "glyma/core/adjustments.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/selection.hpp"

using glyma::core::AdjustmentSettings;
using glyma::core::adjustBrightness;
using glyma::core::adjustContrast;
using glyma::core::adjustExposure;
using glyma::core::adjustGrayscale;
using glyma::core::adjustSaturation;
using glyma::core::gaussianBlur;
using glyma::core::ImageBuffer;
using glyma::core::Pixel;
using glyma::core::processImage;
using glyma::core::Selection;
using glyma::core::SelectionMask;
using glyma::core::sharpen;

namespace {

SelectionMask noMask() {
    return SelectionMask{};
}

void testBrightness() {
    ImageBuffer buffer(2, 2);
    buffer.clear((Pixel{100, 100, 100, 200}));

    assert(adjustBrightness(buffer, 0.0F, noMask()) == 0);
    assert(buffer.getPixel(0, 0) == (Pixel{100, 100, 100, 200}));

    // Full positive and negative adjustments saturate the channel range.
    assert(adjustBrightness(buffer, 1.0F, noMask()) == 4);
    assert(buffer.getPixel(0, 0) == (Pixel{255, 255, 255, 200}));
    assert(adjustBrightness(buffer, -1.0F, noMask()) == 4);
    assert(buffer.getPixel(0, 0) == (Pixel{0, 0, 0, 200}));

    // Mid adjustment brightens, preserves alpha and clamps.
    buffer.clear((Pixel{210, 10, 0, 64}));
    assert(adjustBrightness(buffer, 0.5F, noMask()) == 4);
    assert(buffer.getPixel(0, 0) == (Pixel{255, 138, 128, 64}));
}


void testContrast() {
    ImageBuffer buffer(1, 1);
    buffer.clear((Pixel{180, 60, 128, 255}));

    assert(adjustContrast(buffer, 0.0F, noMask()) == 0);

    // factor = 1.5 around the 127.5 pivot.
    assert(adjustContrast(buffer, 0.5F, noMask()) == 1);
    assert(buffer.getPixel(0, 0) == (Pixel{206, 26, 128, 255}));

    // Minimum contrast flattens everything to the pivot, alpha preserved.
    buffer.clear((Pixel{10, 200, 64, 90}));
    assert(adjustContrast(buffer, -1.0F, noMask()) == 1);
    const Pixel flat = buffer.getPixel(0, 0);
    assert(flat.r == flat.g && flat.g == flat.b);
    assert(flat.r == 128);
    assert(flat.a == 90);
}

void testSaturation() {
    ImageBuffer buffer(1, 1);
    buffer.clear((Pixel{255, 0, 0, 255}));

    assert(adjustSaturation(buffer, 0.0F, noMask()) == 0);

    // factor 0 -> luminance gray (Rec.601: 0.299 * 255 = 76.245 -> 76).
    assert(adjustSaturation(buffer, -1.0F, noMask()) == 1);
    assert(buffer.getPixel(0, 0) == (Pixel{76, 76, 76, 255}));

    // factor 2 expands away from luminance, clamped.
    buffer.clear((Pixel{200, 100, 64, 255}));
    assert(adjustSaturation(buffer, 1.0F, noMask()) == 1);
    assert(buffer.getPixel(0, 0) == (Pixel{255, 74, 2, 255}));
}

void testGrayscale() {
    ImageBuffer buffer(5, 1);
    buffer.setPixel(0, 0, (Pixel{255, 0, 0, 255}));
    buffer.setPixel(1, 0, (Pixel{0, 255, 0, 128}));
    buffer.setPixel(2, 0, (Pixel{0, 0, 255, 255}));
    buffer.setPixel(3, 0, (Pixel{255, 255, 255, 255}));
    buffer.setPixel(4, 0, (Pixel{0, 0, 0, 0}));

    assert(adjustGrayscale(buffer, noMask()) == 3);

    assert(buffer.getPixel(0, 0) == (Pixel{76, 76, 76, 255}));
    assert(buffer.getPixel(1, 0) == (Pixel{150, 150, 150, 128}));
    assert(buffer.getPixel(2, 0) == (Pixel{29, 29, 29, 255}));
    // White and black are unchanged (so they were not counted).
    assert(buffer.getPixel(3, 0) == (Pixel{255, 255, 255, 255}));
    assert(buffer.getPixel(4, 0) == (Pixel{0, 0, 0, 0}));
}

void testExposure() {
    ImageBuffer buffer(2, 1);
    buffer.setPixel(0, 0, (Pixel{100, 100, 100, 128}));
    buffer.setPixel(1, 0, (Pixel{200, 200, 200, 255}));

    assert(adjustExposure(buffer, 0.0F, noMask()) == 0);

    assert(adjustExposure(buffer, 1.0F, noMask()) == 2);
    assert(buffer.getPixel(0, 0) == (Pixel{200, 200, 200, 128}));
    assert(buffer.getPixel(1, 0) == (Pixel{255, 255, 255, 255}));

    assert(adjustExposure(buffer, -1.0F, noMask()) == 2);
    assert(buffer.getPixel(0, 0) == (Pixel{100, 100, 100, 128}));
}

void testInvalidParameters() {
    ImageBuffer buffer(1, 1);
    buffer.clear((Pixel{10, 20, 30, 255}));

    bool threw = false;
    try {
        (void)adjustBrightness(buffer, 1.5F, noMask());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        (void)adjustContrast(buffer, 2.0F, noMask());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        (void)adjustSaturation(buffer, -2.0F, noMask());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        (void)adjustExposure(buffer, 5.0F, noMask());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        (void)gaussianBlur(buffer, -1.0F, noMask());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        (void)sharpen(buffer, 3.0F, noMask());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    AdjustmentSettings settings;
    settings.brightness = 2.0F;
    assert(!settings.isValid());
    threw = false;
    try {
        settings.validate();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    assert(AdjustmentSettings{}.isDefault());
}

void testSelectionMaskRespectsGeometryAndOffset() {
    ImageBuffer buffer(3, 3);
    buffer.clear((Pixel{100, 100, 100, 255}));

    Selection selection;
    selection.setDocumentSize(3, 3);
    selection.setRectangle(1.0, 1.0, 3.0, 3.0);

    // With no offset, the selection covers buffer pixels (1,1)..(2,2).
    const std::size_t changed =
        adjustBrightness(buffer, 1.0F, SelectionMask{&selection, 0.0, 0.0});
    assert(changed == 4);
    assert(buffer.getPixel(1, 1) == (Pixel{255, 255, 255, 255}));
    assert(buffer.getPixel(0, 0) == (Pixel{100, 100, 100, 255}));

    // Offsetting the layer by (1, 1) shifts which buffer pixels map into the
    // selection: buffer (0,0) now maps to the document point (1.5, 1.5).
    ImageBuffer offsetBuffer(3, 3);
    offsetBuffer.clear((Pixel{100, 100, 100, 255}));
    const std::size_t offsetChanged =
        adjustBrightness(offsetBuffer, 1.0F, SelectionMask{&selection, 1.0, 1.0});
    assert(offsetChanged == 4);
    assert(offsetBuffer.getPixel(0, 0) == (Pixel{255, 255, 255, 255}));
    assert(offsetBuffer.getPixel(2, 2) == (Pixel{100, 100, 100, 255}));
}

void testBlurSpreadsAndHandlesBoundaries() {
    ImageBuffer buffer(5, 5);
    buffer.clear((Pixel{0, 0, 0, 255}));
    buffer.setPixel(2, 2, (Pixel{255, 255, 255, 255}));

    const std::size_t changed = gaussianBlur(buffer, 1.0F, noMask());
    assert(changed > 0);

    const Pixel center = buffer.getPixel(2, 2);
    assert(center.r < 255 && center.r > 0);
    assert(center.r == center.g && center.g == center.b);
    assert(center.a == 255);

    // The impulse spreads into its neighbors.
    assert(buffer.getPixel(1, 2).r > 0);
    assert(buffer.getPixel(3, 2).r > 0);

    // Boundary pixels are valid and did not crash.
    assert(buffer.getPixel(0, 0).a == 255);
    assert(buffer.getPixel(4, 4).a == 255);

    // sigma 0 is a no-op.
    ImageBuffer other(3, 3);
    other.clear((Pixel{10, 20, 30, 255}));
    assert(gaussianBlur(other, 0.0F, noMask()) == 0);
}

void testBlurTransparentNoFringe() {
    ImageBuffer buffer(9, 9);
    buffer.clear((Pixel{0, 0, 0, 0}));
    for (std::uint32_t y = 3; y <= 5; ++y) {
        for (std::uint32_t x = 3; x <= 5; ++x) {
            buffer.setPixel(x, y, (Pixel{255, 0, 0, 255}));
        }
    }

    (void)gaussianBlur(buffer, 1.0F, noMask());

    // Far from the opaque block the image stays fully transparent with no
    // leaked color.
    assert(buffer.getPixel(0, 0) == (Pixel{0, 0, 0, 0}));

    // Immediately outside the block alpha spreads, and because the source had
    // no green/blue, no colored fringe appears.
    const Pixel fringe = buffer.getPixel(2, 4);
    assert(fringe.a > 0);
    assert(fringe.g == 0);
    assert(fringe.b == 0);
    assert(fringe.r > 0);
}

void testBlurSmallImageLargeRadius() {
    ImageBuffer single(1, 1);
    single.setPixel(0, 0, (Pixel{10, 20, 30, 255}));
    // A single pixel blurred by any radius must remain itself.
    (void)gaussianBlur(single, 20.0F, noMask());
    assert(single.getPixel(0, 0) == (Pixel{10, 20, 30, 255}));

    ImageBuffer strip(2, 1);
    strip.setPixel(0, 0, (Pixel{0, 0, 0, 255}));
    strip.setPixel(1, 0, (Pixel{255, 255, 255, 255}));
    (void)gaussianBlur(strip, 50.0F, noMask());
    for (std::uint32_t x = 0; x < 2; ++x) {
        const Pixel pixel = strip.getPixel(x, 0);
        assert(pixel.r <= 255 && pixel.a == 255);
    }
}

void testSharpenIncreasesLocalContrast() {
    ImageBuffer buffer(7, 7);
    buffer.clear((Pixel{100, 100, 100, 255}));
    buffer.setPixel(3, 3, (Pixel{200, 200, 200, 255}));

    (void)sharpen(buffer, 1.0F, noMask());

    // The peak is pushed brighter and its neighbor darker.
    assert(buffer.getPixel(3, 3).r > 200);
    assert(buffer.getPixel(2, 3).r < 100);
    assert(buffer.getPixel(3, 3).a == 255);

    ImageBuffer plain(3, 3);
    plain.clear((Pixel{50, 50, 50, 255}));
    assert(sharpen(plain, 0.0F, noMask()) == 0);
}

void testSharpenTransparentNoFringe() {
    ImageBuffer buffer(7, 7);
    buffer.clear((Pixel{0, 0, 0, 0}));
    for (std::uint32_t y = 2; y <= 4; ++y) {
        for (std::uint32_t x = 2; x <= 4; ++x) {
            buffer.setPixel(x, y, (Pixel{255, 0, 0, 255}));
        }
    }

    (void)sharpen(buffer, 1.0F, noMask());
    assert(buffer.getPixel(0, 0) == (Pixel{0, 0, 0, 0}));

    const Pixel fringe = buffer.getPixel(1, 3);
    assert(fringe.g == 0 && fringe.b == 0);
}

void testProcessImagePipeline() {
    ImageBuffer source(2, 2);
    source.clear((Pixel{100, 120, 140, 210}));

    AdjustmentSettings settings;
    settings.brightness = 0.2F;
    settings.grayscale = true;
    settings.blurSigma = 0.0F;

    const auto result = processImage(source, settings, noMask());
    assert(result != nullptr);

    // Source is untouched.
    assert(source.getPixel(0, 0) == (Pixel{100, 120, 140, 210}));

    // Match the documented order: brightness then grayscale.
    ImageBuffer manual(source);
    (void)adjustBrightness(manual, settings.brightness, noMask());
    (void)adjustGrayscale(manual, noMask());
    assert(result->pixels() == manual.pixels());

    // Default settings reproduce the source.
    const auto identity = processImage(source, AdjustmentSettings{}, noMask());
    assert(identity->pixels() == source.pixels());
}

} // namespace

int main() {
    testBrightness();
    testContrast();
    testSaturation();
    testGrayscale();
    testExposure();
    testInvalidParameters();
    testSelectionMaskRespectsGeometryAndOffset();
    testBlurSpreadsAndHandlesBoundaries();
    testBlurTransparentNoFringe();
    testBlurSmallImageLargeRadius();
    testSharpenIncreasesLocalContrast();
    testSharpenTransparentNoFringe();
    testProcessImagePipeline();

    std::cout << "All Glyma adjustment tests passed\n";
    return 0;
}

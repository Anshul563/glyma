#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

#include "glyma/core/brush_engine.hpp"
#include "glyma/core/command_history.hpp"
#include "glyma/core/commands.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/pixel.hpp"

namespace {

using glyma::core::AddLayerCommand;
using glyma::core::BrushMode;
using glyma::core::BrushSettings;
using glyma::core::BrushStroke;
using glyma::core::CommandHistory;
using glyma::core::Document;
using glyma::core::ImageBuffer;
using glyma::core::Layer;
using glyma::core::LayerId;
using glyma::core::Pixel;

BrushSettings settings(
    double diameter,
    Pixel color = Pixel{0, 0, 0, 255},
    float opacity = 1.0F,
    float hardness = 1.0F
) {
    BrushSettings result;
    result.diameter = diameter;
    result.color = color;
    result.opacity = opacity;
    result.hardness = hardness;
    return result;
}

std::size_t opaquePixels(const ImageBuffer& buffer) {
    std::size_t count = 0;
    for (const Pixel& pixel : buffer.pixels()) {
        if (pixel.a != 0) {
            ++count;
        }
    }
    return count;
}

bool alphaNear(Pixel pixel, int expected, int tolerance = 1) {
    return std::abs(static_cast<int>(pixel.a) - expected) <= tolerance;
}

void testSettingsValidation() {
    BrushSettings good;
    assert(good.isValid());
    good.validate();

    const auto expectInvalid = [](BrushSettings candidate) {
        if (candidate.isValid()) {
            return false;
        }
        try {
            candidate.validate();
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };

    auto bad = good;
    bad.diameter = 0.0;
    assert(expectInvalid(bad));
    bad.diameter = -2.0;
    assert(expectInvalid(bad));
    bad.diameter = std::numeric_limits<double>::infinity();
    assert(expectInvalid(bad));
    bad.diameter = BrushSettings::maxDiameter + 1.0;
    assert(expectInvalid(bad));

    bad = good;
    bad.opacity = -0.1F;
    assert(expectInvalid(bad));
    bad.opacity = 1.1F;
    assert(expectInvalid(bad));
    bad.opacity = std::numeric_limits<float>::quiet_NaN();
    assert(expectInvalid(bad));

    bad = good;
    bad.hardness = -0.1F;
    assert(expectInvalid(bad));
    bad.hardness = 1.1F;
    assert(expectInvalid(bad));
    bad.hardness = std::numeric_limits<float>::infinity();
    assert(expectInvalid(bad));

    ImageBuffer buffer(4, 4);
    bool threw = false;
    try {
        BrushStroke stroke(buffer, 1, bad, BrushMode::Paint);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testTinyDab() {
    ImageBuffer buffer(4, 4);
    BrushStroke stroke(buffer, 1, settings(1.0), BrushMode::Paint);

    assert(stroke.start(0.5, 0.5));
    assert(stroke.changeCount() == 1);
    assert(buffer.getPixel(0, 0) == Pixel(0, 0, 0, 255));
    assert(buffer.getPixel(1, 0).a == 0);
    assert(buffer.getPixel(0, 1).a == 0);
    assert(stroke.finish() != nullptr);
}

void testSolidCircleAndAntialiasing() {
    ImageBuffer buffer(16, 16);
    BrushStroke stroke(
        buffer, 1, settings(4.0, Pixel{255, 0, 0, 255}), BrushMode::Paint
    );

    assert(stroke.start(8.5, 8.5));
    assert(buffer.getPixel(8, 8) == Pixel(255, 0, 0, 255));
    assert(buffer.getPixel(9, 8) == Pixel(255, 0, 0, 255));
    assert(buffer.getPixel(7, 8) == Pixel(255, 0, 0, 255));
    assert(alphaNear(buffer.getPixel(10, 8), 128));
    assert(alphaNear(buffer.getPixel(6, 8), 128));
    assert(buffer.getPixel(11, 8).a == 0);
    assert(buffer.getPixel(5, 8).a == 0);
    assert(buffer.getPixel(8, 11).a == 0);
}

void testDiameterChangesCoverage() {
    ImageBuffer small(16, 16);
    BrushStroke smallStroke(small, 1, settings(1.0), BrushMode::Paint);
    assert(smallStroke.start(8.5, 8.5));
    assert(opaquePixels(small) == 1);

    ImageBuffer large(16, 16);
    BrushStroke largeStroke(large, 1, settings(7.0), BrushMode::Paint);
    assert(largeStroke.start(8.5, 8.5));
    assert(opaquePixels(large) > opaquePixels(small));
    assert(large.getPixel(8, 8) == Pixel(0, 0, 0, 255));
}

void testColorAndOpacity() {
    ImageBuffer buffer(8, 8);
    BrushStroke stroke(
        buffer,
        1,
        settings(1.0, Pixel{12, 34, 56, 200}),
        BrushMode::Paint
    );
    assert(stroke.start(4.5, 4.5));
    assert(buffer.getPixel(4, 4) == Pixel(12, 34, 56, 200));

    ImageBuffer half(8, 8);
    BrushStroke halfStroke(
        half, 1, settings(1.0, Pixel{0, 0, 0, 255}, 0.5F), BrushMode::Paint
    );
    assert(halfStroke.start(4.5, 4.5));
    assert(alphaNear(half.getPixel(4, 4), 128));

    ImageBuffer opaque(8, 8);
    opaque.clear(Pixel{0, 0, 255, 255});
    BrushStroke over(
        opaque, 1, settings(1.0, Pixel{255, 0, 0, 255}, 0.5F),
        BrushMode::Paint
    );
    assert(over.start(4.5, 4.5));
    const Pixel blended = opaque.getPixel(4, 4);
    assert(alphaNear(blended, 255));
    assert(std::abs(static_cast<int>(blended.r) - 128) <= 1);
    assert(blended.g == 0);
    assert(std::abs(static_cast<int>(blended.b) - 128) <= 1);
}

void testHardAndSoftBrushes() {
    ImageBuffer hard(16, 16);
    BrushStroke hardStroke(
        hard, 1, settings(4.0, Pixel{0, 0, 0, 255}, 1.0F, 1.0F),
        BrushMode::Paint
    );
    assert(hardStroke.start(8.5, 8.5));

    ImageBuffer soft(16, 16);
    BrushStroke softStroke(
        soft, 1, settings(4.0, Pixel{0, 0, 0, 255}, 1.0F, 0.0F),
        BrushMode::Paint
    );
    assert(softStroke.start(8.5, 8.5));

    assert(hard.getPixel(8, 8) == Pixel(0, 0, 0, 255));
    assert(soft.getPixel(8, 8) == Pixel(0, 0, 0, 255));
    assert(hard.getPixel(9, 8).a == 255);
    assert(alphaNear(soft.getPixel(9, 8), 153));
    assert(soft.getPixel(9, 8).a < hard.getPixel(9, 8).a);
    assert(hard.getPixel(10, 8).a > soft.getPixel(10, 8).a);
}

void testContinuousStrokeHasNoGaps() {
    ImageBuffer buffer(20, 6);
    BrushStroke stroke(buffer, 1, settings(3.0), BrushMode::Paint);
    assert(stroke.start(4.5, 3.5));
    assert(stroke.lineTo(15.5, 3.5));

    for (std::uint32_t x = 3; x <= 16; ++x) {
        assert(buffer.getPixel(x, 3).a == 255);
    }
    assert(buffer.getPixel(0, 3).a == 0);
    assert(buffer.getPixel(19, 3).a == 0);
}

void testEdgesAndClipping() {
    const Pixel black{0, 0, 0, 255};
    const auto dab = [](double x, double y) {
        ImageBuffer buffer(8, 8);
        BrushStroke stroke(buffer, 1, settings(3.0), BrushMode::Paint);
        assert(stroke.start(x, y));
        assert(stroke.finish() != nullptr);
        return buffer;
    };

    assert(dab(0.5, 0.5).getPixel(0, 0) == black);
    assert(dab(7.5, 0.5).getPixel(7, 0) == black);
    assert(dab(0.5, 7.5).getPixel(0, 7) == black);
    assert(dab(7.5, 7.5).getPixel(7, 7) == black);

    ImageBuffer outside(8, 8);
    BrushStroke outsideStroke(outside, 1, settings(3.0), BrushMode::Paint);
    assert(!outsideStroke.start(-10.0, -10.0));
    assert(outsideStroke.changeCount() == 0);
    assert(outsideStroke.finish() == nullptr);
    assert(opaquePixels(outside) == 0);

    ImageBuffer entering(8, 8);
    BrushStroke enteringStroke(entering, 1, settings(3.0), BrushMode::Paint);
    assert(!enteringStroke.start(-2.5, 3.5));
    assert(enteringStroke.lineTo(5.5, 3.5));
    assert(entering.getPixel(0, 3).a == 255);
}

void testPaintingTransparentAndOpaqueDestinations() {
    ImageBuffer transparent(4, 4);
    BrushStroke paint(
        transparent, 1, settings(1.0, Pixel{10, 20, 30, 255}), BrushMode::Paint
    );
    assert(paint.start(1.5, 1.5));
    assert(transparent.getPixel(1, 1) == (Pixel{10, 20, 30, 255}));

    ImageBuffer opaque(4, 4);
    opaque.clear(Pixel{10, 20, 30, 255});
    BrushStroke repaint(
        opaque, 1, settings(1.0, Pixel{200, 100, 50, 255}), BrushMode::Paint
    );
    assert(repaint.start(1.5, 1.5));
    assert(opaque.getPixel(1, 1) == (Pixel{200, 100, 50, 255}));
}

void testErasingOpaqueAndPartiallyTransparentPixels() {
    ImageBuffer opaque(9, 9);
    opaque.clear(Pixel{200, 100, 50, 255});
    BrushStroke erase(
        opaque, 1, settings(1.0, Pixel{255, 255, 255, 255}, 1.0F),
        BrushMode::Erase
    );
    assert(erase.start(4.5, 4.5));
    assert(opaque.getPixel(4, 4) == (Pixel{0, 0, 0, 0}));
    assert(opaque.getPixel(0, 0) == (Pixel{200, 100, 50, 255}));
    assert(opaque.getPixel(5, 4) == (Pixel{200, 100, 50, 255}));

    ImageBuffer partial(9, 9);
    partial.clear(Pixel{200, 100, 50, 128});
    BrushStroke halfErase(
        partial, 1, settings(1.0, Pixel{}, 0.5F), BrushMode::Erase
    );
    assert(halfErase.start(4.5, 4.5));
    const Pixel erased = partial.getPixel(4, 4);
    assert(erased.r == 200);
    assert(erased.g == 100);
    assert(erased.b == 50);
    assert(alphaNear(erased, 64));
}

void testEraserOpacityAndHardness() {
    ImageBuffer half(8, 8);
    half.clear(Pixel{10, 20, 30, 255});
    BrushStroke halfStroke(
        half, 1, settings(1.0, Pixel{}, 0.5F), BrushMode::Erase
    );
    assert(halfStroke.start(4.5, 4.5));
    assert(alphaNear(half.getPixel(4, 4), 128));

    ImageBuffer hard(16, 16);
    hard.clear(Pixel{10, 20, 30, 255});
    BrushStroke hardStroke(
        hard, 1, settings(4.0, Pixel{}, 1.0F, 1.0F), BrushMode::Erase
    );
    assert(hardStroke.start(8.5, 8.5));

    ImageBuffer soft(16, 16);
    soft.clear(Pixel{10, 20, 30, 255});
    BrushStroke softStroke(
        soft, 1, settings(4.0, Pixel{}, 1.0F, 0.0F), BrushMode::Erase
    );
    assert(softStroke.start(8.5, 8.5));

    assert(hard.getPixel(9, 8).a == 0);
    assert(alphaNear(soft.getPixel(9, 8), 102));
    assert(soft.getPixel(9, 8).a > hard.getPixel(9, 8).a);
    assert(hard.getPixel(0, 0) == (Pixel{10, 20, 30, 255}));
}

void testUndoRedoOfWholeStroke() {
    Document document("Doc", 20, 20);
    const LayerId id = document.addCanvasLayer("Paint");
    Layer* layer = document.findLayer(id);
    assert(layer != nullptr);

    const ImageBuffer original = layer->buffer();
    BrushStroke stroke(
        layer->buffer(),
        id,
        settings(4.0, Pixel{255, 0, 0, 255}, 0.8F, 0.7F),
        BrushMode::Paint
    );
    assert(stroke.start(2.5, 10.5));
    assert(stroke.lineTo(17.5, 10.5));
    assert(stroke.lineTo(17.5, 3.5));
    assert(stroke.lineTo(10.5, 8.5));
    assert(stroke.changeCount() > 1);

    auto command = stroke.finish();
    assert(command != nullptr);
    assert(command->changeCount() > 1);

    CommandHistory history;
    assert(history.pushExecuted(std::move(command)));
    assert(history.undoCount() == 1);
    const ImageBuffer painted = layer->buffer();
    assert(painted.pixels() != original.pixels());

    assert(history.undo(document));
    assert(layer->buffer().pixels() == original.pixels());

    assert(history.redo(document));
    assert(layer->buffer().pixels() == painted.pixels());
}

void testPushExecutedDiscardsRedoBranch() {
    Document document("Doc", 8, 8);
    const LayerId id = document.addCanvasLayer("A");

    CommandHistory history;
    assert(history.execute(
        std::make_unique<AddLayerCommand>("B", 8, 8), document
    ));
    assert(history.undo(document));
    assert(history.canRedo());

    Layer* layer = document.findLayer(id);
    assert(layer != nullptr);
    BrushStroke stroke(layer->buffer(), id, settings(2.0), BrushMode::Paint);
    assert(stroke.start(2.5, 2.5));
    auto command = stroke.finish();
    assert(command != nullptr);
    assert(history.pushExecuted(std::move(command)));
    assert(!history.canRedo());
    assert(history.undoCount() == 1);

    assert(!history.pushExecuted(nullptr));
}

void testCancelledStrokeRestoresPixels() {
    ImageBuffer buffer(20, 20);
    buffer.clear(Pixel{1, 2, 3, 4});
    const ImageBuffer original = buffer;

    BrushStroke stroke(buffer, 1, settings(4.0), BrushMode::Paint);
    assert(stroke.start(2.5, 2.5));
    assert(stroke.lineTo(17.5, 17.5));
    assert(stroke.changeCount() > 0);
    assert(stroke.cancel());
    assert(buffer.pixels() == original.pixels());
    assert(stroke.changeCount() == 0);
    assert(!stroke.active());
    assert(stroke.finish() == nullptr);
}

void testOnlyTargetLayerChanges() {
    Document document("Doc", 12, 12);
    const LayerId bottomId = document.addCanvasLayer("Bottom");
    const LayerId topId = document.addCanvasLayer("Top");
    Layer* bottom = document.findLayer(bottomId);
    Layer* top = document.findLayer(topId);
    assert(bottom != nullptr);
    assert(top != nullptr);

    const ImageBuffer bottomBefore = bottom->buffer();
    const ImageBuffer topBefore = top->buffer();

    BrushStroke stroke(top->buffer(), topId, settings(3.0), BrushMode::Paint);
    assert(stroke.start(6.5, 6.5));
    assert(stroke.lineTo(9.5, 6.5));
    assert(stroke.finish() != nullptr);

    assert(bottom->buffer().pixels() == bottomBefore.pixels());
    assert(top->buffer().pixels() != topBefore.pixels());
}

void testDirtyRegionTracksChanges() {
    ImageBuffer buffer(32, 32);
    BrushStroke stroke(buffer, 1, settings(6.0), BrushMode::Paint);

    // No call yet: nothing dirty.
    assert(stroke.lastDirtyRegion().isEmpty());

    assert(stroke.start(8.5, 9.5));
    const glyma::core::PixelRegion first = stroke.lastDirtyRegion();
    assert(!first.isEmpty());

    // The reported region must exactly bound the pixels that changed.
    const Pixel untouched{0, 0, 0, 0};
    int minX = 32;
    int minY = 32;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            if (buffer.getPixel(
                    static_cast<std::uint32_t>(x),
                    static_cast<std::uint32_t>(y)
                ) != untouched) {
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    }
    assert(maxX >= 0);
    assert(static_cast<int>(first.x) == minX);
    assert(static_cast<int>(first.y) == minY);
    assert(static_cast<int>(first.width) == maxX - minX + 1);
    assert(static_cast<int>(first.height) == maxY - minY + 1);

    // A later segment reports exactly the pixels changed by that call.
    const std::vector<Pixel> before = buffer.pixels();
    assert(stroke.lineTo(24.5, 9.5));
    const glyma::core::PixelRegion second = stroke.lastDirtyRegion();
    assert(!second.isEmpty());

    int sMinX = 32;
    int sMinY = 32;
    int sMaxX = -1;
    int sMaxY = -1;
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const std::size_t index =
                static_cast<std::size_t>(y) * 32 + static_cast<std::size_t>(x);
            if (buffer.getPixel(
                    static_cast<std::uint32_t>(x),
                    static_cast<std::uint32_t>(y)
                ) != before[index]) {
                sMinX = std::min(sMinX, x);
                sMinY = std::min(sMinY, y);
                sMaxX = std::max(sMaxX, x);
                sMaxY = std::max(sMaxY, y);
            }
        }
    }
    assert(sMaxX >= 0);
    assert(static_cast<int>(second.x) == sMinX);
    assert(static_cast<int>(second.y) == sMinY);
    assert(static_cast<int>(second.width) == sMaxX - sMinX + 1);
    assert(static_cast<int>(second.height) == sMaxY - sMinY + 1);
}

void testDirtyRegionEmptyOffCanvas() {
    ImageBuffer buffer(16, 16);
    BrushStroke stroke(buffer, 1, settings(4.0), BrushMode::Paint);
    // A dab entirely outside the buffer changes nothing and reports no region.
    assert(!stroke.start(-100.0, -100.0));
    assert(stroke.lastDirtyRegion().isEmpty());
}

void testDeterminism() {
    const auto run = [] {
        ImageBuffer buffer(24, 24);
        BrushStroke stroke(
            buffer,
            1,
            settings(6.0, Pixel{9, 8, 7, 255}, 0.65F, 0.3F),
            BrushMode::Paint
        );
        assert(stroke.start(3.2, 4.1));
        assert(stroke.lineTo(18.7, 9.4));
        assert(stroke.lineTo(7.3, 20.2));
        return buffer;
    };

    const ImageBuffer first = run();
    const ImageBuffer second = run();
    assert(first.pixels() == second.pixels());
}

} // namespace

int main() {
    testSettingsValidation();
    testTinyDab();
    testSolidCircleAndAntialiasing();
    testDiameterChangesCoverage();
    testColorAndOpacity();
    testHardAndSoftBrushes();
    testContinuousStrokeHasNoGaps();
    testEdgesAndClipping();
    testPaintingTransparentAndOpaqueDestinations();
    testErasingOpaqueAndPartiallyTransparentPixels();
    testEraserOpacityAndHardness();
    testUndoRedoOfWholeStroke();
    testPushExecutedDiscardsRedoBranch();
    testCancelledStrokeRestoresPixels();
    testOnlyTargetLayerChanges();
    testDirtyRegionTracksChanges();
    testDirtyRegionEmptyOffCanvas();
    testDeterminism();

    std::cout << "All Glyma brush tests passed!\n";
    return 0;
}

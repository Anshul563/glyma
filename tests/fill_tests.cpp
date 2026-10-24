#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "glyma/core/commands.hpp"
#include "glyma/core/document.hpp"
#include <cassert>
#include "glyma/core/fill.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/selection.hpp"

using glyma::core::Document;
using glyma::core::FillCommand;
using glyma::core::FillSettings;
using glyma::core::ImageBuffer;
using glyma::core::Layer;
using glyma::core::LayerId;
using glyma::core::Pixel;
using glyma::core::Selection;

void testFillUniformImage() {
    ImageBuffer buffer(4, 4);
    buffer.clear((Pixel{100, 100, 100, 255}));

    Selection selection;
    selection.setDocumentSize(4, 4);
    FillSettings settings;
    settings.color = (Pixel{255, 0, 0, 255});
    settings.opacity = 1.0F;
    settings.tolerance = 0;

    const std::vector<Pixel> before = buffer.pixels();
    const auto changes =
        glyma::core::fillRegion(buffer, 2, 2, settings, selection);

    assert(!changes.empty());
    assert(changes.size() == 16);
    for (std::uint32_t y = 0; y < 4; ++y) {
        for (std::uint32_t x = 0; x < 4; ++x) {
            assert(buffer.getPixel(x, y) == (Pixel{255, 0, 0, 255}));
        }
    }
    // Every pixel changed from the before state.
    assert(before != buffer.pixels());
}

void testFillBoundedRegion() {
    ImageBuffer buffer(5, 5);
    buffer.clear((Pixel{0, 0, 0, 255}));
    for (std::uint32_t y = 1; y <= 3; ++y) {
        for (std::uint32_t x = 1; x <= 3; ++x) {
            buffer.setPixel(x, y, (Pixel{255, 255, 255, 255}));
        }
    }

    Selection selection;
    selection.setDocumentSize(5, 5);
    FillSettings settings;
    settings.color = (Pixel{255, 0, 0, 255});
    settings.opacity = 1.0F;
    settings.tolerance = 0;

    const auto changes = glyma::core::fillRegion(buffer, 2, 2, settings, selection);
    assert(changes.size() == 9);

    for (std::uint32_t y = 1; y <= 3; ++y) {
        for (std::uint32_t x = 1; x <= 3; ++x) {
            assert(buffer.getPixel(x, y) == (Pixel{255, 0, 0, 255}));
        }
    }
    // Surrounding black pixels are unchanged.
    assert(buffer.getPixel(0, 0) == ((Pixel{0, 0, 0, 255})));
    assert(buffer.getPixel(1, 0) == ((Pixel{0, 0, 0, 255})));
    assert(buffer.getPixel(0, 1) == ((Pixel{0, 0, 0, 255})));
    assert(buffer.getPixel(4, 4) == ((Pixel{0, 0, 0, 255})));
}

void testDisconnectedRegionsUnchanged() {
    ImageBuffer buffer(5, 5);
    buffer.clear((Pixel{0, 0, 0, 255}));
    buffer.setPixel(1, 1, (Pixel{255, 255, 255, 255}));
    buffer.setPixel(3, 3, (Pixel{255, 255, 255, 255})); // disconnected

    Selection selection;
    selection.setDocumentSize(5, 5);
    FillSettings settings;
    settings.color = (Pixel{255, 0, 0, 255});
    settings.opacity = 1.0F;
    settings.tolerance = 0;

    const auto changes = glyma::core::fillRegion(buffer, 1, 1, settings, selection);
    assert(changes.size() == 1);
    assert(buffer.getPixel(1, 1) == ((Pixel{255, 0, 0, 255})));
    // The disconnected white pixel is unchanged.
    assert(buffer.getPixel(3, 3) == ((Pixel{255, 255, 255, 255})));
}

void testFourConnectedDiagonalBoundary() {
    ImageBuffer buffer(3, 3);
    buffer.clear((Pixel{0, 0, 0, 255}));
    buffer.setPixel(0, 0, (Pixel{255, 255, 255, 255}));
    buffer.setPixel(2, 2, (Pixel{255, 255, 255, 255})); // diagonal, not connected

    Selection selection;
    selection.setDocumentSize(3, 3);
    FillSettings settings;
    settings.color = (Pixel{255, 0, 0, 255});
    settings.opacity = 1.0F;
    settings.tolerance = 0;

    const auto changes = glyma::core::fillRegion(buffer, 0, 0, settings, selection);
    assert(changes.size() == 1);
    assert(buffer.getPixel(0, 0) == ((Pixel{255, 0, 0, 255})));
    // The diagonal pixel and the center are unchanged.
    assert(buffer.getPixel(1, 1) == ((Pixel{0, 0, 0, 255})));
    assert(buffer.getPixel(2, 2) == ((Pixel{255, 255, 255, 255})));
}

void testToleranceBehavior() {
    ImageBuffer buffer(2, 1);
    buffer.setPixel(0, 0, (Pixel{100, 100, 100, 255}));
    buffer.setPixel(1, 0, (Pixel{130, 100, 100, 255})); // red channel differs by 30

    Selection selection;
    selection.setDocumentSize(2, 1);

    // Exact match only: only the seed pixel fills.
    {
        FillSettings settings;
        settings.color = (Pixel{0, 0, 255, 255});
        settings.opacity = 1.0F;
        settings.tolerance = 0;
        const auto changes = glyma::core::fillRegion(buffer, 0, 0, settings, selection);
        assert(changes.size() == 1);
        assert(buffer.getPixel(0, 0) == ((Pixel{0, 0, 255, 255})));
        assert(buffer.getPixel(1, 0) == ((Pixel{130, 100, 100, 255})));
    }

    // Restore pixel 0, then a tolerance of 30 matches both.
    buffer.setPixel(0, 0, (Pixel{100, 100, 100, 255}));
    {
        FillSettings settings;
        settings.color = (Pixel{0, 0, 255, 255});
        settings.opacity = 1.0F;
        settings.tolerance = 30;
        const auto changes = glyma::core::fillRegion(buffer, 0, 0, settings, selection);
        assert(changes.size() == 2);
        assert(buffer.getPixel(0, 0) == ((Pixel{0, 0, 255, 255})));
        assert(buffer.getPixel(1, 0) == ((Pixel{0, 0, 255, 255})));
    }
}

void testTransparentPixelBehavior() {
    ImageBuffer buffer(2, 1);
    buffer.setPixel(0, 0, (Pixel{0, 0, 0, 0}));       // fully transparent
    buffer.setPixel(1, 0, (Pixel{0, 0, 0, 128}));     // semitransparent

    Selection selection;
    selection.setDocumentSize(2, 1);
    FillSettings settings;
    settings.color = (Pixel{255, 255, 255, 255});
    settings.opacity = 1.0F;
    settings.tolerance = 0;

    const auto changes = glyma::core::fillRegion(buffer, 0, 0, settings, selection);
    assert(changes.size() == 1);
    // Filling a fully transparent pixel with opaque white yields opaque white.
    assert(buffer.getPixel(0, 0) == ((Pixel{255, 255, 255, 255})));
    // Alpha differs from seed (128 vs 0), outside tolerance 0 -> unchanged.
    assert(buffer.getPixel(1, 0) == ((Pixel{0, 0, 0, 128})));
}

void testRespectingSelection() {
    ImageBuffer buffer(3, 3);
    buffer.clear((Pixel{100, 100, 100, 255}));

    Selection selection;
    selection.setDocumentSize(3, 3);
    selection.setRectangle(0.0, 0.0, 2.0, 2.0); // top-left 2x2 pixels selected

    FillSettings settings;
    settings.color = (Pixel{255, 0, 0, 255});
    settings.opacity = 1.0F;
    settings.tolerance = 0;

    const auto changes = glyma::core::fillRegion(buffer, 1, 1, settings, selection);
    assert(changes.size() == 4);

    for (std::uint32_t y = 0; y <= 1; ++y) {
        for (std::uint32_t x = 0; x <= 1; ++x) {
            assert(buffer.getPixel(x, y) == ((Pixel{255, 0, 0, 255})));
        }
    }
    // Pixels outside the selection are unchanged even though they match.
    assert(buffer.getPixel(2, 2) == ((Pixel{100, 100, 100, 255})));
    assert(buffer.getPixel(2, 0) == ((Pixel{100, 100, 100, 255})));
    assert(buffer.getPixel(0, 2) == ((Pixel{100, 100, 100, 255})));
}

void testSeedOutsideSelectionIsNoOp() {
    ImageBuffer buffer(3, 3);
    buffer.clear((Pixel{100, 100, 100, 255}));

    Selection selection;
    selection.setDocumentSize(3, 3);
    selection.setRectangle(0.0, 0.0, 1.0, 1.0); // only pixel (0, 0) selected

    FillSettings settings;
    settings.color = (Pixel{255, 0, 0, 255});
    settings.opacity = 1.0F;
    settings.tolerance = 0;

    const auto changes = glyma::core::fillRegion(buffer, 2, 2, settings, selection);
    assert(changes.empty());
    assert(buffer.getPixel(2, 2) == ((Pixel{100, 100, 100, 255})));
}

void testFillClippedToBufferBounds() {
    ImageBuffer buffer(3, 3);
    buffer.clear((Pixel{255, 255, 255, 255}));

    Selection selection;
    selection.setDocumentSize(3, 3);
    FillSettings settings;
    settings.color = (Pixel{0, 0, 0, 255});
    settings.opacity = 1.0F;
    settings.tolerance = 0;

    // Seed at the corner fills the whole image (clipped to bounds).
    const auto changes = glyma::core::fillRegion(buffer, 0, 0, settings, selection);
    assert(changes.size() == 9);
    for (std::uint32_t y = 0; y < 3; ++y) {
        for (std::uint32_t x = 0; x < 3; ++x) {
            assert(buffer.getPixel(x, y) == ((Pixel{0, 0, 0, 255})));
        }
    }
}

void testNoOpFill() {
    ImageBuffer buffer(2, 2);
    buffer.clear((Pixel{255, 0, 0, 255}));

    Selection selection;
    selection.setDocumentSize(2, 2);
    FillSettings settings;
    settings.color = (Pixel{255, 0, 0, 255}); // same as seed
    settings.opacity = 1.0F;
    settings.tolerance = 0;

    const auto changes = glyma::core::fillRegion(buffer, 0, 0, settings, selection);
    assert(changes.empty());
    assert(buffer.getPixel(0, 0) == ((Pixel{255, 0, 0, 255})));
}

void testLargeRegionWithoutStackOverflow() {
    constexpr std::uint32_t size = 1000;
    ImageBuffer buffer(size, size);
    buffer.clear((Pixel{0, 0, 255, 255}));

    Selection selection;
    selection.setDocumentSize(size, size);
    FillSettings settings;
    settings.color = (Pixel{0, 255, 0, 255});
    settings.opacity = 1.0F;
    settings.tolerance = 0;

    const auto changes =
        glyma::core::fillRegion(buffer, size / 2, size / 2, settings, selection);
    assert(changes.size() == size * size);
    for (std::uint32_t y = 0; y < size; ++y) {
        for (std::uint32_t x = 0; x < size; ++x) {
            assert(buffer.getPixel(x, y) == ((Pixel{0, 255, 0, 255})));
        }
    }
}

void testFillCommandUndoRedo() {
    Document document("Fill", 4, 4);
    const LayerId id = document.addCanvasLayer("Layer 1");
    Layer* layer = document.findLayer(id);
    assert(layer != nullptr);
    layer->buffer().clear((Pixel{50, 50, 50, 255}));

    Selection selection;
    selection.setDocumentSize(4, 4);
    FillSettings settings;
    settings.color = (Pixel{200, 100, 50, 255});
    settings.opacity = 1.0F;
    settings.tolerance = 0;

    auto changes = glyma::core::fillRegion(layer->buffer(), 2, 2, settings, selection);
    assert(!changes.empty());
    const std::size_t changeCount = changes.size();

    auto command = std::make_unique<FillCommand>(id, std::move(changes));
    assert(command->changeCount() == changeCount);

    assert(command->execute(document));
    assert(layer->buffer().getPixel(2, 2) == settings.color);

    assert(command->undo(document));
    assert(layer->buffer().getPixel(2, 2) == ((Pixel{50, 50, 50, 255})));

    assert(command->execute(document)); // redo
    assert(layer->buffer().getPixel(2, 2) == settings.color);
}

void testMultipleFillsUndoOrder() {
    Document document("Fills", 4, 4);
    const LayerId id = document.addCanvasLayer("Layer 1");
    Layer* layer = document.findLayer(id);
    assert(layer != nullptr);
    layer->buffer().clear((Pixel{100, 100, 100, 255}));

    Selection selection;
    selection.setDocumentSize(4, 4);

    // A gray image with a black vertical barrier at x = 2, separating a left
    // region (x in {0,1}) from a right region (x = 3). Both regions start gray.
    for (std::uint32_t y = 0; y < 4; ++y) {
        layer->buffer().setPixel(2, y, (Pixel{0, 0, 0, 255}));
    }

    FillSettings red;
    red.color = (Pixel{255, 0, 0, 255});
    red.opacity = 1.0F;
    red.tolerance = 0;

    FillSettings blue;
    blue.color = (Pixel{0, 0, 255, 255});
    blue.opacity = 1.0F;
    blue.tolerance = 0;

    auto changesA = glyma::core::fillRegion(layer->buffer(), 0, 0, red, selection);
    auto cmdA = std::make_unique<FillCommand>(id, std::move(changesA));
    assert(cmdA->execute(document));
    assert(layer->buffer().getPixel(0, 0) == red.color);
    assert(layer->buffer().getPixel(1, 0) == red.color);
    assert(layer->buffer().getPixel(0, 3) == red.color);
    assert(layer->buffer().getPixel(2, 0) == (Pixel{0, 0, 0, 255})); // barrier

    auto changesB = glyma::core::fillRegion(layer->buffer(), 3, 0, blue, selection);
    auto cmdB = std::make_unique<FillCommand>(id, std::move(changesB));
    assert(cmdB->execute(document));
    assert(layer->buffer().getPixel(3, 0) == blue.color);
    assert(layer->buffer().getPixel(3, 3) == blue.color);

    // Undo the most recent fill first; the right region returns to gray.
    assert(cmdB->undo(document));
    assert(layer->buffer().getPixel(3, 0) == (Pixel{100, 100, 100, 255}));
    assert(layer->buffer().getPixel(3, 3) == (Pixel{100, 100, 100, 255}));
    assert(layer->buffer().getPixel(0, 0) == red.color); // fill A still applied

    assert(cmdA->undo(document));
    assert(layer->buffer().getPixel(0, 0) == (Pixel{100, 100, 100, 255}));
    assert(layer->buffer().getPixel(1, 0) == (Pixel{100, 100, 100, 255}));

    // Redo in order.
    assert(cmdA->execute(document));
    assert(cmdB->execute(document));
    assert(layer->buffer().getPixel(0, 0) == red.color);
    assert(layer->buffer().getPixel(3, 0) == blue.color);
}

int main() {
    testFillUniformImage();
    testFillBoundedRegion();
    testDisconnectedRegionsUnchanged();
    testFourConnectedDiagonalBoundary();
    testToleranceBehavior();
    testTransparentPixelBehavior();
    testRespectingSelection();
    testSeedOutsideSelectionIsNoOp();
    testFillClippedToBufferBounds();
    testNoOpFill();
    testLargeRegionWithoutStackOverflow();
    testFillCommandUndoRedo();
    testMultipleFillsUndoOrder();

    std::cout << "All Glyma fill tests passed\n";
    return 0;
}

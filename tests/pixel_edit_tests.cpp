#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/pixel_edit.hpp"

using glyma::core::Document;
using glyma::core::ImageBuffer;
using glyma::core::makePixelEditCommand;
using glyma::core::Pixel;
using glyma::core::PixelEditCommand;

namespace {

void testSparseCommandUndoRedo() {
    Document document("Edit", 4, 4);
    const auto id = document.addCanvasLayer("Layer 1");
    glyma::core::Layer* layer = document.findLayer(id);
    assert(layer != nullptr);

    layer->buffer().clear((Pixel{10, 20, 30, 255}));
    const ImageBuffer before = layer->buffer();

    ImageBuffer after = before;
    after.setPixel(0, 0, (Pixel{255, 0, 0, 255}));
    after.setPixel(3, 2, (Pixel{0, 255, 0, 128}));

    auto command = makePixelEditCommand(id, before, after);
    assert(command != nullptr);
    assert(!command->usesSnapshot());
    assert(command->changeCount() == 2);

    // Restore the "before" state, then exercise execute/undo/redo.
    layer->buffer() = before;
    assert(command->execute(document));
    assert(layer->buffer().pixels() == after.pixels());

    assert(command->undo(document));
    assert(layer->buffer().pixels() == before.pixels());

    assert(command->execute(document));
    assert(layer->buffer().pixels() == after.pixels());
}

void testDenseCommandUsesSnapshot() {
    Document document("Edit", 4, 4);
    const auto id = document.addCanvasLayer("Layer 1");
    glyma::core::Layer* layer = document.findLayer(id);
    assert(layer != nullptr);

    layer->buffer().clear((Pixel{10, 10, 10, 255}));
    const ImageBuffer before = layer->buffer();

    ImageBuffer after = before;
    for (std::uint32_t y = 0; y < after.height(); ++y) {
        for (std::uint32_t x = 0; x < after.width(); ++x) {
            after.setPixel(x, y, (Pixel{200, 100, 50, 255}));
        }
    }

    auto command = makePixelEditCommand(id, before, after);
    assert(command != nullptr);
    assert(command->usesSnapshot());
    assert(command->changeCount() == 16);

    layer->buffer() = after;
    assert(command->undo(document));
    assert(layer->buffer().pixels() == before.pixels());
    assert(command->execute(document));
    assert(layer->buffer().pixels() == after.pixels());
}

void testIdenticalBuffersProduceNoCommand() {
    ImageBuffer buffer(3, 3);
    buffer.clear((Pixel{5, 6, 7, 255}));
    auto command = makePixelEditCommand(1, buffer, buffer);
    assert(command == nullptr);
}

void testDimensionMismatchThrows() {
    ImageBuffer a(2, 2);
    ImageBuffer b(3, 3);
    bool threw = false;
    try {
        (void)makePixelEditCommand(1, a, b);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testMissingLayerFailsCleanly() {
    Document document("Edit", 2, 2);
    ImageBuffer before(2, 2);
    before.clear((Pixel{0, 0, 0, 255}));
    ImageBuffer after = before;
    after.setPixel(0, 0, (Pixel{255, 255, 255, 255}));

    auto command = makePixelEditCommand(999999, before, after);
    assert(command != nullptr);
    // No layer with that ID exists, so applying must fail without crashing.
    assert(!command->execute(document));
    assert(!command->undo(document));
}

void testLayerMetadataIntact() {
    Document document("Edit", 3, 3);
    const auto id = document.addCanvasLayer("Keep");
    glyma::core::Layer* layer = document.findLayer(id);
    assert(layer != nullptr);
    layer->setOpacity(0.5F);
    layer->setVisible(false);
    layer->setPosition(3, 4);

    layer->buffer().clear((Pixel{1, 2, 3, 255}));
    const ImageBuffer before = layer->buffer();
    ImageBuffer after = before;
    after.setPixel(1, 1, (Pixel{9, 9, 9, 255}));

    auto command = makePixelEditCommand(id, before, after);
    assert(command != nullptr);
    assert(command->execute(document));

    const glyma::core::Layer* applied = document.findLayer(id);
    assert(applied != nullptr);
    assert(applied->name() == "Keep");
    assert(applied->opacity() == 0.5F);
    assert(!applied->visible());
    assert(applied->x() == 3);
    assert(applied->y() == 4);
}

} // namespace

int main() {
    testSparseCommandUndoRedo();
    testDenseCommandUsesSnapshot();
    testIdenticalBuffersProduceNoCommand();
    testDimensionMismatchThrows();
    testMissingLayerFailsCleanly();
    testLayerMetadataIntact();

    std::cout << "All Glyma pixel edit tests passed\n";
    return 0;
}

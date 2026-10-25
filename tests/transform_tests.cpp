#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "glyma/core/command_history.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/pixel.hpp"
#include "glyma/core/selection.hpp"
#include "glyma/core/transform.hpp"

using glyma::core::Anchor;
using glyma::core::ArbitraryRotateDocumentCommand;
using glyma::core::ArbitraryRotateLayerCommand;
using glyma::core::CanvasResizeCommand;
using glyma::core::CommandHistory;
using glyma::core::CropCommand;
using glyma::core::CropRect;
using glyma::core::Document;
using glyma::core::DocumentRotation;
using glyma::core::FlipDocumentCommand;
using glyma::core::ImageBuffer;
using glyma::core::ImageResizeCommand;
using glyma::core::Layer;
using glyma::core::LayerTransform;
using glyma::core::Pixel;
using glyma::core::RotateDocumentCommand;
using glyma::core::ScaleLayerCommand;
using glyma::core::Selection;
using glyma::core::TransformLayerCommand;
using glyma::core::TranslateLayerCommand;

namespace {

constexpr Pixel kA{10, 20, 30, 255};
constexpr Pixel kB{200, 100, 50, 255};
constexpr Pixel kC{1, 2, 3, 255};
constexpr Pixel kD{250, 240, 230, 255};
constexpr Pixel kE{77, 88, 99, 255};
constexpr Pixel kF{5, 6, 7, 255};

ImageBuffer filled(std::uint32_t w, std::uint32_t h, Pixel color) {
    ImageBuffer buffer(w, h);
    buffer.clear(color);
    return buffer;
}

// A 2x3 source with six distinct pixels: row-major 1..6.
ImageBuffer labelled2x3() {
    ImageBuffer buffer(2, 3);
    buffer.setPixel(0, 0, Pixel{1, 1, 1, 255});
    buffer.setPixel(1, 0, Pixel{2, 2, 2, 255});
    buffer.setPixel(0, 1, Pixel{3, 3, 3, 255});
    buffer.setPixel(1, 1, Pixel{4, 4, 4, 255});
    buffer.setPixel(0, 2, Pixel{5, 5, 5, 255});
    buffer.setPixel(1, 2, Pixel{6, 6, 6, 255});
    return buffer;
}

Pixel gray(std::uint8_t v) {
    return Pixel{v, v, v, 255};
}

// ---------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------

void testCanvasValidation() {
    assert(!glyma::core::isValidCanvasSize(0, 1));
    assert(!glyma::core::isValidCanvasSize(1, 0));
    assert(glyma::core::isValidCanvasSize(1, 1));
    assert(glyma::core::isValidCanvasSize(8192, 8192));
    assert(!glyma::core::isValidCanvasSize(8193, 8192));

    assert(glyma::core::anchorFactorX(Anchor::TopLeft) == 0.0);
    assert(glyma::core::anchorFactorX(Anchor::Center) == 0.5);
    assert(glyma::core::anchorFactorX(Anchor::BottomRight) == 1.0);
    assert(glyma::core::anchorFactorY(Anchor::TopLeft) == 0.0);
    assert(glyma::core::anchorFactorY(Anchor::Center) == 0.5);
    assert(glyma::core::anchorFactorY(Anchor::BottomRight) == 1.0);
}

void testClipCropRect() {
    const CropRect inside{1, 2, 3, 4};
    const CropRect clipped = glyma::core::clipCropRect(inside, 10, 10);
    assert(clipped.x == 1 && clipped.y == 2);
    assert(clipped.width == 3 && clipped.height == 4);

    // Extending past the left/top and right/bottom edges.
    const CropRect oversized{-5, -5, 20, 20};
    const CropRect clamped = glyma::core::clipCropRect(oversized, 10, 10);
    assert(clamped.x == 0 && clamped.y == 0);
    assert(clamped.width == 10 && clamped.height == 10);

    // No overlap.
    const CropRect outside{20, 20, 5, 5};
    assert(glyma::core::clipCropRect(outside, 10, 10).isEmpty());

    assert((CropRect{0, 0, 0, 5}.isEmpty()));
    assert((CropRect{0, 0, 5, 0}.isEmpty()));
    assert((CropRect{2, 3, 4, 5}.right() == 6));
    assert((CropRect{2, 3, 4, 5}.bottom() == 8));
}

void testCropRectFromPoints() {
    const CropRect a = glyma::core::cropRectFromPoints(2.2, 3.9, 7.1, 8.4, 20, 20);
    const CropRect b = glyma::core::cropRectFromPoints(7.1, 8.4, 2.2, 3.9, 20, 20);
    assert(a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height);
    assert(a.x == 2 && a.y == 3);
    assert(a.width == 6 && a.height == 6); // floor(2.2)..ceil(7.1)=3..8

    // Clamped to the canvas.
    const CropRect clamped =
        glyma::core::cropRectFromPoints(-10.0, -10.0, 100.0, 100.0, 20, 20);
    assert(clamped.x == 0 && clamped.y == 0);
    assert(clamped.width == 20 && clamped.height == 20);

    // Degenerate and non-finite inputs.
    assert(glyma::core::cropRectFromPoints(5.0, 5.0, 5.0, 5.0, 20, 20).isEmpty());
    assert(glyma::core::cropRectFromPoints(
               std::nan(""), 0.0, 5.0, 5.0, 20, 20).isEmpty());
}

// ---------------------------------------------------------------------------
// Image primitives
// ---------------------------------------------------------------------------

void testCropImage() {
    ImageBuffer source(3, 3);
    source.clear(kA);
    source.setPixel(2, 2, kB);

    // Exact sub-region at (1,1) 2x2.
    const ImageBuffer sub = glyma::core::cropImage(source, 1, 1, 2, 2);
    assert(sub.width() == 2 && sub.height() == 2);
    assert(sub.getPixel(0, 0) == kA);
    assert(sub.getPixel(1, 1) == kB);

    // Negative origin: uncovered destination pixels are transparent.
    const ImageBuffer shifted = glyma::core::cropImage(source, -1, -1, 2, 2);
    assert(shifted.getPixel(0, 0) == (Pixel{0, 0, 0, 0}));
    assert(shifted.getPixel(1, 1) == kA);

    bool threw = false;
    try {
        (void)glyma::core::cropImage(source, 0, 0, 0, 2);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testScaleImage() {
    ImageBuffer source(2, 2);
    source.clear(kA);
    source.setPixel(1, 1, kB);

    // Identity returns the same pixels.
    const ImageBuffer identity = glyma::core::scaleImage(source, 2, 2);
    assert(identity.pixels() == source.pixels());

    // Downscale a solid buffer stays solid.
    const ImageBuffer solid = glyma::core::scaleImage(filled(4, 4, kC), 2, 2);
    assert(solid.width() == 2 && solid.height() == 2);
    for (const Pixel& p : solid.pixels()) {
        assert(p == kC);
    }

    // Upscale dimensions are exact.
    const ImageBuffer up = glyma::core::scaleImage(source, 8, 6);
    assert(up.width() == 8 && up.height() == 6);

    bool threw = false;
    try {
        (void)glyma::core::scaleImage(source, 0, 4);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testQuarterTurnsAndFlips() {
    const ImageBuffer source = labelled2x3();

    const ImageBuffer cw = glyma::core::rotateImage90(source, true);
    assert(cw.width() == 3 && cw.height() == 2);
    // result(2-y, x) = source(x, y)
    assert(cw.getPixel(2, 0) == gray(1));
    assert(cw.getPixel(2, 1) == gray(2));
    assert(cw.getPixel(1, 0) == gray(3));
    assert(cw.getPixel(1, 1) == gray(4));
    assert(cw.getPixel(0, 0) == gray(5));
    assert(cw.getPixel(0, 1) == gray(6));

    const ImageBuffer ccw = glyma::core::rotateImage90(source, false);
    assert(ccw.width() == 3 && ccw.height() == 2);
    assert(ccw.getPixel(0, 0) == gray(2));
    assert(ccw.getPixel(0, 1) == gray(1));
    assert(ccw.getPixel(1, 0) == gray(4));
    assert(ccw.getPixel(1, 1) == gray(3));
    assert(ccw.getPixel(2, 0) == gray(6));
    assert(ccw.getPixel(2, 1) == gray(5));

    const ImageBuffer turn180 = glyma::core::rotateImage180(source);
    assert(turn180.width() == 2 && turn180.height() == 3);
    assert(turn180.getPixel(1, 2) == gray(1));
    assert(turn180.getPixel(0, 2) == gray(2));
    assert(turn180.getPixel(1, 1) == gray(3));
    assert(turn180.getPixel(0, 0) == gray(6));

    const ImageBuffer hflip = glyma::core::flipImage(source, true);
    assert(hflip.getPixel(1, 0) == gray(1));
    assert(hflip.getPixel(0, 0) == gray(2));
    assert(hflip.getPixel(1, 2) == gray(5));

    const ImageBuffer vflip = glyma::core::flipImage(source, false);
    assert(vflip.getPixel(0, 2) == gray(1));
    assert(vflip.getPixel(0, 0) == gray(5));
    assert(vflip.getPixel(1, 0) == gray(6));
}

void testArbitraryRotation() {
    const ImageBuffer source = labelled2x3();

    // Quarter-turn angles delegate to the exact routines.
    assert(glyma::core::rotateImage(source, 90.0).pixels() ==
           glyma::core::rotateImage90(source, true).pixels());
    assert(glyma::core::rotateImage(source, -90.0).pixels() ==
           glyma::core::rotateImage90(source, false).pixels());
    assert(glyma::core::rotateImage(source, 180.0).pixels() ==
           glyma::core::rotateImage180(source).pixels());
    assert(glyma::core::rotateImage(source, 0.0).pixels() == source.pixels());
    assert(glyma::core::rotateImage(source, 360.0).pixels() == source.pixels());

    // A 45 degree rotation expands to the axis-aligned bounding box and keeps
    // the centre colour intact.
    const ImageBuffer square = filled(4, 4, kA);
    const ImageBuffer rotated = glyma::core::rotateImage(square, 45.0);
    assert(rotated.width() == 6 && rotated.height() == 6);
    const Pixel centre = rotated.getPixel(3, 3);
    assert(centre.r == kA.r && centre.g == kA.g && centre.b == kA.b);
    assert(centre.a == 255);

    bool threw = false;
    try {
        (void)glyma::core::rotateImage(source, std::nan(""));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// ---------------------------------------------------------------------------
// Document operations
// ---------------------------------------------------------------------------

void testCropDocument() {
    Document doc("Crop", 10, 10);
    Layer layer("L", 4, 4);
    layer.buffer().clear(kA);
    layer.setPosition(2, 3);
    const auto id = doc.addLayer(std::move(layer));

    Selection selection;
    selection.setDocumentSize(10, 10);
    selection.setRectangle(3.0, 3.0, 6.0, 6.0);
    doc.setSelection(selection);

    glyma::core::cropDocument(doc, CropRect{2, 3, 5, 4});
    assert(doc.width() == 5 && doc.height() == 4);

    const Layer* cropped = doc.findLayer(id);
    assert(cropped != nullptr);
    assert(cropped->x() == 0 && cropped->y() == 0);
    assert(cropped->width() == 4 && cropped->height() == 4);

    double l = 0, t = 0, r = 0, b = 0;
    assert(doc.selection().bounds(l, t, r, b));
    assert(std::abs(l - 1.0) < 1e-9 && std::abs(t - 0.0) < 1e-9);
    assert(std::abs(r - 4.0) < 1e-9 && std::abs(b - 3.0) < 1e-9);

    // A layer fully outside the crop survives with a 1x1 transparent buffer.
    Document doc2("Crop2", 10, 10);
    Layer far("Far", 2, 2);
    far.buffer().clear(kB);
    far.setPosition(8, 8);
    const auto farId = doc2.addLayer(std::move(far));
    glyma::core::cropDocument(doc2, CropRect{0, 0, 4, 4});
    const Layer* kept = doc2.findLayer(farId);
    assert(kept != nullptr);
    assert(kept->width() == 1 && kept->height() == 1);
    assert(kept->buffer().getPixel(0, 0) == (Pixel{0, 0, 0, 0}));
    assert(kept->x() == 8 && kept->y() == 8);

    bool threw = false;
    try {
        glyma::core::cropDocument(doc2, CropRect{20, 20, 2, 2});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testResizeCanvas() {
    Document doc("Resize", 4, 4);
    Layer layer("L", 4, 4);
    layer.setPosition(0, 0);
    const auto id = doc.addLayer(std::move(layer));

    glyma::core::resizeCanvas(doc, 8, 8, Anchor::Center);
    assert(doc.width() == 8 && doc.height() == 8);
    assert(doc.findLayer(id)->x() == 2 && doc.findLayer(id)->y() == 2);

    glyma::core::resizeCanvas(doc, 12, 12, Anchor::TopLeft);
    assert(doc.findLayer(id)->x() == 2 && doc.findLayer(id)->y() == 2);

    glyma::core::resizeCanvas(doc, 16, 16, Anchor::BottomRight);
    assert(doc.findLayer(id)->x() == 6 && doc.findLayer(id)->y() == 6);

    // Shrinking with a centre anchor moves content negative (clipped later).
    Document doc2("Shrink", 4, 4);
    Layer l2("L", 4, 4);
    const auto id2 = doc2.addLayer(std::move(l2));
    glyma::core::resizeCanvas(doc2, 2, 2, Anchor::Center);
    assert(doc2.findLayer(id2)->x() == -1 && doc2.findLayer(id2)->y() == -1);

    bool threw = false;
    try {
        glyma::core::resizeCanvas(doc2, 0, 5);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testResizeDocumentImage() {
    Document doc("Image", 4, 4);
    Layer layer("L", 4, 4);
    layer.buffer().clear(kC);
    layer.setPosition(0, 0);
    const auto id = doc.addLayer(std::move(layer));

    glyma::core::resizeDocumentImage(doc, 8, 8);
    assert(doc.width() == 8 && doc.height() == 8);
    const Layer* scaled = doc.findLayer(id);
    assert(scaled->width() == 8 && scaled->height() == 8);
    assert(scaled->x() == 0 && scaled->y() == 0);

    // Identity is a no-op.
    glyma::core::resizeDocumentImage(doc, 8, 8);
    assert(doc.width() == 8 && doc.height() == 8);

    // Position and size follow the scale factors.
    Document doc2("Image2", 4, 4);
    Layer l2("L", 2, 2);
    l2.setPosition(1, 1);
    const auto id2 = doc2.addLayer(std::move(l2));
    glyma::core::resizeDocumentImage(doc2, 2, 2);
    const Layer* s2 = doc2.findLayer(id2);
    assert(s2->width() == 1 && s2->height() == 1);
    assert(s2->x() == 1 && s2->y() == 1);
}

void testRotateDocumentQuarterTurns() {
    Document doc("Rotate", 4, 3);
    Layer layer("L", 4, 3);
    layer.buffer().clear(kA);
    layer.buffer().setPixel(0, 0, kB);
    layer.buffer().setPixel(3, 2, kC);
    layer.setPosition(0, 0);
    const auto id = doc.addLayer(std::move(layer));

    glyma::core::rotateDocument90(doc, true);
    assert(doc.width() == 3 && doc.height() == 4);
    const Layer* cw = doc.findLayer(id);
    assert(cw->width() == 3 && cw->height() == 4);
    assert(cw->x() == 0 && cw->y() == 0);
    assert(cw->buffer().getPixel(2, 0) == kB); // source(0,0)
    assert(cw->buffer().getPixel(0, 3) == kC); // source(3,2)

    glyma::core::rotateDocument90(doc, false);
    assert(doc.width() == 4 && doc.height() == 3);
    const Layer* back = doc.findLayer(id);
    assert(back->width() == 4 && back->height() == 3);
    assert(back->buffer().getPixel(0, 0) == kB);
    assert(back->buffer().getPixel(3, 2) == kC);

    glyma::core::rotateDocument180(doc);
    assert(doc.width() == 4 && doc.height() == 3);
    const Layer* turn = doc.findLayer(id);
    assert(turn->buffer().getPixel(3, 2) == kB);
    assert(turn->buffer().getPixel(0, 0) == kC);
}

void testArbitraryRotateDocument() {
    Document doc("Arb", 4, 4);
    Layer layer("L", 4, 4);
    layer.buffer().clear(kD);
    const auto id = doc.addLayer(std::move(layer));

    Selection selection;
    selection.setDocumentSize(4, 4);
    selection.setRectangle(0.0, 0.0, 4.0, 4.0);
    doc.setSelection(selection);

    // Quarter turns delegate.
    glyma::core::rotateDocument(doc, 90.0, true);
    assert(doc.width() == 4 && doc.height() == 4);
    assert(doc.findLayer(id)->buffer().width() == 4);

    // A 45 degree rotation expands the canvas and clears the selection.
    glyma::core::rotateDocument(doc, 45.0, true);
    assert(doc.width() == 6 && doc.height() == 6);
    assert(doc.selection().isEmpty());
    assert(doc.findLayer(id)->buffer().width() == 6);

    // Zero degrees is a no-op.
    const std::uint32_t beforeW = doc.width();
    glyma::core::rotateDocument(doc, 0.0, true);
    assert(doc.width() == beforeW);

    bool threw = false;
    try {
        glyma::core::rotateDocument(doc, std::nan(""), true);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testFlipDocument() {
    Document doc("Flip", 4, 3);
    Layer layer("L", 4, 3);
    layer.buffer().clear(kA);
    layer.buffer().setPixel(0, 0, kB);
    layer.buffer().setPixel(0, 2, kC);
    const auto id = doc.addLayer(std::move(layer));

    glyma::core::flipDocument(doc, true);
    const Layer* h = doc.findLayer(id);
    assert(h->buffer().getPixel(3, 0) == kB);
    assert(h->buffer().getPixel(3, 2) == kC);

    glyma::core::flipDocument(doc, false);
    const Layer* v = doc.findLayer(id);
    assert(v->buffer().getPixel(3, 2) == kB);
    assert(v->buffer().getPixel(3, 0) == kC);
}

void testSingleLayerOperations() {
    Document doc("Layer", 10, 10);
    Layer layer("L", 4, 2);
    layer.buffer().clear(kA);
    layer.buffer().setPixel(0, 0, kB);
    layer.setPosition(1, 1);
    const auto id = doc.addLayer(std::move(layer));

    // 90 cw keeps the centre and swaps dimensions.
    glyma::core::transformLayer(doc, id, LayerTransform::Rotate90Clockwise);
    const Layer* rotated = doc.findLayer(id);
    assert(rotated->width() == 2 && rotated->height() == 4);
    assert(rotated->x() == 2 && rotated->y() == 0);

    // Scaling around the centre.
    glyma::core::scaleLayer(doc, id, 4, 8);
    const Layer* scaled = doc.findLayer(id);
    assert(scaled->width() == 4 && scaled->height() == 8);
    assert(scaled->x() == 1 && scaled->y() == -2);

    // Translation changes only the position.
    const std::uint32_t w = scaled->width();
    glyma::core::translateLayer(doc, id, 3, -1);
    assert(scaled->width() == w);
    assert(scaled->x() == 4 && scaled->y() == -3);

    // Arbitrary rotation about the centre.
    glyma::core::rotateLayer(doc, id, 180.0);
    const Layer* spun = doc.findLayer(id);
    assert(spun->width() == 4 && spun->height() == 8);
    assert(spun->x() == 4 && spun->y() == -3);

    bool threw = false;
    try {
        glyma::core::translateLayer(doc, 9999, 1, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// ---------------------------------------------------------------------------
// Undo / redo
// ---------------------------------------------------------------------------

struct LayerSig {
    glyma::core::LayerId id;
    std::string name;
    std::int32_t x;
    std::int32_t y;
    std::uint32_t w;
    std::uint32_t h;
    bool visible;
    float opacity;
    std::vector<Pixel> pixels;
};

struct DocSig {
    std::uint32_t w;
    std::uint32_t h;
    bool solid;
    Pixel background;
    std::vector<Pixel> render;
    std::vector<LayerSig> layers;
    Selection::Type selectionType;
    bool selectionEmpty;
};

DocSig snapshot(const Document& doc) {
    DocSig sig;
    sig.w = doc.width();
    sig.h = doc.height();
    sig.solid = doc.hasSolidBackground();
    sig.background = doc.backgroundColor();
    sig.render = doc.render().pixels();
    sig.selectionType = doc.selection().type();
    sig.selectionEmpty = doc.selection().isEmpty();
    for (const Layer& layer : doc.layerStack().layers()) {
        sig.layers.push_back(LayerSig{
            layer.id(),
            layer.name(),
            layer.x(),
            layer.y(),
            layer.width(),
            layer.height(),
            layer.visible(),
            layer.opacity(),
            layer.buffer().pixels()
        });
    }
    return sig;
}

bool sameSignature(const DocSig& a, const DocSig& b) {
    if (a.w != b.w || a.h != b.h || a.solid != b.solid) {
        return false;
    }
    if (a.background != b.background) {
        return false;
    }
    if (a.render != b.render) {
        return false;
    }
    if (a.selectionType != b.selectionType || a.selectionEmpty != b.selectionEmpty) {
        return false;
    }
    if (a.layers.size() != b.layers.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.layers.size(); ++i) {
        const LayerSig& la = a.layers[i];
        const LayerSig& lb = b.layers[i];
        if (la.id != lb.id || la.name != lb.name || la.x != lb.x ||
            la.y != lb.y || la.w != lb.w || la.h != lb.h ||
            la.visible != lb.visible || la.opacity != lb.opacity ||
            la.pixels != lb.pixels) {
            return false;
        }
    }
    return true;
}

Document makeUndoDocument() {
    Document doc("Undo", 6, 4);
    Layer a("A", 6, 4);
    a.buffer().clear(kA);
    a.buffer().setPixel(1, 1, kB);
    doc.addLayer(std::move(a));
    Layer b("B", 2, 2);
    b.buffer().clear(kC);
    b.setPosition(3, 1);
    doc.addLayer(std::move(b));
    Selection selection;
    selection.setDocumentSize(6, 4);
    selection.setRectangle(1.0, 1.0, 4.0, 3.0);
    doc.setSelection(selection);
    return doc;
}

void runUndoRedo(
    const std::string& label,
    const std::function<glyma::core::CommandPtr(const Document&)>& factory
) {
    Document doc = makeUndoDocument();
    CommandHistory history;
    const DocSig before = snapshot(doc);

    const bool executed = history.execute(factory(doc), doc);
    assert(executed);
    const DocSig after = snapshot(doc);
    assert(!sameSignature(before, after));
    assert(history.canUndo());
    assert(history.undoCount() == 1);

    assert(history.undo(doc));
    assert(sameSignature(before, snapshot(doc)));
    assert(history.canRedo());

    assert(history.redo(doc));
    assert(sameSignature(after, snapshot(doc)));

    std::cout << "  undo/redo ok: " << label << "\n";
}

void testCommandUndoRedo() {
    using namespace glyma::core;

    runUndoRedo("crop", [](const Document&) {
        return std::make_unique<CropCommand>(CropRect{1, 1, 4, 2});
    });
    runUndoRedo("canvas resize", [](const Document&) {
        return std::make_unique<CanvasResizeCommand>(9, 7, Anchor::Center);
    });
    runUndoRedo("image resize", [](const Document&) {
        return std::make_unique<ImageResizeCommand>(3, 2);
    });
    runUndoRedo("rotate cw", [](const Document&) {
        return std::make_unique<RotateDocumentCommand>(
            DocumentRotation::Clockwise90);
    });
    runUndoRedo("rotate ccw", [](const Document&) {
        return std::make_unique<RotateDocumentCommand>(
            DocumentRotation::CounterClockwise90);
    });
    runUndoRedo("rotate 180", [](const Document&) {
        return std::make_unique<RotateDocumentCommand>(
            DocumentRotation::Rotate180);
    });
    runUndoRedo("arbitrary rotate", [](const Document&) {
        return std::make_unique<ArbitraryRotateDocumentCommand>(30.0);
    });
    runUndoRedo("flip horizontal", [](const Document&) {
        return std::make_unique<FlipDocumentCommand>(true);
    });
    runUndoRedo("flip vertical", [](const Document&) {
        return std::make_unique<FlipDocumentCommand>(false);
    });
}

void testLayerCommandUndoRedo() {
    using namespace glyma::core;

    // The factory receives the freshly built document so it can target real
    // layer IDs; IDs are preserved across snapshot restore.
    runUndoRedo("transform layer", [](const Document& doc) {
        return std::make_unique<TransformLayerCommand>(
            doc.layerStack().layers().front().id(),
            LayerTransform::Rotate90Clockwise);
    });
    runUndoRedo("rotate layer", [](const Document& doc) {
        return std::make_unique<ArbitraryRotateLayerCommand>(
            doc.layerStack().layers().front().id(), 45.0);
    });
    runUndoRedo("scale layer", [](const Document& doc) {
        return std::make_unique<ScaleLayerCommand>(
            doc.layerStack().layers().front().id(), 3, 2);
    });
    runUndoRedo("translate layer", [](const Document& doc) {
        return std::make_unique<TranslateLayerCommand>(
            doc.layerStack().layers().back().id(), 2, -1);
    });
}

void testFailedCommandIsNotRecorded() {
    Document doc = makeUndoDocument();
    CommandHistory history;
    const DocSig before = snapshot(doc);

    // An empty crop is rejected and must not advance history.
    const bool executed = history.execute(
        std::make_unique<CropCommand>(CropRect{0, 0, 0, 0}), doc);
    assert(!executed);
    assert(!history.canUndo());
    assert(history.undoCount() == 0);
    assert(sameSignature(before, snapshot(doc)));
}

// ---------------------------------------------------------------------------
// Selection transforms
// ---------------------------------------------------------------------------

void testSelectionTranslateScale() {
    Selection selection;
    selection.setDocumentSize(100, 100);
    selection.setRectangle(10.0, 20.0, 40.0, 60.0);

    selection.translate(5.0, -10.0);
    double l = 0, t = 0, r = 0, b = 0;
    assert(selection.bounds(l, t, r, b));
    assert(std::abs(l - 15.0) < 1e-9 && std::abs(t - 10.0) < 1e-9);
    assert(std::abs(r - 45.0) < 1e-9 && std::abs(b - 50.0) < 1e-9);

    selection.scale(2.0, 2.0);
    assert(selection.documentWidth() == 200);
    assert(selection.documentHeight() == 200);
    assert(selection.bounds(l, t, r, b));
    assert(std::abs(l - 30.0) < 1e-9 && std::abs(t - 20.0) < 1e-9);
    assert(std::abs(r - 90.0) < 1e-9 && std::abs(b - 100.0) < 1e-9);

    // Non-finite / non-positive scale factors are ignored.
    selection.scale(0.0, 1.0);
    assert(selection.documentWidth() == 200);
}

void testSelectionRotations() {
    Selection selection;
    selection.setDocumentSize(100, 50);
    selection.setRectangle(10.0, 20.0, 40.0, 40.0);

    double l = 0, t = 0, r = 0, b = 0;

    Selection cw = selection;
    cw.rotate90Clockwise();
    assert(cw.documentWidth() == 50 && cw.documentHeight() == 100);
    assert(cw.bounds(l, t, r, b));
    assert(std::abs(l - 10.0) < 1e-9 && std::abs(t - 10.0) < 1e-9);
    assert(std::abs(r - 30.0) < 1e-9 && std::abs(b - 40.0) < 1e-9);

    Selection ccw = selection;
    ccw.rotate90CounterClockwise();
    assert(ccw.documentWidth() == 50 && ccw.documentHeight() == 100);
    assert(ccw.bounds(l, t, r, b));
    assert(std::abs(l - 20.0) < 1e-9 && std::abs(t - 60.0) < 1e-9);
    assert(std::abs(r - 40.0) < 1e-9 && std::abs(b - 90.0) < 1e-9);

    Selection turn = selection;
    turn.rotate180();
    assert(turn.documentWidth() == 100 && turn.documentHeight() == 50);
    assert(turn.bounds(l, t, r, b));
    assert(std::abs(l - 60.0) < 1e-9 && std::abs(t - 10.0) < 1e-9);
    assert(std::abs(r - 90.0) < 1e-9 && std::abs(b - 30.0) < 1e-9);
}

void testSelectionFlips() {
    Selection selection;
    selection.setDocumentSize(100, 50);
    selection.setRectangle(10.0, 20.0, 40.0, 40.0);

    double l = 0, t = 0, r = 0, b = 0;

    Selection h = selection;
    h.flipHorizontal();
    assert(h.bounds(l, t, r, b));
    assert(std::abs(l - 60.0) < 1e-9 && std::abs(r - 90.0) < 1e-9);
    assert(std::abs(t - 20.0) < 1e-9 && std::abs(b - 40.0) < 1e-9);

    Selection v = selection;
    v.flipVertical();
    assert(v.bounds(l, t, r, b));
    assert(std::abs(t - 10.0) < 1e-9 && std::abs(b - 30.0) < 1e-9);
    assert(std::abs(l - 10.0) < 1e-9 && std::abs(r - 40.0) < 1e-9);
}

void testSelectionClearedWhenOutOfBounds() {
    Selection selection;
    selection.setDocumentSize(100, 100);
    selection.setRectangle(30.0, 30.0, 40.0, 40.0);
    assert(!selection.isEmpty());

    // Shrinking the document past the selection clears it.
    selection.setDocumentSize(20, 20);
    assert(selection.isEmpty());
}

} // namespace

int main() {
    testCanvasValidation();
    testClipCropRect();
    testCropRectFromPoints();
    testCropImage();
    testScaleImage();
    testQuarterTurnsAndFlips();
    testArbitraryRotation();
    testCropDocument();
    testResizeCanvas();
    testResizeDocumentImage();
    testRotateDocumentQuarterTurns();
    testArbitraryRotateDocument();
    testFlipDocument();
    testSingleLayerOperations();
    testCommandUndoRedo();
    testLayerCommandUndoRedo();
    testFailedCommandIsNotRecorded();
    testSelectionTranslateScale();
    testSelectionRotations();
    testSelectionFlips();
    testSelectionClearedWhenOutOfBounds();

    std::cout << "All Glyma transform tests passed\n";
    return 0;
}

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "glyma/core/command_history.hpp"
#include "glyma/core/commands.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/pixel.hpp"
#include "glyma/core/shape_commands.hpp"
#include "glyma/core/shape_layer.hpp"
#include "glyma/core/transform.hpp"
#include "glyma/io/project_reader.hpp"
#include "glyma/io/project_writer.hpp"

using glyma::core::AddShapeLayerCommand;
using glyma::core::CommandHistory;
using glyma::core::CropRect;
using glyma::core::Document;
using glyma::core::Layer;
using glyma::core::LayerId;
using glyma::core::LayerTransform;
using glyma::core::Pixel;
using glyma::core::ProjectReader;
using glyma::core::ProjectWriter;
using glyma::core::RasterizeShapeLayerCommand;
using glyma::core::RemoveLayerCommand;
using glyma::core::SetShapeDataCommand;
using glyma::core::ShapeBounds;
using glyma::core::ShapeLayerData;
using glyma::core::ShapePoint;
using glyma::core::ShapeType;
using glyma::core::StrokeStyle;

namespace {

bool nearly(double a, double b, double epsilon = 1.0e-6) {
    return std::abs(a - b) <= epsilon;
}

ShapeLayerData plainRect(double x, double y, double w, double h) {
    ShapeLayerData shape;
    shape.type = ShapeType::Rectangle;
    shape.fill.enabled = false;
    shape.stroke.enabled = false;
    glyma::core::setRectGeometry(shape, x, y, w, h);
    return shape;
}

void testValidation() {
    ShapeLayerData defaults;
    assert(defaults.isValid());
    defaults.validate();

    ShapeLayerData bad = defaults;
    bad.width = -1.0;
    assert(!bad.isValid());

    bad = defaults;
    bad.x = std::nan("");
    assert(!bad.isValid());

    bad = defaults;
    bad.stroke.width = -1.0F;
    assert(!bad.isValid());
    bad.stroke.width = ShapeLayerData::maxStrokeWidth + 1.0F;
    assert(!bad.isValid());

    bad = defaults;
    bad.type = ShapeType::Polygon;
    bad.points.assign(ShapeLayerData::maxPoints + 1, ShapePoint{0.0, 0.0});
    assert(!bad.isValid());

    bad = defaults;
    bad.x = ShapeLayerData::maxCoordinate + 1.0;
    assert(!bad.isValid());

    bool threw = false;
    try {
        bad.validate();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void testTypeConversion() {
    ShapeType type = ShapeType::Line;
    assert(glyma::core::parseShapeType("ellipse", type));
    assert(type == ShapeType::Ellipse);
    assert(std::string(glyma::core::shapeTypeName(type)) == "ellipse");
    assert(glyma::core::parseShapeType("rounded-rectangle", type));
    assert(type == ShapeType::RoundedRectangle);
    assert(!glyma::core::parseShapeType("triangle", type));

    StrokeStyle style = StrokeStyle::Solid;
    assert(glyma::core::parseStrokeStyle("dashed", style));
    assert(style == StrokeStyle::Dashed);
    assert(std::string(glyma::core::strokeStyleName(style)) == "dashed");
    assert(!glyma::core::parseStrokeStyle("wavy", style));

    assert(glyma::core::isBoxShape(ShapeType::Rectangle));
    assert(glyma::core::isBoxShape(ShapeType::Ellipse));
    assert(!glyma::core::isBoxShape(ShapeType::Line));
    assert(glyma::core::isLineShape(ShapeType::Arrow));
}

void testGeometryBasics() {
    const ShapeBounds rect = glyma::core::normalizeRect(10.0, 20.0, 5.0, 8.0);
    assert(nearly(rect.x, 5.0) && nearly(rect.y, 8.0));
    assert(nearly(rect.width, 5.0) && nearly(rect.height, 12.0));

    // Negative drag dimensions normalize.
    ShapeLayerData box;
    glyma::core::setRectGeometry(box, 30.0, 40.0, -10.0, -20.0);
    assert(nearly(box.x, 20.0) && nearly(box.y, 20.0));
    assert(nearly(box.width, 10.0) && nearly(box.height, 20.0));

    // Line geometry keeps the bounds in sync.
    ShapeLayerData line;
    line.type = ShapeType::Line;
    glyma::core::setLineGeometry(line, 5.0, 9.0, 25.0, 1.0);
    assert(nearly(line.x, 5.0) && nearly(line.y, 1.0));
    assert(nearly(line.width, 20.0) && nearly(line.height, 8.0));

    // Polygon geometry keeps the bounds in sync.
    ShapeLayerData poly;
    poly.type = ShapeType::Polygon;
    glyma::core::setPolygonGeometry(poly, {
        ShapePoint{2.0, 3.0},
        ShapePoint{12.0, 3.0},
        ShapePoint{7.0, 13.0},
    });
    assert(nearly(poly.x, 2.0) && nearly(poly.y, 3.0));
    assert(nearly(poly.width, 10.0) && nearly(poly.height, 10.0));

    const ShapePoint centre = glyma::core::shapeCentre(box);
    assert(nearly(centre.x, 25.0) && nearly(centre.y, 30.0));
}

void testVisualBounds() {
    ShapeLayerData box = plainRect(0.0, 0.0, 10.0, 10.0);
    box.stroke.enabled = true;
    box.stroke.width = 4.0F;
    const ShapeBounds bounds = glyma::core::shapeVisualBounds(box);
    assert(nearly(bounds.x, -2.0) && nearly(bounds.y, -2.0));
    assert(nearly(bounds.width, 14.0) && nearly(bounds.height, 14.0));

    // Arrowheads extend the visual bounds beyond the line geometry.
    ShapeLayerData arrow;
    arrow.type = ShapeType::Arrow;
    arrow.stroke.enabled = false;
    arrow.fill.enabled = false;
    glyma::core::setLineGeometry(arrow, 0.0, 0.0, 10.0, 0.0);
    arrow.arrow.length = 5.0F;
    arrow.arrow.width = 4.0F;
    const ShapeBounds arrowBounds = glyma::core::shapeVisualBounds(arrow);
    assert(nearly(arrowBounds.x, -5.0) && nearly(arrowBounds.y, -5.0));
    assert(nearly(arrowBounds.width, 20.0) && nearly(arrowBounds.height, 10.0));

    // A 90-degree rotation of a square keeps the same axis-aligned bounds.
    ShapeLayerData rotated = plainRect(10.0, 10.0, 20.0, 20.0);
    rotated.rotation = 90.0;
    const ShapeBounds rotatedBounds = glyma::core::shapeVisualBounds(rotated);
    assert(nearly(rotatedBounds.x, 10.0) && nearly(rotatedBounds.y, 10.0));
    assert(nearly(rotatedBounds.width, 20.0) && nearly(rotatedBounds.height, 20.0));
}

void testTransforms() {
    // Translation moves every defining coordinate.
    ShapeLayerData box = plainRect(10.0, 10.0, 20.0, 20.0);
    glyma::core::translateShape(box, 5.0, -3.0);
    assert(nearly(box.x, 15.0) && nearly(box.y, 7.0));

    // Scaling about the origin scales the bounds.
    box = plainRect(10.0, 10.0, 20.0, 20.0);
    glyma::core::scaleShape(box, 0.0, 0.0, 2.0, 2.0);
    assert(nearly(box.x, 20.0) && nearly(box.y, 20.0));
    assert(nearly(box.width, 40.0) && nearly(box.height, 40.0));

    // Rotation about the shape centre only accumulates the angle.
    box = plainRect(10.0, 10.0, 20.0, 20.0);
    glyma::core::rotateShape(box, 20.0, 20.0, 90.0);
    assert(nearly(box.x, 10.0) && nearly(box.y, 10.0));
    assert(nearly(box.rotation, 90.0));

    // Rotation about an external pivot moves the centre.
    box = plainRect(0.0, 0.0, 10.0, 10.0);
    glyma::core::rotateShape(box, 0.0, 0.0, 180.0);
    assert(nearly(box.x, -10.0) && nearly(box.y, -10.0));
    assert(nearly(box.rotation, 180.0));

    // Horizontal flip mirrors the box and updates the rotation.
    box = plainRect(10.0, 20.0, 30.0, 40.0);
    glyma::core::flipShape(box, true, 100.0, 100.0);
    assert(nearly(box.x, 60.0) && nearly(box.y, 20.0));
    assert(nearly(box.rotation, 180.0));

    // Vertical flip mirrors the box.
    box = plainRect(10.0, 20.0, 30.0, 40.0);
    glyma::core::flipShape(box, false, 100.0, 100.0);
    assert(nearly(box.x, 10.0) && nearly(box.y, 40.0));
    assert(nearly(box.rotation, 0.0));
}

void testHitTesting() {
    const ShapeLayerData rect = plainRect(0.0, 0.0, 10.0, 10.0);
    assert(glyma::core::shapeHitTest(rect, 5.0, 5.0, 0.0));
    assert(!glyma::core::shapeHitTest(rect, 15.0, 5.0, 0.0));
    assert(glyma::core::shapeHitTest(rect, 15.0, 5.0, 6.0));

    ShapeLayerData ellipse = plainRect(0.0, 0.0, 10.0, 10.0);
    ellipse.type = ShapeType::Ellipse;
    assert(glyma::core::shapeHitTest(ellipse, 5.0, 5.0, 0.0));
    assert(!glyma::core::shapeHitTest(ellipse, 0.0, 0.0, 0.0));
    assert(!glyma::core::shapeHitTest(ellipse, 9.9, 0.1, 0.0));

    ShapeLayerData line;
    line.type = ShapeType::Line;
    glyma::core::setLineGeometry(line, 0.0, 0.0, 10.0, 0.0);
    assert(glyma::core::shapeHitTest(line, 5.0, 0.0, 0.0));
    assert(!glyma::core::shapeHitTest(line, 5.0, 5.0, 0.0));
    assert(glyma::core::shapeHitTest(line, 5.0, 5.0, 5.0));

    ShapeLayerData poly;
    poly.type = ShapeType::Polygon;
    glyma::core::setPolygonGeometry(poly, {
        ShapePoint{0.0, 0.0},
        ShapePoint{10.0, 0.0},
        ShapePoint{5.0, 10.0},
    });
    assert(glyma::core::shapeHitTest(poly, 5.0, 3.0, 0.0));
    assert(!glyma::core::shapeHitTest(poly, 20.0, 20.0, 0.0));

    // Rotation is accounted for during hit testing.
    ShapeLayerData rotated = plainRect(0.0, 0.0, 10.0, 2.0);
    rotated.rotation = 90.0;
    assert(glyma::core::shapeHitTest(rotated, 5.0, 5.0, 0.0));
}

void testSanitize() {
    ShapeLayerData shape = plainRect(10.0, 10.0, -20.0, 20.0);
    glyma::core::sanitizeShape(shape);
    assert(shape.width >= 0.0 && shape.height >= 0.0);

    shape.x = std::nan("");
    shape.rotation = -90.0;
    glyma::core::sanitizeShape(shape);
    assert(std::isfinite(shape.x));
    assert(nearly(shape.rotation, 270.0));
}

void testLayerVectorType() {
    Layer raster("R", 4, 4);
    assert(raster.isRaster());
    assert(!raster.isVector());
    bool threw = false;
    try {
        (void)raster.shape();
    } catch (const std::logic_error&) {
        threw = true;
    }
    assert(threw);

    Layer vector("V", plainRect(1.0, 2.0, 30.0, 40.0));
    assert(vector.isVector());
    assert(!vector.isRaster());
    assert(nearly(vector.shape().width, 30.0));

    // Promoting a raster layer stores the payload and drops any text payload.
    raster.setShapeData(plainRect(0.0, 0.0, 5.0, 5.0));
    assert(raster.isVector());
    assert(nearly(raster.shape().height, 5.0));

    // Rasterizing drops the payload but keeps the pixels.
    raster.buffer().clear(Pixel{4, 5, 6, 255});
    assert(raster.rasterize());
    assert(raster.isRaster());
    assert(raster.buffer().getPixel(0, 0) == (Pixel{4, 5, 6, 255}));
    assert(!raster.rasterize());
}

void testShapeCommandsUndoRedo() {
    Document doc("Cmd", 64, 64);
    CommandHistory history;

    assert(history.execute(
        std::make_unique<AddShapeLayerCommand>("Shape", plainRect(2.0, 3.0, 10.0, 10.0)),
        doc));
    assert(doc.layerCount() == 1);
    const LayerId id = doc.layerStack().layers().front().id();
    assert(doc.findLayer(id)->isVector());
    assert(nearly(doc.findLayer(id)->shape().x, 2.0));

    assert(history.undo(doc));
    assert(doc.layerCount() == 0);
    assert(history.redo(doc));
    assert(doc.layerCount() == 1);
    assert(doc.findLayer(id)->isVector());

    // Editing geometry and style is undoable.
    ShapeLayerData edited = plainRect(5.0, 6.0, 20.0, 20.0);
    edited.type = ShapeType::Ellipse;
    edited.fill.color = Pixel{10, 20, 30, 255};
    assert(history.execute(std::make_unique<SetShapeDataCommand>(id, edited), doc));
    assert(doc.findLayer(id)->shape().type == ShapeType::Ellipse);
    assert(nearly(doc.findLayer(id)->shape().x, 5.0));
    assert(history.undo(doc));
    assert(doc.findLayer(id)->shape().type == ShapeType::Rectangle);
    assert(nearly(doc.findLayer(id)->shape().x, 2.0));
    assert(history.redo(doc));
    assert(doc.findLayer(id)->shape().type == ShapeType::Ellipse);

    // Rasterizing is undoable.
    assert(history.execute(std::make_unique<RasterizeShapeLayerCommand>(id), doc));
    assert(doc.findLayer(id)->isRaster());
    assert(history.undo(doc));
    assert(doc.findLayer(id)->isVector());
    assert(doc.findLayer(id)->shape().type == ShapeType::Ellipse);

    // Removing and undoing a vector layer preserves its geometry.
    assert(history.execute(std::make_unique<RemoveLayerCommand>(id), doc));
    assert(doc.layerCount() == 0);
    assert(history.undo(doc));
    assert(doc.layerCount() == 1);
    assert(doc.findLayer(id)->isVector());
    assert(nearly(doc.findLayer(id)->shape().width, 20.0));
}

Document makeShapeDoc() {
    Document doc("Transform", 100, 100);
    Layer layer("Shape", plainRect(0.0, 0.0, 10.0, 10.0));
    doc.addLayer(std::move(layer));
    return doc;
}

void testTransformIntegration() {
    // Translation keeps the layer editable and moves the geometry.
    {
        Document doc = makeShapeDoc();
        const LayerId id = doc.layerStack().layers().front().id();
        glyma::core::translateLayer(doc, id, 5, -3);
        const Layer* layer = doc.findLayer(id);
        assert(layer->isVector());
        assert(nearly(layer->shape().x, 5.0) && nearly(layer->shape().y, -3.0));
    }

    // Scaling keeps the layer editable and scales the geometry.
    {
        Document doc = makeShapeDoc();
        const LayerId id = doc.layerStack().layers().front().id();
        glyma::core::scaleLayer(doc, id, 20, 20);
        const Layer* layer = doc.findLayer(id);
        assert(layer->isVector());
        assert(nearly(layer->shape().width, 20.0));
        assert(nearly(layer->shape().height, 20.0));
    }

    // Cropping keeps the layer editable and shifts the geometry.
    {
        Document doc = makeShapeDoc();
        const LayerId id = doc.layerStack().layers().front().id();
        glyma::core::cropDocument(doc, CropRect{2, 3, 50, 50});
        const Layer* layer = doc.findLayer(id);
        assert(layer->isVector());
        assert(nearly(layer->shape().x, -2.0) && nearly(layer->shape().y, -3.0));
    }

    // Document rotation keeps the layer editable and rotates the geometry.
    {
        Document doc = makeShapeDoc();
        const LayerId id = doc.layerStack().layers().front().id();
        glyma::core::rotateDocument90(doc, true);
        const Layer* layer = doc.findLayer(id);
        assert(layer->isVector());
        assert(nearly(layer->shape().rotation, 90.0));
        const ShapePoint centre = glyma::core::shapeCentre(layer->shape());
        assert(nearly(centre.x, 95.0, 1.0e-3) && nearly(centre.y, 5.0, 1.0e-3));
    }

    // Document flip keeps the layer editable and mirrors the geometry.
    {
        Document doc = makeShapeDoc();
        const LayerId id = doc.layerStack().layers().front().id();
        glyma::core::flipDocument(doc, true);
        const Layer* layer = doc.findLayer(id);
        assert(layer->isVector());
        assert(nearly(layer->shape().x, 90.0));
        assert(nearly(layer->shape().rotation, 180.0));
    }

    // A 180-degree single-layer transform keeps the layer editable.
    {
        Document doc = makeShapeDoc();
        const LayerId id = doc.layerStack().layers().front().id();
        glyma::core::transformLayer(doc, id, LayerTransform::Rotate180);
        assert(doc.findLayer(id)->isVector());
        assert(nearly(doc.findLayer(id)->shape().rotation, 180.0));
    }
}

void testSerializationRoundTrip() {
    std::random_device rd;
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("glyma_shape_test_" + std::to_string(rd()));
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / "shapes.glyma";

    Document doc("Vectors", 64, 48);

    Layer raster("Background", 64, 48);
    raster.buffer().clear(Pixel{1, 2, 3, 255});
    const LayerId rasterId = doc.addLayer(std::move(raster));

    ShapeLayerData shape;
    shape.type = ShapeType::RoundedRectangle;
    glyma::core::setRectGeometry(shape, 4.0, 6.0, 20.0, 12.0);
    shape.rotation = 30.0;
    shape.cornerRadius = 5.0F;
    shape.fill.enabled = true;
    shape.fill.color = Pixel{200, 100, 50, 220};
    shape.stroke.enabled = true;
    shape.stroke.color = Pixel{10, 20, 30, 255};
    shape.stroke.width = 3.0F;
    shape.stroke.style = StrokeStyle::Dashed;

    Layer vector("Badge", shape);
    vector.setOpacity(0.5F);
    vector.buffer() = glyma::core::ImageBuffer(24, 16);
    vector.buffer().clear(Pixel{9, 8, 7, 255});
    const LayerId vectorId = doc.addLayer(std::move(vector));

    ProjectWriter::save(doc, path.string());
    Document loaded = ProjectReader::load(path.string());

    assert(loaded.name() == "Vectors");
    assert(loaded.layerCount() == 2);
    assert(loaded.findLayer(rasterId)->isRaster());

    const Layer* loadedShape = loaded.findLayer(vectorId);
    assert(loadedShape != nullptr);
    assert(loadedShape->isVector());
    const ShapeLayerData& s = loadedShape->shape();
    assert(s.type == ShapeType::RoundedRectangle);
    assert(nearly(s.x, 4.0) && nearly(s.y, 6.0));
    assert(nearly(s.width, 20.0) && nearly(s.height, 12.0));
    assert(nearly(s.rotation, 30.0));
    assert(std::abs(s.cornerRadius - 5.0F) < 1.0e-3F);
    assert(s.fill.enabled);
    assert(s.fill.color == (Pixel{200, 100, 50, 220}));
    assert(s.stroke.enabled);
    assert(s.stroke.color == (Pixel{10, 20, 30, 255}));
    assert(std::abs(s.stroke.width - 3.0F) < 1.0e-4F);
    assert(s.stroke.style == StrokeStyle::Dashed);
    assert(std::abs(loadedShape->opacity() - 0.5F) < 1.0e-4F);
    assert(loadedShape->buffer().getPixel(0, 0) == (Pixel{9, 8, 7, 255}));

    std::filesystem::remove_all(dir);
}

void testPolygonSerialization() {
    std::random_device rd;
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("glyma_shape_poly_" + std::to_string(rd()));
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / "polygon.glyma";

    Document doc("Poly", 32, 32);
    ShapeLayerData poly;
    poly.type = ShapeType::Polygon;
    glyma::core::setPolygonGeometry(poly, {
        ShapePoint{1.0, 2.0},
        ShapePoint{11.0, 2.0},
        ShapePoint{6.0, 12.0},
    });
    Layer vector("Triangle", poly);
    const LayerId id = doc.addLayer(std::move(vector));

    ProjectWriter::save(doc, path.string());
    Document loaded = ProjectReader::load(path.string());

    const Layer* layer = loaded.findLayer(id);
    assert(layer != nullptr && layer->isVector());
    const ShapeLayerData& s = layer->shape();
    assert(s.type == ShapeType::Polygon);
    assert(s.points.size() == 3);
    assert(nearly(s.points[2].x, 6.0) && nearly(s.points[2].y, 12.0));

    std::filesystem::remove_all(dir);
}

} // namespace

int main() {
    testValidation();
    testTypeConversion();
    testGeometryBasics();
    testVisualBounds();
    testTransforms();
    testHitTesting();
    testSanitize();
    testLayerVectorType();
    testShapeCommandsUndoRedo();
    testTransformIntegration();
    testSerializationRoundTrip();
    testPolygonSerialization();

    std::cout << "All Glyma shape layer tests passed\n";
    return 0;
}

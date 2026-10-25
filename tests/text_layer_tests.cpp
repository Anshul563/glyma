#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <zip.h>

#include "glyma/core/command_history.hpp"
#include "glyma/core/commands.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/pixel.hpp"
#include "glyma/core/text_commands.hpp"
#include "glyma/core/text_layer.hpp"
#include "glyma/core/transform.hpp"
#include "glyma/io/project_reader.hpp"
#include "glyma/io/project_writer.hpp"

using glyma::core::AddTextLayerCommand;
using glyma::core::CommandHistory;
using glyma::core::CropRect;
using glyma::core::Document;
using glyma::core::ImageBuffer;
using glyma::core::Layer;
using glyma::core::LayerId;
using glyma::core::LayerTransform;
using glyma::core::Pixel;
using glyma::core::ProjectReader;
using glyma::core::ProjectWriter;
using glyma::core::RasterizeTextLayerCommand;
using glyma::core::RemoveLayerCommand;
using glyma::core::SetTextDataCommand;
using glyma::core::TextHorizontalAlign;
using glyma::core::TextLayerData;
using glyma::core::TextStyle;
using glyma::core::TextVerticalAlign;

namespace {

TextLayerData fixedText(std::string content = "Hello") {
    TextLayerData data;
    data.text = std::move(content);
    data.autoSize = false;
    data.boxWidth = 40;
    data.boxHeight = 10;
    return data;
}

void testValidation() {
    TextLayerData defaults;
    assert(defaults.isValid());
    defaults.validate();

    TextLayerData bad = defaults;
    bad.style.fontSize = 0.0F;
    assert(!bad.isValid());
    bool threw = false;
    try {
        bad.validate();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    bad = defaults;
    bad.style.fontFamily.clear();
    assert(!bad.isValid());

    bad = defaults;
    bad.style.fontWeight = 0;
    assert(!bad.isValid());
    bad.style.fontWeight = 1001;
    assert(!bad.isValid());

    bad = defaults;
    bad.style.lineSpacing = 0.0F;
    assert(!bad.isValid());

    // Auto-sized boxes ignore the stored dimensions.
    TextLayerData autoBox;
    autoBox.autoSize = true;
    autoBox.boxWidth = 0;
    autoBox.boxHeight = 0;
    assert(autoBox.isValid());

    // Fixed boxes must be non-zero.
    TextLayerData fixed = defaults;
    fixed.autoSize = false;
    fixed.boxWidth = 0;
    fixed.boxHeight = 10;
    assert(!fixed.isValid());

    // Dimensions beyond the supported range are rejected.
    TextLayerData huge = defaults;
    huge.autoSize = false;
    huge.boxWidth = TextLayerData::maxBoxDimension + 1;
    huge.boxHeight = 1;
    assert(!huge.isValid());
}

void testAlignmentConversion() {
    TextHorizontalAlign ha = TextHorizontalAlign::Left;
    assert(glyma::core::parseTextHorizontalAlign("center", ha));
    assert(ha == TextHorizontalAlign::Center);
    assert(std::string(glyma::core::textHorizontalAlignName(ha)) == "center");
    assert(!glyma::core::parseTextHorizontalAlign("sideways", ha));

    TextVerticalAlign va = TextVerticalAlign::Top;
    assert(glyma::core::parseTextVerticalAlign("bottom", va));
    assert(va == TextVerticalAlign::Bottom);
    assert(std::string(glyma::core::textVerticalAlignName(va)) == "bottom");
    assert(!glyma::core::parseTextVerticalAlign("middle-ish", va));
}

void testLayerTextType() {
    Layer raster("R", 4, 4);
    assert(raster.isRaster());
    assert(!raster.isText());
    bool threw = false;
    try {
        (void)raster.text();
    } catch (const std::logic_error&) {
        threw = true;
    }
    assert(threw);

    Layer text("T", fixedText());
    assert(text.isText());
    assert(!text.isRaster());
    assert(text.text().text == "Hello");
    assert(text.width() == 40 && text.height() == 10);

    // Auto-sized text starts with a 1x1 cache.
    TextLayerData autoBox;
    autoBox.text = "Auto";
    Layer autoLayer("A", autoBox);
    assert(autoLayer.width() == 1 && autoLayer.height() == 1);

    // Promoting a raster layer stores the payload.
    raster.setTextData(fixedText("Promoted"));
    assert(raster.isText());
    assert(raster.text().text == "Promoted");

    // Rasterizing drops the payload but keeps the pixels.
    raster.buffer().clear(Pixel{1, 2, 3, 255});
    assert(raster.rasterize());
    assert(raster.isRaster());
    assert(raster.buffer().getPixel(0, 0) == (Pixel{1, 2, 3, 255}));
    assert(!raster.rasterize());
}

void testTextCommandsUndoRedo() {
    Document doc("Cmd", 64, 64);
    CommandHistory history;

    assert(history.execute(
        std::make_unique<AddTextLayerCommand>("Text", fixedText("One")), doc));
    assert(doc.layerCount() == 1);
    const LayerId id = doc.layerStack().layers().front().id();
    assert(doc.findLayer(id)->isText());
    assert(doc.findLayer(id)->text().text == "One");

    assert(history.undo(doc));
    assert(doc.layerCount() == 0);
    assert(history.redo(doc));
    assert(doc.layerCount() == 1);
    assert(doc.findLayer(id)->isText());

    // Editing the payload is undoable.
    TextLayerData edited = fixedText("Two");
    edited.style.fontSize = 48.0F;
    edited.style.italic = true;
    assert(history.execute(std::make_unique<SetTextDataCommand>(id, edited), doc));
    assert(doc.findLayer(id)->text().text == "Two");
    assert(doc.findLayer(id)->text().style.fontSize == 48.0F);
    assert(history.undo(doc));
    assert(doc.findLayer(id)->text().text == "One");
    assert(doc.findLayer(id)->text().style.fontSize == 32.0F);
    assert(history.redo(doc));
    assert(doc.findLayer(id)->text().text == "Two");

    // Rasterizing is undoable.
    assert(history.execute(std::make_unique<RasterizeTextLayerCommand>(id), doc));
    assert(doc.findLayer(id)->isRaster());
    assert(history.undo(doc));
    assert(doc.findLayer(id)->isText());
    assert(doc.findLayer(id)->text().text == "Two");

    // Removing and undoing a text layer preserves its editable payload.
    assert(history.execute(std::make_unique<RemoveLayerCommand>(id), doc));
    assert(doc.layerCount() == 0);
    assert(history.undo(doc));
    assert(doc.layerCount() == 1);
    assert(doc.findLayer(id)->isText());
    assert(doc.findLayer(id)->text().text == "Two");
}

Document makeTextDoc() {
    Document doc("Transform", 100, 100);
    Layer layer("Text", fixedText());
    layer.setPosition(10, 20);
    doc.addLayer(std::move(layer));
    return doc;
}

void testTransformIntegration() {
    // Translation keeps text editable.
    {
        Document doc = makeTextDoc();
        const LayerId id = doc.layerStack().layers().front().id();
        glyma::core::translateLayer(doc, id, 5, -3);
        const Layer* layer = doc.findLayer(id);
        assert(layer->isText());
        assert(layer->x() == 15 && layer->y() == 17);
    }

    // Scaling keeps text editable and scales the font and box.
    {
        Document doc = makeTextDoc();
        const LayerId id = doc.layerStack().layers().front().id();
        glyma::core::scaleLayer(doc, id, 80, 20);
        const Layer* layer = doc.findLayer(id);
        assert(layer->isText());
        assert(std::abs(layer->text().style.fontSize - 64.0F) < 1e-3F);
        assert(layer->text().boxWidth == 80);
        assert(layer->text().boxHeight == 20);
        assert(layer->x() == -10 && layer->y() == 15);
    }

    // Cropping keeps text editable and shifts the box origin.
    {
        Document doc = makeTextDoc();
        const LayerId id = doc.layerStack().layers().front().id();
        glyma::core::cropDocument(doc, CropRect{5, 5, 50, 50});
        const Layer* layer = doc.findLayer(id);
        assert(layer->isText());
        assert(layer->x() == 5 && layer->y() == 15);
    }

    // Document rotation keeps text editable, swapping a fixed box.
    {
        Document doc = makeTextDoc();
        const LayerId id = doc.layerStack().layers().front().id();
        glyma::core::rotateDocument90(doc, true);
        const Layer* layer = doc.findLayer(id);
        assert(layer->isText());
        assert(layer->text().boxWidth == 10);
        assert(layer->text().boxHeight == 40);
        assert(layer->x() == 70 && layer->y() == 10);
    }

    // A 180-degree single-layer transform bakes the pixels (undo restores the
    // editable text through the command snapshot).
    {
        Document doc = makeTextDoc();
        const LayerId id = doc.layerStack().layers().front().id();
        glyma::core::transformLayer(doc, id, LayerTransform::Rotate180);
        assert(doc.findLayer(id)->isRaster());
    }
}

void writeArchive(const std::filesystem::path& path,
                  const std::map<std::string, std::string>& entries) {
    int error = 0;
    zip_t* za = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    assert(za != nullptr);
    std::vector<std::string> storage;
    storage.reserve(entries.size());
    for (const auto& [name, content] : entries) {
        storage.push_back(content);
        zip_source_t* src = zip_source_buffer(
            za, storage.back().data(), storage.back().size(), 0);
        assert(src != nullptr);
        assert(zip_file_add(za, name.c_str(), src, ZIP_FL_OVERWRITE) >= 0);
    }
    assert(zip_close(za) == 0);
}

void testSerializationRoundTrip() {
    std::random_device rd;
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("glyma_text_test_" + std::to_string(rd()));
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / "text.glyma";

    Document doc("Typography", 32, 16);

    Layer raster("Background", 32, 16);
    raster.buffer().clear(Pixel{10, 20, 30, 255});
    const LayerId rasterId = doc.addLayer(std::move(raster));

    TextLayerData data;
    data.text = "Hello\nWorld";
    data.autoSize = false;
    data.boxWidth = 20;
    data.boxHeight = 8;
    data.style.fontFamily = "DejaVu Sans";
    data.style.fontSize = 24.0F;
    data.style.fontWeight = 700;
    data.style.italic = true;
    data.style.color = Pixel{200, 100, 50, 220};
    data.style.lineSpacing = 1.5F;
    data.style.letterSpacing = 2.0F;
    data.style.horizontalAlign = TextHorizontalAlign::Center;
    data.style.verticalAlign = TextVerticalAlign::Bottom;
    data.style.wordWrap = true;

    Layer text("Caption", data);
    text.setPosition(3, 4);
    text.setOpacity(0.5F);
    text.buffer().clear(Pixel{9, 8, 7, 255});
    const LayerId textId = doc.addLayer(std::move(text));

    ProjectWriter::save(doc, path.string());
    Document loaded = ProjectReader::load(path.string());

    assert(loaded.name() == "Typography");
    assert(loaded.width() == 32 && loaded.height() == 16);
    assert(loaded.layerCount() == 2);

    const Layer* loadedRaster = loaded.findLayer(rasterId);
    assert(loadedRaster != nullptr);
    assert(loadedRaster->isRaster());
    assert(loadedRaster->buffer().getPixel(0, 0) == (Pixel{10, 20, 30, 255}));

    const Layer* loadedText = loaded.findLayer(textId);
    assert(loadedText != nullptr);
    assert(loadedText->isText());
    const TextLayerData& t = loadedText->text();
    assert(t.text == "Hello\nWorld");
    assert(t.autoSize == false);
    assert(t.boxWidth == 20 && t.boxHeight == 8);
    assert(t.style.fontFamily == "DejaVu Sans");
    assert(std::abs(t.style.fontSize - 24.0F) < 1e-3F);
    assert(t.style.fontWeight == 700);
    assert(t.style.italic);
    assert(t.style.color == (Pixel{200, 100, 50, 220}));
    assert(std::abs(t.style.lineSpacing - 1.5F) < 1e-4F);
    assert(std::abs(t.style.letterSpacing - 2.0F) < 1e-4F);
    assert(t.style.horizontalAlign == TextHorizontalAlign::Center);
    assert(t.style.verticalAlign == TextVerticalAlign::Bottom);
    assert(t.style.wordWrap);
    assert(loadedText->x() == 3 && loadedText->y() == 4);
    assert(std::abs(loadedText->opacity() - 0.5F) < 1e-4F);
    assert(loadedText->buffer().getPixel(0, 0) == (Pixel{9, 8, 7, 255}));

    std::filesystem::remove_all(dir);
}

void testAutoSizeSerialization() {
    std::random_device rd;
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("glyma_text_auto_" + std::to_string(rd()));
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / "auto.glyma";

    Document doc("Auto", 32, 32);
    TextLayerData data;
    data.text = "Auto sized";
    data.autoSize = true;
    Layer text("Auto", data);
    // Simulate a rendered cache whose dimensions exceed the 1x1 placeholder.
    text.buffer() = ImageBuffer(30, 12);
    text.buffer().clear(Pixel{5, 6, 7, 255});
    const LayerId id = doc.addLayer(std::move(text));

    ProjectWriter::save(doc, path.string());
    Document loaded = ProjectReader::load(path.string());

    const Layer* layer = loaded.findLayer(id);
    assert(layer != nullptr);
    assert(layer->isText());
    assert(layer->text().autoSize);
    assert(layer->width() == 30 && layer->height() == 12);
    assert(layer->buffer().getPixel(0, 0) == (Pixel{5, 6, 7, 255}));

    std::filesystem::remove_all(dir);
}

void testVersionOneMigration() {
    std::random_device rd;
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() /
        ("glyma_text_v1_" + std::to_string(rd()));
    std::filesystem::create_directories(dir);
    const std::filesystem::path path = dir / "legacy.glyma";

    nlohmann::json manifest;
    manifest["format"] = "glyma";
    manifest["version"] = 1;
    manifest["name"] = "Legacy";
    manifest["width"] = 2;
    manifest["height"] = 2;
    manifest["background"] = nlohmann::json{{"type", "transparent"}};
    nlohmann::json layer;
    layer["id"] = 1;
    layer["name"] = "L1";
    layer["visible"] = true;
    layer["opacity"] = 1.0;
    layer["x"] = 0;
    layer["y"] = 0;
    layer["width"] = 2;
    layer["height"] = 2;
    layer["data"] = "layers/layer-1.rgba";
    manifest["layers"] = nlohmann::json::array({layer});

    std::string pixels(2 * 2 * 4, '\0');
    for (std::size_t i = 0; i < 4; ++i) {
        pixels[i * 4 + 3] = static_cast<char>(255);
    }

    writeArchive(path, {
        {"manifest.json", manifest.dump()},
        {"layers/layer-1.rgba", pixels},
    });

    Document loaded = ProjectReader::load(path.string());
    assert(loaded.layerCount() == 1);
    assert(loaded.layerStack().layers().front().isRaster());
    assert(loaded.layerStack().layers().front().buffer().getPixel(0, 0).a == 255);

    std::filesystem::remove_all(dir);
}

} // namespace

int main() {
    testValidation();
    testAlignmentConversion();
    testLayerTextType();
    testTextCommandsUndoRedo();
    testTransformIntegration();
    testSerializationRoundTrip();
    testAutoSizeSerialization();
    testVersionOneMigration();

    std::cout << "All Glyma text layer tests passed\n";
    return 0;
}

#include "glyma/core/document.hpp"
#include "glyma/core/pixel.hpp"
#include "glyma/io/project_reader.hpp"
#include "glyma/io/project_writer.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <zip.h>

using glyma::core::Document;
using glyma::core::Pixel;
using glyma::core::ProjectReader;
using glyma::core::ProjectWriter;

namespace {

void writeArchive(const std::filesystem::path& path,
                  const std::map<std::string, std::string>& entries) {
    int error = 0;
    zip_t* za = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    assert(za != nullptr);
    std::vector<std::string> storage;
    storage.reserve(entries.size());
    for (const auto& [name, content] : entries) {
        storage.push_back(content);
        zip_source_t* src = zip_source_buffer(za, storage.back().data(), storage.back().size(), 0);
        assert(src != nullptr);
        assert(zip_file_add(za, name.c_str(), src, ZIP_FL_OVERWRITE) >= 0);
    }
    assert(zip_close(za) == 0);
}

nlohmann::json baseManifest(const std::string& name = "Doc",
                            std::uint32_t width = 1,
                            std::uint32_t height = 1) {
    nlohmann::json m;
    m["format"] = "glyma";
    m["version"] = 1;
    m["name"] = name;
    m["width"] = width;
    m["height"] = height;
    m["background"] = nlohmann::json{{"type", "transparent"}};
    m["layers"] = nlohmann::json::array();
    return m;
}

nlohmann::json layerEntry(std::uint64_t id,
                          const std::string& data,
                          std::uint32_t w = 1,
                          std::uint32_t h = 1) {
    nlohmann::json l;
    l["id"] = id;
    l["name"] = "L" + std::to_string(id);
    l["visible"] = true;
    l["opacity"] = 1.0;
    l["x"] = 0;
    l["y"] = 0;
    l["width"] = w;
    l["height"] = h;
    l["data"] = data;
    return l;
}

} // namespace

int main() {
    std::random_device rd;
    const std::filesystem::path tmp = std::filesystem::temp_directory_path() / ("glyma_proj_test_" + std::to_string(rd()));
    std::filesystem::create_directories(tmp);

    // Empty document
    {
        Document doc("Empty", 4, 3);
        doc.setTransparentBackground();
        const auto path = tmp / "empty.glyma";
        ProjectWriter::save(doc, path.string());
        auto loaded = ProjectReader::load(path.string());
        assert(loaded.width() == 4 && loaded.height() == 3);
        assert(loaded.name() == "Empty");
        assert(loaded.layerCount() == 0);
        assert(loaded.render().getPixel(0, 0) == doc.render().getPixel(0, 0));
        std::filesystem::remove(path);
    }

    // Multiple layers, order, properties, background
    {
        Document doc("Multi", 4, 4);
        doc.setSolidBackground(Pixel(10, 20, 30, 255));
        auto id1 = doc.addCanvasLayer("Bottom");
        doc.findLayer(id1)->buffer().clear(Pixel(255, 0, 0, 200));
        doc.findLayer(id1)->setOpacity(0.8F);
        doc.findLayer(id1)->setPosition(0, 0);
        doc.findLayer(id1)->setVisible(true);
        auto id2 = doc.addCanvasLayer("Top");
        doc.findLayer(id2)->buffer().clear(Pixel(0, 255, 0, 128));
        doc.findLayer(id2)->setOpacity(0.5F);
        doc.findLayer(id2)->setPosition(1, -1);
        doc.findLayer(id2)->setVisible(true);
        assert(doc.layerStack().layers()[0].id() == id1);
        assert(doc.layerStack().layers()[1].id() == id2);

        const auto before = doc.render();
        const auto path = tmp / "multi.glyma";
        ProjectWriter::save(doc, path.string());
        auto loaded = ProjectReader::load(path.string());
        assert(loaded.layerCount() == 2);
        assert(loaded.layerStack().layers()[0].id() == id1);
        assert(loaded.layerStack().layers()[1].id() == id2);
        assert(loaded.layerStack().layers()[0].name() == "Bottom");
        assert(loaded.layerStack().layers()[1].name() == "Top");
        assert(std::abs(loaded.layerStack().layers()[0].opacity() - 0.8F) < 1e-6F);
        assert(std::abs(loaded.layerStack().layers()[1].opacity() - 0.5F) < 1e-6F);
        assert(loaded.layerStack().layers()[1].x() == 1);
        assert(loaded.layerStack().layers()[1].y() == -1);
        assert(loaded.layerStack().layers()[1].visible() == true);
        const auto after = loaded.render();
        for (std::uint32_t y = 0; y < doc.height(); ++y) {
            for (std::uint32_t x = 0; x < doc.width(); ++x) {
                assert(before.getPixel(x, y) == after.getPixel(x, y));
            }
        }
        std::filesystem::remove(path);
    }

    // Transparent pixels
    {
        Document doc("Trans", 2, 2);
        doc.setTransparentBackground();
        auto id = doc.addCanvasLayer("A");
        doc.findLayer(id)->buffer().clear(Pixel(0, 0, 0, 0));
        doc.findLayer(id)->buffer().setPixel(0, 0, Pixel(100, 150, 200, 50));
        const auto before = doc.render();
        const auto path = tmp / "trans.glyma";
        ProjectWriter::save(doc, path.string());
        auto loaded = ProjectReader::load(path.string());
        const auto after = loaded.render();
        assert(before.getPixel(0, 0) == after.getPixel(0, 0));
        assert(before.getPixel(1, 0) == after.getPixel(1, 0));
        std::filesystem::remove(path);
    }

    // Nonexistent file
    {
        bool threw = false;
        try {
            (void)ProjectReader::load(tmp / "nope.glyma");
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
    }

    // Corrupted archive
    {
        const auto path = tmp / "bad.glyma";
        std::ofstream f(path.string());
        f << "not a zip";
        f.close();
        bool threw = false;
        try {
            (void)ProjectReader::load(path.string());
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
        std::filesystem::remove(path);
    }

    // Malformed manifest (invalid JSON)
    {
        const auto path = tmp / "bad_json.glyma";
        writeArchive(path, {{"manifest.json", "{ not valid json"}});
        bool threw = false;
        try {
            (void)ProjectReader::load(path.string());
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
        std::filesystem::remove(path);
    }

    // Unsupported version
    {
        const auto path = tmp / "bad_version.glyma";
        auto m = baseManifest();
        m["version"] = 3;
        writeArchive(path, {{"manifest.json", m.dump()}});
        bool threw = false;
        try {
            (void)ProjectReader::load(path.string());
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
        std::filesystem::remove(path);
    }

    // Missing layer data
    {
        const auto path = tmp / "missing_data.glyma";
        auto m = baseManifest();
        m["layers"].push_back(layerEntry(1, "layers/layer-1.rgba"));
        writeArchive(path, {{"manifest.json", m.dump()}});
        bool threw = false;
        try {
            (void)ProjectReader::load(path.string());
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
        std::filesystem::remove(path);
    }

    // Duplicate layer IDs
    {
        const auto path = tmp / "dup_ids.glyma";
        auto m = baseManifest();
        m["layers"].push_back(layerEntry(7, "layers/a.rgba"));
        m["layers"].push_back(layerEntry(7, "layers/b.rgba"));
        writeArchive(path, {
            {"manifest.json", m.dump()},
            {"layers/a.rgba", std::string(4, '\0')},
            {"layers/b.rgba", std::string(4, '\0')},
        });
        bool threw = false;
        try {
            (void)ProjectReader::load(path.string());
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
        std::filesystem::remove(path);
    }

    // Unsafe archive path (traversal)
    {
        const auto path = tmp / "traversal.glyma";
        auto m = baseManifest();
        m["layers"].push_back(layerEntry(1, "../outside.rgba"));
        writeArchive(path, {{"manifest.json", m.dump()}});
        bool threw = false;
        try {
            (void)ProjectReader::load(path.string());
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
        std::filesystem::remove(path);
    }

    // Duplicate archive entry names
    {
        const auto path = tmp / "dup_entries.glyma";
        int error = 0;
        zip_t* za = zip_open(path.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
        assert(za != nullptr);
        const std::string first = baseManifest().dump();
        const std::string second = "{}";
        zip_source_t* s1 = zip_source_buffer(za, first.data(), first.size(), 0);
        assert(zip_file_add(za, "manifest.json", s1, 0) >= 0);
        zip_source_t* s2 = zip_source_buffer(za, second.data(), second.size(), 0);
        const zip_int64_t dupIndex = zip_file_add(za, "manifest.json", s2, 0);
        if (dupIndex < 0) {
            zip_source_free(s2);
        }
        assert(zip_close(za) == 0);

        if (dupIndex >= 0) {
            // libzip allowed a duplicate; the reader must reject it.
            bool threw = false;
            try {
                (void)ProjectReader::load(path.string());
            } catch (const std::exception&) {
                threw = true;
            }
            assert(threw);
        }
        std::filesystem::remove(path);
    }

    // Invalid canvas dimensions in manifest
    {
        const auto path = tmp / "bad_dims.glyma";
        auto m = baseManifest("Doc", 0, 5);
        writeArchive(path, {{"manifest.json", m.dump()}});
        bool threw = false;
        try {
            (void)ProjectReader::load(path.string());
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
        std::filesystem::remove(path);
    }

    // Excessive canvas dimensions rejected by resource limit
    {
        const auto path = tmp / "huge.glyma";
        auto m = baseManifest("Doc", 100000, 100000);
        writeArchive(path, {{"manifest.json", m.dump()}});
        bool threw = false;
        try {
            (void)ProjectReader::load(path.string());
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
        std::filesystem::remove(path);
    }

    // Invalid opacity in manifest
    {
        const auto path = tmp / "bad_opacity.glyma";
        auto m = baseManifest();
        auto l = layerEntry(1, "layers/a.rgba");
        l["opacity"] = 1.5;
        m["layers"].push_back(l);
        writeArchive(path, {
            {"manifest.json", m.dump()},
            {"layers/a.rgba", std::string(4, '\0')},
        });
        bool threw = false;
        try {
            (void)ProjectReader::load(path.string());
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
        std::filesystem::remove(path);
    }

    // Wrong layer data length
    {
        const auto path = tmp / "bad_len.glyma";
        auto m = baseManifest();
        m["layers"].push_back(layerEntry(1, "layers/a.rgba", 2, 2));
        writeArchive(path, {
            {"manifest.json", m.dump()},
            {"layers/a.rgba", std::string(3, '\0')},
        });
        bool threw = false;
        try {
            (void)ProjectReader::load(path.string());
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
        std::filesystem::remove(path);
    }

    // Failed save does not corrupt an existing destination file
    {
        const auto path = tmp / "keep.glyma";
        Document doc("Keep", 1, 1);
        ProjectWriter::save(doc, path.string());
        const auto originalSize = std::filesystem::file_size(path);
        const auto originalBytes = [&] {
            std::ifstream in(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(in),
                               std::istreambuf_iterator<char>());
        }();

        // Exceed the documented layer-count limit so save fails after opening.
        Document tooMany("TooMany", 1, 1);
        for (std::size_t i = 0; i <= ProjectWriter::maxLayerCount; ++i) {
            tooMany.addCanvasLayer("L" + std::to_string(i));
        }
        bool threw = false;
        try {
            ProjectWriter::save(tooMany, path.string());
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);
        assert(std::filesystem::exists(path));
        assert(std::filesystem::file_size(path) == originalSize);
        std::ifstream in(path, std::ios::binary);
        const std::string afterBytes{std::istreambuf_iterator<char>(in),
                                     std::istreambuf_iterator<char>()};
        assert(afterBytes == originalBytes);
        std::filesystem::remove(path);
    }

    std::filesystem::remove_all(tmp);
    std::cout << "All Glyma project format tests passed!\n";
    return 0;
}

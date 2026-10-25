#include "glyma/io/project_writer.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <zip.h>

namespace glyma::core {
namespace {

// Removes a temporary file unless explicitly released (on successful rename).
class TempFileGuard {
public:
    explicit TempFileGuard(std::string path) : path_(std::move(path)) {}
    ~TempFileGuard() {
        if (!released_) {
            std::remove(path_.c_str());
        }
    }
    TempFileGuard(const TempFileGuard&) = delete;
    TempFileGuard& operator=(const TempFileGuard&) = delete;

    void release() noexcept { released_ = true; }

private:
    std::string path_;
    bool released_{false};
};

class ZipWriter {
public:
    ZipWriter(const std::string& path, int flags) {
        int error = 0;
        za_ = zip_open(path.c_str(), flags, &error);
        if (!za_) {
            throw std::runtime_error("Failed to create project archive");
        }
    }

    ~ZipWriter() {
        if (za_) {
            zip_discard(za_);
        }
    }

    ZipWriter(const ZipWriter&) = delete;
    ZipWriter& operator=(const ZipWriter&) = delete;

    [[nodiscard]] zip_t* get() const noexcept { return za_; }

    // Closes and releases ownership. After a successful call get() is null.
    void close() {
        if (!za_) {
            return;
        }
        zip_t* za = za_;
        za_ = nullptr;
        if (zip_close(za) < 0) {
            throw std::runtime_error("Failed to finalize project archive");
        }
    }

private:
    zip_t* za_{nullptr};
};

void addEntry(zip_t* za, const std::string& name, const std::string& content) {
    zip_source_t* src = zip_source_buffer(za, content.data(), content.size(), 0);
    if (!src) {
        throw std::runtime_error("Failed to create zip source");
    }
    if (zip_file_add(za, name.c_str(), src, ZIP_FL_OVERWRITE) < 0) {
        zip_source_free(src);
        throw std::runtime_error("Failed to add archive entry");
    }
}

} // namespace

void ProjectWriter::save(const Document& document, const std::string& path) {
    if (document.pixelCount() > maxCanvasPixels) {
        throw std::invalid_argument("Canvas exceeds resource limits");
    }
    if (document.layerCount() > maxLayerCount) {
        throw std::invalid_argument("Too many layers");
    }

    const std::string tmpPath = path + ".tmp";
    TempFileGuard tempGuard(tmpPath);

    ZipWriter archive(tmpPath, ZIP_CREATE | ZIP_TRUNCATE);
    zip_t* za = archive.get();

    // zip_source_buffer with freep=0 does not copy; keep the payloads alive
    // until zip_close() finalizes the archive.
    std::vector<std::string> payloads;
    payloads.reserve(document.layerCount() * 2 + 1);

    nlohmann::json manifest;
    manifest["format"] = "glyma";
    manifest["version"] = 2;
    manifest["name"] = std::string(document.name());
    manifest["width"] = document.width();
    manifest["height"] = document.height();
    if (document.hasSolidBackground()) {
        manifest["background"]["type"] = "solid";
        manifest["background"]["r"] = document.backgroundColor().r;
        manifest["background"]["g"] = document.backgroundColor().g;
        manifest["background"]["b"] = document.backgroundColor().b;
        manifest["background"]["a"] = document.backgroundColor().a;
    } else {
        manifest["background"] = nlohmann::json{{"type", "transparent"}};
    }

    nlohmann::json layersArr = nlohmann::json::array();
    for (const auto& layer : document.layerStack().layers()) {
        const std::string dataName =
            std::string("layers/layer-") + std::to_string(layer.id()) + ".rgba";

        nlohmann::json lj;
        lj["id"] = static_cast<std::uint64_t>(layer.id());
        lj["name"] = std::string(layer.name());
        lj["visible"] = layer.visible();
        lj["opacity"] = layer.opacity();
        lj["x"] = layer.x();
        lj["y"] = layer.y();
        lj["width"] = layer.width();
        lj["height"] = layer.height();
        lj["data"] = dataName;

        if (layer.isText()) {
            const TextLayerData& text = layer.text();
            lj["type"] = "text";
            nlohmann::json tj;
            tj["content"] = text.text;
            tj["fontFamily"] = text.style.fontFamily;
            tj["fontSize"] = text.style.fontSize;
            tj["fontWeight"] = text.style.fontWeight;
            tj["italic"] = text.style.italic;
            tj["color"] = {
                {"r", text.style.color.r},
                {"g", text.style.color.g},
                {"b", text.style.color.b},
                {"a", text.style.color.a},
            };
            tj["lineSpacing"] = text.style.lineSpacing;
            tj["letterSpacing"] = text.style.letterSpacing;
            tj["horizontalAlign"] =
                textHorizontalAlignName(text.style.horizontalAlign);
            tj["verticalAlign"] =
                textVerticalAlignName(text.style.verticalAlign);
            tj["wordWrap"] = text.style.wordWrap;
            tj["autoSize"] = text.autoSize;
            tj["boxWidth"] = text.boxWidth;
            tj["boxHeight"] = text.boxHeight;
            lj["text"] = std::move(tj);
        } else {
            lj["type"] = "raster";
        }

        layersArr.push_back(std::move(lj));

        const auto& buf = layer.buffer();
        const std::size_t dataSize =
            static_cast<std::size_t>(buf.width()) * static_cast<std::size_t>(buf.height()) * 4;
        std::string data(dataSize, '\0');
        for (std::uint32_t y = 0; y < buf.height(); ++y) {
            for (std::uint32_t x = 0; x < buf.width(); ++x) {
                const Pixel p = buf.getPixel(x, y);
                const std::size_t i = (static_cast<std::size_t>(y) * buf.width() + x) * 4;
                data[i + 0] = static_cast<char>(p.r);
                data[i + 1] = static_cast<char>(p.g);
                data[i + 2] = static_cast<char>(p.b);
                data[i + 3] = static_cast<char>(p.a);
            }
        }
        payloads.push_back(std::move(data));
        addEntry(za, dataName, payloads.back());
    }

    manifest["layers"] = std::move(layersArr);
    payloads.push_back(manifest.dump(2));
    addEntry(za, "manifest.json", payloads.back());

    archive.close();

    // Replace the destination only after the archive was written successfully.
    if (std::rename(tmpPath.c_str(), path.c_str()) != 0) {
        throw std::runtime_error("Failed to replace project file");
    }
    tempGuard.release();
}

} // namespace glyma::core

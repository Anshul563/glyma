#include "glyma/io/project_reader.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <zip.h>

namespace glyma::core {
namespace {

void validateArchiveEntries(zip_t* za) {
    const zip_int64_t count = zip_get_num_entries(za, 0);
    if (count < 0) {
        throw std::runtime_error("Failed to inspect project archive");
    }

    std::set<std::string> names;
    std::uint64_t totalUncompressed = 0;

    for (zip_uint64_t i = 0; i < static_cast<zip_uint64_t>(count); ++i) {
        const char* rawName = zip_get_name(za, i, 0);
        if (!rawName) {
            throw std::runtime_error("Failed to read archive entry name");
        }
        const std::string name(rawName);
        if (name.empty() || name.front() == '/' ||
            name.find("..") != std::string::npos) {
            throw std::runtime_error("Unsafe archive path");
        }
        if (!names.insert(name).second) {
            throw std::runtime_error("Duplicate archive entry name");
        }

        zip_stat_t st{};
        if (zip_stat_index(za, i, 0, &st) != 0 || !(st.valid & ZIP_STAT_SIZE)) {
            throw std::runtime_error("Failed to read archive entry size");
        }
        totalUncompressed += static_cast<std::uint64_t>(st.size);
        if (totalUncompressed > ProjectReader::maxArchiveUncompressed) {
            throw std::runtime_error("Archive exceeds uncompressed size limits");
        }
    }
}

std::string readZipEntry(zip_t* za, const std::string& name) {
    // Prevent path traversal and absolute paths.
    if (name.find("..") != std::string::npos) {
        throw std::runtime_error("Unsafe archive path");
    }
    if (name.empty() || name.front() == '/') {
        throw std::runtime_error("Unsafe archive path");
    }

    zip_stat_t st{};
    if (zip_stat(za, name.c_str(), 0, &st) != 0) {
        throw std::runtime_error("Missing required archive entry");
    }
    if (st.size > static_cast<zip_uint64_t>(ProjectReader::maxArchiveUncompressed)) {
        throw std::runtime_error("Archive entry exceeds size limits");
    }

    zip_file_t* zf = zip_fopen(za, name.c_str(), 0);
    if (!zf) {
        throw std::runtime_error("Failed to open archive entry");
    }
    std::string data;
    data.resize(static_cast<std::size_t>(st.size));
    const zip_int64_t nread = zip_fread(zf, data.data(), static_cast<zip_uint64_t>(data.size()));
    zip_fclose(zf);
    if (nread < 0 || static_cast<std::size_t>(nread) != data.size()) {
        throw std::runtime_error("Failed to read archive entry");
    }
    return data;
}

// Ensures the archive is closed exactly once, on success or error.
class ZipHandle {
public:
    explicit ZipHandle(zip_t* za) : za_(za) {}
    ~ZipHandle() {
        if (za_) {
            zip_close(za_);
        }
    }
    ZipHandle(const ZipHandle&) = delete;
    ZipHandle& operator=(const ZipHandle&) = delete;

    [[nodiscard]] zip_t* get() const noexcept { return za_; }

    void closeAndRelease() {
        if (za_) {
            const int result = zip_close(za_);
            za_ = nullptr;
            if (result < 0) {
                throw std::runtime_error("Failed to close project archive");
            }
        }
    }

private:
    zip_t* za_;
};

} // namespace

Document ProjectReader::load(const std::string& path) {
    int error = 0;
    zip_t* raw = zip_open(path.c_str(), 0, &error);
    if (!raw) {
        throw std::runtime_error("Failed to open project archive");
    }
    ZipHandle handle(raw);
    zip_t* za = handle.get();

    validateArchiveEntries(za);

    const std::string manifestStr = readZipEntry(za, "manifest.json");
    nlohmann::json manifest = nlohmann::json::parse(manifestStr, nullptr, true);

    if (!manifest.contains("format") || !manifest["format"].is_string() ||
        manifest["format"].get<std::string>() != "glyma") {
        throw std::runtime_error("Invalid project format");
    }
    if (!manifest.contains("version") || !manifest["version"].is_number_integer() ||
        manifest["version"].get<int>() != 1) {
        throw std::runtime_error("Unsupported project format version");
    }
    if (!manifest.contains("name") || !manifest["name"].is_string() ||
        !manifest.contains("width") || !manifest.contains("height")) {
        throw std::runtime_error("Invalid project manifest");
    }

    Document doc(
        manifest["name"].get<std::string>(),
        manifest["width"].get<std::uint32_t>(),
        manifest["height"].get<std::uint32_t>()
    );

    if (manifest.contains("background") && manifest["background"].contains("type") &&
        manifest["background"]["type"] == "solid") {
        const auto& bg = manifest["background"];
        Pixel color{
            bg.value("r", static_cast<unsigned char>(0)),
            bg.value("g", static_cast<unsigned char>(0)),
            bg.value("b", static_cast<unsigned char>(0)),
            bg.value("a", static_cast<unsigned char>(255))
        };
        doc.setSolidBackground(color);
    } else {
        doc.setTransparentBackground();
    }

    if (!manifest.contains("layers") || !manifest["layers"].is_array()) {
        throw std::runtime_error("Invalid project manifest");
    }
    if (manifest["layers"].size() > maxLayerCount) {
        throw std::runtime_error("Too many layers");
    }

    std::vector<LayerId> seenIds;
    for (const auto& lj : manifest["layers"]) {
        if (!lj.contains("id") || !lj.contains("name") || !lj.contains("data") ||
            !lj.contains("width") || !lj.contains("height")) {
            throw std::runtime_error("Invalid layer entry");
        }
        if (!lj["name"].is_string() || !lj["data"].is_string()) {
            throw std::runtime_error("Invalid layer entry");
        }

        const LayerId lid = lj["id"].get<LayerId>();
        if (lid == Layer::invalidId) {
            throw std::runtime_error("Invalid layer ID");
        }
        if (std::find(seenIds.begin(), seenIds.end(), lid) != seenIds.end()) {
            throw std::runtime_error("Duplicate layer ID");
        }
        seenIds.push_back(lid);

        const float opacity = lj.value("opacity", 1.0F);
        if (!std::isfinite(opacity) || opacity < 0.0F || opacity > 1.0F) {
            throw std::runtime_error("Invalid layer opacity");
        }

        const std::uint32_t lw = lj.value("width", static_cast<std::uint32_t>(0));
        const std::uint32_t lh = lj.value("height", static_cast<std::uint32_t>(0));
        if (lw == 0 || lh == 0) {
            throw std::runtime_error("Invalid layer dimensions");
        }

        // Guard against multiplication overflow on 32-bit size_t.
        const std::uint64_t expected =
            static_cast<std::uint64_t>(lw) * static_cast<std::uint64_t>(lh) * 4ULL;
        if (expected > maxArchiveUncompressed) {
            throw std::runtime_error("Layer image exceeds size limits");
        }

        const std::string dataPath = lj["data"].get<std::string>();
        const std::string raw = readZipEntry(za, dataPath);
        if (raw.size() != static_cast<std::size_t>(expected)) {
            throw std::runtime_error("Invalid layer image data length");
        }

        Layer layer(lid, lj["name"].get<std::string>(), lw, lh);
        layer.setVisible(lj.value("visible", true));
        layer.setOpacity(opacity);
        layer.setPosition(lj.value("x", 0), lj.value("y", 0));

        auto& buf = layer.buffer();
        const unsigned char* d = reinterpret_cast<const unsigned char*>(raw.data());
        for (std::uint32_t y = 0; y < lh; ++y) {
            for (std::uint32_t x = 0; x < lw; ++x) {
                const std::size_t idx = (static_cast<std::size_t>(y) * lw + x) * 4;
                buf.setPixel(x, y, Pixel{d[idx + 0], d[idx + 1], d[idx + 2], d[idx + 3]});
            }
        }

        doc.addLayer(std::move(layer));
    }

    handle.closeAndRelease();
    return doc;
}

} // namespace glyma::core

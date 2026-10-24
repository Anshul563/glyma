#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "glyma/core/document.hpp"

namespace glyma::core {

class ProjectReader {
public:
    static constexpr std::uint64_t maxCanvasPixels = 64ULL * 1024ULL * 1024ULL;
    static constexpr std::size_t maxLayerCount = 1024;
    static constexpr std::uint64_t maxArchiveUncompressed = 512ULL * 1024ULL * 1024ULL;

    // Load .glyma file into a new Document. Throws on error.
    [[nodiscard]] static Document load(const std::string& path);
};

} // namespace glyma::core

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "glyma/core/document.hpp"

namespace glyma::core {

class ProjectWriter {
public:
    static constexpr std::uint64_t maxCanvasPixels = 64ULL * 1024ULL * 1024ULL;
    static constexpr std::size_t maxLayerCount = 1024;

    // Save document to .glyma (ZIP container). Writes atomically when possible.
    static void save(const Document& document, const std::string& path);
};

} // namespace glyma::core

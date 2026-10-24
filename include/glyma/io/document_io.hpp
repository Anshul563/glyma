#pragma once

#include <string>

#include "glyma/core/document.hpp"

namespace glyma::core {

class DocumentIO {
public:
    // Render document and export as PNG. Does not modify document.
    static void exportPng(const Document& document, const std::string& path);
};

} // namespace glyma::core

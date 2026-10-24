#include "glyma/io/document_io.hpp"

#include "glyma/io/png_writer.hpp"

namespace glyma::core {

void DocumentIO::exportPng(const Document& document, const std::string& path) {
    const auto rendered = document.render();
    PngWriter::write(rendered, path);
}

} // namespace glyma::core

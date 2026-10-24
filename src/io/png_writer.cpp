#include "glyma/io/png_writer.hpp"

#include <cstdio>
#include <stdexcept>
#include <vector>

#include <png.h>

namespace glyma::core {

void PngWriter::write(const ImageBuffer& image, const std::string& path) {
    // Ensure parent directory exists if possible (best effort)
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        throw std::runtime_error("Failed to create PNG file: " + path);
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        std::fclose(fp);
        throw std::runtime_error("Failed to create PNG write struct");
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, nullptr);
        std::fclose(fp);
        throw std::runtime_error("Failed to create PNG info struct");
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        std::fclose(fp);
        throw std::runtime_error("Error writing PNG file");
    }

    png_init_io(png_ptr, fp);

    const png_uint_32 width = static_cast<png_uint_32>(image.width());
    const png_uint_32 height = static_cast<png_uint_32>(image.height());
    if (width == 0 || height == 0) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        std::fclose(fp);
        throw std::invalid_argument("Invalid image dimensions");
    }

    png_set_IHDR(
        png_ptr,
        info_ptr,
        width,
        height,
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );

    png_write_info(png_ptr, info_ptr);

    for (png_uint_32 y = 0; y < height; ++y) {
        std::vector<unsigned char> row(width * 4);
        for (png_uint_32 x = 0; x < width; ++x) {
            const Pixel p = image.getPixel(x, y);
            row[x * 4 + 0] = p.r;
            row[x * 4 + 1] = p.g;
            row[x * 4 + 2] = p.b;
            row[x * 4 + 3] = p.a;
        }
        png_write_row(png_ptr, row.data());
    }

    png_write_end(png_ptr, nullptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    std::fclose(fp);
}

} // namespace glyma::core

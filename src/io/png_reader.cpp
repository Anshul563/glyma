#include "glyma/io/png_reader.hpp"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

#include <png.h>

namespace glyma::core {

ImageBuffer PngReader::read(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        throw std::runtime_error("Failed to open PNG file: " + path);
    }

    unsigned char header[8];
    if (std::fread(header, 1, 8, fp) != 8) {
        std::fclose(fp);
        throw std::runtime_error("Invalid or corrupted PNG file");
    }
    if (png_sig_cmp(header, 0, 8)) {
        std::fclose(fp);
        throw std::runtime_error("Invalid or corrupted PNG file");
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        std::fclose(fp);
        throw std::runtime_error("Failed to create PNG read struct");
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        std::fclose(fp);
        throw std::runtime_error("Failed to create PNG info struct");
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        std::fclose(fp);
        throw std::runtime_error("Error reading PNG file");
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bit_depth = 0;
    int color_type = 0;
    int interlace_type = 0;
    int compression_type = 0;
    int filter_method = 0;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                 &interlace_type, &compression_type, &filter_method);

    if (width == 0 || height == 0) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        std::fclose(fp);
        throw std::invalid_argument("Invalid image dimensions");
    }

    const std::uint64_t px = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (px > maxPixelCount) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        std::fclose(fp);
        throw std::invalid_argument("Image dimensions exceed resource limits");
    }

    // Normalize to RGBA8
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    if (bit_depth == 16) {
        png_set_strip_16(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }
    if (!(color_type & PNG_COLOR_MASK_ALPHA)) {
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    }

    png_read_update_info(png_ptr, info_ptr);

    size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    std::vector<std::vector<unsigned char> > rows(height);
    for (png_uint_32 y = 0; y < height; ++y) {
        rows[y].resize(rowbytes);
    }

    std::vector<unsigned char*> row_pointers(height);
    for (png_uint_32 y = 0; y < height; ++y) {
        row_pointers[y] = rows[y].data();
    }

    png_read_image(png_ptr, row_pointers.data());

    ImageBuffer buffer(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
    for (png_uint_32 y = 0; y < height; ++y) {
        const unsigned char* row = rows[y].data();
        for (png_uint_32 x = 0; x < width; ++x) {
            const unsigned char* p = row + x * 4;
            Pixel pxl{p[0], p[1], p[2], p[3]};
            buffer.setPixel(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), pxl);
        }
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    std::fclose(fp);
    return buffer;
}

} // namespace glyma::core

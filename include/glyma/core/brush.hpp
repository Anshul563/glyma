#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "glyma/core/pixel.hpp"

namespace glyma::core {

enum class BrushMode {
    Paint,
    Erase,
};

// Parameters for the CPU round brush.
//
// Positions passed to BrushStroke are continuous image coordinates: the centre
// of pixel (x, y) is (x + 0.5, y + 0.5). Diameter is measured in document
// pixels. Opacity and hardness are normalized to [0, 1].
struct BrushSettings {
    static constexpr double maxDiameter = 10000.0;

    double diameter{20.0};
    Pixel color{0, 0, 0, 255};
    float opacity{1.0F};
    float hardness{1.0F};

    [[nodiscard]] bool isValid() const noexcept;
    // Throws std::invalid_argument when a parameter is out of range or not
    // finite.
    void validate() const;
};

// A single pixel mutated by a stroke, stored as an index into the layer's
// row-major RGBA8 buffer. `before` and `after` make the stroke reversible
// without keeping a full-document snapshot.
struct PixelChange {
    std::size_t index{0};
    Pixel before{};
    Pixel after{};
};

} // namespace glyma::core

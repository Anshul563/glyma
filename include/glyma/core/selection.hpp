#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace glyma::core {

// A selection over a document canvas.
//
// Mask-value semantics: a pixel is "selected" when the selection contains its
// centre; mask value `true` means selected, `false` means unselected.
//
// Geometric selections (rectangle, ellipse) are stored analytically, so no
// full-document mask is allocated for simple tools. Inversion is represented
// by a flag applied to the geometric test, which keeps inverted selections
// cheap as well. A full pixel mask can be added later as another variant to
// support arbitrary freehand or magic-wand selections without changing this
// interface.
//
// All geometry lives in continuous document coordinates, where the centre of
// pixel (x, y) is (x + 0.5, y + 0.5). Geometry is clipped to the document
// bounds [0, width] x [0, height].
class Selection {
public:
    enum class Type : std::uint8_t {
        None,        // nothing selected
        Rectangle,   // axis-aligned rectangle in document coordinates
        Ellipse,     // axis-aligned ellipse in document coordinates
    };

    Selection() = default;

    // Set the document dimensions; existing geometry is re-clipped to them.
    void setDocumentSize(
        std::uint32_t width,
        std::uint32_t height
    ) noexcept;

    [[nodiscard]] std::uint32_t documentWidth() const noexcept;
    [[nodiscard]] std::uint32_t documentHeight() const noexcept;

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] Type type() const noexcept;
    [[nodiscard]] bool inverted() const noexcept;

    void clear() noexcept;

    // Define an axis-aligned rectangular selection. The endpoints may be
    // supplied in any order; they are normalized and clipped. A zero-area
    // rectangle is treated as an empty selection.
    void setRectangle(
        double left,
        double top,
        double right,
        double bottom
    );

    // Define an elliptical selection bounded by the given rectangle. The
    // endpoints may be supplied in any order; they are normalized and clipped.
    // A zero-area ellipse is treated as an empty selection.
    void setEllipse(
        double left,
        double top,
        double right,
        double bottom
    );

    // Invert the selection within the document bounds. Applying it twice
    // restores the original selection.
    void invert() noexcept;

    // Move the selection by (dx, dy) in document coordinates. Geometry is
    // re-clipped to the current document bounds; the inverted flag is
    // preserved.
    void translate(double dx, double dy) noexcept;

    // Scale the selection geometry and the stored document dimensions by the
    // supplied factors. Used when the whole document is resampled.
    void scale(double scaleX, double scaleY) noexcept;

    // Rotate the selection geometry with the document. The stored document
    // dimensions are updated to the rotated canvas size.
    void rotate90Clockwise() noexcept;
    void rotate90CounterClockwise() noexcept;
    void rotate180() noexcept;

    // Mirror the selection geometry within the document. Document dimensions
    // are unchanged.
    void flipHorizontal() noexcept;
    void flipVertical() noexcept;

    // True when the continuous document point (x, y) is selected.
    [[nodiscard]] bool contains(double x, double y) const noexcept;

    // True when the centre of pixel (x, y) is selected.
    [[nodiscard]] bool pixelSelected(
        std::uint32_t x,
        std::uint32_t y
    ) const noexcept;

    // Bounding box of the selected region in document coordinates. Returns
    // false when the selection is empty. When inverted, the bounds are the
    // entire document.
    [[nodiscard]] bool bounds(
        double& left,
        double& top,
        double& right,
        double& bottom
    ) const noexcept;

    // Invoke `callback(x, y)` for every selected pixel whose centre lies in
    // the half-open range [x0, x1) x [y0, y1). The range is intersected with
    // the document bounds.
    template <typename Callback>
    void forEachSelectedPixel(
        std::uint32_t x0,
        std::uint32_t y0,
        std::uint32_t x1,
        std::uint32_t y1,
        Callback&& callback
    ) const;

private:
    [[nodiscard]] bool containsGeometry(double x, double y) const noexcept;
    static void clipToDocument(
        double& left,
        double& top,
        double& right,
        double& bottom,
        std::uint32_t width,
        std::uint32_t height
    ) noexcept;

    Type type_{Type::None};
    bool inverted_{false};
    double left_{0.0};
    double top_{0.0};
    double right_{0.0};
    double bottom_{0.0}; // clipped document coordinates
    std::uint32_t docWidth_{0};
    std::uint32_t docHeight_{0};
};

template <typename Callback>
void Selection::forEachSelectedPixel(
    std::uint32_t x0,
    std::uint32_t y0,
    std::uint32_t x1,
    std::uint32_t y1,
    Callback&& callback
) const {
    const std::uint32_t startX = std::min(x0, docWidth_);
    const std::uint32_t startY = std::min(y0, docHeight_);
    const std::uint32_t endX = std::min(x1, docWidth_);
    const std::uint32_t endY = std::min(y1, docHeight_);

    for (std::uint32_t y = startY; y < endY; ++y) {
        const double cy = static_cast<double>(y) + 0.5;
        for (std::uint32_t x = startX; x < endX; ++x) {
            if (pixelSelected(x, y)) {
                callback(x, y);
            }
        }
    }
}

} // namespace glyma::core

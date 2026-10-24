#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "glyma/core/selection.hpp"

using glyma::core::Selection;

void testRectangleSelectionGeometry() {
    Selection selection;
    selection.setDocumentSize(100, 100);
    selection.setRectangle(10.0, 20.0, 40.0, 60.0);

    assert(selection.type() == Selection::Type::Rectangle);
    assert(!selection.isEmpty());
    assert(!selection.inverted());

    // Pixel centres fall inside the rectangle.
    assert(selection.pixelSelected(10, 20));
    assert(selection.pixelSelected(25, 40));
    assert(selection.pixelSelected(39, 59));

    // Pixels whose centres lie outside are unselected. The right/bottom edges
    // are exclusive in document space: pixel 40 spans [40, 41).
    assert(!selection.pixelSelected(9, 20));
    assert(!selection.pixelSelected(40, 20));
    assert(!selection.pixelSelected(20, 60));
    assert(!selection.pixelSelected(25, 60));

    // Continuous point containment.
    assert(selection.contains(10.0, 20.0));
    assert(selection.contains(40.0, 60.0));
    assert(!selection.contains(40.5, 30.0));
    assert(!selection.contains(25.0, 60.5));

    double left = 0, top = 0, right = 0, bottom = 0;
    assert(selection.bounds(left, top, right, bottom));
    assert(std::abs(left - 10.0) < 1e-9);
    assert(std::abs(top - 20.0) < 1e-9);
    assert(std::abs(right - 40.0) < 1e-9);
    assert(std::abs(bottom - 60.0) < 1e-9);
}

void testEllipseSelectionGeometry() {
    Selection selection;
    selection.setDocumentSize(100, 100);
    selection.setEllipse(10.0, 10.0, 50.0, 50.0); // centre (30,30), radius 20

    assert(selection.type() == Selection::Type::Ellipse);
    assert(!selection.isEmpty());

    // Centre is inside.
    assert(selection.pixelSelected(30, 30));
    assert(selection.contains(30.0, 30.0));

    // Just inside the ellipse boundary.
    assert(selection.pixelSelected(20, 20));
    assert(!selection.pixelSelected(10, 10)); // centre near corner, outside
    assert(!selection.pixelSelected(50, 30));  // just past the right edge
    assert(!selection.pixelSelected(30, 50));  // just past the bottom edge

    double left = 0, top = 0, right = 0, bottom = 0;
    assert(selection.bounds(left, top, right, bottom));
    assert(std::abs(left - 10.0) < 1e-9);
    assert(std::abs(right - 50.0) < 1e-9);
}

void testReverseDirectionDrag() {
    Selection selection;
    selection.setDocumentSize(100, 100);
    // Endpoints supplied bottom-right to top-left.
    selection.setRectangle(40.0, 60.0, 10.0, 20.0);

    assert(selection.type() == Selection::Type::Rectangle);
    assert(!selection.isEmpty());
    double left = 0, top = 0, right = 0, bottom = 0;
    assert(selection.bounds(left, top, right, bottom));
    assert(std::abs(left - 10.0) < 1e-9);
    assert(std::abs(top - 20.0) < 1e-9);
    assert(std::abs(right - 40.0) < 1e-9);
    assert(std::abs(bottom - 60.0) < 1e-9);
    assert(selection.pixelSelected(25, 40));
}

void testZeroSizeSelection() {
    Selection selection;
    selection.setDocumentSize(100, 100);

    selection.setRectangle(10.0, 10.0, 10.0, 50.0); // zero width
    assert(selection.isEmpty());
    assert(!selection.pixelSelected(10, 30));

    selection.setRectangle(10.0, 10.0, 50.0, 10.0); // zero height
    assert(selection.isEmpty());
    assert(!selection.pixelSelected(30, 10));

    selection.setEllipse(10.0, 10.0, 10.0, 50.0); // zero width
    assert(selection.isEmpty());
}

void testSelectionClippingAtDocumentBounds() {
    Selection selection;
    selection.setDocumentSize(100, 100);
    selection.setRectangle(-50.0, -50.0, 150.0, 150.0);

    assert(!selection.isEmpty());
    double left = 0, top = 0, right = 0, bottom = 0;
    assert(selection.bounds(left, top, right, bottom));
    assert(std::abs(left - 0.0) < 1e-9);
    assert(std::abs(top - 0.0) < 1e-9);
    assert(std::abs(right - 100.0) < 1e-9);
    assert(std::abs(bottom - 100.0) < 1e-9);

    assert(selection.pixelSelected(0, 0));
    assert(selection.pixelSelected(99, 99));
}

void testEmptySelection() {
    Selection selection;
    assert(selection.isEmpty());
    assert(selection.type() == Selection::Type::None);
    assert(!selection.inverted());
    assert(!selection.pixelSelected(0, 0));
    assert(!selection.contains(0.0, 0.0));

    double left = 0, top = 0, right = 0, bottom = 0;
    assert(!selection.bounds(left, top, right, bottom));

    selection.clear();
    assert(selection.isEmpty());
}

void testClearAndInvertSelection() {
    Selection selection;
    selection.setDocumentSize(10, 10);
    selection.setRectangle(2.0, 2.0, 6.0, 6.0);
    assert(!selection.isEmpty());
    assert(!selection.inverted());

    selection.invert();
    assert(selection.inverted());
    // Inverted bounds are the whole document.
    double left = 0, top = 0, right = 0, bottom = 0;
    assert(selection.bounds(left, top, right, bottom));
    assert(std::abs(left - 0.0) < 1e-9);
    assert(std::abs(top - 0.0) < 1e-9);
    assert(std::abs(right - 10.0) < 1e-9);
    assert(std::abs(bottom - 10.0) < 1e-9);

    // Outside the original rectangle is now selected; inside is not.
    assert(selection.pixelSelected(0, 0));
    assert(selection.pixelSelected(9, 9));
    assert(!selection.pixelSelected(4, 4));

    selection.invert(); // back to original
    assert(!selection.inverted());
    assert(selection.pixelSelected(4, 4));
    assert(!selection.pixelSelected(0, 0));

    selection.clear();
    assert(selection.isEmpty());
    // Inverting an empty selection selects the whole document.
    selection.invert();
    assert(selection.inverted());
    assert(selection.pixelSelected(5, 5));
}

void testMaskValuesAndBoundary() {
    Selection selection;
    selection.setDocumentSize(5, 5);

    // The whole document is selected.
    selection.setRectangle(0.0, 0.0, 5.0, 5.0);
    for (std::uint32_t y = 0; y < 5; ++y) {
        for (std::uint32_t x = 0; x < 5; ++x) {
            assert(selection.pixelSelected(x, y));
        }
    }

    // Only the top-left pixel is selected.
    selection.setRectangle(0.0, 0.0, 1.0, 1.0);
    assert(selection.pixelSelected(0, 0));
    assert(!selection.pixelSelected(1, 0));
    assert(!selection.pixelSelected(0, 1));
    assert(!selection.pixelSelected(1, 1));
}

void testInvertedSelectionExcludesInterior() {
    Selection selection;
    selection.setDocumentSize(4, 4);
    selection.setRectangle(1.0, 1.0, 3.0, 3.0);
    selection.invert();

    // The border pixels are selected; the interior 2x2 is not.
    assert(selection.pixelSelected(0, 0));
    assert(selection.pixelSelected(3, 3));
    assert(selection.pixelSelected(0, 3));
    assert(selection.pixelSelected(3, 0));
    assert(!selection.pixelSelected(1, 1));
    assert(!selection.pixelSelected(2, 2));
}

void testForEachSelectedPixel() {
    Selection selection;
    selection.setDocumentSize(100, 100);
    selection.setRectangle(50.0, 50.0, 60.0, 60.0);

    std::size_t count = 0;
    selection.forEachSelectedPixel(0, 0, 100, 100, [&count](std::uint32_t x, std::uint32_t y) {
        (void)x;
        (void)y;
        ++count;
    });
    assert(count == 10 * 10);

    count = 0;
    selection.forEachSelectedPixel(0, 0, 100, 100, [&count](std::uint32_t x, std::uint32_t y) {
        assert(x >= 50 && x < 60);
        assert(y >= 50 && y < 60);
        ++count;
    });
    assert(count == 100);

    // Range partially outside the document is clamped.
    count = 0;
    selection.forEachSelectedPixel(40, 40, 200, 200, [&count](std::uint32_t, std::uint32_t) {
        ++count;
    });
    assert(count == 100);
}

void testDocumentSizeChangeReclips() {
    Selection selection;
    selection.setDocumentSize(100, 100);
    selection.setRectangle(50.0, 50.0, 150.0, 150.0);
    double left = 0, top = 0, right = 0, bottom = 0;
    assert(selection.bounds(left, top, right, bottom));
    assert(std::abs(right - 100.0) < 1e-9);
    assert(std::abs(bottom - 100.0) < 1e-9);

    selection.setDocumentSize(60, 60);
    assert(selection.bounds(left, top, right, bottom));
    assert(std::abs(left - 50.0) < 1e-9);
    assert(std::abs(top - 50.0) < 1e-9);
    assert(std::abs(right - 60.0) < 1e-9);
    assert(std::abs(bottom - 60.0) < 1e-9);
    assert(selection.pixelSelected(59, 59));
    assert(!selection.pixelSelected(60, 60));
}

int main() {
    testRectangleSelectionGeometry();
    testEllipseSelectionGeometry();
    testReverseDirectionDrag();
    testZeroSizeSelection();
    testSelectionClippingAtDocumentBounds();
    testEmptySelection();
    testClearAndInvertSelection();
    testMaskValuesAndBoundary();
    testInvertedSelectionExcludesInterior();
    testForEachSelectedPixel();
    testDocumentSizeChangeReclips();

    std::cout << "All Glyma selection tests passed\n";
    return 0;
}

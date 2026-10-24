#include "glyma/core/compositor.hpp"
#include "glyma/core/image_buffer.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

using glyma::core::Compositor;
using glyma::core::ImageBuffer;
using glyma::core::Pixel;

int main() {
    // Image buffer tests.
    ImageBuffer image(10, 20);

    assert(image.width() == 10);
    assert(image.height() == 20);
    assert(image.pixels().size() == 200);
    assert(image.getPixel(0, 0) == Pixel(0, 0, 0, 0));

    image.setPixel(3, 4, Pixel(255, 0, 0));
    assert(image.getPixel(3, 4) == Pixel(255, 0, 0));

    image.clear(Pixel(0, 0, 255));
    assert(image.getPixel(3, 4) == Pixel(0, 0, 255));

    bool threw = false;

    try {
        image.getPixel(10, 0);
    } catch (const std::out_of_range&) {
        threw = true;
    }

    assert(threw);

    threw = false;

    try {
        ImageBuffer invalid(0, 10);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);

    // Test 1: Opaque source replaces destination.
    {
        ImageBuffer source(1, 1);
        ImageBuffer destination(1, 1);

        source.setPixel(0, 0, Pixel(255, 0, 0, 255));
        destination.setPixel(0, 0, Pixel(0, 0, 255, 255));

        Compositor::composite(source, destination);

        assert(destination.getPixel(0, 0) == Pixel(255, 0, 0, 255));
    }

    // Test 2: Half-transparent red over opaque blue.
    {
        ImageBuffer source(1, 1);
        ImageBuffer destination(1, 1);

        source.setPixel(0, 0, Pixel(255, 0, 0, 128));
        destination.setPixel(0, 0, Pixel(0, 0, 255, 255));

        Compositor::composite(source, destination);

        assert(destination.getPixel(0, 0) == Pixel(128, 0, 127, 255));
    }

    // Test 3: Fully transparent source leaves destination unchanged.
    {
        ImageBuffer source(1, 1);
        ImageBuffer destination(1, 1);

        source.setPixel(0, 0, Pixel(255, 0, 0, 0));
        destination.setPixel(0, 0, Pixel(0, 255, 0, 255));

        Compositor::composite(source, destination);

        assert(destination.getPixel(0, 0) == Pixel(0, 255, 0, 255));
    }

    // Test 4: Zero layer opacity leaves destination unchanged.
    {
        ImageBuffer source(1, 1);
        ImageBuffer destination(1, 1);

        source.setPixel(0, 0, Pixel(255, 0, 0, 255));
        destination.setPixel(0, 0, Pixel(0, 0, 255, 255));

        Compositor::composite(source, destination, 0.0F);

        assert(destination.getPixel(0, 0) == Pixel(0, 0, 255, 255));
    }

    // Test 5: Half-transparent source over transparent destination.
    {
        ImageBuffer source(1, 1);
        ImageBuffer destination(1, 1);

        source.setPixel(0, 0, Pixel(255, 0, 0, 128));

        Compositor::composite(source, destination);

        assert(destination.getPixel(0, 0) == Pixel(255, 0, 0, 128));
    }

    // Test 6: Invalid opacity must be rejected.
    {
        ImageBuffer source(1, 1);
        ImageBuffer destination(1, 1);

        bool invalidOpacityRejected = false;

        try {
            Compositor::composite(source, destination, 1.5F);
        } catch (const std::invalid_argument&) {
            invalidOpacityRejected = true;
        }

        assert(invalidOpacityRejected);
    }

    // Test 7: Mismatched image dimensions must be rejected.
    {
        ImageBuffer source(2, 2);
        ImageBuffer destination(1, 1);

        bool dimensionsRejected = false;

        try {
            Compositor::composite(source, destination);
        } catch (const std::invalid_argument&) {
            dimensionsRejected = true;
        }

        assert(dimensionsRejected);
    }

    std::cout << "All Glyma core tests passed!\n";

    return 0;
}
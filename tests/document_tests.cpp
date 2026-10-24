#include "glyma/core/compositor.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/pixel.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using glyma::core::Document;
using glyma::core::ImageBuffer;
using glyma::core::Layer;
using glyma::core::Pixel;

int main() {
    // Document construction and default properties
    {
        Document doc("Untitled", 4, 3);
        assert(doc.width() == 4);
        assert(doc.height() == 3);
        assert(doc.pixelCount() == 12);
        assert(doc.name() == "Untitled");
        assert(doc.id() != Document::invalidId);
        assert(doc.layerCount() == 0);
        assert(doc.hasSolidBackground() == false); // transparent by default
    }

    // Invalid dimensions
    {
        bool threw = false;
        try {
            Document bad("Bad", 0, 5);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);

        threw = false;
        try {
            Document bad("Bad", 5, 0);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);

        threw = false;
        try {
            Document bad("", 1, 1);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);

        threw = false;
        try {
            Document bad("   ", 1, 1);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);
    }

    // Document naming
    {
        Document doc("Doc", 1, 1);
        doc.setName("Renamed");
        assert(doc.name() == "Renamed");
    }

    // Adding and removing layers, lookup, reordering
    {
        Document doc("Doc", 2, 2);
        auto id1 = doc.addCanvasLayer("Layer1");
        auto id2 = doc.addCanvasLayer("Layer2");
        assert(doc.layerCount() == 2);
        assert(doc.findLayer(id1) != nullptr);
        assert(doc.findLayer(id2) != nullptr);
        assert(doc.findLayer(999999) == nullptr);
    }

    // Simpler reordering check
    {
        Document doc("Doc", 2, 2);
        auto id1 = doc.addCanvasLayer("L1");
        auto id2 = doc.addCanvasLayer("L2");
        auto id3 = doc.addCanvasLayer("L3");
        // bottom-to-top order by stack
        assert(doc.layerStack().layers()[0].id() == id1);
        assert(doc.layerStack().layers()[2].id() == id3);
        assert(doc.moveLayerUp(id1) == true);
        assert(doc.layerStack().layers()[0].id() == id2);
        assert(doc.moveLayerDown(id1) == true);
        assert(doc.layerStack().layers()[0].id() == id1);
    }

    // Transparent backgrounds
    {
        Document doc("Doc", 2, 2); // transparent default
        auto out = doc.render();
        assert(out.getPixel(0, 0).a == 0);
    }

    // Solid-color backgrounds
    {
        Document doc("Doc", 2, 2);
        doc.setSolidBackground(Pixel(10, 20, 30, 255));
        auto out = doc.render();
        assert(out.getPixel(0, 0) == Pixel(10, 20, 30, 255));
    }

    // Rendering one layer
    {
        Document doc("Doc", 2, 2);
        auto id = doc.addCanvasLayer("Red");
        doc.findLayer(id)->buffer().clear(Pixel(255, 0, 0, 255));
        auto out = doc.render();
        assert(out.getPixel(0, 0) == Pixel(255, 0, 0, 255));
    }

    // Rendering overlapping layers
    {
        Document doc("Doc", 1, 1);
        auto id1 = doc.addCanvasLayer("Bottom");
        doc.findLayer(id1)->buffer().clear(Pixel(255, 0, 0, 255));
        auto id2 = doc.addCanvasLayer("Top");
        doc.findLayer(id2)->buffer().clear(Pixel(0, 0, 255, 128));
        auto out = doc.render();
        assert(out.getPixel(0, 0).r == 127);
        assert(out.getPixel(0, 0).g == 0);
        assert(out.getPixel(0, 0).b == 128);
        assert(out.getPixel(0, 0).a == 255);
    }

    // Rendering with layer opacity
    {
        Document doc("Doc", 1, 1);
        doc.setSolidBackground(Pixel(0, 0, 255, 255));
        auto id = doc.addCanvasLayer("Red");
        doc.findLayer(id)->buffer().clear(Pixel(255, 0, 0, 255));
        doc.findLayer(id)->setOpacity(0.5F);
        auto out = doc.render();
        assert(out.getPixel(0, 0).r == 128);
        assert(out.getPixel(0, 0).g == 0);
        assert(out.getPixel(0, 0).b == 128);
        assert(out.getPixel(0, 0).a == 255);
    }

    // Rendering with negative and positive offsets + clipping
    {
        Document doc("Doc", 4, 4);
        doc.setSolidBackground(Pixel(0, 0, 0, 255));
        auto id = doc.addCanvasLayer("Patch");
        doc.findLayer(id)->buffer().clear(Pixel(255, 255, 0, 255));
        doc.findLayer(id)->setPosition(-1, -1);
        auto out = doc.render();
        assert(out.getPixel(0, 0) == Pixel(255, 255, 0, 255));
        assert(out.getPixel(3, 0) == Pixel(0, 0, 0, 255));

        auto id2 = doc.addCanvasLayer("Blue");
        doc.findLayer(id2)->buffer().clear(Pixel(0, 0, 255, 255));
        doc.findLayer(id2)->setPosition(2, 2);
        out = doc.render();
        assert(out.getPixel(2, 2) == Pixel(0, 0, 255, 255));
        assert(out.getPixel(0, 0) == Pixel(255, 255, 0, 255));
    }

    // Canvas clipping
    {
        Document doc("Doc", 2, 2);
        doc.setSolidBackground(Pixel(10, 10, 10, 255));
        auto id = doc.addCanvasLayer("Big");
        // make layer larger conceptually by adding existing large layer
        Layer big("Big", 10, 10);
        big.buffer().clear(Pixel(0, 255, 0, 255));
        big.setPosition(-5, -5);
        doc.addLayer(std::move(big));
        auto out = doc.render();
        assert(out.getPixel(0, 0) == Pixel(0, 255, 0, 255));
        assert(out.getPixel(1, 1) == Pixel(0, 255, 0, 255));
    }

    // Ensuring rendering does not mutate layer pixels
    {
        Document doc("Doc", 2, 2);
        auto id = doc.addCanvasLayer("Red");
        doc.findLayer(id)->buffer().clear(Pixel(255, 0, 0, 255));
        auto before = doc.findLayer(id)->buffer().getPixel(0, 0);
        auto out = doc.render();
        auto after = doc.findLayer(id)->buffer().getPixel(0, 0);
        assert(before == after);
        (void)out;
    }

    // Resource-limit validation
    {
        bool threw = false;
        try {
            // exceed limit
            Document bad("Bad", 100000, 100000);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);
    }

    // Remove layer
    {
        Document doc("Doc", 2, 2);
        auto id = doc.addCanvasLayer("X");
        assert(doc.layerCount() == 1);
        assert(doc.removeLayer(id) == true);
        assert(doc.layerCount() == 0);
        assert(doc.removeLayer(id) == false);
    }

    std::cout << "All Glyma document tests passed!\n";
    return 0;
}

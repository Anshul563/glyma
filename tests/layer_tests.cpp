#include "glyma/core/compositor.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/layer_stack.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

using glyma::core::Compositor;
using glyma::core::ImageBuffer;
using glyma::core::Layer;
using glyma::core::LayerId;
using glyma::core::LayerStack;
using glyma::core::Pixel;

int main() {
    // Layer construction and default properties
    {
        Layer layer("Background", 10, 5);
        assert(layer.width() == 10);
        assert(layer.height() == 5);
        assert(layer.name() == "Background");
        assert(layer.visible() == true);
        assert(std::abs(layer.opacity() - 1.0F) < 1e-6F);
        assert(layer.x() == 0);
        assert(layer.y() == 0);
        assert(layer.id() != Layer::invalidId);
    }

    // Unique layer IDs
    {
        Layer a("A", 1, 1);
        Layer b("B", 1, 1);
        assert(a.id() != b.id());
    }

    // Layer naming and opacity validation
    {
        bool threw = false;
        try {
            Layer bad("", 1, 1);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);

        threw = false;
        try {
            Layer bad("   ", 1, 1);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);

        threw = false;
        try {
            Layer layer("Ok", 1, 1);
            layer.setOpacity(-0.1F);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);

        threw = false;
        try {
            Layer layer("Ok", 1, 1);
            layer.setOpacity(1.1F);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);

        threw = false;
        try {
            Layer layer("Ok", 1, 1);
            layer.setOpacity(std::numeric_limits<float>::infinity());
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);

        Layer layer("Ok", 1, 1);
        layer.setName("Renamed");
        assert(layer.name() == "Renamed");
        layer.setOpacity(0.5F);
        assert(std::abs(layer.opacity() - 0.5F) < 1e-6F);
    }

    // Visibility changes
    {
        Layer layer("L", 1, 1);
        layer.setVisible(false);
        assert(layer.visible() == false);
        layer.setVisible(true);
        assert(layer.visible() == true);
    }

    // Copy/move behavior
    {
        Layer layer("L", 2, 3);
        layer.setPosition(5, -3);
        layer.setOpacity(0.25F);
        layer.buffer().setPixel(0, 0, Pixel(10, 20, 30, 40));

        Layer copy = layer;
        assert(copy.id() == layer.id()); // IDs copied as-is
        assert(copy.name() == layer.name());
        assert(copy.x() == layer.x());
        assert(copy.y() == layer.y());
        assert(std::abs(copy.opacity() - layer.opacity()) < 1e-6F);
        assert(copy.buffer().getPixel(0, 0) == Pixel(10, 20, 30, 40));

        Layer moved = std::move(layer);
        assert(moved.id() == copy.id());
        assert(moved.name() == "L");
    }

    // LayerStack: add, find, remove, duplicate ID prevention
    {
        LayerStack stack;
        assert(stack.empty());
        assert(stack.size() == 0);

        Layer l1("L1", 1, 1);
        LayerId id1 = l1.id();
        Layer l2("L2", 1, 1);
        LayerId id2 = l2.id();

        stack.addLayer(std::move(l1));
        stack.addLayer(std::move(l2));
        assert(stack.size() == 2);

        assert(stack.findLayer(id1) != nullptr);
        assert(stack.findLayer(id2) != nullptr);
        assert(stack.findLayer(Layer::invalidId) == nullptr);
        assert(stack.findLayer(123456789ULL) == nullptr);

        const auto* cl1 = stack.findLayer(id1);
        assert(cl1 && cl1->name() == "L1");

        // Duplicate ID rejected
        {
            bool threw = false;
            try {
                Layer dup2(id1, "Dup2", 1, 1);
                stack.addLayer(std::move(dup2));
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            assert(threw);
        }

        assert(stack.removeLayer(id1) == true);
        assert(stack.findLayer(id1) == nullptr);
        assert(stack.removeLayer(id1) == false);
        assert(stack.size() == 1);
    }

    // Reordering layers
    {
        LayerStack stack;
        Layer a("A", 1, 1);
        Layer b("B", 1, 1);
        Layer c("C", 1, 1);
        LayerId ida = a.id(), idb = b.id(), idc = c.id();
        stack.addLayer(std::move(a));
        stack.addLayer(std::move(b));
        stack.addLayer(std::move(c));
        // Order bottom->top: A, B, C
        assert(stack.layers()[0].id() == ida);
        assert(stack.layers()[1].id() == idb);
        assert(stack.layers()[2].id() == idc);

        assert(stack.moveLayerUp(ida) == true); // A moves to index 1
        assert(stack.layers()[0].id() == idb);
        assert(stack.layers()[1].id() == ida);
        assert(stack.layers()[2].id() == idc);

        assert(stack.moveLayerDown(ida) == true); // back
        assert(stack.layers()[0].id() == ida);
        assert(stack.layers()[1].id() == idb);
        assert(stack.layers()[2].id() == idc);

        assert(stack.moveLayerDown(ida) == false); // already at bottom
        assert(stack.moveLayerUp(idc) == false);   // already at top
        assert(stack.moveLayerUp(999999) == false);
        assert(stack.moveLayerDown(999999) == false);
    }

    // Compositing with layers: transparent/opaque, opacity, offsets, clipping, invisible, empty stack
    {
        ImageBuffer canvas(4, 4);
        canvas.clear({0, 0, 0, 255});
        LayerStack stack;
        Compositor::composite(stack, canvas); // empty stack
        assert(canvas.getPixel(0, 0) == Pixel(0, 0, 0, 255));
    }

    {
        // Opaque layer replaces
        ImageBuffer canvas(2, 2);
        canvas.clear({0, 0, 255, 255});
        LayerStack stack;
        Layer layer("Red", 2, 2);
        layer.buffer().clear({255, 0, 0, 255});
        stack.addLayer(std::move(layer));
        Compositor::composite(stack, canvas);
        assert(canvas.getPixel(0, 0) == Pixel(255, 0, 0, 255));
        assert(canvas.getPixel(1, 1) == Pixel(255, 0, 0, 255));
    }

    {
        // Layer opacity
        ImageBuffer canvas(1, 1);
        canvas.clear({0, 0, 255, 255});
        LayerStack stack;
        Layer layer("Red", 1, 1);
        layer.buffer().clear({255, 0, 0, 255});
        layer.setOpacity(0.5F);
        stack.addLayer(std::move(layer));
        Compositor::composite(stack, canvas);
        const auto p = canvas.getPixel(0, 0);
        assert(p.r == 128 && p.g == 0 && p.b == 128 && p.a == 255);
    }

    {
        // X/Y offsets and clipping
        ImageBuffer canvas(4, 4);
        canvas.clear({10, 10, 10, 255});
        LayerStack stack;
        Layer layer("Blue", 2, 2);
        layer.buffer().clear({0, 0, 255, 255});
        layer.setPosition(2, 2);
        stack.addLayer(std::move(layer));
        Compositor::composite(stack, canvas);
        // outside offset area unchanged
        assert(canvas.getPixel(0, 0) == Pixel(10, 10, 10, 255));
        // inside
        assert(canvas.getPixel(2, 0) == Pixel(10, 10, 10, 255));
        assert(canvas.getPixel(2, 2) == Pixel(0, 0, 255, 255));
        assert(canvas.getPixel(3, 3) == Pixel(0, 0, 255, 255));
    }

    {
        // Negative offsets and canvas clipping
        ImageBuffer canvas(4, 4);
        canvas.clear({0, 0, 0, 255});
        LayerStack stack;
        Layer layer("Patch", 4, 4);
        layer.buffer().clear({255, 255, 0, 255});
        layer.setPosition(-1, -1);
        stack.addLayer(std::move(layer));
        Compositor::composite(stack, canvas);
        assert(canvas.getPixel(0, 0) == Pixel(255, 255, 0, 255));
        assert(canvas.getPixel(1, 0) == Pixel(255, 255, 0, 255));
        assert(canvas.getPixel(2, 0) == Pixel(255, 255, 0, 255));
        assert(canvas.getPixel(3, 0) == Pixel(0, 0, 0, 255));
        assert(canvas.getPixel(0, 1) == Pixel(255, 255, 0, 255));
    }

    {
        // Invisible layers ignored
        ImageBuffer canvas(2, 2);
        canvas.clear({0, 255, 0, 255});
        LayerStack stack;
        Layer layer("Red", 2, 2);
        layer.buffer().clear({255, 0, 0, 255});
        layer.setVisible(false);
        stack.addLayer(std::move(layer));
        Compositor::composite(stack, canvas);
        assert(canvas.getPixel(0, 0) == Pixel(0, 255, 0, 255));
    }

    {
        // Fully transparent layer ignored
        ImageBuffer canvas(2, 2);
        canvas.clear({0, 255, 0, 255});
        LayerStack stack;
        Layer layer("Red", 2, 2);
        layer.buffer().clear({255, 0, 0, 255});
        layer.setOpacity(0.0F);
        stack.addLayer(std::move(layer));
        Compositor::composite(stack, canvas);
        assert(canvas.getPixel(0, 0) == Pixel(0, 255, 0, 255));
    }

    {
        // Transparent and opaque compositing (bottom to top)
        ImageBuffer canvas(1, 1);
        canvas.clear({0, 0, 0, 0});
        LayerStack stack;
        Layer bottom("Bottom", 1, 1);
        bottom.buffer().clear({255, 0, 0, 128}); // half red
        stack.addLayer(std::move(bottom));
        Layer top("Top", 1, 1);
        top.buffer().clear({0, 0, 255, 128}); // half blue over red
        stack.addLayer(std::move(top));
        Compositor::composite(stack, canvas);
        // Compute expected via standard source-over: first bottom over transparent -> (255,0,0,128)
        // then top over that: sourceAlpha 0.5, destAlpha 0.5; outputAlpha 0.75
        // r = ((0*0.5) + (255*0.5*(0.5))) / 0.75 = (63.75)/0.75 = 85
        // b = ((255*0.5) + (0)) / 0.75 = 127.5/0.75 = 170; alpha rounds to 192
        const auto p = canvas.getPixel(0, 0);
        assert(p.r == 85 && p.g == 0 && p.b == 170 && p.a == 192);
    }

    std::cout << "All Glyma layer tests passed!\n";
    return 0;
}

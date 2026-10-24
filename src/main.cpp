#include "glyma/core/compositor.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/layer_stack.hpp"

#include <iostream>

int main() {
    glyma::core::ImageBuffer canvas(4, 4);
    canvas.clear({0, 0, 0, 255});

    glyma::core::LayerStack stack;

    // Bottom layer: red background
    glyma::core::Layer background("Background", 4, 4);
    background.buffer().clear({255, 0, 0, 255});
    stack.addLayer(std::move(background));

    // Top layer: blue square, partially transparent, offset
    glyma::core::Layer foreground("Foreground", 2, 2);
    foreground.buffer().clear({0, 0, 255, 128});
    foreground.setPosition(1, 1);
    foreground.setOpacity(1.0F);
    stack.addLayer(std::move(foreground));

    glyma::core::Compositor::composite(stack, canvas);

    std::cout << "Glyma Image Engine\n";
    std::cout << "Canvas: " << canvas.width() << " x " << canvas.height() << "\n";
    std::cout << "Layer stack size: " << stack.size() << "\n";

    const auto p00 = canvas.getPixel(0, 0);
    const auto p22 = canvas.getPixel(2, 2);
    const auto p11 = canvas.getPixel(1, 1);

    std::cout << "Pixel (0,0): RGBA("
              << static_cast<int>(p00.r) << ", "
              << static_cast<int>(p00.g) << ", "
              << static_cast<int>(p00.b) << ", "
              << static_cast<int>(p00.a) << ")\n";
    std::cout << "Pixel (1,1): RGBA("
              << static_cast<int>(p11.r) << ", "
              << static_cast<int>(p11.g) << ", "
              << static_cast<int>(p11.b) << ", "
              << static_cast<int>(p11.a) << ")\n";
    std::cout << "Pixel (2,2): RGBA("
              << static_cast<int>(p22.r) << ", "
              << static_cast<int>(p22.g) << ", "
              << static_cast<int>(p22.b) << ", "
              << static_cast<int>(p22.a) << ")\n";

    return 0;
}

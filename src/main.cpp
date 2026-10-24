#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/pixel.hpp"

#include <iostream>

int main() {
    glyma::core::Document document("Demo", 4, 4);
    document.setSolidBackground(glyma::core::Pixel{255, 0, 0, 255});

    auto bg = document.addCanvasLayer("BackgroundFill");
    if (auto* layer = document.findLayer(bg)) {
        layer->buffer().clear(glyma::core::Pixel{255, 0, 0, 255});
    }

    auto fg = document.addCanvasLayer("Foreground");
    if (auto* layer = document.findLayer(fg)) {
        layer->buffer().clear(glyma::core::Pixel{0, 0, 255, 128});
        layer->setPosition(1, 1);
        layer->setOpacity(1.0F);
    }

    const glyma::core::ImageBuffer canvas = document.render();

    std::cout << "Glyma Image Engine\n";
    std::cout << "Document: " << document.name() << "\n";
    std::cout << "Canvas: " << canvas.width() << " x " << canvas.height() << "\n";
    std::cout << "Layers: " << document.layerCount() << "\n";

    const auto p00 = canvas.getPixel(0, 0);
    const auto p11 = canvas.getPixel(1, 1);
    const auto p22 = canvas.getPixel(2, 2);

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


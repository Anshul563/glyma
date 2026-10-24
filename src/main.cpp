#include "glyma/core/command_history.hpp"
#include "glyma/core/commands.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/pixel.hpp"

#include <iostream>

int main() {
    glyma::core::Document document("Demo", 4, 4);
    document.setSolidBackground(glyma::core::Pixel{255, 0, 0, 255});

    glyma::core::CommandHistory history;

    std::cout << "Glyma Image Engine\n";
    std::cout << "Document: " << document.name() << "\n";
    std::cout << "Canvas: " << document.width() << " x " << document.height() << "\n";
    std::cout << "Layers (initial): " << document.layerCount() << "\n";

    // Execute command to add a layer
    history.execute(std::make_unique<glyma::core::AddLayerCommand>("Foreground", 4, 4), document);
    if (auto* layer = document.findLayer(document.layerStack().layers().empty() ? 0 : document.layerStack().layers()[0].id())) {
        layer->buffer().clear(glyma::core::Pixel{0, 0, 255, 128});
        layer->setPosition(1, 1);
    }
    std::cout << "Layers (after add): " << document.layerCount() << "\n";

    // Undo
    history.undo(document);
    std::cout << "Layers (after undo): " << document.layerCount() << "\n";

    // Redo
    history.redo(document);
    std::cout << "Layers (after redo): " << document.layerCount() << "\n";

    const glyma::core::ImageBuffer canvas = document.render();
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



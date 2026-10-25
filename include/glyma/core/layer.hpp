#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "glyma/core/image_buffer.hpp"
#include "glyma/core/text_layer.hpp"

namespace glyma::core {

using LayerId = std::uint64_t;

// Discriminates the payload a layer carries. Raster layers paint pixels
// directly; text layers keep editable text metadata and use their ImageBuffer
// as a rendered cache maintained by the application renderer.
enum class LayerType : std::uint8_t {
    Raster,
    Text,
};

// A single drawable layer in a document.
//
// Each layer owns its ImageBuffer. The layer's X/Y offset positions the
// buffer relative to the canvas origin; the compositor is responsible for
// clipping. IDs are generated automatically and are unique for every
// independently constructed layer. Copies intentionally preserve the source
// ID (so a layer can be duplicated and then re-identified if needed), and a
// LayerStack rejects duplicate IDs.
//
// Moved-from layers are valid but unspecified; only destruction or
// assignment is supported for them.
class Layer {
public:
    // ID 0 is reserved and never handed out by generateId().
    static constexpr LayerId invalidId = 0;

    Layer(std::string name, std::uint32_t width, std::uint32_t height);

    // Constructor for future serialization/deserialization. The caller is
    // responsible for choosing an ID that is unique within its stack.
    Layer(
        LayerId id,
        std::string name,
        std::uint32_t width,
        std::uint32_t height
    );

    // Text-layer constructors. The buffer starts as a 1x1 transparent cache
    // until the renderer produces the first frame (or as boxWidth x boxHeight
    // for fixed-size boxes). Throws std::invalid_argument for invalid data.
    Layer(std::string name, TextLayerData text);
    Layer(LayerId id, std::string name, TextLayerData text);

    Layer(const Layer&) = default;
    Layer(Layer&&) noexcept = default;
    Layer& operator=(const Layer&) = default;
    Layer& operator=(Layer&&) noexcept = default;
    ~Layer() = default;

    [[nodiscard]] LayerId id() const noexcept;

    [[nodiscard]] const std::string& name() const noexcept;
    // Throws std::invalid_argument for empty or whitespace-only names.
    void setName(std::string name);

    [[nodiscard]] ImageBuffer& buffer() noexcept;
    [[nodiscard]] const ImageBuffer& buffer() const noexcept;

    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;

    [[nodiscard]] LayerType type() const noexcept;
    [[nodiscard]] bool isRaster() const noexcept;
    [[nodiscard]] bool isText() const noexcept;

    // Text payload accessors. The const/non-const text() accessors throw
    // std::logic_error when the layer is not a text layer.
    [[nodiscard]] const TextLayerData& text() const;
    [[nodiscard]] TextLayerData& text();
    // Promote this layer to a text layer and store `data`. Throws
    // std::invalid_argument for invalid data.
    void setTextData(TextLayerData data);
    // Drop the text payload, converting the layer to a raster layer that keeps
    // its current rendered pixels. Returns false when already raster.
    bool rasterize() noexcept;

    [[nodiscard]] bool visible() const noexcept;
    void setVisible(bool visible) noexcept;

    [[nodiscard]] float opacity() const noexcept;
    // Throws std::invalid_argument unless opacity is finite and in [0, 1].
    void setOpacity(float opacity);

    [[nodiscard]] std::int32_t x() const noexcept;
    [[nodiscard]] std::int32_t y() const noexcept;
    void setX(std::int32_t x) noexcept;
    void setY(std::int32_t y) noexcept;
    void setPosition(std::int32_t x, std::int32_t y) noexcept;

    // Returns a new process-wide unique layer ID.
    [[nodiscard]] static LayerId generateId() noexcept;

private:
    LayerId id_;
    std::string name_;
    ImageBuffer buffer_;
    LayerType type_{LayerType::Raster};
    std::optional<TextLayerData> text_;
    bool visible_{true};
    float opacity_{1.0F};
    std::int32_t x_{0};
    std::int32_t y_{0};
};

} // namespace glyma::core

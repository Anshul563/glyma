#pragma once

#include <cstdint>
#include <string>

#include "glyma/core/image_buffer.hpp"

namespace glyma::core {

using LayerId = std::uint64_t;

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
    bool visible_{true};
    float opacity_{1.0F};
    std::int32_t x_{0};
    std::int32_t y_{0};
};

} // namespace glyma::core

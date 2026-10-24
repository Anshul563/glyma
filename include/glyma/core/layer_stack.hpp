#pragma once

#include <cstddef>
#include <vector>

#include "glyma/core/layer.hpp"

namespace glyma::core {

// An ordered collection of layers.
//
// Index 0 is the bottom-most layer and the last index is the top-most layer.
// Rendering therefore processes layers from index 0 upwards. Pointers and
// references returned by findLayer() (and references into layers()) are
// invalidated by any operation that adds or removes layers.
class LayerStack {
public:
    LayerStack() = default;
    LayerStack(const LayerStack&) = default;
    LayerStack(LayerStack&&) noexcept = default;
    LayerStack& operator=(const LayerStack&) = default;
    LayerStack& operator=(LayerStack&&) noexcept = default;
    ~LayerStack() = default;

    // Adds a layer above all existing layers. Returns its ID.
    // Throws std::invalid_argument if a layer with the same ID already exists.
    LayerId addLayer(Layer layer);

    // Removes the layer with the given ID. Returns false if it was not found.
    bool removeLayer(LayerId id) noexcept;

    // Returns nullptr if no layer with the given ID exists.
    [[nodiscard]] Layer* findLayer(LayerId id) noexcept;
    [[nodiscard]] const Layer* findLayer(LayerId id) const noexcept;

    // Moving "up" moves a layer towards the top of the stack (higher index).
    // Returns false if the layer is missing or already at the requested edge.
    bool moveLayerUp(LayerId id) noexcept;
    bool moveLayerDown(LayerId id) noexcept;

    // Move a layer to an explicit bottom-to-top index. Returns false if the
    // layer is missing, the index is out of range, or the position is unchanged.
    bool moveLayer(LayerId id, std::size_t newIndex) noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    // Layers are ordered bottom-to-top.
    [[nodiscard]] const std::vector<Layer>& layers() const noexcept;

private:
    [[nodiscard]] std::size_t indexOf(LayerId id) const noexcept;

    std::vector<Layer> layers_;
};

} // namespace glyma::core

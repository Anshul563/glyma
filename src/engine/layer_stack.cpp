#include "glyma/core/layer_stack.hpp"

#include <algorithm>
#include <stdexcept>

namespace glyma::core {

std::size_t LayerStack::indexOf(LayerId id) const noexcept {
    for (std::size_t i = 0; i < layers_.size(); ++i) {
        if (layers_[i].id() == id) {
            return i;
        }
    }
    return static_cast<std::size_t>(-1);
}

LayerId LayerStack::addLayer(Layer layer) {
    if (layer.id() == Layer::invalidId) {
        // We do not overwrite caller-chosen IDs; if someone passes 0, reject it.
        throw std::invalid_argument("Layer ID 0 is reserved and invalid.");
    }

    if (indexOf(layer.id()) != static_cast<std::size_t>(-1)) {
        throw std::invalid_argument("A layer with this ID already exists.");
    }

    const LayerId id = layer.id();
    layers_.push_back(std::move(layer));
    return id;
}

bool LayerStack::removeLayer(LayerId id) noexcept {
    const auto it = std::find_if(
        layers_.begin(),
        layers_.end(),
        [id](const Layer& layer) { return layer.id() == id; }
    );

    if (it == layers_.end()) {
        return false;
    }

    layers_.erase(it);
    return true;
}

Layer* LayerStack::findLayer(LayerId id) noexcept {
    const auto it = std::find_if(
        layers_.begin(),
        layers_.end(),
        [id](Layer& layer) { return layer.id() == id; }
    );

    if (it == layers_.end()) {
        return nullptr;
    }

    return &*it;
}

const Layer* LayerStack::findLayer(LayerId id) const noexcept {
    const auto it = std::find_if(
        layers_.begin(),
        layers_.end(),
        [id](const Layer& layer) { return layer.id() == id; }
    );

    if (it == layers_.end()) {
        return nullptr;
    }

    return &*it;
}

bool LayerStack::moveLayerUp(LayerId id) noexcept {
    const auto index = indexOf(id);
    if (index == static_cast<std::size_t>(-1)) {
        return false;
    }
    if (index + 1 >= layers_.size()) {
        return false;
    }

    std::swap(layers_[index], layers_[index + 1]);
    return true;
}

bool LayerStack::moveLayerDown(LayerId id) noexcept {
    const auto index = indexOf(id);
    if (index == static_cast<std::size_t>(-1)) {
        return false;
    }
    if (index == 0) {
        return false;
    }

    std::swap(layers_[index], layers_[index - 1]);
    return true;
}

std::size_t LayerStack::size() const noexcept {
    return layers_.size();
}

bool LayerStack::empty() const noexcept {
    return layers_.empty();
}

const std::vector<Layer>& LayerStack::layers() const noexcept {
    return layers_;
}

} // namespace glyma::core

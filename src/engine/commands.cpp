#include "glyma/core/commands.hpp"

#include <algorithm>
#include <stdexcept>

#include "glyma/core/document.hpp"

namespace glyma::core {

AddLayerCommand::AddLayerCommand(std::string name, std::uint32_t width, std::uint32_t height)
    : name_(std::move(name)), width_(width), height_(height) {}

std::string AddLayerCommand::name() const {
    return "AddLayer";
}

bool AddLayerCommand::execute(Document& document) {
    if (width_ == 0 || height_ == 0) {
        return false;
    }
    try {
        Layer layer(name_, width_, height_);
        addedId_ = layer.id();
        // Record index before adding (stack is bottom-to-top; added at end)
        addedIndex_ = document.layerStack().size();
        addedId_ = document.addLayer(std::move(layer));
        return addedId_ != Layer::invalidId;
    } catch (...) {
        return false;
    }
}

bool AddLayerCommand::undo(Document& document) {
    if (addedId_ == Layer::invalidId) {
        return false;
    }
    // Remove the layer we just added
    if (document.removeLayer(addedId_)) {
        return true;
    }
    return false;
}

RemoveLayerCommand::RemoveLayerCommand(LayerId id) : targetId_(id), capturedBuffer_(1, 1) {}

std::string RemoveLayerCommand::name() const {
    return "RemoveLayer";
}

bool RemoveLayerCommand::execute(Document& document) {
    Layer* layer = document.findLayer(targetId_);
    if (!layer) {
        return false;
    }

    // Capture state and index
    const auto& layers = document.layerStack().layers();
    for (std::size_t i = 0; i < layers.size(); ++i) {
        if (layers[i].id() == targetId_) {
            capturedIndex_ = i;
            break;
        }
    }

    capturedId_ = layer->id();
    capturedName_ = layer->name();
    // Deep copy buffer
    capturedBuffer_ = layer->buffer();
    capturedVisible_ = layer->visible();
    capturedOpacity_ = layer->opacity();
    capturedX_ = layer->x();
    capturedY_ = layer->y();
    captured_ = true;

    if (!document.removeLayer(targetId_)) {
        return false;
    }
    return true;
}

bool RemoveLayerCommand::undo(Document& document) {
    if (!captured_) {
        return false;
    }
    try {
        Layer restored(capturedId_, capturedName_, capturedBuffer_.width(), capturedBuffer_.height());
        // Copy buffer contents
        restored.buffer() = capturedBuffer_;
        restored.setVisible(capturedVisible_);
        restored.setOpacity(capturedOpacity_);
        restored.setPosition(capturedX_, capturedY_);

        // Try to restore at original position
        auto& stack = document.layerStack();
        // If inserting at arbitrary position, easier to add then move; but stack
        // API doesn't have insert at index - move up/down until position.
        const LayerId rid = stack.addLayer(std::move(restored));
        if (rid == Layer::invalidId) {
            return false;
        }
        // Move to original index (bottom-to-top). Move up/down repeatedly.
        const std::size_t targetIndex = capturedIndex_;
        // Adjust by moving
        // Find current position
        for (int steps = 0; steps < 10000; ++steps) {
            const auto& ls = stack.layers();
            // find index
            std::size_t cur = static_cast<std::size_t>(-1);
            for (std::size_t i = 0; i < ls.size(); ++i) {
                if (ls[i].id() == rid) {
                    cur = i;
                    break;
                }
            }
            if (cur == targetIndex) {
                break;
            }
            if (cur < targetIndex) {
                if (!stack.moveLayerUp(rid)) break;
            } else {
                if (!stack.moveLayerDown(rid)) break;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

ChangeLayerPropertiesCommand::ChangeLayerPropertiesCommand(
    LayerId id,
    std::string newName,
    bool newVisible,
    float newOpacity,
    std::int32_t newX,
    std::int32_t newY
)
    : targetId_(id),
      newName_(std::move(newName)),
      newVisible_(newVisible),
      newOpacity_(newOpacity),
      newX_(newX),
      newY_(newY) {}

std::string ChangeLayerPropertiesCommand::name() const {
    return "ChangeLayerProperties";
}

bool ChangeLayerPropertiesCommand::execute(Document& document) {
    Layer* layer = document.findLayer(targetId_);
    if (!layer) {
        return false;
    }
    if (!captured_) {
        oldName_ = layer->name();
        oldVisible_ = layer->visible();
        oldOpacity_ = layer->opacity();
        oldX_ = layer->x();
        oldY_ = layer->y();
        captured_ = true;
    }
    try {
        layer->setName(newName_);
        layer->setVisible(newVisible_);
        layer->setOpacity(newOpacity_);
        layer->setPosition(newX_, newY_);
        return true;
    } catch (...) {
        return false;
    }
}

bool ChangeLayerPropertiesCommand::undo(Document& document) {
    if (!captured_) {
        return false;
    }
    Layer* layer = document.findLayer(targetId_);
    if (!layer) {
        return false;
    }
    try {
        layer->setName(oldName_);
        layer->setVisible(oldVisible_);
        layer->setOpacity(oldOpacity_);
        layer->setPosition(oldX_, oldY_);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace glyma::core

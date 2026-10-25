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
    if (layer->isText()) {
        capturedText_ = layer->text();
    } else {
        capturedText_.reset();
    }
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
        Layer restored = capturedText_.has_value()
            ? Layer(capturedId_, capturedName_, *capturedText_)
            : Layer(capturedId_, capturedName_,
                    capturedBuffer_.width(), capturedBuffer_.height());
        // Copy buffer contents (the rendered cache for text layers).
        restored.buffer() = capturedBuffer_;
        restored.setVisible(capturedVisible_);
        restored.setOpacity(capturedOpacity_);
        restored.setPosition(capturedX_, capturedY_);

        // Restore at the original bottom-to-top index. The layer is appended
        // first, then moved into place.
        auto& stack = document.layerStack();
        const LayerId rid = stack.addLayer(std::move(restored));
        if (rid == Layer::invalidId) {
            return false;
        }
        if (capturedIndex_ >= stack.size()) {
            return false;
        }
        if (stack.layers()[capturedIndex_].id() != rid &&
            !stack.moveLayer(rid, capturedIndex_)) {
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

MoveLayerCommand::MoveLayerCommand(LayerId id, std::size_t newIndex)
    : targetId_(id), newIndex_(newIndex) {}

std::string MoveLayerCommand::name() const {
    return "MoveLayer";
}

bool MoveLayerCommand::execute(Document& document) {
    auto& stack = document.layerStack();
    const auto& layers = stack.layers();

    std::size_t currentIndex = static_cast<std::size_t>(-1);
    for (std::size_t i = 0; i < layers.size(); ++i) {
        if (layers[i].id() == targetId_) {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex == static_cast<std::size_t>(-1) ||
        newIndex_ >= stack.size() || currentIndex == newIndex_) {
        return false;
    }

    if (!captured_) {
        oldIndex_ = currentIndex;
        captured_ = true;
    }
    return stack.moveLayer(targetId_, newIndex_);
}

bool MoveLayerCommand::undo(Document& document) {
    if (!captured_) {
        return false;
    }
    return document.layerStack().moveLayer(targetId_, oldIndex_);
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

SelectCommand::SelectCommand(
    const core::Selection& previousSelection,
    const core::Selection& nextSelection
) : previous_(previousSelection), next_(nextSelection) {}

std::string SelectCommand::name() const {
    return "Select";
}

bool SelectCommand::execute(Document& document) {
    document.setSelection(next_);
    return true;
}

bool SelectCommand::undo(Document& document) {
    document.setSelection(previous_);
    return true;
}

ClearSelectionCommand::ClearSelectionCommand(const core::Selection& previousSelection)
    : previous_(previousSelection) {}

std::string ClearSelectionCommand::name() const {
    return "ClearSelection";
}

bool ClearSelectionCommand::execute(Document& document) {
    document.setSelection(core::Selection{});
    return true;
}

bool ClearSelectionCommand::undo(Document& document) {
    document.setSelection(previous_);
    return true;
}

InvertSelectionCommand::InvertSelectionCommand(const core::Selection& previousSelection)
    : previous_(previousSelection) {}

std::string InvertSelectionCommand::name() const {
    return "InvertSelection";
}

bool InvertSelectionCommand::execute(Document& document) {
    core::Selection inverted = document.selection();
    inverted.invert();
    document.setSelection(inverted);
    return true;
}

bool InvertSelectionCommand::undo(Document& document) {
    document.setSelection(previous_);
    return true;
}

} // namespace glyma::core

#include "glyma/core/shape_commands.hpp"

#include <utility>

#include "glyma/core/document.hpp"

namespace glyma::core {

AddShapeLayerCommand::AddShapeLayerCommand(
    std::string name,
    ShapeLayerData data,
    std::int32_t x,
    std::int32_t y
)
    : name_(std::move(name)),
      data_(std::move(data)),
      x_(x),
      y_(y) {}

std::string AddShapeLayerCommand::name() const {
    return "AddShapeLayer";
}

bool AddShapeLayerCommand::execute(Document& document) {
    try {
        // Reuse the id from a previous execution so redo restores the same
        // layer identity that downstream references (selection, commands)
        // captured.
        Layer layer = (addedId_ != Layer::invalidId)
            ? Layer(addedId_, name_, data_)
            : Layer(name_, data_);
        layer.setPosition(x_, y_);
        addedId_ = document.addLayer(std::move(layer));
        return addedId_ != Layer::invalidId;
    } catch (...) {
        return false;
    }
}

bool AddShapeLayerCommand::undo(Document& document) {
    if (addedId_ == Layer::invalidId) {
        return false;
    }
    return document.removeLayer(addedId_);
}

SetShapeDataCommand::SetShapeDataCommand(LayerId id, ShapeLayerData next)
    : targetId_(id), next_(std::move(next)) {}

std::string SetShapeDataCommand::name() const {
    return "EditShape";
}

bool SetShapeDataCommand::execute(Document& document) {
    Layer* layer = document.findLayer(targetId_);
    if (layer == nullptr || !layer->isVector()) {
        return false;
    }
    if (!captured_) {
        previous_ = layer->shape();
        captured_ = true;
    }
    try {
        layer->setShapeData(next_);
        return true;
    } catch (...) {
        return false;
    }
}

bool SetShapeDataCommand::undo(Document& document) {
    if (!captured_) {
        return false;
    }
    Layer* layer = document.findLayer(targetId_);
    if (layer == nullptr || !layer->isVector()) {
        return false;
    }
    try {
        layer->setShapeData(previous_);
        return true;
    } catch (...) {
        return false;
    }
}

RasterizeShapeLayerCommand::RasterizeShapeLayerCommand(LayerId id)
    : targetId_(id) {}

std::string RasterizeShapeLayerCommand::name() const {
    return "RasterizeShape";
}

bool RasterizeShapeLayerCommand::execute(Document& document) {
    Layer* layer = document.findLayer(targetId_);
    if (layer == nullptr || !layer->isVector()) {
        return false;
    }
    if (!captured_) {
        previous_ = layer->shape();
        captured_ = true;
    }
    return layer->rasterize();
}

bool RasterizeShapeLayerCommand::undo(Document& document) {
    if (!captured_) {
        return false;
    }
    Layer* layer = document.findLayer(targetId_);
    if (layer == nullptr) {
        return false;
    }
    try {
        layer->setShapeData(previous_);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace glyma::core

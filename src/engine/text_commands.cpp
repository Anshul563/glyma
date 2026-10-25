#include "glyma/core/text_commands.hpp"

#include <utility>

#include "glyma/core/document.hpp"

namespace glyma::core {

AddTextLayerCommand::AddTextLayerCommand(
    std::string name,
    TextLayerData data,
    std::int32_t x,
    std::int32_t y
)
    : name_(std::move(name)),
      data_(std::move(data)),
      x_(x),
      y_(y) {}

std::string AddTextLayerCommand::name() const {
    return "AddTextLayer";
}

bool AddTextLayerCommand::execute(Document& document) {
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

bool AddTextLayerCommand::undo(Document& document) {
    if (addedId_ == Layer::invalidId) {
        return false;
    }
    return document.removeLayer(addedId_);
}

SetTextDataCommand::SetTextDataCommand(LayerId id, TextLayerData next)
    : targetId_(id), next_(std::move(next)) {}

std::string SetTextDataCommand::name() const {
    return "EditText";
}

bool SetTextDataCommand::execute(Document& document) {
    Layer* layer = document.findLayer(targetId_);
    if (layer == nullptr || !layer->isText()) {
        return false;
    }
    if (!captured_) {
        previous_ = layer->text();
        captured_ = true;
    }
    try {
        layer->setTextData(next_);
        return true;
    } catch (...) {
        return false;
    }
}

bool SetTextDataCommand::undo(Document& document) {
    if (!captured_) {
        return false;
    }
    Layer* layer = document.findLayer(targetId_);
    if (layer == nullptr || !layer->isText()) {
        return false;
    }
    try {
        layer->setTextData(previous_);
        return true;
    } catch (...) {
        return false;
    }
}

RasterizeTextLayerCommand::RasterizeTextLayerCommand(LayerId id)
    : targetId_(id) {}

std::string RasterizeTextLayerCommand::name() const {
    return "RasterizeText";
}

bool RasterizeTextLayerCommand::execute(Document& document) {
    Layer* layer = document.findLayer(targetId_);
    if (layer == nullptr || !layer->isText()) {
        return false;
    }
    if (!captured_) {
        previous_ = layer->text();
        captured_ = true;
    }
    return layer->rasterize();
}

bool RasterizeTextLayerCommand::undo(Document& document) {
    if (!captured_) {
        return false;
    }
    Layer* layer = document.findLayer(targetId_);
    if (layer == nullptr) {
        return false;
    }
    try {
        layer->setTextData(previous_);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace glyma::core

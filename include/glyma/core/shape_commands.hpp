#pragma once

#include <cstdint>
#include <string>

#include "glyma/core/command.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/shape_layer.hpp"

namespace glyma::core {

// Add a vector shape layer to the document. On undo, removes the layer it
// created.
class AddShapeLayerCommand : public Command {
public:
    AddShapeLayerCommand(
        std::string name,
        ShapeLayerData data,
        std::int32_t x = 0,
        std::int32_t y = 0
    );

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    std::string name_;
    ShapeLayerData data_;
    std::int32_t x_{0};
    std::int32_t y_{0};
    LayerId addedId_{Layer::invalidId};
};

// Replace the editable payload of an existing vector layer, storing the
// previous payload so the change can be undone. Used for geometry and style
// edits.
class SetShapeDataCommand : public Command {
public:
    SetShapeDataCommand(LayerId id, ShapeLayerData next);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    LayerId targetId_;
    ShapeLayerData next_;
    bool captured_{false};
    ShapeLayerData previous_;
};

// Convert a vector layer into a raster layer, keeping its current rendered
// pixels. Undo restores the editable shape payload.
class RasterizeShapeLayerCommand : public Command {
public:
    explicit RasterizeShapeLayerCommand(LayerId id);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    LayerId targetId_;
    bool captured_{false};
    ShapeLayerData previous_;
};

} // namespace glyma::core

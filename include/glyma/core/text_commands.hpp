#pragma once

#include <cstddef>
#include <string>

#include "glyma/core/command.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/text_layer.hpp"

namespace glyma::core {

// Add a text layer to the document. On undo, removes the layer it created.
class AddTextLayerCommand : public Command {
public:
    AddTextLayerCommand(
        std::string name,
        TextLayerData data,
        std::int32_t x = 0,
        std::int32_t y = 0
    );

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    std::string name_;
    TextLayerData data_;
    std::int32_t x_{0};
    std::int32_t y_{0};
    LayerId addedId_{Layer::invalidId};
};

// Replace the editable payload of an existing text layer, storing the previous
// payload so the change can be undone. Used for content and typography edits.
class SetTextDataCommand : public Command {
public:
    SetTextDataCommand(LayerId id, TextLayerData next);
    // Pre-captured form: the caller already knows the previous payload (for
    // example a text-editing session that applied live previews), so execute()
    // must not recapture it. Used with CommandHistory::pushExecuted().
    SetTextDataCommand(LayerId id, TextLayerData previous, TextLayerData next);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    LayerId targetId_;
    TextLayerData next_;
    bool captured_{false};
    TextLayerData previous_;
};

// Convert a text layer into a raster layer, keeping its current rendered
// pixels. Undo restores the editable text payload.
class RasterizeTextLayerCommand : public Command {
public:
    explicit RasterizeTextLayerCommand(LayerId id);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    LayerId targetId_;
    bool captured_{false};
    TextLayerData previous_;
};

} // namespace glyma::core

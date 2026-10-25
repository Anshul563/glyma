#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "glyma/core/command.hpp"
#include "glyma/core/image_buffer.hpp"
#include "glyma/core/layer.hpp"
#include "glyma/core/pixel.hpp"
#include "glyma/core/selection.hpp"

namespace glyma::core {

// Add a new layer to document. On undo, removes the layer (by stored ID).
class AddLayerCommand : public Command {
public:
    AddLayerCommand(std::string name, std::uint32_t width, std::uint32_t height);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    std::string name_;
    std::uint32_t width_;
    std::uint32_t height_;
    LayerId addedId_{Layer::invalidId};
    std::size_t addedIndex_{static_cast<std::size_t>(-1)};
};

// Remove a layer by ID. Stores full layer snapshot to restore at original position.
class RemoveLayerCommand : public Command {
public:
    explicit RemoveLayerCommand(LayerId id);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    LayerId targetId_;
    bool captured_{false};
    LayerId capturedId_{Layer::invalidId};
    std::string capturedName_;
    ImageBuffer capturedBuffer_{1, 1};
    // Preserved so removing and undoing a text layer keeps its editable text.
    std::optional<TextLayerData> capturedText_;
    bool capturedVisible_{true};
    float capturedOpacity_{1.0F};
    std::int32_t capturedX_{0};
    std::int32_t capturedY_{0};
    std::size_t capturedIndex_{static_cast<std::size_t>(-1)};
};

// Move a layer to an explicit bottom-to-top stack index.
class MoveLayerCommand : public Command {
public:
    MoveLayerCommand(LayerId id, std::size_t newIndex);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    LayerId targetId_;
    std::size_t newIndex_;
    std::size_t oldIndex_{static_cast<std::size_t>(-1)};
    bool captured_{false};
};

// Change arbitrary layer properties (store old values).
class ChangeLayerPropertiesCommand : public Command {
public:
    ChangeLayerPropertiesCommand(
        LayerId id,
        std::string newName,
        bool newVisible,
        float newOpacity,
        std::int32_t newX,
        std::int32_t newY
    );

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    LayerId targetId_;
    std::string newName_;
    bool newVisible_;
    float newOpacity_;
    std::int32_t newX_;
    std::int32_t newY_;

    bool captured_{false};
    std::string oldName_;
    bool oldVisible_{true};
    float oldOpacity_{1.0F};
    std::int32_t oldX_{0};
    std::int32_t oldY_{0};
};

// Replace the document selection with `nextSelection`, storing the previous
// selection so the change can be undone.
class SelectCommand : public Command {
public:
    SelectCommand(const core::Selection& previousSelection,
                  const core::Selection& nextSelection);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    core::Selection previous_;
    core::Selection next_;
};

// Clear the document selection, storing the previous selection for undo.
class ClearSelectionCommand : public Command {
public:
    explicit ClearSelectionCommand(const core::Selection& previousSelection);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    core::Selection previous_;
};

// Invert the document selection, storing the previous selection for undo.
class InvertSelectionCommand : public Command {
public:
    explicit InvertSelectionCommand(const core::Selection& previousSelection);

    [[nodiscard]] std::string name() const override;
    bool execute(Document& document) override;
    bool undo(Document& document) override;

private:
    core::Selection previous_;
};

} // namespace glyma::core

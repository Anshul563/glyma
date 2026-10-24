#include "glyma/core/command_history.hpp"
#include "glyma/core/commands.hpp"
#include "glyma/core/document.hpp"
#include "glyma/core/pixel.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

using glyma::core::AddLayerCommand;
using glyma::core::ChangeLayerPropertiesCommand;
using glyma::core::CommandHistory;
using glyma::core::Document;
using glyma::core::Layer;
using glyma::core::MoveLayerCommand;
using glyma::core::Pixel;
using glyma::core::RemoveLayerCommand;

int main() {
    // Empty history
    {
        Document doc("Doc", 4, 4);
        CommandHistory history;
        assert(history.canUndo() == false);
        assert(history.canRedo() == false);
        assert(history.undoCount() == 0);
        assert(history.redoCount() == 0);
        assert(history.undo(doc) == false);
        assert(history.redo(doc) == false);
    }

    // Execute and undo
    {
        Document doc("Doc", 4, 4);
        CommandHistory history;
        assert(doc.layerCount() == 0);
        assert(history.execute(std::make_unique<AddLayerCommand>("L1", 4, 4), doc) == true);
        assert(doc.layerCount() == 1);
        assert(history.canUndo() == true);
        assert(history.undo(doc) == true);
        assert(doc.layerCount() == 0);
        assert(history.canUndo() == false);
        assert(history.canRedo() == true);
    }

    // Execute, undo, redo
    {
        Document doc("Doc", 4, 4);
        CommandHistory history;
        history.execute(std::make_unique<AddLayerCommand>("L1", 4, 4), doc);
        assert(doc.layerCount() == 1);
        history.undo(doc);
        assert(doc.layerCount() == 0);
        history.redo(doc);
        assert(doc.layerCount() == 1);
        assert(history.canRedo() == false);
    }

    // Multiple sequential commands
    {
        Document doc("Doc", 4, 4);
        CommandHistory history;
        history.execute(std::make_unique<AddLayerCommand>("L1", 4, 4), doc);
        history.execute(std::make_unique<AddLayerCommand>("L2", 4, 4), doc);
        history.execute(std::make_unique<AddLayerCommand>("L3", 4, 4), doc);
        assert(doc.layerCount() == 3);
        history.undo(doc);
        assert(doc.layerCount() == 2);
        history.undo(doc);
        assert(doc.layerCount() == 1);
        history.undo(doc);
        assert(doc.layerCount() == 0);
    }

    // Executing a new command after undo discards redo branch
    {
        Document doc("Doc", 4, 4);
        CommandHistory history;
        history.execute(std::make_unique<AddLayerCommand>("L1", 4, 4), doc);
        history.execute(std::make_unique<AddLayerCommand>("L2", 4, 4), doc);
        history.undo(doc); // back to 1
        assert(doc.layerCount() == 1);
        assert(history.canRedo() == true);
        history.execute(std::make_unique<AddLayerCommand>("L3", 4, 4), doc); // discards redo
        assert(doc.layerCount() == 2);
        assert(history.canRedo() == false);
    }

    // Add layer, undo, redo - properties preserved
    {
        Document doc("Doc", 4, 4);
        CommandHistory history;
        history.execute(std::make_unique<AddLayerCommand>("MyLayer", 4, 4), doc);
        assert(doc.layerCount() == 1);
        auto* layer = doc.layerStack().layers()[0].id() != 0 ? doc.findLayer(doc.layerStack().layers()[0].id()) : nullptr;
        if (!layer && doc.layerCount() > 0) {
            // find by scanning
            auto id = doc.layerStack().layers()[0].id();
            layer = doc.findLayer(id);
        }
        history.undo(doc);
        assert(doc.layerCount() == 0);
        history.redo(doc);
        assert(doc.layerCount() == 1);
        assert(doc.layerStack().layers()[0].name() == "MyLayer");
    }

    // Remove layer, undo, redo - restore at correct stack position
    {
        Document doc("Doc", 4, 4);
        CommandHistory history;
        history.execute(std::make_unique<AddLayerCommand>("Bottom", 4, 4), doc);
        history.execute(std::make_unique<AddLayerCommand>("Top", 4, 4), doc);
        assert(doc.layerCount() == 2);
        glyma::core::LayerId topId = doc.layerStack().layers()[1].id();
        history.execute(std::make_unique<RemoveLayerCommand>(topId), doc);
        assert(doc.layerCount() == 1);
        history.undo(doc);
        assert(doc.layerCount() == 2);
        // Top should be restored at index 1
        assert(doc.layerStack().layers()[1].id() == topId);
        assert(doc.layerStack().layers()[1].name() == "Top");
        history.redo(doc);
        assert(doc.layerCount() == 1);
        assert(doc.findLayer(topId) == nullptr);
    }

    // Reversible property changes
    {
        Document doc("Doc", 4, 4);
        CommandHistory history;
        history.execute(std::make_unique<AddLayerCommand>("L1", 4, 4), doc);
        auto id = doc.layerStack().layers()[0].id();
        history.execute(std::make_unique<ChangeLayerPropertiesCommand>(id, "Renamed", false, 0.5F, 10, -5), doc);
        auto* layer = doc.findLayer(id);
        assert(layer->name() == "Renamed");
        assert(layer->visible() == false);
        assert(layer->opacity() == 0.5F);
        assert(layer->x() == 10);
        assert(layer->y() == -5);
        history.undo(doc);
        layer = doc.findLayer(id);
        assert(layer->name() == "L1");
        assert(layer->visible() == true);
        assert(layer->opacity() == 1.0F);
        assert(layer->x() == 0);
        assert(layer->y() == 0);
        history.redo(doc);
        layer = doc.findLayer(id);
        assert(layer->name() == "Renamed");
        assert(layer->visible() == false);
    }

    // Moving a layer is undoable at its original position
    {
        Document doc("Doc", 4, 4);
        CommandHistory history;
        history.execute(std::make_unique<AddLayerCommand>("A", 4, 4), doc);
        history.execute(std::make_unique<AddLayerCommand>("B", 4, 4), doc);
        history.execute(std::make_unique<AddLayerCommand>("C", 4, 4), doc);

        const auto& before = doc.layerStack().layers();
        const auto aId = before[0].id();
        const auto bId = before[1].id();
        const auto cId = before[2].id();

        assert(history.execute(std::make_unique<MoveLayerCommand>(aId, 2), doc));
        assert(doc.layerStack().layers()[0].id() == bId);
        assert(doc.layerStack().layers()[1].id() == cId);
        assert(doc.layerStack().layers()[2].id() == aId);

        assert(history.undo(doc));
        assert(doc.layerStack().layers()[0].id() == aId);
        assert(doc.layerStack().layers()[1].id() == bId);
        assert(doc.layerStack().layers()[2].id() == cId);

        assert(history.redo(doc));
        assert(doc.layerStack().layers()[2].id() == aId);

        assert(!history.execute(std::make_unique<MoveLayerCommand>(aId, 9), doc));
        assert(!history.execute(std::make_unique<MoveLayerCommand>(999999, 0), doc));
    }

    // Invalid layer IDs
    {
        Document doc("Doc", 4, 4);
        CommandHistory history;
        assert(history.execute(std::make_unique<RemoveLayerCommand>(12345), doc) == false);
        assert(history.canUndo() == false);
        assert(history.execute(std::make_unique<ChangeLayerPropertiesCommand>(12345, "X", true, 1.0F, 0, 0), doc) == false);
        assert(history.canUndo() == false);
    }

    // Failed command execution does not corrupt history
    {
        Document doc("Doc", 4, 4);
        CommandHistory history;
        // AddLayer with zero size should fail
        assert(history.execute(std::make_unique<AddLayerCommand>("Bad", 0, 4), doc) == false);
        assert(history.canUndo() == false);
        assert(doc.layerCount() == 0);
    }

    // History clearing
    {
        Document doc("Doc", 4, 4);
        CommandHistory history;
        history.execute(std::make_unique<AddLayerCommand>("L1", 4, 4), doc);
        history.execute(std::make_unique<AddLayerCommand>("L2", 4, 4), doc);
        assert(history.canUndo() == true);
        history.clear();
        assert(history.canUndo() == false);
        assert(history.canRedo() == false);
        assert(doc.layerCount() == 2); // document unchanged
    }

    // History limits
    {
        Document doc("Doc", 4, 4);
        CommandHistory history(2);
        history.execute(std::make_unique<AddLayerCommand>("L1", 4, 4), doc);
        history.execute(std::make_unique<AddLayerCommand>("L2", 4, 4), doc);
        history.execute(std::make_unique<AddLayerCommand>("L3", 4, 4), doc);
        assert(history.undoCount() <= 2);
    }

    std::cout << "All Glyma history tests passed!\n";
    return 0;
}

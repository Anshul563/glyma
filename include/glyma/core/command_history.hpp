#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "glyma/core/command.hpp"

namespace glyma::core {

// Command-based history with undo/redo. Executing a new command after undo
// discards the redo branch. History is count-limited (default 100); older
// entries are dropped from the bottom. Commands must only be recorded if
// execute() succeeds.
class CommandHistory {
public:
    explicit CommandHistory(std::size_t maxHistory = 100);

    CommandHistory(const CommandHistory&) = delete;
    CommandHistory& operator=(const CommandHistory&) = delete;
    CommandHistory(CommandHistory&&) = default;
    CommandHistory& operator=(CommandHistory&&) = default;
    ~CommandHistory() = default;

    // Execute a command and push onto history if successful. Discards redo
    // stack if present.
    bool execute(CommandPtr command, Document& document);

    // Undo the most recent executed command.
    bool undo(Document& document);

    // Redo the most recently undone command.
    bool redo(Document& document);

    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;

    [[nodiscard]] std::size_t undoCount() const noexcept;
    [[nodiscard]] std::size_t redoCount() const noexcept;
    [[nodiscard]] std::size_t maxHistory() const noexcept;

    void clear() noexcept;
    void setMaxHistory(std::size_t maxHistory);

private:
    void trim();

    std::vector<CommandPtr> undoStack_;
    std::vector<CommandPtr> redoStack_;
    std::size_t maxHistory_;
};

} // namespace glyma::core

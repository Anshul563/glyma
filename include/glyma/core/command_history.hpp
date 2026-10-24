#pragma once

#include <cstddef>
#include <cstdint>
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

    // Record a command whose change has already been applied. This is used by
    // progressive tools (such as a brush stroke) so the result is not applied
    // twice. Discards the redo stack. Returns false for a null command.
    bool pushExecuted(CommandPtr command);

    // Undo the most recent executed command.
    bool undo(Document& document);

    // Redo the most recently undone command.
    bool redo(Document& document);

    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;

    [[nodiscard]] std::size_t undoCount() const noexcept;
    [[nodiscard]] std::size_t redoCount() const noexcept;
    [[nodiscard]] std::size_t maxHistory() const noexcept;

    // Monotonic token identifying the document state represented by the
    // current position in the history. It is reset by clear(). Two states are
    // equal only if they have the same revision, which lets callers detect
    // whether undo/redo returned to a previously saved state.
    [[nodiscard]] std::uint64_t currentRevision() const noexcept;

    void clear() noexcept;
    void setMaxHistory(std::size_t maxHistory);

private:
    struct Entry {
        CommandPtr command;
        std::uint64_t revisionBefore{0};
        std::uint64_t revisionAfter{0};
    };

    void trim();

    std::vector<Entry> undoStack_;
    std::vector<Entry> redoStack_;
    std::size_t maxHistory_;
    std::uint64_t currentRevision_{0};
    std::uint64_t nextRevision_{0};
};

} // namespace glyma::core

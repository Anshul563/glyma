#include "glyma/core/command_history.hpp"

#include "glyma/core/document.hpp"

namespace glyma::core {

CommandHistory::CommandHistory(std::size_t maxHistory) : maxHistory_(maxHistory) {
    if (maxHistory_ == 0) {
        maxHistory_ = 1;
    }
}

bool CommandHistory::execute(CommandPtr command, Document& document) {
    if (!command) {
        return false;
    }

    // Discard redo branch before attempting execution (new branch)
    redoStack_.clear();

    if (!command->execute(document)) {
        return false;
    }

    undoStack_.push_back(std::move(command));
    trim();
    return true;
}

bool CommandHistory::undo(Document& document) {
    if (undoStack_.empty()) {
        return false;
    }

    auto cmd = std::move(undoStack_.back());
    undoStack_.pop_back();

    if (!cmd->undo(document)) {
        // On failure, do not restore to undo stack in a broken state - push back
        // but mark as failed path is exceptional; keep simple and consistent
        undoStack_.push_back(std::move(cmd));
        return false;
    }

    redoStack_.push_back(std::move(cmd));
    return true;
}

bool CommandHistory::redo(Document& document) {
    if (redoStack_.empty()) {
        return false;
    }

    auto cmd = std::move(redoStack_.back());
    redoStack_.pop_back();

    if (!cmd->execute(document)) {
        // Put it back if execution fails
        redoStack_.push_back(std::move(cmd));
        return false;
    }

    undoStack_.push_back(std::move(cmd));
    trim();
    return true;
}

bool CommandHistory::canUndo() const noexcept {
    return !undoStack_.empty();
}

bool CommandHistory::canRedo() const noexcept {
    return !redoStack_.empty();
}

std::size_t CommandHistory::undoCount() const noexcept {
    return undoStack_.size();
}

std::size_t CommandHistory::redoCount() const noexcept {
    return redoStack_.size();
}

std::size_t CommandHistory::maxHistory() const noexcept {
    return maxHistory_;
}

void CommandHistory::clear() noexcept {
    undoStack_.clear();
    redoStack_.clear();
}

void CommandHistory::setMaxHistory(std::size_t maxHistory) {
    if (maxHistory == 0) {
        maxHistory_ = 1;
    } else {
        maxHistory_ = maxHistory;
    }
    trim();
}

void CommandHistory::trim() {
    while (undoStack_.size() > maxHistory_) {
        undoStack_.erase(undoStack_.begin());
    }
}

} // namespace glyma::core

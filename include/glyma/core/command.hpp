#pragma once

#include <memory>
#include <string>

namespace glyma::core {

class Document;

// Abstract reversible command operating on a Document.
// execute() applies the change; undo() reverts it. Commands must be
// deterministic and should leave document in a consistent state on failure
// (throwing or returning false without advancing history).
class Command {
public:
    virtual ~Command() = default;

    // Human-readable description for debugging/UI.
    [[nodiscard]] virtual std::string name() const = 0;

    // Apply the command. Returns true if successful. If it throws, caller
    // must not record it as executed in the history.
    virtual bool execute(Document& document) = 0;

    // Revert the command. Returns true if successful.
    virtual bool undo(Document& document) = 0;
};

using CommandPtr = std::unique_ptr<Command>;

} // namespace glyma::core

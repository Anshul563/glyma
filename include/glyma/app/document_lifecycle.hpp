#pragma once

#include <functional>

namespace glyma::app {

enum class UnsavedChoice {
    Save,
    Discard,
    Cancel,
};

using UnsavedPrompt = std::function<UnsavedChoice()>;
using SaveAction = std::function<bool()>;

// Centralized unsaved-changes decision used before New, Open, and Exit.
//
// Returns true when the destructive action may proceed. If the document is
// modified, Save only proceeds when the supplied save action succeeds; Discard
// proceeds immediately; Cancel (or a missing prompt) aborts. A missing save
// action is treated as a failed save. The prompt is never called for a clean
// document.
[[nodiscard]] bool confirmUnsavedChanges(
    bool documentModified,
    const UnsavedPrompt& prompt,
    const SaveAction& save
);

} // namespace glyma::app

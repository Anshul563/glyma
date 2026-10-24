#include "glyma/app/document_lifecycle.hpp"

namespace glyma::app {

bool confirmUnsavedChanges(
    bool documentModified,
    const UnsavedPrompt& prompt,
    const SaveAction& save
) {
    if (!documentModified) {
        return true;
    }
    if (!prompt) {
        return false;
    }

    switch (prompt()) {
    case UnsavedChoice::Discard:
        return true;
    case UnsavedChoice::Cancel:
        return false;
    case UnsavedChoice::Save:
        return save ? save() : false;
    }
    return false;
}

} // namespace glyma::app

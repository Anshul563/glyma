#include <cassert>
#include <iostream>

#include "glyma/app/document_lifecycle.hpp"

namespace {

using glyma::app::confirmUnsavedChanges;
using glyma::app::SaveAction;
using glyma::app::UnsavedChoice;
using glyma::app::UnsavedPrompt;

void testCleanDocumentProceedsWithoutPrompt() {
    int promptCalls = 0;
    int saveCalls = 0;
    const bool proceed = confirmUnsavedChanges(
        false,
        [&promptCalls] {
            ++promptCalls;
            return UnsavedChoice::Cancel;
        },
        [&saveCalls] {
            ++saveCalls;
            return false;
        }
    );
    assert(proceed);
    assert(promptCalls == 0);
    assert(saveCalls == 0);
}

void testDiscardProceeds() {
    int saveCalls = 0;
    const bool proceed = confirmUnsavedChanges(
        true,
        [] { return UnsavedChoice::Discard; },
        [&saveCalls] {
            ++saveCalls;
            return false;
        }
    );
    assert(proceed);
    assert(saveCalls == 0);
}

void testCancelAborts() {
    int saveCalls = 0;
    const bool proceed = confirmUnsavedChanges(
        true,
        [] { return UnsavedChoice::Cancel; },
        [&saveCalls] {
            ++saveCalls;
            return true;
        }
    );
    assert(!proceed);
    assert(saveCalls == 0);
}

void testSaveSuccessProceeds() {
    const bool proceed = confirmUnsavedChanges(
        true,
        [] { return UnsavedChoice::Save; },
        [] { return true; }
    );
    assert(proceed);
}

void testSaveFailureAborts() {
    const bool proceed = confirmUnsavedChanges(
        true,
        [] { return UnsavedChoice::Save; },
        [] { return false; }
    );
    assert(!proceed);
}

void testMissingActionsAreSafe() {
    assert(!confirmUnsavedChanges(true, UnsavedPrompt{}, SaveAction{}));
    assert(!confirmUnsavedChanges(true, [] { return UnsavedChoice::Save; }, SaveAction{}));
}

} // namespace

int main() {
    testCleanDocumentProceedsWithoutPrompt();
    testDiscardProceeds();
    testCancelAborts();
    testSaveSuccessProceeds();
    testSaveFailureAborts();
    testMissingActionsAreSafe();

    std::cout << "lifecycle tests passed\n";
    return 0;
}

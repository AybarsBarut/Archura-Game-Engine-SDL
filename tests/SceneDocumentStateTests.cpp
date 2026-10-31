#include "editor/SceneDocumentState.h"

#include <iostream>

namespace {
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __LINE__ << ": failed: " << #condition << '\n'; ++failures; } } while (false)
using Document = Archura::SceneDocumentState;
using Action = Document::Action;

void TrackChangesAndUndo() {
    Document document;
    CHECK(!document.Initialized());
    CHECK(document.IsDirty());
    document.Observe("scene");
    CHECK(document.IsDirty());
    document.Reset("scene", "original.gameproj");
    CHECK(document.Initialized() && !document.IsDirty());
    CHECK(document.Path() == "original.gameproj");
    document.Observe("scene");
    CHECK(!document.IsDirty());
    document.Observe("edited scene");
    CHECK(document.IsDirty());
    document.Observe("scene");
    CHECK(!document.IsDirty());
    document.Observe(std::nullopt);
    CHECK(document.IsDirty());
    document.Reset(std::nullopt);
    document.Observe("scene");
    CHECK(document.IsDirty());
    CHECK(document.Path().empty());
}

void CancelAndDiscard() {
    Document document;
    document.Reset("scene");
    CHECK(!document.Request(Action::None));
    CHECK(document.Request(Action::NewScene));
    CHECK(document.HasPending() && !document.NeedsDecision());
    document.Cancel();
    document.Observe("edited");
    CHECK(document.Request(Action::Close));
    CHECK(document.NeedsDecision());
    CHECK(!document.Request(Action::NewScene));
    CHECK(document.PendingAction() == Action::Close);
    document.Cancel();
    CHECK(!document.HasPending() && !document.NeedsDecision());
    CHECK(document.IsDirty());
    CHECK(document.Request(Action::OpenProject, "target.gameproj"));
    document.Discard();
    CHECK(document.HasPending() && !document.NeedsDecision());
    CHECK(document.IsDirty());
    document.Observe("edited again");
    CHECK(!document.NeedsDecision());
    CHECK(document.Target() == "target.gameproj");
    document.Cancel();
    CHECK(document.Target().empty());
}

void SavePreservesPendingTarget() {
    Document document;
    document.Reset("old", "old.gameproj");
    document.Observe("edited");
    CHECK(document.Request(Action::OpenLegacy, "load.scene"));
    CHECK(!document.Saved(std::nullopt, "save-as.gameproj"));
    CHECK(document.IsDirty() && document.NeedsDecision());
    CHECK(document.Path() == "old.gameproj");
    CHECK(document.Target() == "load.scene");
    CHECK(!document.Saved("edited", ""));
    CHECK(document.IsDirty() && document.NeedsDecision());
    CHECK(document.Path() == "old.gameproj");
    CHECK(document.Saved("edited", "save-as.gameproj"));
    CHECK(!document.IsDirty() && !document.NeedsDecision());
    CHECK(document.Path() == "save-as.gameproj");
    CHECK(document.PendingAction() == Action::OpenLegacy);
    CHECK(document.Target() == "load.scene");
    CHECK(!document.Request(Action::Close));
    document.Observe("edited");
    CHECK(!document.IsDirty());
    document.Reset("replacement", "load.gameproj");
    CHECK(!document.HasPending() && document.Target().empty());
    CHECK(document.Path() == "load.gameproj" && !document.IsDirty());
    // An empty serialized scene is valid; absence is represented by nullopt.
    CHECK(document.Saved("", "empty.gameproj"));
    document.Observe("");
    CHECK(!document.IsDirty());
}
} // namespace

int main() {
    TrackChangesAndUndo();
    CancelAndDiscard();
    SavePreservesPendingTarget();
    return failures == 0 ? 0 : 1;
}

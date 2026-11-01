#pragma once

namespace Archura {
class Application;
class Editor;
class Scene;

// Implemented by tests/support only in startup-validation builds.
void RunEditorStartupChecks(Editor& editor, Application& application, Scene* scene);
} // namespace Archura

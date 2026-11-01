#include "core/StartupProbeSupport.h"
#include "EditorDocumentSmoke.h"
#include "EditorProjectSmoke.h"
#include "EditorHistorySmoke.h"

namespace Archura {
void RunEditorStartupChecks(Editor& editor, Application& application, Scene* scene) {
    EditorDocumentSmoke::Run(editor, application, scene);
    EditorProjectSmoke::Run(editor, application, scene);
    EditorHistorySmoke::Run(editor, application, scene);
}
} // namespace Archura

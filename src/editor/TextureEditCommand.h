#pragma once
#include "TransformUndoHistory.h"
#include <filesystem>

namespace Archura {
// File-backed recipes only: history never owns Texture/GPU objects or cache
// entries. Generated textures without a source file cannot be reconstructed.
class TextureEditCommand final : public EditorHistoryCommand {
public:
    static std::shared_ptr<TextureEditCommand> Create(Scene& scene, EntityHandle handle,
        const std::string& authoredPath, const std::filesystem::path& documentRoot, std::string& error);
    bool CanApply(const Scene& scene, bool undo) const override;
    ActionResult Apply(Scene& scene, bool undo) override;
    size_t ByteSize() const noexcept override;
    const char* Label() const noexcept override { return m_After.present ? "Assign Texture" : "Remove Texture"; }
    const std::string& Error() const noexcept { return m_Error; }
private:
    struct Recipe {
        bool present = false, mipmaps = true, srgb = false;
        std::string authored, canonical, loadPath;
    };
    static bool Capture(const MeshRenderer& renderer, Recipe& recipe, const std::filesystem::path& documentRoot);
    static bool Matches(const MeshRenderer& renderer, const Recipe& recipe);
    Scene* m_Scene = nullptr;
    std::weak_ptr<Scene*> m_Lifetime;
    std::string m_ID, m_Error;
    std::filesystem::path m_ResourceRoot;
    bool m_ExplicitRoot = false;
    Recipe m_Before, m_After;
};
} // namespace Archura

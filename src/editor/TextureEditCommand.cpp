#include "TextureEditCommand.h"
#include "../core/ResourceManager.h"
#include "../core/GameSaveManager.h"
#include "../rendering/Texture.h"
#include <stb_image.h>

namespace Archura {
bool TextureEditCommand::Capture(const MeshRenderer& renderer, Recipe& recipe, const std::filesystem::path& root) {
    const auto* texture = renderer.GetTexture();
    if (!texture) return true;
    // Raw borrowed textures and generated images cannot satisfy restore ownership.
    if (!renderer.textureAsset || !ResourceManager::IsSafeAssetPath(texture->GetPath()) ||
        texture->GetWidth() <= 0 || texture->GetHeight() <= 0) return false;
    recipe.present = true;
    recipe.authored = texture->GetPath();
    recipe.canonical = texture->GetCanonicalPath();
    recipe.loadPath = !recipe.canonical.empty() ? recipe.canonical : ResourceManager::Get().CanonicalAssetKey(recipe.authored, root);
    recipe.mipmaps = texture->GeneratesMipmaps();
    recipe.srgb = texture->IsSrgb();
    return ResourceManager::IsSafeAssetPath(recipe.loadPath) && std::filesystem::path(recipe.loadPath).is_absolute();
}
bool TextureEditCommand::Matches(const MeshRenderer& renderer, const Recipe& recipe) {
    const auto* texture = renderer.GetTexture();
    return !recipe.present ? texture == nullptr : texture && renderer.textureAsset &&
        texture->GetPath() == recipe.authored && texture->GetCanonicalPath() == recipe.canonical &&
        texture->GeneratesMipmaps() == recipe.mipmaps && texture->IsSrgb() == recipe.srgb;
}
std::shared_ptr<TextureEditCommand> TextureEditCommand::Create(Scene& scene, EntityHandle handle,
    const std::string& authoredPath, const std::filesystem::path& documentRoot, std::string& error) {
    error.clear();
    const auto* entity = scene.GetEntity(handle);
    const auto* renderer = entity ? entity->GetComponent<MeshRenderer>() : nullptr;
    if (!renderer || scene.IsIterating()) { error = "Choose a live mesh renderer outside scene traversal."; return {}; }
    auto command = std::make_shared<TextureEditCommand>();
    command->m_Scene = &scene;
    command->m_Lifetime = scene.GetLifetimeToken();
    command->m_ID = entity->GetPersistentID();
    command->m_ResourceRoot = ResourceManager::Get().GetProjectRoot();
    command->m_ExplicitRoot = ResourceManager::Get().HasExplicitContentRoot();
    command->m_Error.reserve(192); // Diagnostics never grow a published payload.
    if (!Capture(*renderer, command->m_Before, documentRoot)) {
        error = "This texture has no reconstructable owned file source; assignment was retained."; return {};
    }
    auto& after = command->m_After;
    if (!authoredPath.empty()) {
        if (!ResourceManager::IsSafeAssetPath(authoredPath)) { error = "Unsafe texture path."; return {}; }
        const auto resolved = ResourceManager::Get().ResolveAssetPath(authoredPath, documentRoot, true);
        if (resolved.empty()) { error = "The texture file is missing."; return {}; }
        after.present = true;
        after.authored = authoredPath;
        after.loadPath = ResourceManager::Get().CanonicalAssetKey(resolved.string());
        after.canonical = after.loadPath;
    }
    if (Matches(*renderer, after)) return {}; // No-op preserves Redo.
    if (command->ByteSize() > TransformUndoHistory::MaxGestureBytes) { error = "Texture recipe exceeds the history limit."; return {}; }
    return command;
}
bool TextureEditCommand::CanApply(const Scene& scene, bool undo) const {
    const auto lifetime = m_Lifetime.lock();
    if (&scene != m_Scene || !lifetime || *lifetime != &scene ||
        lifetime != scene.GetLifetimeToken().lock() || scene.IsIterating() ||
        ResourceManager::Get().GetProjectRoot() != m_ResourceRoot ||
        ResourceManager::Get().HasExplicitContentRoot() != m_ExplicitRoot) return false;
    const auto* entity = scene.GetEntityByPersistentID(m_ID);
    const auto* renderer = entity ? entity->GetComponent<MeshRenderer>() : nullptr;
    return renderer && Matches(*renderer, undo ? m_After : m_Before);
}
ActionResult TextureEditCommand::Apply(Scene& scene, bool undo) {
    if (!CanApply(scene, undo)) { m_Error = "The renderer, texture or project context changed; history was retained."; return {ActionResult::Status::Blocked}; }
    const auto& desired = undo ? m_Before : m_After;
    std::shared_ptr<Texture> staged;
    if (desired.present) {
        // Inspect the pinned source directly before decode/upload. Do not use a
        // cache whose mutable authored aliases may belong to unrelated entities.
        const auto budget = GameSaveManager::GetReconstructionBudget();
        std::error_code error;
        const auto size = std::filesystem::file_size(desired.loadPath, error);
        int width = 0, height = 0, channels = 0;
        const bool inspected = !error && stbi_info(desired.loadPath.c_str(), &width, &height, &channels);
        // stb's BMP info reports a signed height for valid top-down files;
        // LoadFromFile normalizes it during decode. Promote before negating.
        const int64_t imageHeight = height < 0 ? -static_cast<int64_t>(height) : height;
        if (error || size > budget.maxSingleAssetFileBytes || !inspected ||
            width <= 0 || imageHeight == 0 || channels < 1 || channels > 4 ||
            static_cast<size_t>(width) > budget.maxTotalTextureBytes / static_cast<size_t>(channels) / static_cast<size_t>(imageHeight)) {
            m_Error = "Texture source is missing, invalid or over budget; scene and history retained.";
            return {ActionResult::Status::Blocked};
        }
        staged = std::make_shared<Texture>();
        if (!staged->LoadFromFile(desired.loadPath, desired.mipmaps, desired.srgb)) {
            m_Error = "Texture loading failed; scene and history retained."; return {ActionResult::Status::Blocked};
        }
        if (staged->GetWidth() <= 0 || staged->GetHeight() <= 0 || staged->GetChannels() < 1 ||
            static_cast<size_t>(staged->GetWidth()) > budget.maxTotalTextureBytes / static_cast<size_t>(staged->GetChannels()) / static_cast<size_t>(staged->GetHeight())) {
            m_Error = "Decoded texture exceeds its budget; scene and history retained.";
            return {ActionResult::Status::Blocked};
        }
        staged->SetPath(desired.authored);
        staged->SetCanonicalPath(desired.canonical);
    }
    auto* entity = scene.GetEntityByPersistentID(m_ID);
    entity->GetComponent<MeshRenderer>()->SetTextureAsset(std::move(staged));
    return {ActionResult::Status::Applied, entity, {}};
}
size_t TextureEditCommand::ByteSize() const noexcept {
    size_t bytes = sizeof(*this) + m_ID.capacity() + m_Error.capacity() + m_ResourceRoot.native().capacity() * sizeof(std::filesystem::path::value_type);
    for (const auto* recipe : {&m_Before, &m_After}) bytes += recipe->authored.capacity() + recipe->canonical.capacity() + recipe->loadPath.capacity();
    return bytes;
}
} // namespace Archura

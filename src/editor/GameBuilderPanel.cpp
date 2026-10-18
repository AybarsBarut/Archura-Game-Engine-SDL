#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "GameBuilderPanel.h"
#include "../ecs/Entity.h"
#include "../ecs/Component.h"
#include "../rendering/Mesh.h"
#include "../rendering/Texture.h"
#include "../core/AudioSystem.h"
#include "../core/ResourceManager.h"

#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <iostream>

namespace Archura {

// ---------------------------------------------------------------------------
// Draw – ana giriş noktası
// ---------------------------------------------------------------------------
void GameBuilderPanel::Draw(Scene* scene, Entity* selectedEntity) {
    if (!m_Open) return;
    if (selectedEntity && (!scene || !scene->OwnsEntityPointer(selectedEntity))) {
        selectedEntity = nullptr;
    }

    ImGui::SetNextWindowSize(ImVec2(500, 550), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Game Builder", &m_Open)) {
        ImGui::End();
        return;
    }

    // Başlık
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Game Builder");
    ImGui::TextDisabled("Sahnenize obje, texture ve ses ekleyin.");
    ImGui::Separator();
    ImGui::Spacing();

    // Tab bar
    if (ImGui::BeginTabBar("GameBuilderTabs")) {
        if (ImGui::BeginTabItem("Objects")) {
            m_ActiveTab = 0;
            DrawObjectsTab(scene);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Textures")) {
            m_ActiveTab = 1;
            DrawTexturesTab(scene, selectedEntity);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Audio")) {
            m_ActiveTab = 2;
            DrawAudioTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Objects Tab
// ---------------------------------------------------------------------------
void GameBuilderPanel::DrawObjectsTab(Scene* scene) {
    ImGui::TextDisabled("Sahneye eklemek istediginiz nesneyi secin:");
    ImGui::Spacing();

    // Primitives
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Primitives");
    ImGui::Separator();

    float btnW = 220.0f, btnH = 38.0f;

    if (ImGui::Button("Kup (Cube)", ImVec2(btnW, btnH))) {
        if (scene) {
            Entity* e = scene->CreateEntity("MyObject_Cube");
            auto* mr  = e->AddComponent<MeshRenderer>();
            mr->SetMeshAsset(Mesh::CreateCubeShared());
            mr->color = glm::vec3(0.7f, 0.7f, 0.9f);
            e->AddComponent<BoxCollider>()->size = glm::vec3(1.f);
            auto* t = e->GetComponent<Transform>();
            if (t) t->position = glm::vec3(0, 3, 0);
            std::cout << "[GameBuilder] Cube spawn edildi." << std::endl;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Kure (Sphere)", ImVec2(btnW, btnH))) {
        if (scene) {
            Entity* e = scene->CreateEntity("MyObject_Sphere");
            auto* mr  = e->AddComponent<MeshRenderer>();
            mr->SetMeshAsset(Mesh::CreateSphereShared(1.0f, 16));
            mr->color = glm::vec3(0.9f, 0.5f, 0.2f);
            e->AddComponent<BoxCollider>()->size = glm::vec3(1.f);
            auto* t = e->GetComponent<Transform>();
            if (t) t->position = glm::vec3(0, 3, 0);
            std::cout << "[GameBuilder] Sphere spawn edildi." << std::endl;
        }
    }

    if (ImGui::Button("Kapsul (Capsule)", ImVec2(btnW, btnH))) {
        if (scene) {
            Entity* e = scene->CreateEntity("MyObject_Capsule");
            auto* mr  = e->AddComponent<MeshRenderer>();
            mr->SetMeshAsset(Mesh::CreateCapsuleShared(0.5f, 2.0f));
            mr->color = glm::vec3(0.4f, 0.9f, 0.4f);
            e->AddComponent<BoxCollider>()->size = glm::vec3(1.f, 2.f, 1.f);
            auto* t = e->GetComponent<Transform>();
            if (t) t->position = glm::vec3(0, 3, 0);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Rampa (Ramp)", ImVec2(btnW, btnH))) {
        if (scene) {
            Entity* e = scene->CreateEntity("MyObject_Ramp");
            auto* mr  = e->AddComponent<MeshRenderer>();
            mr->SetMeshAsset(Mesh::CreateRampShared(4.0f, 2.0f, 4.0f));
            mr->color = glm::vec3(0.8f, 0.6f, 0.3f);
            auto* collider = e->AddComponent<BoxCollider>();
            collider->size = glm::vec3(4.f, 2.f, 4.f);
            collider->center = glm::vec3(0.f, 1.f, 0.f);
            collider->shape = BoxCollider::Shape::Ramp;
            auto* t = e->GetComponent<Transform>();
            if (t) t->position = glm::vec3(0, 0, 0);
        }
    }

    if (ImGui::Button("Merdiven (Stairs)", ImVec2(btnW, btnH))) {
        if (scene) {
            Entity* e = scene->CreateEntity("MyObject_Stairs");
            auto* mr  = e->AddComponent<MeshRenderer>();
            mr->SetMeshAsset(Mesh::CreateStairsShared(2.0f, 0.5f, 0.5f, 4));
            mr->color = glm::vec3(0.7f, 0.7f, 0.7f);
            e->AddComponent<BoxCollider>();
            auto* t = e->GetComponent<Transform>();
            if (t) t->position = glm::vec3(0, 0, 0);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Isik (Light)", ImVec2(btnW, btnH))) {
        if (scene) {
            Entity* e = scene->CreateEntity("MyObject_Light");
            auto* lc  = e->AddComponent<LightComponent>();
            lc->type  = LightComponent::Type::Point;
            lc->color = glm::vec3(1.f, 0.9f, 0.7f);
            lc->intensity = 2.0f;
            lc->range     = 20.0f;
            auto* t = e->GetComponent<Transform>();
            if (t) t->position = glm::vec3(0, 5, 0);
            std::cout << "[GameBuilder] Point Light spawn edildi." << std::endl;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Custom .obj dosyaları
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                       ("3D Modeller (" + m_ActiveModelDir + "/)").c_str());
    ImGui::Separator();

    if (ImGui::Button("Listeyi Yenile", ImVec2(-1, 0))) {
        m_ModelsDirty = true;
    }
    if (m_ModelsDirty) {
        RefreshModelList();
        m_ModelsDirty = false;
    }

    if (m_ModelFiles.empty()) {
        ImGui::TextDisabled("assets/objects/ veya assets/models/ klasorunde .obj dosyasi bulunamadi.");
        ImGui::TextDisabled("Kendi modellerinizi buraya ekleyin.");
    } else {
        for (const auto& modelFile : m_ModelFiles) {
            if (ImGui::Button((modelFile + "  [OBJ]").c_str(), ImVec2(-1, 32))) {
                SpawnModelEntity(scene, modelFile);
            }
        }
    }
}

std::string GameBuilderPanel::ResolveModelSpawnPath(const std::string& modelFile) const {
    if (modelFile.empty() || !ResourceManager::IsSafeAssetPath(modelFile)) {
        return {};
    }

    const std::string activeCandidate = m_ActiveModelDir + "/" + modelFile;
    const std::filesystem::path resolvedActive =
        ResourceManager::Get().ResolveAssetPath(activeCandidate);
    if (!resolvedActive.empty()) {
        return resolvedActive.string();
    }

    for (const char* fallbackDir : {"assets/objects", "assets/models"}) {
        const std::string candidate = std::string(fallbackDir) + "/" + modelFile;
        const std::filesystem::path resolved =
            ResourceManager::Get().ResolveAssetPath(candidate);
        if (!resolved.empty()) {
            return resolved.string();
        }
    }
    return {};
}

Entity* GameBuilderPanel::SpawnModelEntity(Scene* scene, const std::string& modelFile) {
    if (!scene) return nullptr;
    const std::string fullPath = ResolveModelSpawnPath(modelFile);
    if (fullPath.empty()) {
        return nullptr;
    }
    auto mesh = Mesh::LoadFromOBJShared(fullPath);
    if (!mesh) {
        return nullptr;
    }
    const std::string stem = std::filesystem::path(modelFile).stem().string();
    Entity* e = scene->CreateEntity("Model_" + stem);
    auto* mr = e->AddComponent<MeshRenderer>();
    mr->SetMeshAsset(std::move(mesh));
    mr->color = glm::vec3(1.f);
    e->AddComponent<BoxCollider>()->size = glm::vec3(1.f);
    auto* t = e->GetComponent<Transform>();
    if (t) t->position = glm::vec3(0, 2, 0);
    std::cout << "[GameBuilder] Model yuklendi: " << fullPath << std::endl;
    return e;
}

bool GameBuilderPanel::AssignTextureToEntity(Entity* selectedEntity,
                                             const std::string& textureFile) {
    if (!selectedEntity || textureFile.empty() ||
        !ResourceManager::IsSafeAssetPath(textureFile)) {
        return false;
    }
    auto* mr = selectedEntity->GetComponent<MeshRenderer>();
    if (!mr) {
        std::cout << "[GameBuilder] Secili entity'de MeshRenderer yok!" << std::endl;
        return false;
    }
    const std::string relativePath = "assets/textures/" + textureFile;
    const std::filesystem::path resolvedPath =
        ResourceManager::Get().ResolveAssetPath(relativePath);
    if (resolvedPath.empty()) {
        return false;
    }
    const std::string resolvedStr = resolvedPath.string();
    const std::string canonicalKey =
        ResourceManager::Get().CanonicalAssetKey(resolvedStr);
    auto tex = TextureManager::Get().LoadShared(canonicalKey, resolvedStr);
    if (!tex) {
        return false;
    }
    tex->SetPath(relativePath);
    mr->SetTextureAsset(std::move(tex));
    std::cout << "[GameBuilder] Texture atandi: " << relativePath << std::endl;
    return true;
}

// ---------------------------------------------------------------------------
// Textures Tab
// ---------------------------------------------------------------------------
void GameBuilderPanel::DrawTexturesTab(Scene* scene, Entity* selectedEntity) {
    (void)scene;
    if (m_TexturesDirty) {
        RefreshTextureList();
        m_TexturesDirty = false;
    }

    if (ImGui::Button("Listeyi Yenile##tex", ImVec2(-1, 0))) {
        RefreshTextureList();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Secili entity: %s",
        selectedEntity ? selectedEntity->GetName().c_str() : "(Hiyerarsiden entity secin)");
    ImGui::Separator();

    if (m_TextureFiles.empty()) {
        ImGui::TextDisabled("assets/textures/ klasorunde texture bulunamadi.");
        return;
    }

    if (ImGui::BeginListBox("##textures", ImVec2(-1, 350))) {
        for (int i = 0; i < (int)m_TextureFiles.size(); ++i) {
            const bool selected = (m_SelectedTextureIdx == i);
            if (ImGui::Selectable(m_TextureFiles[i].c_str(), selected)) {
                m_SelectedTextureIdx = i;
                AssignTextureToEntity(selectedEntity, m_TextureFiles[i]);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }
}

// ---------------------------------------------------------------------------
// Audio Tab
// ---------------------------------------------------------------------------
void GameBuilderPanel::DrawAudioTab() {
    if (m_AudioDirty) {
        RefreshAudioList();
        m_AudioDirty = false;
    }

    if (ImGui::Button("Listeyi Yenile##audio", ImVec2(-1, 0))) {
        RefreshAudioList();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("assets/audio/ klasorunuzdeki ses dosyalari:");
    ImGui::Separator();

    if (m_AudioFiles.empty()) {
        ImGui::TextDisabled("Hen\xC3\xBCz ses dosyasi bulunamadi.");
        ImGui::TextDisabled("assets/audio/ klasorune .wav veya .ogg");
        ImGui::TextDisabled("dosyalari ekleyin.");
        return;
    }

    if (ImGui::BeginListBox("##audios", ImVec2(-1, 300))) {
        for (int i = 0; i < (int)m_AudioFiles.size(); ++i) {
            const bool selected = (m_SelectedAudioIdx == i);
            if (ImGui::Selectable(m_AudioFiles[i].c_str(), selected)) {
                m_SelectedAudioIdx = i;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }

    ImGui::Spacing();
    if (m_SelectedAudioIdx >= 0 && m_SelectedAudioIdx < (int)m_AudioFiles.size()) {
        const std::string& audioFile = m_AudioFiles[m_SelectedAudioIdx];
        ImGui::Text("Secilen: %s", audioFile.c_str());
        if (ImGui::Button("Ses Cal (Test)", ImVec2(-1, 36))) {
            AudioSystem::Get().PlayOneShot(audioFile);
            std::cout << "[GameBuilder] Ses caliniyor: " << audioFile << std::endl;
        }
    }
}

// ---------------------------------------------------------------------------
// Refresh Helpers
// ---------------------------------------------------------------------------
void GameBuilderPanel::RefreshTextureList() {
    m_TextureFiles.clear();
    const std::filesystem::path dir =
        ResourceManager::Get().ResolveAssetPath("assets/textures");
    const std::filesystem::path scanRoot =
        dir.empty() ? std::filesystem::path("assets/textures") : dir;
    std::error_code ec;
    if (!std::filesystem::exists(scanRoot, ec)) return;
    for (const auto& entry : std::filesystem::directory_iterator(scanRoot, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
            ext == ".tga" || ext == ".bmp") {
            m_TextureFiles.push_back(entry.path().filename().string());
        }
    }
}

void GameBuilderPanel::RefreshAudioList() {
    m_AudioFiles.clear();
    const std::filesystem::path dir =
        ResourceManager::Get().ResolveAssetPath("assets/audio");
    const std::filesystem::path scanRoot =
        dir.empty() ? std::filesystem::path("assets/audio") : dir;
    std::error_code ec;
    if (!std::filesystem::exists(scanRoot, ec)) return;
    for (const auto& entry : std::filesystem::directory_iterator(scanRoot, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") {
            m_AudioFiles.push_back(entry.path().filename().string());
        }
    }
}

void GameBuilderPanel::RefreshModelList() {
    m_ModelFiles.clear();
    m_ActiveModelDir = "assets/objects";
    std::error_code ec;

    auto collectModels = [&](const std::string& relativeDir) -> bool {
        const std::filesystem::path resolvedDir =
            ResourceManager::Get().ResolveAssetPath(relativeDir);
        const std::filesystem::path scanRoot =
            resolvedDir.empty() ? std::filesystem::path(relativeDir) : resolvedDir;
        if (!std::filesystem::exists(scanRoot, ec)) {
            return false;
        }
        std::vector<std::string> found;
        for (const auto& entry : std::filesystem::directory_iterator(scanRoot, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            std::string ext = entry.path().extension().string();
            if (ext == ".obj" || ext == ".OBJ") {
                found.push_back(entry.path().filename().string());
            }
        }
        if (found.empty()) {
            return false;
        }
        m_ActiveModelDir = relativeDir;
        m_ModelFiles = std::move(found);
        return true;
    };

    if (!collectModels("assets/objects")) {
        collectModels("assets/models");
    }
}

} // namespace Archura

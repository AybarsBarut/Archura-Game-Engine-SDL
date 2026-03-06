#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "GameBuilderPanel.h"
#include "../ecs/Entity.h"
#include "../ecs/Component.h"
#include "../rendering/Mesh.h"
#include "../rendering/Texture.h"
#include "../core/AudioSystem.h"

#include <imgui.h>
#include <filesystem>
#include <iostream>

namespace Archura {

// ---------------------------------------------------------------------------
// Draw – ana giriş noktası
// ---------------------------------------------------------------------------
void GameBuilderPanel::Draw(Scene* scene, Entity* selectedEntity) {
    if (!m_Open) return;

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
            mr->mesh  = Mesh::CreateCube();
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
            mr->mesh  = Mesh::CreateSphere(1.0f, 16);
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
            mr->mesh  = Mesh::CreateCapsule(0.5f, 2.0f);
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
            mr->mesh  = Mesh::CreateRamp(4.0f, 2.0f, 4.0f);
            mr->color = glm::vec3(0.8f, 0.6f, 0.3f);
            e->AddComponent<BoxCollider>()->size = glm::vec3(4.f, 2.f, 4.f);
            auto* t = e->GetComponent<Transform>();
            if (t) t->position = glm::vec3(0, 0, 0);
        }
    }

    if (ImGui::Button("Merdiven (Stairs)", ImVec2(btnW, btnH))) {
        if (scene) {
            Entity* e = scene->CreateEntity("MyObject_Stairs");
            auto* mr  = e->AddComponent<MeshRenderer>();
            mr->mesh  = Mesh::CreateStairs(2.0f, 0.5f, 0.5f, 4);
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
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "3D Modeller (assets/objects/)");
    ImGui::Separator();

    if (ImGui::Button("Listeyi Yenile", ImVec2(-1, 0))) {
        m_ModelsDirty = true;
    }
    if (m_ModelsDirty) {
        RefreshModelList();
        m_ModelsDirty = false;
    }

    if (m_ModelFiles.empty()) {
        ImGui::TextDisabled("assets/objects/ klasorunde .obj dosyasi bulunamadi.");
        ImGui::TextDisabled("Kendi modellerinizi buraya ekleyin.");
    } else {
        for (const auto& modelFile : m_ModelFiles) {
            if (ImGui::Button((modelFile + "  [OBJ]").c_str(), ImVec2(-1, 32))) {
                if (scene) {
                    std::string fullPath = "assets/objects/" + modelFile;
                    std::string stem = std::filesystem::path(fullPath).stem().string();
                    Entity* e = scene->CreateEntity("Model_" + stem);
                    auto* mr  = e->AddComponent<MeshRenderer>();
                    mr->mesh  = Mesh::LoadFromOBJ(fullPath);
                    mr->color = glm::vec3(1.f);
                    e->AddComponent<BoxCollider>()->size = glm::vec3(1.f);
                    auto* t = e->GetComponent<Transform>();
                    if (t) t->position = glm::vec3(0, 2, 0);
                    std::cout << "[GameBuilder] Model yuklendi: " << fullPath << std::endl;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Textures Tab
// ---------------------------------------------------------------------------
void GameBuilderPanel::DrawTexturesTab(Scene* scene, Entity* selectedEntity) {
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

                // Seçili entity'ye texture ata
                if (selectedEntity) {
                    auto* mr = selectedEntity->GetComponent<MeshRenderer>();
                    if (mr) {
                        std::string path = "assets/textures/" + m_TextureFiles[i];
                        std::string stem = std::filesystem::path(path).stem().string();
                        Texture* tex = TextureManager::Get().Load(stem, path);
                        if (tex) {
                            mr->texture = tex;
                            std::cout << "[GameBuilder] Texture atandi: " << path << std::endl;
                        }
                    } else {
                        std::cout << "[GameBuilder] Secili entity'de MeshRenderer yok!" << std::endl;
                    }
                }
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
        std::string path = "assets/audio/" + m_AudioFiles[m_SelectedAudioIdx];
        ImGui::Text("Secilen: %s", m_AudioFiles[m_SelectedAudioIdx].c_str());
        if (ImGui::Button("Ses Cal (Test)", ImVec2(-1, 36))) {
            AudioSystem::Get().PlayOneShot(path);
            std::cout << "[GameBuilder] Ses caliniyor: " << path << std::endl;
        }
    }
}

// ---------------------------------------------------------------------------
// Refresh Helpers
// ---------------------------------------------------------------------------
void GameBuilderPanel::RefreshTextureList() {
    m_TextureFiles.clear();
    std::string dir = "assets/textures";
    if (!std::filesystem::exists(dir)) return;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" ||
            ext == ".tga" || ext == ".bmp") {
            m_TextureFiles.push_back(entry.path().filename().string());
        }
    }
}

void GameBuilderPanel::RefreshAudioList() {
    m_AudioFiles.clear();
    std::string dir = "assets/audio";
    if (!std::filesystem::exists(dir)) return;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") {
            m_AudioFiles.push_back(entry.path().filename().string());
        }
    }
}

void GameBuilderPanel::RefreshModelList() {
    m_ModelFiles.clear();
    std::string dir = "assets/objects";
    if (!std::filesystem::exists(dir)) {
        dir = "assets/models";
        if (!std::filesystem::exists(dir)) return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        if (ext == ".obj" || ext == ".OBJ") {
            m_ModelFiles.push_back(entry.path().filename().string());
        }
    }
}

} // namespace Archura

#include "AudioSystem.h"

// Windows headers must come first with proper guards
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cmath>
#include "ResourceManager.h"
#include "../game/AudioSource.h"
#include "../rendering/Camera.h"
#include "../ecs/Entity.h"
#include "../ecs/Component.h"
#include "../ecs/components/AudioSourceComponent.h"
#include <glm/glm.hpp>

// Link with winmm.lib
#pragma comment(lib, "winmm.lib")

namespace Archura {

AudioSystem::AudioSystem() {
    // Varsayilan ses seviyeleri
    m_MasterVolume = 1.0f;
    m_MusicVolume = 1.0f;
    m_SFXVolume = 1.0f;
}

void AudioSystem::Init() {

}

void AudioSystem::Shutdown() {
    StopMusic();
    // Tum sesleri kapat
    mciSendString("close all", NULL, 0, NULL);
    m_OneShotAliases.clear();
}

std::string AudioSystem::ResolveAudioPath(const std::string& filename) {
    if (!ResourceManager::IsSafeAssetPath(filename)) {
        return {};
    }

    std::string normalized = filename;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    while (normalized.rfind("./", 0) == 0) {
        normalized.erase(0, 2);
    }

    // Collapse accidental duplicate "assets/audio/assets/audio/" prefixes.
    const std::string prefix = "assets/audio/";
    while (normalized.rfind(prefix + prefix, 0) == 0) {
        normalized.erase(0, prefix.size());
    }

    if (const std::filesystem::path resolvedDirect =
            ResourceManager::Get().ResolveAssetPath(normalized);
        !resolvedDirect.empty()) {
        return resolvedDirect.string();
    }

    const std::filesystem::path rawPath(normalized);
    if (!rawPath.is_absolute() && normalized.rfind(prefix, 0) != 0) {
        const std::string prefixed = prefix + normalized;
        if (const std::filesystem::path resolvedPrefixed =
                ResourceManager::Get().ResolveAssetPath(prefixed);
            !resolvedPrefixed.empty()) {
            return resolvedPrefixed.string();
        }
        return prefixed;
    }

    return normalized;
}

void AudioSystem::PlayMusic(const std::string& filename, bool loop) {
    StopMusic();

    const std::string path = ResolveAudioPath(filename);
    if (path.empty() || !std::filesystem::exists(path)) {
        std::cerr << "Audio file not found: " << path << std::endl;
        return;
    }

    // Alias olustur (Music)
    std::string alias = "Music";
    std::string cmdOpen = "open \"" + path + "\" type mpegvideo alias " + alias;
    
    MCIERROR err = mciSendString(cmdOpen.c_str(), NULL, 0, NULL);
    if (err != 0) {
        char buffer[128];
        mciGetErrorString(err, buffer, 128);
        std::cerr << "MCI Error (Open): " << buffer << std::endl;
        return;
    }

    std::string cmdPlay = "play " + alias;
    if (loop) {
        cmdPlay += " repeat";
    }

    // Ses seviyesini ayarla
    ApplyVolume(alias, m_MusicVolume);

    mciSendString(cmdPlay.c_str(), NULL, 0, NULL);
    m_CurrentMusicAlias = alias;
}

void AudioSystem::PlayOneShot(const std::string& filename) {
    const std::string path = ResolveAudioPath(filename);
    if (path.empty() || !std::filesystem::exists(path)) {
        // Hata mesaji yazdirma (cok sik olabilir)
        return;
    }

    // Benzersiz alias (SFX_0, SFX_1, ...)
    std::string alias = "SFX_" + std::to_string(m_NextSoundID++);
    
    // Dosyayi ac
    std::string cmdOpen = "open \"" + path + "\" type mpegvideo alias " + alias;
    if (mciSendString(cmdOpen.c_str(), NULL, 0, NULL) != 0)
        return;

    // Ses seviyesini ayarla
    ApplyVolume(alias, m_SFXVolume);

    std::string cmdPlay = "play " + alias;
    if (mciSendString(cmdPlay.c_str(), NULL, 0, NULL) == 0)
        m_OneShotAliases.push_back(alias);
    else
        mciSendString(("close " + alias).c_str(), NULL, 0, NULL);
}

void AudioSystem::StopMusic() {
    if (!m_CurrentMusicAlias.empty()) {
        std::string cmd = "close " + m_CurrentMusicAlias;
        mciSendString(cmd.c_str(), NULL, 0, NULL);
        m_CurrentMusicAlias.clear();
    }
}

void AudioSystem::SetMasterVolume(float volume) {
    m_MasterVolume = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 1.0f;
    // Muzik sesini guncelle
    if (!m_CurrentMusicAlias.empty()) {
        ApplyVolume(m_CurrentMusicAlias, m_MusicVolume);
    }
}

void AudioSystem::SetMusicVolume(float volume) {
    m_MusicVolume = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 1.0f;
    if (!m_CurrentMusicAlias.empty()) {
        ApplyVolume(m_CurrentMusicAlias, m_MusicVolume);
    }
}

void AudioSystem::SetSFXVolume(float volume) {
    m_SFXVolume = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 1.0f;
}

void AudioSystem::ApplyVolume(const std::string& alias, float channelVolume) {
    // MCI volume range: 0 to 1000
    const float safeChannelVolume = std::isfinite(channelVolume)
                                        ? std::clamp(channelVolume, 0.0f, 1.0f)
                                        : 0.0f;
    int finalVol = static_cast<int>(m_MasterVolume * safeChannelVolume * 1000.0f);
    if (finalVol < 0) finalVol = 0;
    if (finalVol > 1000) finalVol = 1000;

    std::string cmd = "setaudio " + alias + " volume to " + std::to_string(finalVol);
    mciSendString(cmd.c_str(), NULL, 0, NULL);
}

void AudioSystem::Update(Scene* scene, Camera* camera) {
    CleanupFinishedOneShots();
    if (!scene || !camera) return;

    for (auto& entityPtr : scene->GetEntities()) {
        if (!entityPtr) continue;
        auto* source = entityPtr->GetComponent<AudioSource>();
        if (!source || !source->isPlaying) continue;

        auto* transform = entityPtr->GetComponent<Transform>();
        if (!transform) continue;

        std::string alias = source->runtimeAlias;
        if (alias.empty()) {
            alias = std::string("sound_") + std::to_string(entityPtr->GetID());
            source->runtimeAlias = alias;
            if (!source->clipName.empty()) {
                Play(source, alias);
            }
        }

        // Calculate world-space distance
        const glm::vec3 worldPos = glm::vec3(entityPtr->GetWorldTransform()[3]);
        const float distance = glm::distance(worldPos, camera->GetPosition());

        // Linear attenuation
        // minDistance: volume = max
        // maxDistance: volume = 0
        float volume = source->volume;
        
        const float minDistance = std::max(0.0f, source->minDistance);
        const float maxDistance = std::max(minDistance, source->maxDistance);
        const float sourceVolume = std::isfinite(source->volume)
                                       ? std::clamp(source->volume, 0.0f, 1.0f)
                                       : 0.0f;
        if (distance < minDistance || maxDistance <= minDistance) {
            volume = sourceVolume;
        } else if (distance > maxDistance) {
            volume = 0.0f;
        } else {
            const float pct = 1.0f - ((distance - minDistance) / (maxDistance - minDistance));
            volume = sourceVolume * std::clamp(pct, 0.0f, 1.0f);
        }
        
        // MCI uses volume 0-1000
        int finalVol = static_cast<int>(std::clamp(volume, 0.0f, 1.0f) * 1000.0f);
        if (finalVol < 0) finalVol = 0;
        if (finalVol > 1000) finalVol = 1000;

        std::string cmd = "setaudio " + alias + " volume to " + std::to_string(finalVol);
        mciSendStringA(cmd.c_str(), NULL, 0, NULL);
    }
}

void AudioSystem::Play(AudioSource* source, const std::string& alias) {
    if (!source) return;

    const std::string path = ResolveAudioPath(source->clipName);
    if (path.empty() || !std::filesystem::exists(path)) return;

    // Close previous if any
    std::string cmdClose = "close " + alias;
    mciSendString(cmdClose.c_str(), NULL, 0, NULL);

    // Open
    std::string cmdOpen = "open \"" + path + "\" type mpegvideo alias " + alias;
    MCIERROR err = mciSendString(cmdOpen.c_str(), NULL, 0, NULL);
    
    if (err == 0) {
        std::string cmdPlay = "play " + alias;
        if (source->loop) cmdPlay += " repeat";
        mciSendString(cmdPlay.c_str(), NULL, 0, NULL);
        source->isPlaying = true;
        source->runtimeAlias = alias;
    } else {
        std::cerr << "AudioSource Open Error: " << path << std::endl;
    }
}

void AudioSystem::Stop(AudioSource* source) {
    if (!source) return;
    if (source->runtimeAlias.empty()) {
        source->isPlaying = false;
        return;
    }

    const std::string alias = source->runtimeAlias;
    std::string cmd = "stop " + alias;
    mciSendStringA(cmd.c_str(), NULL, 0, NULL);
    mciSendStringA(("close " + alias).c_str(), NULL, 0, NULL);
    source->runtimeAlias.clear();
    source->isPlaying = false;
}

void AudioSystem::StopAllSceneAudio(Scene* scene) {
    if (scene) {
        for (const auto& entityPtr : scene->GetEntities()) {
            if (!entityPtr) continue;
            if (auto* source = entityPtr->GetComponent<AudioSource>()) {
                Stop(source);
            }
            if (auto* asc = entityPtr->GetComponent<AudioSourceComponent>()) {
                asc->playing = false;
            }
        }
    }
    for (const std::string& alias : m_OneShotAliases) {
        mciSendStringA(("stop " + alias).c_str(), nullptr, 0, nullptr);
        mciSendStringA(("close " + alias).c_str(), nullptr, 0, nullptr);
    }
    m_OneShotAliases.clear();
    StopMusic();
}

void AudioSystem::CleanupFinishedOneShots() {
    char mode[32]{};
    for (auto it = m_OneShotAliases.begin(); it != m_OneShotAliases.end();) {
        const std::string query = "status " + *it + " mode";
        const MCIERROR error = mciSendStringA(query.c_str(), mode, sizeof(mode), nullptr);
        const bool playing = error == 0 && std::string(mode) == "playing";
        if (!playing) {
            mciSendStringA(("close " + *it).c_str(), nullptr, 0, nullptr);
            it = m_OneShotAliases.erase(it);
        } else {
            ++it;
        }
        mode[0] = '\0';
    }
}

} // namespace Archura

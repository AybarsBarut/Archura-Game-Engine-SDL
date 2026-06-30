#pragma once

#include "../ecs/System.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Archura {

class Camera;

class OpenALAudioSystem : public System {
public:
    OpenALAudioSystem() = default;
    ~OpenALAudioSystem() override;

    void Init(Scene* scene) override;
    void Update(float deltaTime) override;
    void Shutdown() override;

    void SetListenerCamera(Camera* camera) { m_ListenerCamera = camera; }
    bool IsAvailable() const { return m_Available; }

private:
    struct SourceState {
        unsigned int source_id = 0;
        unsigned int buffer_id = 0;
        std::string clip_path;
        bool was_playing = false;
    };

    struct AudioClipData {
        std::vector<std::int16_t> samples;
        int channels = 0;
        int sample_rate = 0;
    };

    void UpdateListener();
    void UpdateSources();
    void DestroyMissingSources(const std::vector<std::uint32_t>& visible_entities);

    unsigned int GetOrLoadBuffer(const std::string& clip_path);
    bool LoadAudioClip(const std::string& path, AudioClipData& out_clip) const;
    bool LoadWav(const std::string& path, AudioClipData& out_clip) const;
    bool LoadOgg(const std::string& path, AudioClipData& out_clip) const;
    std::string ResolveClipPath(const std::string& clip_path) const;

    void DestroySource(SourceState& state);
    void ClearBuffers();

private:
    Camera* m_ListenerCamera = nullptr;
    void* m_Device = nullptr;
    void* m_Context = nullptr;
    bool m_Available = false;

    std::unordered_map<std::uint32_t, SourceState> m_SourceStates;
    std::unordered_map<std::string, unsigned int> m_BufferCache;
};

} // namespace Archura

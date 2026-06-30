#include "OpenALAudioSystem.h"

#ifdef ARCHURA_OPENAL

#include "../ecs/Entity.h"
#include "../ecs/Component.h"
#include "../ecs/components/AudioSourceComponent.h"
#include "../rendering/Camera.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4100 4127 4242 4244 4267 4365 4456 4701 4996)
#endif

#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <stb_vorbis.c>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace Archura {

namespace {

ALenum GetFormatForChannels(int channels) {
    if (channels == 1) {
        return AL_FORMAT_MONO16;
    }
    if (channels == 2) {
        return AL_FORMAT_STEREO16;
    }
    return 0;
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool ContainsEntity(const std::vector<std::uint32_t>& entities,
                    std::uint32_t id) {
    return std::find(entities.begin(), entities.end(), id) != entities.end();
}

void LogOpenALError(const char* operation) {
    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cerr << "[OpenAL] " << operation << " failed: 0x" << std::hex
                  << error << std::dec << std::endl;
    }
}

} // namespace

OpenALAudioSystem::~OpenALAudioSystem() { Shutdown(); }

void OpenALAudioSystem::Init(Scene* scene) {
    System::Init(scene);

    ALCdevice* device = alcOpenDevice(nullptr);
    if (!device) {
        std::cerr << "[OpenAL] No audio device available" << std::endl;
        return;
    }

    ALCcontext* context = alcCreateContext(device, nullptr);
    if (!context) {
        alcCloseDevice(device);
        std::cerr << "[OpenAL] Failed to create context" << std::endl;
        return;
    }

    if (!alcMakeContextCurrent(context)) {
        alcDestroyContext(context);
        alcCloseDevice(device);
        std::cerr << "[OpenAL] Failed to make context current" << std::endl;
        return;
    }

    m_Device = device;
    m_Context = context;
    m_Available = true;

    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    alDopplerFactor(1.0f);
    alSpeedOfSound(343.3f);

    std::cout << "[OpenAL] Spatial audio initialized" << std::endl;
}

void OpenALAudioSystem::Update(float deltaTime) {
    (void)deltaTime;
    if (!m_Available || !m_Scene) {
        return;
    }

    UpdateListener();
    UpdateSources();
}

void OpenALAudioSystem::Shutdown() {
    for (auto& entry : m_SourceStates) {
        DestroySource(entry.second);
    }
    m_SourceStates.clear();
    ClearBuffers();

    if (m_Context) {
        ALCcontext* context = static_cast<ALCcontext*>(m_Context);
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);
        m_Context = nullptr;
    }

    if (m_Device) {
        alcCloseDevice(static_cast<ALCdevice*>(m_Device));
        m_Device = nullptr;
    }

    m_Available = false;
}

void OpenALAudioSystem::UpdateListener() {
    if (!m_ListenerCamera) {
        return;
    }

    const glm::vec3& position = m_ListenerCamera->GetPosition();
    const glm::vec3& forward = m_ListenerCamera->GetFront();
    const glm::vec3& up = m_ListenerCamera->GetUp();

    const ALfloat orientation[6] = {
        forward.x, forward.y, forward.z,
        up.x,      up.y,      up.z,
    };

    alListener3f(AL_POSITION, position.x, position.y, position.z);
    alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alListenerfv(AL_ORIENTATION, orientation);
}

void OpenALAudioSystem::UpdateSources() {
    std::vector<std::uint32_t> visible_entities;
    visible_entities.reserve(m_Scene->GetEntities().size());

    for (const auto& entity_ptr : m_Scene->GetEntities()) {
        Entity* entity = entity_ptr.get();
        auto* source_component = entity->GetComponent<AudioSourceComponent>();
        auto* transform = entity->GetComponent<Transform>();

        if (!source_component || !transform) {
            continue;
        }

        const std::uint32_t entity_id = entity->GetID();
        visible_entities.push_back(entity_id);

        SourceState& state = m_SourceStates[entity_id];
        if (state.source_id == 0) {
            ALuint source = 0;
            alGenSources(1, &source);
            LogOpenALError("alGenSources");
            state.source_id = source;
        }

        if (state.clip_path != source_component->clip_path) {
            ALuint buffer = GetOrLoadBuffer(source_component->clip_path);
            if (buffer == 0) {
                source_component->playing = false;
                continue;
            }

            alSourceStop(state.source_id);
            alSourcei(state.source_id, AL_BUFFER, static_cast<ALint>(buffer));
            state.buffer_id = buffer;
            state.clip_path = source_component->clip_path;
            state.was_playing = false;
        }

        alSource3f(state.source_id, AL_POSITION, transform->position.x,
                   transform->position.y, transform->position.z);
        alSource3f(state.source_id, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        alSourcef(state.source_id, AL_GAIN, source_component->gain);
        alSourcef(state.source_id, AL_PITCH, source_component->pitch);
        alSourcef(state.source_id, AL_REFERENCE_DISTANCE,
                  source_component->ref_distance);
        alSourcei(state.source_id, AL_LOOPING,
                  source_component->looping ? AL_TRUE : AL_FALSE);

        ALint current_state = AL_INITIAL;
        alGetSourcei(state.source_id, AL_SOURCE_STATE, &current_state);

        if (source_component->playing) {
            if (!state.was_playing || current_state == AL_STOPPED ||
                current_state == AL_INITIAL) {
                alSourcePlay(state.source_id);
                LogOpenALError("alSourcePlay");
            }
            state.was_playing = true;
        } else {
            if (state.was_playing) {
                alSourceStop(state.source_id);
            }
            state.was_playing = false;
        }

        if (!source_component->looping && state.was_playing) {
            alGetSourcei(state.source_id, AL_SOURCE_STATE, &current_state);
            if (current_state == AL_STOPPED) {
                source_component->playing = false;
                state.was_playing = false;
            }
        }
    }

    DestroyMissingSources(visible_entities);
}

void OpenALAudioSystem::DestroyMissingSources(
    const std::vector<std::uint32_t>& visible_entities) {
    for (auto it = m_SourceStates.begin(); it != m_SourceStates.end();) {
        if (ContainsEntity(visible_entities, it->first)) {
            ++it;
            continue;
        }

        DestroySource(it->second);
        it = m_SourceStates.erase(it);
    }
}

unsigned int OpenALAudioSystem::GetOrLoadBuffer(const std::string& clip_path) {
    const std::string resolved_path = ResolveClipPath(clip_path);
    if (resolved_path.empty()) {
        return 0;
    }

    auto existing = m_BufferCache.find(resolved_path);
    if (existing != m_BufferCache.end()) {
        return existing->second;
    }

    AudioClipData clip;
    if (!LoadAudioClip(resolved_path, clip)) {
        return 0;
    }

    ALenum format = GetFormatForChannels(clip.channels);
    if (format == 0) {
        std::cerr << "[OpenAL] Unsupported channel count " << clip.channels
                  << " for " << resolved_path << std::endl;
        return 0;
    }

    ALuint buffer = 0;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, format, clip.samples.data(),
                 static_cast<ALsizei>(clip.samples.size() * sizeof(std::int16_t)),
                 static_cast<ALsizei>(clip.sample_rate));
    LogOpenALError("alBufferData");

    m_BufferCache[resolved_path] = buffer;
    return buffer;
}

bool OpenALAudioSystem::LoadAudioClip(const std::string& path,
                                      AudioClipData& out_clip) const {
    const std::string extension =
        ToLower(std::filesystem::path(path).extension().string());

    if (extension == ".wav") {
        return LoadWav(path, out_clip);
    }
    if (extension == ".ogg") {
        return LoadOgg(path, out_clip);
    }

    std::cerr << "[OpenAL] Unsupported audio format: " << path << std::endl;
    return false;
}

bool OpenALAudioSystem::LoadWav(const std::string& path,
                                AudioClipData& out_clip) const {
    unsigned int channels = 0;
    unsigned int sample_rate = 0;
    drwav_uint64 frame_count = 0;

    drwav_int16* pcm = drwav_open_file_and_read_pcm_frames_s16(
        path.c_str(), &channels, &sample_rate, &frame_count, nullptr);
    if (!pcm) {
        std::cerr << "[OpenAL] Failed to decode WAV: " << path << std::endl;
        return false;
    }

    const std::size_t sample_count =
        static_cast<std::size_t>(frame_count) * channels;
    out_clip.samples.assign(pcm, pcm + sample_count);
    out_clip.channels = static_cast<int>(channels);
    out_clip.sample_rate = static_cast<int>(sample_rate);

    drwav_free(pcm, nullptr);
    return !out_clip.samples.empty();
}

bool OpenALAudioSystem::LoadOgg(const std::string& path,
                                AudioClipData& out_clip) const {
    int channels = 0;
    int sample_rate = 0;
    short* pcm = nullptr;

    int frames = stb_vorbis_decode_filename(path.c_str(), &channels,
                                            &sample_rate, &pcm);
    if (frames < 0 || !pcm) {
        std::cerr << "[OpenAL] Failed to decode OGG: " << path << std::endl;
        return false;
    }

    const std::size_t sample_count =
        static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels);
    out_clip.samples.assign(pcm, pcm + sample_count);
    out_clip.channels = channels;
    out_clip.sample_rate = sample_rate;

    std::free(pcm);
    return !out_clip.samples.empty();
}

std::string OpenALAudioSystem::ResolveClipPath(
    const std::string& clip_path) const {
    if (clip_path.empty()) {
        return {};
    }

    std::filesystem::path path(clip_path);
    if (std::filesystem::exists(path)) {
        return path.string();
    }

    path = std::filesystem::path("assets/audio") / clip_path;
    if (std::filesystem::exists(path)) {
        return path.string();
    }

    std::cerr << "[OpenAL] Audio clip not found: " << clip_path << std::endl;
    return {};
}

void OpenALAudioSystem::DestroySource(SourceState& state) {
    if (state.source_id != 0) {
        ALuint source = state.source_id;
        alSourceStop(source);
        alDeleteSources(1, &source);
        state.source_id = 0;
    }

    state.buffer_id = 0;
    state.clip_path.clear();
    state.was_playing = false;
}

void OpenALAudioSystem::ClearBuffers() {
    for (auto& entry : m_BufferCache) {
        ALuint buffer = entry.second;
        if (buffer != 0) {
            alDeleteBuffers(1, &buffer);
        }
    }
    m_BufferCache.clear();
}

} // namespace Archura

#endif // ARCHURA_OPENAL

#include "pch.h"
#include "audio_system.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <filesystem>

namespace NexAur {
    namespace {
        constexpr ma_uint32 kDefaultSoundFlags = MA_SOUND_FLAG_NO_SPATIALIZATION;

        float clampVolume(float volume) {
            return std::clamp(volume, 0.0f, 1.0f);
        }

        std::string toAudioPathString(const std::filesystem::path& path) {
            return path.string();
        }

        struct SoundInstance {
            ma_sound sound{};
            bool initialized = false;
            bool paused = false;

            ~SoundInstance() {
                if (initialized) {
                    ma_sound_uninit(&sound);
                }
            }
        };
    } // namespace

    struct AudioSystem::Impl {
        ma_engine engine{};
        ma_sound_group music_group{};
        ma_sound_group sfx_group{};
        bool engine_initialized = false;
        bool music_group_initialized = false;
        bool sfx_group_initialized = false;

        std::unique_ptr<SoundInstance> music;
        std::vector<std::unique_ptr<SoundInstance>> one_shots;

        float master_volume = 1.0f;
        float music_volume = 1.0f;
        float sfx_volume = 1.0f;
        bool paused = false;

        bool initialize() {
            ma_result result = ma_engine_init(nullptr, &engine);
            if (result != MA_SUCCESS) {
                NX_CORE_WARN("AudioSystem failed to initialize miniaudio engine: {}", static_cast<int>(result));
                return false;
            }
            engine_initialized = true;

            result = ma_sound_group_init(&engine, 0, nullptr, &music_group);
            if (result != MA_SUCCESS) {
                NX_CORE_WARN("AudioSystem failed to initialize music sound group: {}", static_cast<int>(result));
                shutdown();
                return false;
            }
            music_group_initialized = true;

            result = ma_sound_group_init(&engine, 0, nullptr, &sfx_group);
            if (result != MA_SUCCESS) {
                NX_CORE_WARN("AudioSystem failed to initialize sfx sound group: {}", static_cast<int>(result));
                shutdown();
                return false;
            }
            sfx_group_initialized = true;

            applyVolumes();
            return true;
        }

        void shutdown() {
            one_shots.clear();
            music.reset();

            if (sfx_group_initialized) {
                ma_sound_group_uninit(&sfx_group);
                sfx_group_initialized = false;
            }
            if (music_group_initialized) {
                ma_sound_group_uninit(&music_group);
                music_group_initialized = false;
            }
            if (engine_initialized) {
                ma_engine_uninit(&engine);
                engine_initialized = false;
            }

            paused = false;
        }

        bool available() const {
            return engine_initialized && music_group_initialized && sfx_group_initialized;
        }

        void applyVolumes() {
            if (!engine_initialized) {
                return;
            }

            ma_engine_set_volume(&engine, master_volume);
            if (music_group_initialized) {
                ma_sound_group_set_volume(&music_group, music_volume);
            }
            if (sfx_group_initialized) {
                ma_sound_group_set_volume(&sfx_group, sfx_volume);
            }
        }
    };

    AudioSystem::AudioSystem()
        : m_impl(std::make_unique<Impl>()) {
    }

    AudioSystem::~AudioSystem() {
        shutdown();
    }

    bool AudioSystem::initialize() {
        return m_impl && m_impl->initialize();
    }

    void AudioSystem::tick() {
        if (!isAvailable() || m_impl->paused) {
            return;
        }

        auto& one_shots = m_impl->one_shots;
        one_shots.erase(
            std::remove_if(
                one_shots.begin(),
                one_shots.end(),
                [](const std::unique_ptr<SoundInstance>& instance) {
                    return instance && !instance->paused && !ma_sound_is_playing(&instance->sound);
                }),
            one_shots.end());
    }

    void AudioSystem::shutdown() {
        if (m_impl) {
            m_impl->shutdown();
        }
    }

    bool AudioSystem::isAvailable() const {
        return m_impl && m_impl->available();
    }

    void AudioSystem::playOneShot(const std::filesystem::path& path, float volume) {
        if (!isAvailable() || path.empty()) {
            return;
        }

        auto instance = std::make_unique<SoundInstance>();
        const std::string audio_path = toAudioPathString(path);
        const ma_result result = ma_sound_init_from_file(
            &m_impl->engine,
            audio_path.c_str(),
            kDefaultSoundFlags,
            &m_impl->sfx_group,
            nullptr,
            &instance->sound);

        if (result != MA_SUCCESS) {
            NX_CORE_WARN("AudioSystem failed to load one-shot sound '{}': {}", audio_path, static_cast<int>(result));
            return;
        }

        instance->initialized = true;
        ma_sound_set_volume(&instance->sound, clampVolume(volume));
        if (ma_sound_start(&instance->sound) != MA_SUCCESS) {
            NX_CORE_WARN("AudioSystem failed to start one-shot sound '{}'.", audio_path);
            return;
        }

        m_impl->one_shots.push_back(std::move(instance));
    }

    void AudioSystem::playMusic(const std::filesystem::path& path, bool loop, float volume) {
        if (!isAvailable() || path.empty()) {
            return;
        }

        stopMusic();

        auto music = std::make_unique<SoundInstance>();
        const std::string audio_path = toAudioPathString(path);
        const ma_result result = ma_sound_init_from_file(
            &m_impl->engine,
            audio_path.c_str(),
            kDefaultSoundFlags,
            &m_impl->music_group,
            nullptr,
            &music->sound);

        if (result != MA_SUCCESS) {
            NX_CORE_WARN("AudioSystem failed to load music '{}': {}", audio_path, static_cast<int>(result));
            return;
        }

        music->initialized = true;
        ma_sound_set_looping(&music->sound, loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&music->sound, clampVolume(volume));
        if (ma_sound_start(&music->sound) != MA_SUCCESS) {
            NX_CORE_WARN("AudioSystem failed to start music '{}'.", audio_path);
            return;
        }

        m_impl->music = std::move(music);
    }

    void AudioSystem::stopMusic() {
        if (!m_impl) {
            return;
        }

        m_impl->music.reset();
    }

    void AudioSystem::setMasterVolume(float volume) {
        if (!m_impl) {
            return;
        }

        m_impl->master_volume = clampVolume(volume);
        m_impl->applyVolumes();
    }

    void AudioSystem::setMusicVolume(float volume) {
        if (!m_impl) {
            return;
        }

        m_impl->music_volume = clampVolume(volume);
        m_impl->applyVolumes();
    }

    void AudioSystem::setSfxVolume(float volume) {
        if (!m_impl) {
            return;
        }

        m_impl->sfx_volume = clampVolume(volume);
        m_impl->applyVolumes();
    }

    void AudioSystem::pauseAll() {
        if (!isAvailable() || m_impl->paused) {
            return;
        }

        if (m_impl->music && ma_sound_is_playing(&m_impl->music->sound)) {
            ma_sound_stop(&m_impl->music->sound);
            m_impl->music->paused = true;
        }

        for (const std::unique_ptr<SoundInstance>& instance : m_impl->one_shots) {
            if (instance && ma_sound_is_playing(&instance->sound)) {
                ma_sound_stop(&instance->sound);
                instance->paused = true;
            }
        }

        m_impl->paused = true;
    }

    void AudioSystem::resumeAll() {
        if (!isAvailable() || !m_impl->paused) {
            return;
        }

        if (m_impl->music && m_impl->music->paused) {
            if (ma_sound_start(&m_impl->music->sound) == MA_SUCCESS) {
                m_impl->music->paused = false;
            }
        }

        for (const std::unique_ptr<SoundInstance>& instance : m_impl->one_shots) {
            if (instance && instance->paused) {
                if (ma_sound_start(&instance->sound) == MA_SUCCESS) {
                    instance->paused = false;
                }
            }
        }

        m_impl->paused = false;
    }
} // namespace NexAur

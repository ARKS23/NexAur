#pragma once

#include <memory>

#include "Core/Base.h"
#include "Function/Audio/audio_service.h"

namespace NexAur {
    class NEXAUR_API AudioSystem final : public AudioService {
    public:
        AudioSystem();
        ~AudioSystem() override;

        AudioSystem(const AudioSystem&) = delete;
        AudioSystem& operator=(const AudioSystem&) = delete;
        AudioSystem(AudioSystem&&) = delete;
        AudioSystem& operator=(AudioSystem&&) = delete;

        bool initialize();
        void tick();
        void shutdown();

        bool isAvailable() const override;

        void playOneShot(const std::filesystem::path& path, float volume = 1.0f) override;
        void playMusic(const std::filesystem::path& path, bool loop = true, float volume = 1.0f) override;
        void stopMusic() override;

        void setMasterVolume(float volume) override;
        void setMusicVolume(float volume) override;
        void setSfxVolume(float volume) override;

        void pauseAll() override;
        void resumeAll() override;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace NexAur

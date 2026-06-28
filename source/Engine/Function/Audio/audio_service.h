#pragma once

#include <filesystem>

#include "Core/Base.h"

namespace NexAur {
    class NEXAUR_API AudioService {
    public:
        virtual ~AudioService() = default;

        virtual bool isAvailable() const = 0;

        virtual void playOneShot(const std::filesystem::path& path, float volume = 1.0f) = 0;
        virtual void playMusic(const std::filesystem::path& path, bool loop = true, float volume = 1.0f) = 0;
        virtual void stopMusic() = 0;

        virtual void setMasterVolume(float volume) = 0;
        virtual void setMusicVolume(float volume) = 0;
        virtual void setSfxVolume(float volume) = 0;

        virtual void pauseAll() = 0;
        virtual void resumeAll() = 0;
    };
} // namespace NexAur

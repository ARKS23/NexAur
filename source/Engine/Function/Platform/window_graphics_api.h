#pragma once

namespace NexAur {
    enum class WindowGraphicsAPI {
        Vulkan
    };

    inline const char* toString(WindowGraphicsAPI api) {
        switch (api) {
            case WindowGraphicsAPI::Vulkan:
                return "Vulkan";
        }

        return "Unknown";
    }
} // namespace NexAur

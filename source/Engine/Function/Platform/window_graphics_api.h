#pragma once

namespace NexAur {
    enum class WindowGraphicsAPI {
        OpenGL,
        Vulkan
    };

    inline const char* toString(WindowGraphicsAPI api) {
        switch (api) {
            case WindowGraphicsAPI::OpenGL:
                return "OpenGL";
            case WindowGraphicsAPI::Vulkan:
                return "Vulkan";
        }

        return "Unknown";
    }
} // namespace NexAur

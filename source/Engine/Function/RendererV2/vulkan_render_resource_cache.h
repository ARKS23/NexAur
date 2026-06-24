#pragma once

#include "Core/Base.h"

namespace NexAur {
    // RendererV2 专属资源缓存。D5 只建立生命周期边界，
    // D7 再接入 AssetHandle -> Vulkan renderer resource。
    class NEXAUR_API VulkanRenderResourceCache {
    public:
        void init();
        void clear();
        void shutdown();

    private:
        bool m_initialized = false;
    };
} // namespace NexAur

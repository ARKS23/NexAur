#include "pch.h"
#include "vulkan_render_resource_cache.h"

namespace NexAur {
    void VulkanRenderResourceCache::init() {
        m_initialized = true;
    }

    void VulkanRenderResourceCache::clear() {}

    void VulkanRenderResourceCache::shutdown() {
        if (!m_initialized) {
            return;
        }

        clear();
        m_initialized = false;
    }
} // namespace NexAur

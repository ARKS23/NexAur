#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace NexAur {
    // RendererV2 初始化 GPU 资源缓存时传入的后端上下文。
    // 这里只保存创建资源必需的 Vulkan 句柄，不向 Scene / Editor 暴露。
    struct VulkanResourceContext {
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue graphics_queue = VK_NULL_HANDLE;
        uint32_t graphics_queue_family = 0;
        uint32_t api_version = VK_API_VERSION_1_3;

        bool valid() const {
            return instance != VK_NULL_HANDLE &&
                   physical_device != VK_NULL_HANDLE &&
                   device != VK_NULL_HANDLE &&
                   graphics_queue != VK_NULL_HANDLE &&
                   api_version >= VK_API_VERSION_1_3;
        }
    };

    // 单次资源创建时使用的上传上下文，由 VulkanRenderResourceCache 组装。
    // cache 持有 allocator / upload command pool，resource 只在 create/reset 中消费。
    struct VulkanResourceUploadContext {
        VmaAllocator allocator = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue graphics_queue = VK_NULL_HANDLE;
        VkCommandPool command_pool = VK_NULL_HANDLE;

        bool valid() const {
            return allocator != VK_NULL_HANDLE &&
                   device != VK_NULL_HANDLE &&
                   graphics_queue != VK_NULL_HANDLE &&
                   command_pool != VK_NULL_HANDLE;
        }
    };
} // namespace NexAur

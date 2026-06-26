#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    struct RenderDebugDrawData;

    class NEXAUR_API VulkanDebugDrawBuffer {
    public:
        VulkanDebugDrawBuffer() = default;
        ~VulkanDebugDrawBuffer();

        VulkanDebugDrawBuffer(const VulkanDebugDrawBuffer&) = delete;
        VulkanDebugDrawBuffer& operator=(const VulkanDebugDrawBuffer&) = delete;

        bool init(const VulkanResourceContext& context);
        void shutdown();

        bool upload(const RenderDebugDrawData& debug_draw);

        VkBuffer getVertexBuffer() const { return m_vertex_buffer; }
        uint32_t getVertexCount() const { return m_vertex_count; }
        bool hasVertices() const { return m_vertex_count > 0 && m_vertex_buffer != VK_NULL_HANDLE; }

    private:
        bool ensureCapacity(VkDeviceSize required_size);
        bool createBuffer(VkDeviceSize size);
        void cleanupBuffer();
        uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const;

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_vertex_memory = VK_NULL_HANDLE;
        VkDeviceSize m_capacity = 0;
        uint32_t m_vertex_count = 0;
    };
} // namespace NexAur

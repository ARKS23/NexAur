#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    struct RenderDebugDrawData;

    class VulkanDebugDrawBuffer {
    public:
        VulkanDebugDrawBuffer() = default;
        ~VulkanDebugDrawBuffer();

        VulkanDebugDrawBuffer(const VulkanDebugDrawBuffer&) = delete;
        VulkanDebugDrawBuffer& operator=(const VulkanDebugDrawBuffer&) = delete;

        bool init(const VulkanResourceContext& context);
        void shutdown();

        bool upload(const RenderDebugDrawData& debug_draw);

        VkBuffer getVertexBuffer() const { return m_vertex_buffer.get(); }
        uint32_t getVertexCount() const { return m_vertex_count; }
        bool hasVertices() const { return m_vertex_count > 0 && m_vertex_buffer.isReady(); }

    private:
        bool ensureCapacity(VkDeviceSize required_size);
        bool createBuffer(VkDeviceSize size);
        void cleanupBuffer();

    private:
        const VulkanGpuAllocator* m_gpu_allocator = nullptr;
        VulkanOwnedBuffer m_vertex_buffer;
        VkDeviceSize m_capacity = 0;
        uint32_t m_vertex_count = 0;
    };
} // namespace NexAur

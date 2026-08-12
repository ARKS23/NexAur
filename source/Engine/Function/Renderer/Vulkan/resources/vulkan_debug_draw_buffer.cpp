#include "pch.h"
#include "vulkan_debug_draw_buffer.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/data/render_debug_draw.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_debug_draw_types.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace NexAur {
    VulkanDebugDrawBuffer::~VulkanDebugDrawBuffer() {
        shutdown();
    }

    bool VulkanDebugDrawBuffer::init(const VulkanResourceContext& context) {
        shutdown();

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized()) {
            NX_CORE_ERROR("VulkanDebugDrawBuffer requires a valid Vulkan context.");
            return false;
        }

        m_gpu_allocator = context.gpu_allocator;
        return true;
    }

    void VulkanDebugDrawBuffer::shutdown() {
        cleanupBuffer();
        m_gpu_allocator = nullptr;
        m_vertex_count = 0;
    }

    bool VulkanDebugDrawBuffer::upload(const RenderDebugDrawData& debug_draw) {
        m_vertex_count = 0;
        if (debug_draw.lines.empty()) {
            return true;
        }

        std::vector<VulkanDebugDrawVertex> vertices;
        vertices.reserve(debug_draw.lines.size() * 2);
        for (const RenderDebugLine& line : debug_draw.lines) {
            if (!line.depth_test) {
                continue;
            }

            vertices.push_back({ line.start, line.color });
            vertices.push_back({ line.end, line.color });
        }

        if (vertices.empty()) {
            return true;
        }

        const VkDeviceSize required_size = static_cast<VkDeviceSize>(vertices.size() * sizeof(VulkanDebugDrawVertex));
        if (!ensureCapacity(required_size)) {
            return false;
        }

        void* mapped_data = nullptr;
        if (!m_vertex_buffer.map(mapped_data)) {
            return false;
        }

        std::memcpy(mapped_data, vertices.data(), static_cast<size_t>(required_size));
        const bool flushed = m_vertex_buffer.isHostCoherent() || m_vertex_buffer.flush(0, required_size);
        m_vertex_buffer.unmap();
        if (!flushed) {
            return false;
        }

        m_vertex_count = static_cast<uint32_t>(vertices.size());
        return true;
    }

    bool VulkanDebugDrawBuffer::ensureCapacity(VkDeviceSize required_size) {
        if (required_size == 0) {
            return true;
        }

        if (m_vertex_buffer.isReady() && required_size <= m_capacity) {
            return true;
        }

        VkDeviceSize new_capacity = std::max<VkDeviceSize>(required_size, 4096);
        if (m_capacity > 0) {
            new_capacity = std::max<VkDeviceSize>(new_capacity, m_capacity * 2);
        }

        cleanupBuffer();
        return createBuffer(new_capacity);
    }

    bool VulkanDebugDrawBuffer::createBuffer(VkDeviceSize size) {
        if (!m_vertex_buffer.create(
                *m_gpu_allocator,
                size,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                "VulkanDebugDrawBuffer vertex buffer")) {
            return false;
        }
        m_capacity = size;
        return true;
    }

    void VulkanDebugDrawBuffer::cleanupBuffer() {
        m_vertex_buffer.reset();

        m_capacity = 0;
        m_vertex_count = 0;
    }

} // namespace NexAur

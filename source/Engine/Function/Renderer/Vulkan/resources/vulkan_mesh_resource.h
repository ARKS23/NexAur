#pragma once

#include <cstdint>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class Mesh;

    class NEXAUR_API VulkanMeshResource {
    public:
        VulkanMeshResource() = default;
        ~VulkanMeshResource();

        VulkanMeshResource(const VulkanMeshResource&) = delete;
        VulkanMeshResource& operator=(const VulkanMeshResource&) = delete;

        VulkanMeshResource(VulkanMeshResource&& other) noexcept;
        VulkanMeshResource& operator=(VulkanMeshResource&& other) noexcept;

        bool create(const VulkanResourceUploadContext& context, const Mesh& mesh);
        void reset();

        bool isReady() const {
            return m_vertex_buffer != VK_NULL_HANDLE &&
                   m_index_buffer != VK_NULL_HANDLE &&
                   m_index_count > 0;
        }

        VkBuffer getVertexBuffer() const { return m_vertex_buffer; }
        VkBuffer getIndexBuffer() const { return m_index_buffer; }
        uint32_t getVertexCount() const { return m_vertex_count; }
        uint32_t getIndexCount() const { return m_index_count; }

    private:
        void moveFrom(VulkanMeshResource&& other) noexcept;

    private:
        VmaAllocator m_allocator = VK_NULL_HANDLE;
        VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
        VmaAllocation m_vertex_allocation = VK_NULL_HANDLE;
        VkBuffer m_index_buffer = VK_NULL_HANDLE;
        VmaAllocation m_index_allocation = VK_NULL_HANDLE;
        uint32_t m_vertex_count = 0;
        uint32_t m_index_count = 0;
    };
} // namespace NexAur

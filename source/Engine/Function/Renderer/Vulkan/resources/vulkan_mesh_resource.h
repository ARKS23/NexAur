#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/upload/vulkan_upload_manager.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class Mesh;

    class VulkanMeshResource {
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
            return m_upload_ticket.isReady() &&
                   m_vertex_buffer.isReady() &&
                   m_index_buffer.isReady() &&
                   m_index_count > 0;
        }

        VulkanUploadStatus getUploadStatus() const {
            return m_upload_ticket.getStatus();
        }
        VkBuffer getVertexBuffer() const { return m_vertex_buffer.get(); }
        VkBuffer getIndexBuffer() const { return m_index_buffer.get(); }
        uint32_t getVertexCount() const { return m_vertex_count; }
        uint32_t getIndexCount() const { return m_index_count; }

    private:
        void moveFrom(VulkanMeshResource&& other) noexcept;

    private:
        VulkanOwnedBuffer m_vertex_buffer;
        VulkanOwnedBuffer m_index_buffer;
        VulkanUploadTicket m_upload_ticket;
        uint32_t m_vertex_count = 0;
        uint32_t m_index_count = 0;
    };
} // namespace NexAur

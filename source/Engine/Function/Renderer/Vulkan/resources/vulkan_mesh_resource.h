#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/upload/vulkan_upload_manager.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class Mesh;

    struct VulkanMeshResourceIdentity {
        AssetHandle model_asset;
        uint32_t mesh_index = 0;

        bool valid() const { return static_cast<bool>(model_asset); }
        bool operator==(const VulkanMeshResourceIdentity& other) const {
            return model_asset == other.model_asset &&
                   mesh_index == other.mesh_index;
        }
    };

    struct VulkanMeshResourceIdentityHash {
        size_t operator()(const VulkanMeshResourceIdentity& identity) const {
            const size_t asset_hash = std::hash<AssetHandle>{}(identity.model_asset);
            const size_t mesh_hash = std::hash<uint32_t>{}(identity.mesh_index);
            return asset_hash ^
                   (mesh_hash + static_cast<size_t>(0x9e3779b9u) +
                    (asset_hash << 6u) + (asset_hash >> 2u));
        }
    };

    struct VulkanMeshResourceKey {
        VulkanMeshResourceIdentity identity;
        uint64_t generation = 0;

        bool valid() const {
            return identity.valid() && generation > 0;
        }
        bool operator==(const VulkanMeshResourceKey& other) const {
            return identity == other.identity && generation == other.generation;
        }
    };

    class VulkanMeshResource {
    public:
        VulkanMeshResource() = default;
        ~VulkanMeshResource();

        VulkanMeshResource(const VulkanMeshResource&) = delete;
        VulkanMeshResource& operator=(const VulkanMeshResource&) = delete;

        VulkanMeshResource(VulkanMeshResource&& other) noexcept;
        VulkanMeshResource& operator=(VulkanMeshResource&& other) noexcept;

        bool create(
            const VulkanResourceUploadContext& context,
            const Mesh& mesh,
            const VulkanMeshResourceKey& key);
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
        VkBufferUsageFlags getVertexBufferUsage() const { return m_vertex_buffer.getUsage(); }
        VkBufferUsageFlags getIndexBufferUsage() const { return m_index_buffer.getUsage(); }
        VkDeviceAddress getVertexBufferDeviceAddress() const {
            return m_vertex_buffer.getDeviceAddress();
        }
        VkDeviceAddress getIndexBufferDeviceAddress() const {
            return m_index_buffer.getDeviceAddress();
        }
        bool hasDeviceAddressBuffers() const {
            return m_vertex_buffer.isDeviceAddressable() &&
                   m_index_buffer.isDeviceAddressable();
        }
        uint32_t getVertexCount() const { return m_vertex_count; }
        uint32_t getIndexCount() const { return m_index_count; }
        const VulkanMeshResourceKey& getKey() const { return m_key; }

    private:
        void moveFrom(VulkanMeshResource&& other) noexcept;

    private:
        VulkanOwnedBuffer m_vertex_buffer;
        VulkanOwnedBuffer m_index_buffer;
        VulkanUploadTicket m_upload_ticket;
        uint32_t m_vertex_count = 0;
        uint32_t m_index_count = 0;
        VulkanMeshResourceKey m_key;
    };
} // namespace NexAur

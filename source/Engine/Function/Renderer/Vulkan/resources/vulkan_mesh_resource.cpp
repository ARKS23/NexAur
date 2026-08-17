#include "pch.h"
#include "vulkan_mesh_resource.h"

#include "Function/Resource/mesh.h"
#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_state_planner.h"

#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace NexAur {
    namespace {
        void recordMeshUpload(
            VkCommandBuffer command_buffer,
            VkBuffer staging_buffer,
            VkDeviceSize staging_offset,
            VkBuffer vertex_buffer,
            VkDeviceSize vertex_bytes,
            VkBuffer index_buffer,
            VkDeviceSize index_bytes,
            bool acceleration_structure_build_input) {
            VkBufferCopy vertex_copy{};
            vertex_copy.srcOffset = staging_offset;
            vertex_copy.size = vertex_bytes;
            vkCmdCopyBuffer(
                command_buffer,
                staging_buffer,
                vertex_buffer,
                1,
                &vertex_copy);

            VkBufferCopy index_copy{};
            index_copy.srcOffset = staging_offset + vertex_bytes;
            index_copy.size = index_bytes;
            vkCmdCopyBuffer(
                command_buffer,
                staging_buffer,
                index_buffer,
                1,
                &index_copy);

            const VulkanGraphBufferState transfer_write =
                VulkanGraphStatePlanner::stateForBufferImport(
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VulkanGraphAccessType::Write);
            const VkPipelineStageFlags2 build_stage = acceleration_structure_build_input ?
                VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR :
                VK_PIPELINE_STAGE_2_NONE;
            const VkAccessFlags2 build_access = acceleration_structure_build_input ?
                VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR :
                VK_ACCESS_2_NONE;
            const VulkanGraphBufferState vertex_read =
                VulkanGraphStatePlanner::stateForBufferImport(
                    VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | build_stage,
                    VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | build_access,
                    VulkanGraphAccessType::Read);
            const VulkanGraphBufferState index_read =
                VulkanGraphStatePlanner::stateForBufferImport(
                    VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | build_stage,
                    VK_ACCESS_2_INDEX_READ_BIT | build_access,
                    VulkanGraphAccessType::Read);
            const VulkanGraphBufferTransitionPlan vertex_transition =
                VulkanGraphStatePlanner::planBufferTransition(transfer_write, vertex_read);
            const VulkanGraphBufferTransitionPlan index_transition =
                VulkanGraphStatePlanner::planBufferTransition(transfer_write, index_read);

            VkBufferMemoryBarrier2 barriers[2]{};
            barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barriers[0].srcStageMask = vertex_transition.source.stage;
            barriers[0].srcAccessMask = vertex_transition.source.access;
            barriers[0].dstStageMask = vertex_transition.destination.stage;
            barriers[0].dstAccessMask = vertex_transition.destination.access;
            barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[0].buffer = vertex_buffer;
            barriers[0].size = VK_WHOLE_SIZE;

            barriers[1] = barriers[0];
            barriers[1].dstAccessMask = index_transition.destination.access;
            barriers[1].buffer = index_buffer;

            VkDependencyInfo dependency_info{};
            dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependency_info.bufferMemoryBarrierCount = 2;
            dependency_info.pBufferMemoryBarriers = barriers;
            vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        }
    } // namespace

    VulkanMeshResource::~VulkanMeshResource() {
        reset();
    }

    VulkanMeshResource::VulkanMeshResource(VulkanMeshResource&& other) noexcept {
        moveFrom(std::move(other));
    }

    VulkanMeshResource& VulkanMeshResource::operator=(VulkanMeshResource&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(std::move(other));
        }
        return *this;
    }

    bool VulkanMeshResource::create(
        const VulkanResourceUploadContext& context,
        const Mesh& mesh,
        const VulkanMeshResourceKey& key) {
        reset();

        static_assert(
            sizeof(unsigned int) == sizeof(uint32_t),
            "NexAur mesh indices must map to VK_INDEX_TYPE_UINT32.");

        if (!context.valid() ||
            !context.upload_manager->isInitialized() ||
            !key.valid()) {
            NX_CORE_ERROR(
                "VulkanMeshResource requires a valid async upload context and resource key.");
            return false;
        }

        const std::vector<Vertex>& vertices = mesh.GetVertices();
        const std::vector<unsigned int>& indices = mesh.GetIndices();
        if (vertices.empty() || indices.empty()) {
            NX_CORE_WARN("VulkanMeshResource skipped empty mesh.");
            return false;
        }
        if (vertices.size() > std::numeric_limits<uint32_t>::max() ||
            indices.size() > std::numeric_limits<uint32_t>::max()) {
            NX_CORE_ERROR("VulkanMeshResource mesh is too large.");
            return false;
        }

        const VkDeviceSize vertex_bytes =
            static_cast<VkDeviceSize>(vertices.size() * sizeof(Vertex));
        const VkDeviceSize index_bytes =
            static_cast<VkDeviceSize>(indices.size() * sizeof(unsigned int));
        const VkDeviceSize upload_bytes = vertex_bytes + index_bytes;
        const bool device_address_enabled =
            context.gpu_allocator->isBufferDeviceAddressEnabled();
        const VkBufferUsageFlags ray_tracing_usage = device_address_enabled ?
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT :
            0;

        VulkanOwnedBufferCreateInfo vertex_buffer_info;
        vertex_buffer_info.size = vertex_bytes;
        vertex_buffer_info.usage =
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            ray_tracing_usage;
        vertex_buffer_info.memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        vertex_buffer_info.debug_name = "Vulkan mesh vertex buffer";

        VulkanOwnedBufferCreateInfo index_buffer_info;
        index_buffer_info.size = index_bytes;
        index_buffer_info.usage =
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            ray_tracing_usage;
        index_buffer_info.memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        index_buffer_info.debug_name = "Vulkan mesh index buffer";

        if (!m_vertex_buffer.create(*context.gpu_allocator, vertex_buffer_info) ||
            !m_index_buffer.create(*context.gpu_allocator, index_buffer_info)) {
            reset();
            return false;
        }

        std::vector<uint8_t> upload_data(static_cast<size_t>(upload_bytes));
        std::memcpy(upload_data.data(), vertices.data(), static_cast<size_t>(vertex_bytes));
        std::memcpy(
            upload_data.data() + vertex_bytes,
            indices.data(),
            static_cast<size_t>(index_bytes));

        const VkBuffer vertex_buffer = m_vertex_buffer.get();
        const VkBuffer index_buffer = m_index_buffer.get();
        m_upload_ticket = context.upload_manager->enqueue(
            std::move(upload_data),
            "Vulkan mesh upload",
            [
                vertex_buffer,
                vertex_bytes,
                index_buffer,
                index_bytes,
                device_address_enabled
            ](
                VkCommandBuffer command_buffer,
                VkBuffer staging_buffer,
                VkDeviceSize staging_offset) {
                recordMeshUpload(
                    command_buffer,
                    staging_buffer,
                    staging_offset,
                    vertex_buffer,
                    vertex_bytes,
                    index_buffer,
                    index_bytes,
                    device_address_enabled);
            });
        if (m_upload_ticket.getStatus() == VulkanUploadStatus::Failed) {
            reset();
            return false;
        }

        m_vertex_count = static_cast<uint32_t>(vertices.size());
        m_index_count = static_cast<uint32_t>(indices.size());
        m_key = key;
        return true;
    }

    void VulkanMeshResource::reset() {
        m_upload_ticket.cancel();
        m_upload_ticket = {};
        m_index_buffer.reset();
        m_vertex_buffer.reset();
        m_vertex_count = 0;
        m_index_count = 0;
        m_key = {};
    }

    void VulkanMeshResource::moveFrom(VulkanMeshResource&& other) noexcept {
        m_vertex_buffer = std::move(other.m_vertex_buffer);
        m_index_buffer = std::move(other.m_index_buffer);
        m_upload_ticket = std::move(other.m_upload_ticket);
        m_vertex_count = other.m_vertex_count;
        m_index_count = other.m_index_count;
        m_key = other.m_key;

        other.m_upload_ticket = {};
        other.m_vertex_count = 0;
        other.m_index_count = 0;
        other.m_key = {};
    }
} // namespace NexAur

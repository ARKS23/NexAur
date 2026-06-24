#include "pch.h"
#include "vulkan_mesh_resource.h"

#include "Function/Resource/mesh.h"

#include <cstring>
#include <limits>
#include <utility>

namespace NexAur {
    namespace {
        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

        bool createBuffer(
            VmaAllocator allocator,
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VmaMemoryUsage memory_usage,
            VmaAllocationCreateFlags allocation_flags,
            VkBuffer& buffer,
            VmaAllocation& allocation,
            VmaAllocationInfo* allocation_info_output = nullptr) {
            if (allocator == VK_NULL_HANDLE || size == 0) {
                return false;
            }

            VkBufferCreateInfo buffer_info{};
            buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_info.size = size;
            buffer_info.usage = usage;
            buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocation_info{};
            allocation_info.usage = memory_usage;
            allocation_info.flags = allocation_flags;

            VmaAllocationInfo created_allocation_info{};
            const VkResult result = vmaCreateBuffer(
                allocator,
                &buffer_info,
                &allocation_info,
                &buffer,
                &allocation,
                &created_allocation_info);
            if (result != VK_SUCCESS) {
                NX_CORE_ERROR("Failed to create Vulkan buffer: {}", static_cast<int>(result));
                buffer = VK_NULL_HANDLE;
                allocation = VK_NULL_HANDLE;
                return false;
            }

            if (allocation_info_output) {
                *allocation_info_output = created_allocation_info;
            }

            return true;
        }

        void destroyBuffer(VmaAllocator allocator, VkBuffer& buffer, VmaAllocation& allocation) {
            if (allocator != VK_NULL_HANDLE && buffer != VK_NULL_HANDLE) {
                vmaDestroyBuffer(allocator, buffer, allocation);
            }

            buffer = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }

        bool copyBuffer(
            const VulkanResourceUploadContext& context,
            VkBuffer source,
            VkBuffer destination,
            VkDeviceSize size) {
            VkCommandBuffer command_buffer = VK_NULL_HANDLE;
            VkFence upload_fence = VK_NULL_HANDLE;

            auto cleanup = [&]() {
                if (upload_fence != VK_NULL_HANDLE) {
                    vkDestroyFence(context.device, upload_fence, nullptr);
                }
                if (command_buffer != VK_NULL_HANDLE) {
                    vkFreeCommandBuffers(context.device, context.command_pool, 1, &command_buffer);
                }
            };

            VkCommandBufferAllocateInfo allocate_info{};
            allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocate_info.commandPool = context.command_pool;
            allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocate_info.commandBufferCount = 1;
            if (!checkVk(vkAllocateCommandBuffers(context.device, &allocate_info, &command_buffer), "vkAllocateCommandBuffers(upload)")) {
                cleanup();
                return false;
            }

            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (!checkVk(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer(upload)")) {
                cleanup();
                return false;
            }

            VkBufferCopy copy_region{};
            copy_region.size = size;
            vkCmdCopyBuffer(command_buffer, source, destination, 1, &copy_region);

            if (!checkVk(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer(upload)")) {
                cleanup();
                return false;
            }

            VkFenceCreateInfo fence_info{};
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            if (!checkVk(vkCreateFence(context.device, &fence_info, nullptr, &upload_fence), "vkCreateFence(upload)")) {
                cleanup();
                return false;
            }

            VkSubmitInfo submit_info{};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &command_buffer;
            if (!checkVk(vkQueueSubmit(context.graphics_queue, 1, &submit_info, upload_fence), "vkQueueSubmit(upload)")) {
                cleanup();
                return false;
            }

            if (!checkVk(vkWaitForFences(context.device, 1, &upload_fence, VK_TRUE, UINT64_MAX), "vkWaitForFences(upload)")) {
                cleanup();
                return false;
            }

            cleanup();
            return true;
        }

        bool createDeviceBufferFromData(
            const VulkanResourceUploadContext& context,
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            const void* source_data,
            VkBuffer& buffer,
            VmaAllocation& allocation) {
            if (!context.valid() || size == 0 || !source_data) {
                return false;
            }

            VkBuffer staging_buffer = VK_NULL_HANDLE;
            VmaAllocation staging_allocation = VK_NULL_HANDLE;
            VmaAllocationInfo staging_allocation_info{};
            if (!createBuffer(
                    context.allocator,
                    size,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VMA_MEMORY_USAGE_AUTO,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT,
                    staging_buffer,
                    staging_allocation,
                    &staging_allocation_info)) {
                return false;
            }

            if (!staging_allocation_info.pMappedData) {
                NX_CORE_ERROR("Failed to map Vulkan staging buffer allocation.");
                destroyBuffer(context.allocator, staging_buffer, staging_allocation);
                return false;
            }

            std::memcpy(staging_allocation_info.pMappedData, source_data, static_cast<size_t>(size));
            vmaFlushAllocation(context.allocator, staging_allocation, 0, VK_WHOLE_SIZE);

            if (!createBuffer(
                    context.allocator,
                    size,
                    usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                    0,
                    buffer,
                    allocation)) {
                destroyBuffer(context.allocator, staging_buffer, staging_allocation);
                return false;
            }

            if (!copyBuffer(context, staging_buffer, buffer, size)) {
                destroyBuffer(context.allocator, buffer, allocation);
                destroyBuffer(context.allocator, staging_buffer, staging_allocation);
                return false;
            }

            destroyBuffer(context.allocator, staging_buffer, staging_allocation);
            return true;
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

    bool VulkanMeshResource::create(const VulkanResourceUploadContext& context, const Mesh& mesh) {
        reset();

        static_assert(sizeof(unsigned int) == sizeof(uint32_t), "NexAur mesh indices must map to VK_INDEX_TYPE_UINT32.");

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanMeshResource requires a valid upload context.");
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

        m_allocator = context.allocator;
        m_vertex_count = static_cast<uint32_t>(vertices.size());
        m_index_count = static_cast<uint32_t>(indices.size());

        const VkDeviceSize vertex_buffer_size = static_cast<VkDeviceSize>(vertices.size() * sizeof(Vertex));
        const VkDeviceSize index_buffer_size = static_cast<VkDeviceSize>(indices.size() * sizeof(unsigned int));

        if (!createDeviceBufferFromData(
                context,
                vertex_buffer_size,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                vertices.data(),
                m_vertex_buffer,
                m_vertex_allocation) ||
            !createDeviceBufferFromData(
                context,
                index_buffer_size,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                indices.data(),
                m_index_buffer,
                m_index_allocation)) {
            reset();
            return false;
        }

        return true;
    }

    void VulkanMeshResource::reset() {
        if (m_allocator != VK_NULL_HANDLE) {
            if (m_index_buffer != VK_NULL_HANDLE) {
                vmaDestroyBuffer(m_allocator, m_index_buffer, m_index_allocation);
            }
            if (m_vertex_buffer != VK_NULL_HANDLE) {
                vmaDestroyBuffer(m_allocator, m_vertex_buffer, m_vertex_allocation);
            }
        }

        m_allocator = VK_NULL_HANDLE;
        m_vertex_buffer = VK_NULL_HANDLE;
        m_vertex_allocation = VK_NULL_HANDLE;
        m_index_buffer = VK_NULL_HANDLE;
        m_index_allocation = VK_NULL_HANDLE;
        m_vertex_count = 0;
        m_index_count = 0;
    }

    void VulkanMeshResource::moveFrom(VulkanMeshResource&& other) noexcept {
        m_allocator = other.m_allocator;
        m_vertex_buffer = other.m_vertex_buffer;
        m_vertex_allocation = other.m_vertex_allocation;
        m_index_buffer = other.m_index_buffer;
        m_index_allocation = other.m_index_allocation;
        m_vertex_count = other.m_vertex_count;
        m_index_count = other.m_index_count;

        other.m_allocator = VK_NULL_HANDLE;
        other.m_vertex_buffer = VK_NULL_HANDLE;
        other.m_vertex_allocation = VK_NULL_HANDLE;
        other.m_index_buffer = VK_NULL_HANDLE;
        other.m_index_allocation = VK_NULL_HANDLE;
        other.m_vertex_count = 0;
        other.m_index_count = 0;
    }
} // namespace NexAur

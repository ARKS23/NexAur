#include "pch.h"
#include "vulkan_debug_draw_buffer.h"

#include "Function/Renderer/data/render_debug_draw.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_debug_draw_types.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace NexAur {
    namespace {
        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }
    } // namespace

    VulkanDebugDrawBuffer::~VulkanDebugDrawBuffer() {
        shutdown();
    }

    bool VulkanDebugDrawBuffer::init(const VulkanResourceContext& context) {
        shutdown();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanDebugDrawBuffer requires a valid Vulkan context.");
            return false;
        }

        m_physical_device = context.physical_device;
        m_device = context.device;
        return true;
    }

    void VulkanDebugDrawBuffer::shutdown() {
        cleanupBuffer();
        m_physical_device = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
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
        if (!checkVk(vkMapMemory(m_device, m_vertex_memory, 0, required_size, 0, &mapped_data), "vkMapMemory(debug draw buffer)")) {
            return false;
        }

        std::memcpy(mapped_data, vertices.data(), static_cast<size_t>(required_size));
        vkUnmapMemory(m_device, m_vertex_memory);

        m_vertex_count = static_cast<uint32_t>(vertices.size());
        return true;
    }

    bool VulkanDebugDrawBuffer::ensureCapacity(VkDeviceSize required_size) {
        if (required_size == 0) {
            return true;
        }

        if (m_vertex_buffer != VK_NULL_HANDLE && required_size <= m_capacity) {
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
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (!checkVk(vkCreateBuffer(m_device, &buffer_info, nullptr, &m_vertex_buffer), "vkCreateBuffer(debug draw vertex buffer)")) {
            return false;
        }

        VkMemoryRequirements memory_requirements{};
        vkGetBufferMemoryRequirements(m_device, m_vertex_buffer, &memory_requirements);

        const uint32_t memory_type = findMemoryType(
            memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (memory_type == UINT32_MAX) {
            NX_CORE_ERROR("VulkanDebugDrawBuffer failed to find host-visible buffer memory.");
            return false;
        }

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;

        if (!checkVk(vkAllocateMemory(m_device, &allocate_info, nullptr, &m_vertex_memory), "vkAllocateMemory(debug draw vertex buffer)") ||
            !checkVk(vkBindBufferMemory(m_device, m_vertex_buffer, m_vertex_memory, 0), "vkBindBufferMemory(debug draw vertex buffer)")) {
            return false;
        }

        m_capacity = size;
        return true;
    }

    void VulkanDebugDrawBuffer::cleanupBuffer() {
        if (m_device != VK_NULL_HANDLE && m_vertex_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_vertex_buffer, nullptr);
            m_vertex_buffer = VK_NULL_HANDLE;
        }
        if (m_device != VK_NULL_HANDLE && m_vertex_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_vertex_memory, nullptr);
            m_vertex_memory = VK_NULL_HANDLE;
        }

        m_capacity = 0;
        m_vertex_count = 0;
    }

    uint32_t VulkanDebugDrawBuffer::findMemoryType(
        uint32_t type_filter,
        VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memory_properties{};
        vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memory_properties);

        for (uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
            const bool type_matches = (type_filter & (1u << index)) != 0;
            const bool properties_match = (memory_properties.memoryTypes[index].propertyFlags & properties) == properties;
            if (type_matches && properties_match) {
                return index;
            }
        }

        return UINT32_MAX;
    }
} // namespace NexAur

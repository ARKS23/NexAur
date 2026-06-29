#pragma once

#include <cstdint>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/data/render_settings.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanDescriptorLayoutCache;
    struct VulkanDrawList;

    class NEXAUR_API VulkanFrameLightingResource {
    public:
        VulkanFrameLightingResource() = default;
        ~VulkanFrameLightingResource();

        VulkanFrameLightingResource(const VulkanFrameLightingResource&) = delete;
        VulkanFrameLightingResource& operator=(const VulkanFrameLightingResource&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VulkanDescriptorLayoutCache& descriptor_layout_cache,
            VulkanDescriptorAllocator& descriptor_allocator);
        void shutdown();

        bool update(
            const VulkanDrawList& draw_list,
            const glm::mat4& shadow_light_view_projection,
            float shadow_map_size,
            const RenderSettings& render_settings);
        bool updateShadowMap(VkImageView shadow_map_view, VkSampler shadow_sampler);

        bool isReady() const { return m_ready; }
        VkDescriptorSet getDescriptorSet() const { return m_descriptor_set; }

    private:
        struct Buffer {
            VkBuffer buffer = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkDeviceSize size = 0;

            bool valid() const {
                return buffer != VK_NULL_HANDLE && memory != VK_NULL_HANDLE && size > 0;
            }
        };

        bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, Buffer& buffer) const;
        void destroyBuffer(Buffer& buffer);
        bool writeBuffer(const Buffer& buffer, const void* data, VkDeviceSize size) const;
        bool updateDescriptorSet(VkDescriptorSetLayout layout);
        uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) const;

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* m_descriptor_allocator = nullptr;
        VulkanDescriptorSetAllocation m_descriptor_allocation;
        VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
        Buffer m_frame_buffer;
        Buffer m_point_light_buffer;
        bool m_ready = false;
    };
} // namespace NexAur

#pragma once

#include <cstdint>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/data/render_settings.h"
#include "Function/Renderer/data/render_shadow_cascade.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanDescriptorLayoutCache;
    struct VulkanDrawList;

    class VulkanFrameLightingResource {
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
            const RenderShadowCascadeFrame& shadow_frame,
            const RenderPointShadowFrame& point_shadow_frame,
            const RenderRectShadowFrame& rect_shadow_frame,
            float shadow_map_size,
            float point_shadow_map_size,
            float rect_shadow_map_size,
            const RenderSettings& render_settings);
        bool updateShadowMap(VkImageView shadow_map_view, VkSampler shadow_sampler);
        bool updatePointShadowMap(VkImageView shadow_map_view, VkSampler shadow_sampler);
        bool updateRectShadowMap(VkImageView shadow_map_view, VkSampler shadow_sampler);

        bool isReady() const { return m_ready; }
        VkDescriptorSet getDescriptorSet() const { return m_descriptor_set; }

    private:
        bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VulkanOwnedBuffer& buffer) const;
        bool writeBuffer(const VulkanOwnedBuffer& buffer, const void* data, VkDeviceSize size) const;
        bool updateDescriptorSet(VkDescriptorSetLayout layout);

    private:
        const VulkanGpuAllocator* m_gpu_allocator = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;
        VulkanDescriptorAllocator* m_descriptor_allocator = nullptr;
        VulkanDescriptorSetAllocation m_descriptor_allocation;
        VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
        VulkanOwnedBuffer m_frame_buffer;
        VulkanOwnedBuffer m_point_light_buffer;
        VulkanOwnedBuffer m_rect_light_buffer;
        bool m_ready = false;
    };
} // namespace NexAur

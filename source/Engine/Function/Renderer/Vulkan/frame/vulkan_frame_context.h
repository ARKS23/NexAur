#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_flight_tracker.h"
#include "Function/Renderer/Vulkan/resources/vulkan_debug_draw_buffer.h"
#include "Function/Renderer/Vulkan/resources/vulkan_frame_lighting_resource.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanDescriptorAllocator;
    class VulkanDescriptorLayoutCache;

    class VulkanFrameContext final {
    public:
        VulkanFrameContext() = default;
        ~VulkanFrameContext();

        VulkanFrameContext(const VulkanFrameContext&) = delete;
        VulkanFrameContext& operator=(const VulkanFrameContext&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VulkanDescriptorLayoutCache& descriptor_layout_cache,
            VulkanDescriptorAllocator& descriptor_allocator,
            uint32_t frame_index);
        void shutdown();

        void markSubmitted(uint64_t serial);
        void markCompleted();

        bool isReady() const { return m_ready; }
        bool isInFlight() const { return m_flight_state.isInFlight(); }
        uint32_t getFrameIndex() const { return m_frame_index; }
        uint64_t getSubmissionSerial() const {
            return m_flight_state.getSubmissionSerial();
        }

        VkCommandPool getCommandPool() const { return m_command_pool; }
        VkCommandBuffer getCommandBuffer() const { return m_command_buffer; }
        VkSemaphore getImageAvailableSemaphore() const { return m_image_available; }
        VkFence getFence() const { return m_fence; }

        VulkanFrameLightingResource& getLightingResource() { return m_lighting_resource; }
        const VulkanFrameLightingResource& getLightingResource() const { return m_lighting_resource; }
        VulkanDebugDrawBuffer& getDebugDrawBuffer() { return m_debug_draw_buffer; }
        const VulkanDebugDrawBuffer& getDebugDrawBuffer() const { return m_debug_draw_buffer; }

    private:
        bool createCommandResources(uint32_t queue_family_index);
        bool createSyncObjects();

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkCommandPool m_command_pool = VK_NULL_HANDLE;
        VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;
        VkSemaphore m_image_available = VK_NULL_HANDLE;
        VkFence m_fence = VK_NULL_HANDLE;
        VulkanFrameLightingResource m_lighting_resource;
        VulkanDebugDrawBuffer m_debug_draw_buffer;
        VulkanFrameSlotState m_flight_state;
        uint32_t m_frame_index = 0;
        bool m_ready = false;
    };
} // namespace NexAur

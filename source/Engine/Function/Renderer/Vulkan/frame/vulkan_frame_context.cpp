#include "pch.h"
#include "vulkan_frame_context.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"

namespace NexAur {
    VulkanFrameContext::~VulkanFrameContext() {
        shutdown();
    }

    bool VulkanFrameContext::init(
        const VulkanResourceContext& context,
        VulkanDescriptorLayoutCache& descriptor_layout_cache,
        VulkanDescriptorAllocator& descriptor_allocator,
        uint32_t frame_index,
        VkDescriptorSetLayout ray_tracing_scene_descriptor_set_layout) {
        shutdown();
        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized()) {
            NX_CORE_ERROR("VulkanFrameContext requires a valid Vulkan context.");
            return false;
        }

        m_device = context.device;
        m_frame_index = frame_index;
        if (!createCommandResources(context.graphics_queue_family) ||
            !createSyncObjects() ||
            !m_lighting_resource.init(
                context,
                descriptor_layout_cache,
                descriptor_allocator) ||
            (ray_tracing_scene_descriptor_set_layout != VK_NULL_HANDLE &&
             !m_ray_tracing_scene_resource.init(
                 context.device,
                 descriptor_allocator,
                 ray_tracing_scene_descriptor_set_layout)) ||
            !m_debug_draw_buffer.init(context)) {
            shutdown();
            return false;
        }

        m_ready = true;
        return true;
    }

    void VulkanFrameContext::shutdown() {
        m_debug_draw_buffer.shutdown();
        m_ray_tracing_scene_resource.shutdown();
        m_lighting_resource.shutdown();

        if (m_device != VK_NULL_HANDLE) {
            if (m_fence != VK_NULL_HANDLE) {
                vkDestroyFence(m_device, m_fence, nullptr);
            }
            if (m_image_available != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device, m_image_available, nullptr);
            }
            if (m_command_pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(m_device, m_command_pool, nullptr);
            }
        }

        m_device = VK_NULL_HANDLE;
        m_command_pool = VK_NULL_HANDLE;
        m_command_buffer = VK_NULL_HANDLE;
        m_image_available = VK_NULL_HANDLE;
        m_fence = VK_NULL_HANDLE;
        m_frame_index = 0;
        m_flight_state.reset();
        m_ready = false;
    }

    void VulkanFrameContext::markSubmitted(uint64_t serial) {
        m_flight_state.markSubmitted(serial);
    }

    void VulkanFrameContext::markCompleted() {
        m_flight_state.markCompleted();
    }

    bool VulkanFrameContext::createCommandResources(uint32_t queue_family_index) {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags =
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = queue_family_index;
        if (!VulkanDiagnosticsCollector::checkVk(
                vkCreateCommandPool(
                    m_device,
                    &pool_info,
                    nullptr,
                    &m_command_pool),
                "vkCreateCommandPool(frame context)")) {
            return false;
        }

        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = m_command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;
        return VulkanDiagnosticsCollector::checkVk(
            vkAllocateCommandBuffers(
                m_device,
                &allocate_info,
                &m_command_buffer),
            "vkAllocateCommandBuffers(frame context)");
    }

    bool VulkanFrameContext::createSyncObjects() {
        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (!VulkanDiagnosticsCollector::checkVk(
                vkCreateSemaphore(
                    m_device,
                    &semaphore_info,
                    nullptr,
                    &m_image_available),
                "vkCreateSemaphore(frame image available)")) {
            return false;
        }

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        return VulkanDiagnosticsCollector::checkVk(
            vkCreateFence(m_device, &fence_info, nullptr, &m_fence),
            "vkCreateFence(frame context)");
    }
} // namespace NexAur

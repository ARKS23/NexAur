#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Function/Renderer/renderer_debug_service.h"
#include "Function/Renderer/renderer_service_types.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"
#include "Function/Renderer/Vulkan/targets/vulkan_picking_target.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanPassGraph;

    class VulkanPickingManager final {
    public:
        VulkanPickingManager() = default;
        ~VulkanPickingManager();

        VulkanPickingManager(const VulkanPickingManager&) = delete;
        VulkanPickingManager& operator=(const VulkanPickingManager&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VkCommandPool command_pool,
            uint32_t width,
            uint32_t height);
        bool resize(uint32_t width, uint32_t height);
        void shutdown();

        bool isReady() const { return m_target.isReady(); }
        VkFormat getObjectIdFormat() const { return m_target.getObjectIdFormat(); }
        VkFormat getDepthFormat() const { return m_target.getDepthFormat(); }
        VulkanRenderTarget getRenderTarget() const { return m_target.getRenderTarget(); }

        VulkanGraphImageHandle addObjectIdImage(VulkanPassGraph& graph);
        VulkanGraphImageHandle addDepthImage(VulkanPassGraph& graph);

        void beginFrameRecording();
        void markPassRecorded();
        void onFrameSubmitted();

        ViewportPickResult pickViewport(const ViewportPickRequest& request);
        RendererDebugPickingTargetStats buildDebugStats() const;

    private:
        bool readPixel(uint32_t x, uint32_t y, int32_t& entity_id);

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkQueue m_graphics_queue = VK_NULL_HANDLE;
        VkCommandPool m_command_pool = VK_NULL_HANDLE;
        VulkanPickingTarget m_target;
        bool m_frame_ready = false;
        bool m_recorded_this_frame = false;
    };
} // namespace NexAur

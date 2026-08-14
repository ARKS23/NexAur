#pragma once

#include <array>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/features/vulkan_render_feature_context.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_constants.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"
#include "Function/Renderer/Vulkan/passes/vulkan_ssr_pass.h"
#include "Function/Renderer/Vulkan/targets/vulkan_ssr_target.h"
#include "Function/Renderer/renderer_debug_service.h"

namespace NexAur {
    class VulkanPassGraph;

    struct VulkanSsrFeatureGraphResources {
        VulkanGraphImageHandle raw_reflection;
        VulkanGraphImageHandle hit_mask;

        bool valid() const { return raw_reflection.valid() && hit_mask.valid(); }
    };

    struct VulkanSsrFeatureInput {
        VkImageView raw_reflection_view = VK_NULL_HANDLE;
        VkImageView hit_mask_view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        bool valid() const {
            return raw_reflection_view != VK_NULL_HANDLE &&
                   hit_mask_view != VK_NULL_HANDLE &&
                   sampler != VK_NULL_HANDLE &&
                   layout != VK_IMAGE_LAYOUT_UNDEFINED;
        }
    };

    class VulkanSsrFeature final {
    public:
        VulkanSsrFeature() = default;
        ~VulkanSsrFeature();

        VulkanSsrFeature(const VulkanSsrFeature&) = delete;
        VulkanSsrFeature& operator=(const VulkanSsrFeature&) = delete;

        bool init(
            const VulkanRenderFeatureContext& context,
            VkFormat reflection_format,
            VkFormat hit_mask_format,
            uint32_t width,
            uint32_t height);
        void shutdown();
        void cleanupSwapchainResources();
        bool recreateSwapchainResources();
        bool resize(uint32_t width, uint32_t height);

        bool isReady() const;
        VulkanSsrFeatureGraphResources addGraphResources(VulkanPassGraph& graph);
        bool addPasses(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle scene_color,
            VulkanGraphImageHandle scene_depth,
            const VulkanSsrFeatureGraphResources& resources,
            const VulkanFeatureImageInput& scene_color_input,
            VkImageView scene_depth_view,
            const VulkanRenderView& view,
            const RenderSsrSettings& settings,
            uint32_t frame_index);

        VulkanSsrFeatureInput getPostProcessInput() const;
        RendererDebugSsrStats buildDebugStats(
            const RenderSsrSettings& settings,
            bool enabled) const;

    private:
        VulkanSsrPass& getPass(uint32_t frame_index);

    private:
        VulkanRenderFeatureContext m_context;
        std::array<VulkanSsrPass, kVulkanFramesInFlight> m_passes;
        VulkanSsrTarget m_target;
    };
} // namespace NexAur

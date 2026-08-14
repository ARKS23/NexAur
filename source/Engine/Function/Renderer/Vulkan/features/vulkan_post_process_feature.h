#pragma once

#include <array>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/features/vulkan_render_feature_context.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_constants.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"
#include "Function/Renderer/Vulkan/passes/vulkan_post_process_pass.h"
#include "Function/Renderer/renderer_debug_service.h"

namespace NexAur {
    class VulkanPassGraph;

    class VulkanPostProcessFeature final {
    public:
        VulkanPostProcessFeature() = default;
        ~VulkanPostProcessFeature();

        VulkanPostProcessFeature(const VulkanPostProcessFeature&) = delete;
        VulkanPostProcessFeature& operator=(const VulkanPostProcessFeature&) = delete;

        bool init(const VulkanRenderFeatureContext& context, VkFormat output_format);
        void shutdown();
        void cleanupSwapchainResources();
        bool recreateSwapchainResources(VkFormat output_format);

        bool isReady() const;
        bool addPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle input_color,
            VulkanGraphImageHandle output_color,
            VulkanGraphImageHandle scene_depth,
            VulkanGraphImageHandle ao_raw,
            VulkanGraphImageHandle ao_blurred,
            VulkanGraphImageHandle ssr_raw_reflection,
            VulkanGraphImageHandle ssr_hit_mask,
            const VulkanPostProcessRenderTarget& target,
            const VulkanPostProcessInput& input,
            const RenderPostProcessSettings& post_process_settings,
            const RenderAoSettings& ao_settings,
            const RenderSsrSettings& ssr_settings,
            const RenderEffectDebugSettings& debug_settings,
            bool isolate_forward_debug,
            uint32_t frame_index);

        RendererDebugPostProcessStats buildDebugStats(
            const RenderPostProcessSettings& settings,
            bool enabled,
            bool bloom_enabled) const;

    private:
        VulkanPostProcessPass& getPass(uint32_t frame_index);

    private:
        VulkanRenderFeatureContext m_context;
        std::array<VulkanPostProcessPass, kVulkanFramesInFlight> m_passes;
    };
} // namespace NexAur

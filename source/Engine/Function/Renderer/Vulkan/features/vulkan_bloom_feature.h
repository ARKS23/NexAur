#pragma once

#include <vector>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/features/vulkan_render_feature_context.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"
#include "Function/Renderer/Vulkan/passes/vulkan_bloom_pass.h"
#include "Function/Renderer/Vulkan/targets/vulkan_bloom_target.h"
#include "Function/Renderer/renderer_debug_service.h"

namespace NexAur {
    class VulkanPassGraph;

    struct VulkanBloomFeatureOutput {
        VulkanGraphImageHandle color;
        VulkanFeatureImageInput input;
    };

    class VulkanBloomFeature final {
    public:
        VulkanBloomFeature() = default;
        ~VulkanBloomFeature();

        VulkanBloomFeature(const VulkanBloomFeature&) = delete;
        VulkanBloomFeature& operator=(const VulkanBloomFeature&) = delete;

        bool init(
            const VulkanRenderFeatureContext& context,
            VkFormat color_format,
            uint32_t width,
            uint32_t height,
            const VulkanFeatureImageInput& scene_color_input);
        void shutdown();
        void cleanupSwapchainResources();
        bool recreateSwapchainResources(const VulkanFeatureImageInput& scene_color_input);
        bool resize(
            uint32_t width,
            uint32_t height,
            const VulkanFeatureImageInput& scene_color_input);

        bool isReady() const { return m_target.isReady() && m_pass.isReady(); }
        bool addPasses(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle scene_color,
            const VulkanFeatureImageInput& scene_color_input,
            const RenderPostProcessSettings& settings,
            const RenderEffectDebugSettings& debug_settings,
            VulkanBloomFeatureOutput& output);

        RendererDebugBloomStats buildDebugStats(bool enabled) const;

    private:
        VulkanGraphImageHandle addImage(
            VulkanPassGraph& graph,
            uint32_t mip_index,
            bool upsample);
        VulkanGraphImageHandle addCompositeImage(VulkanPassGraph& graph);
        VulkanFeatureImageInput makeInput(const VulkanBloomImageView& image) const;
        bool updateInputs(const VulkanFeatureImageInput& scene_color_input);

    private:
        VulkanRenderFeatureContext m_context;
        VulkanBloomPass m_pass;
        VulkanBloomTarget m_target;
    };
} // namespace NexAur

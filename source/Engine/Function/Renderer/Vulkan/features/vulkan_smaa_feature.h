#pragma once

#include <array>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/features/vulkan_render_feature_context.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_constants.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"
#include "Function/Renderer/Vulkan/passes/vulkan_smaa_pass.h"
#include "Function/Renderer/Vulkan/targets/vulkan_smaa_target.h"
#include "Function/Renderer/renderer_debug_service.h"

namespace NexAur {
    class VulkanPassGraph;

    class VulkanSmaaFeature final {
    public:
        VulkanSmaaFeature() = default;
        ~VulkanSmaaFeature();

        VulkanSmaaFeature(const VulkanSmaaFeature&) = delete;
        VulkanSmaaFeature& operator=(const VulkanSmaaFeature&) = delete;

        bool init(
            const VulkanRenderFeatureContext& context,
            VkFormat source_format,
            VkFormat mask_format,
            VkFormat output_format,
            uint32_t width,
            uint32_t height);
        void shutdown();
        void cleanupSwapchainResources();
        bool recreateSwapchainResources(VkFormat output_format);
        bool resize(uint32_t width, uint32_t height);

        bool isReady() const;
        VulkanGraphImageHandle addSourceImage(VulkanPassGraph& graph);
        bool addPasses(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle source_color,
            VulkanGraphImageHandle output_color,
            const VulkanSmaaRenderTarget& output_target,
            const RenderAntiAliasingSettings& settings,
            const RenderEffectDebugSettings& debug_settings,
            uint32_t frame_index);

        VulkanSmaaRenderTarget getSourceRenderTarget() const {
            return m_target.getSourceRenderTarget();
        }
        RendererDebugSmaaStats buildDebugStats(
            const RenderAntiAliasingSettings& settings,
            bool enabled) const;

    private:
        VulkanGraphImageHandle addEdgeImage(VulkanPassGraph& graph);
        VulkanGraphImageHandle addBlendImage(VulkanPassGraph& graph);
        VulkanSmaaInput makeInput(const VulkanSmaaImageView& image) const;
        VulkanSmaaPass& getPass(uint32_t frame_index);

    private:
        VulkanRenderFeatureContext m_context;
        VkFormat m_output_format = VK_FORMAT_UNDEFINED;
        std::array<VulkanSmaaPass, kVulkanFramesInFlight> m_passes;
        VulkanSmaaTarget m_target;
    };
} // namespace NexAur

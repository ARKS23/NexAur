#pragma once

#include <array>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/features/vulkan_render_feature_context.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_constants.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"
#include "Function/Renderer/Vulkan/passes/vulkan_ao_pass.h"
#include "Function/Renderer/Vulkan/targets/vulkan_ao_target.h"
#include "Function/Renderer/renderer_debug_service.h"

namespace NexAur {
    class VulkanPassGraph;

    struct VulkanAoFeatureGraphResources {
        VulkanGraphImageHandle raw;
        VulkanGraphImageHandle blurred;

        bool valid() const { return raw.valid() && blurred.valid(); }
    };

    struct VulkanAoFeatureInput {
        VkImageView raw_view = VK_NULL_HANDLE;
        VkImageView blurred_view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        bool valid() const {
            return raw_view != VK_NULL_HANDLE &&
                   blurred_view != VK_NULL_HANDLE &&
                   sampler != VK_NULL_HANDLE &&
                   layout != VK_IMAGE_LAYOUT_UNDEFINED;
        }
    };

    class VulkanAoFeature final {
    public:
        VulkanAoFeature() = default;
        ~VulkanAoFeature();

        VulkanAoFeature(const VulkanAoFeature&) = delete;
        VulkanAoFeature& operator=(const VulkanAoFeature&) = delete;

        bool init(
            const VulkanRenderFeatureContext& context,
            VkFormat color_format,
            uint32_t width,
            uint32_t height,
            bool half_resolution);
        void shutdown();

        bool prepare(const RenderAoSettings& settings, uint32_t width, uint32_t height);
        bool resize(uint32_t width, uint32_t height, bool half_resolution);

        bool isReady() const;
        VulkanAoFeatureGraphResources addGraphResources(VulkanPassGraph& graph);
        bool addPasses(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle scene_depth,
            const VulkanAoFeatureGraphResources& resources,
            VkImageView scene_depth_view,
            const VulkanRenderView& view,
            const RenderAoSettings& settings,
            uint32_t frame_index);

        VulkanAoFeatureInput getPostProcessInput() const;
        RendererDebugAoStats buildDebugStats(bool enabled) const;

    private:
        bool recreatePassResources();
        VulkanAoPass& getPass(uint32_t frame_index);

    private:
        VulkanRenderFeatureContext m_context;
        std::array<VulkanAoPass, kVulkanFramesInFlight> m_passes;
        VulkanAoTarget m_target;
    };
} // namespace NexAur

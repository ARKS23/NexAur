#pragma once

#include <functional>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/frame/vulkan_render_feature_plan.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"

namespace NexAur {
    class VulkanPassGraph;

    struct VulkanFrameGraphResources {
        VulkanGraphImageHandle directional_shadow_depth;
        VulkanGraphImageHandle point_shadow_depth;
        VulkanGraphImageHandle rect_shadow_depth;
        VulkanGraphImageHandle scene_color;
        VulkanGraphImageHandle scene_depth;
        VulkanGraphImageHandle ao_raw;
        VulkanGraphImageHandle ao_blurred;
        VulkanGraphImageHandle ssr_raw_reflection;
        VulkanGraphImageHandle ssr_hit_mask;
        VulkanGraphImageHandle final_color;
        VulkanGraphImageHandle swapchain_color;
        VulkanGraphImageHandle smaa_source;

        bool valid(const VulkanRenderFeaturePlan& plan) const;
    };

    struct VulkanFrameGraphCallbacks {
        using AddImagePass = std::function<bool(VulkanPassGraph&, VulkanGraphImageHandle)>;
        using RecordPass = std::function<bool(VkCommandBuffer)>;
        using AddAoPass = std::function<bool(
            VulkanPassGraph&,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle)>;
        using AddSsrPass = std::function<bool(
            VulkanPassGraph&,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle)>;
        using AddDebugDrawPass = std::function<bool(
            VulkanPassGraph&,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle)>;
        using AddBloomPass = std::function<bool(
            VulkanPassGraph&,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle&)>;
        using AddPostProcessPass = std::function<bool(
            VulkanPassGraph&,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle)>;
        using AddSmaaPass = std::function<bool(
            VulkanPassGraph&,
            VulkanGraphImageHandle,
            VulkanGraphImageHandle)>;

        AddImagePass add_directional_shadow;
        AddImagePass add_point_shadow;
        AddImagePass add_rect_shadow;
        AddImagePass add_skybox;
        RecordPass record_forward;
        AddAoPass add_ao;
        AddSsrPass add_ssr;
        AddDebugDrawPass add_debug_draw;
        std::function<bool(VulkanPassGraph&)> add_object_id;
        AddBloomPass add_bloom;
        AddPostProcessPass add_post_process;
        AddSmaaPass add_smaa;
        RecordPass record_imgui;

        bool valid(VulkanFrameOutputRoute output_route) const;
    };

    class VulkanFrameGraphBuilder final {
    public:
        bool build(
            VulkanPassGraph& graph,
            const VulkanRenderFeaturePlan& plan,
            const VulkanFrameGraphResources& resources,
            const VulkanFrameGraphCallbacks& callbacks) const;

    private:
        bool buildCommonScenePipeline(
            VulkanPassGraph& graph,
            const VulkanRenderFeaturePlan& plan,
            const VulkanFrameGraphResources& resources,
            const VulkanFrameGraphCallbacks& callbacks,
            VulkanGraphImageHandle& post_process_input) const;
        bool buildViewportOutputTail(
            VulkanPassGraph& graph,
            const VulkanRenderFeaturePlan& plan,
            const VulkanFrameGraphResources& resources,
            const VulkanFrameGraphCallbacks& callbacks,
            VulkanGraphImageHandle post_process_input) const;
        bool buildDirectSwapchainOutputTail(
            VulkanPassGraph& graph,
            const VulkanRenderFeaturePlan& plan,
            const VulkanFrameGraphResources& resources,
            const VulkanFrameGraphCallbacks& callbacks,
            VulkanGraphImageHandle post_process_input) const;
        bool buildFinalColor(
            VulkanPassGraph& graph,
            const VulkanRenderFeaturePlan& plan,
            const VulkanFrameGraphResources& resources,
            const VulkanFrameGraphCallbacks& callbacks,
            VulkanGraphImageHandle post_process_input) const;
        void addPresentTransition(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle swapchain_color) const;
    };
} // namespace NexAur

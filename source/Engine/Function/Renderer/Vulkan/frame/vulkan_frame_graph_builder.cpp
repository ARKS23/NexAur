#include "pch.h"
#include "vulkan_frame_graph_builder.h"

#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"

namespace NexAur {
    bool VulkanFrameGraphResources::valid(const VulkanRenderFeaturePlan& plan) const {
        return directional_shadow_depth.valid() &&
               point_shadow_depth.valid() &&
               rect_shadow_depth.valid() &&
               scene_color.valid() &&
               scene_depth.valid() &&
               ao_raw.valid() &&
               ao_blurred.valid() &&
               ssr_raw_reflection.valid() &&
               ssr_hit_mask.valid() &&
               final_color.valid() &&
               swapchain_color.valid() &&
               (!reflection.anyValid() || reflection.valid()) &&
               (!forward_writes_reflection_surface || reflection.valid()) &&
               (!(plan.usesRayQueryDebug() ||
                  plan.usesRayQueryShadow() ||
                  plan.usesRayQueryAo() ||
                  plan.rendersRayTracedReflection()) ||
                ray_query_scene.valid()) &&
               (!plan.rendersRayTracedReflection() ||
                (forward_writes_reflection_surface && reflection.valid())) &&
               (!plan.rendersSmaa() || smaa_source.valid());
    }

    bool VulkanFrameGraphCallbacks::valid(VulkanFrameOutputRoute output_route) const {
        return static_cast<bool>(add_directional_shadow) &&
               static_cast<bool>(add_point_shadow) &&
               static_cast<bool>(add_rect_shadow) &&
               static_cast<bool>(add_skybox) &&
               static_cast<bool>(record_forward) &&
               static_cast<bool>(add_ao) &&
               static_cast<bool>(add_ssr) &&
               static_cast<bool>(add_debug_draw) &&
               static_cast<bool>(add_object_id) &&
               static_cast<bool>(add_bloom) &&
               static_cast<bool>(add_post_process) &&
               static_cast<bool>(add_smaa) &&
               (output_route != VulkanFrameOutputRoute::Viewport ||
                static_cast<bool>(record_imgui));
    }

    bool VulkanFrameGraphBuilder::build(
        VulkanPassGraph& graph,
        const VulkanRenderFeaturePlan& plan,
        const VulkanFrameGraphResources& resources,
        const VulkanFrameGraphCallbacks& callbacks) const {
        const VulkanFrameOutputRoute output_route = plan.getOutputRoute();
        if (!resources.valid(plan) || !callbacks.valid(output_route)) {
            return false;
        }

        VulkanGraphImageHandle post_process_input;
        if (!buildCommonScenePipeline(
                graph,
                plan,
                resources,
                callbacks,
                post_process_input)) {
            return false;
        }

        return output_route == VulkanFrameOutputRoute::Viewport ?
            buildViewportOutputTail(
                graph,
                plan,
                resources,
                callbacks,
                post_process_input) :
            buildDirectSwapchainOutputTail(
                graph,
                plan,
                resources,
                callbacks,
                post_process_input);
    }

    bool VulkanFrameGraphBuilder::buildCommonScenePipeline(
        VulkanPassGraph& graph,
        const VulkanRenderFeaturePlan& plan,
        const VulkanFrameGraphResources& resources,
        const VulkanFrameGraphCallbacks& callbacks,
        VulkanGraphImageHandle& post_process_input) const {
        if (!callbacks.add_directional_shadow(graph, resources.directional_shadow_depth) ||
            !callbacks.add_point_shadow(graph, resources.point_shadow_depth) ||
            !callbacks.add_rect_shadow(graph, resources.rect_shadow_depth) ||
            !callbacks.add_skybox(graph, resources.scene_color)) {
            return false;
        }

        VulkanGraphPassBuilder forward_pass = graph.addPass("ForwardScene");
        const bool has_reflection_surface =
            resources.forward_writes_reflection_surface &&
            resources.reflection.valid();
        if (plan.usesRayQueryDebug() || plan.usesRayQueryShadow()) {
            forward_pass.readAccelerationStructure(
                resources.ray_query_scene,
                VulkanGraphAccelerationStructureUsage::RayQueryShaderRead);
        }
        forward_pass
            .readImage(resources.directional_shadow_depth, VulkanGraphImageUsage::ShaderRead)
            .readImage(resources.point_shadow_depth, VulkanGraphImageUsage::ShaderRead)
            .readImage(resources.rect_shadow_depth, VulkanGraphImageUsage::ShaderRead)
            .readWriteImage(resources.scene_color, VulkanGraphImageUsage::ColorAttachment)
            .writeImage(resources.scene_depth, VulkanGraphImageUsage::DepthStencilAttachment)
            .execute(callbacks.record_forward);
        if (has_reflection_surface) {
            forward_pass
                .writeImage(
                    resources.reflection.reflection_surface,
                    VulkanGraphImageUsage::ColorAttachment)
                .writeImage(
                    resources.reflection.fallback_specular,
                    VulkanGraphImageUsage::ColorAttachment)
                .writeImage(
                    resources.reflection.motion_vector,
                    VulkanGraphImageUsage::ColorAttachment);
            if (!callbacks.add_reflection_preparation ||
                !callbacks.add_reflection_preparation(
                    graph,
                    resources.reflection)) {
                return false;
            }
        }

        if (plan.rendersAo() &&
            !callbacks.add_ao(
                graph,
                resources.scene_depth,
                resources.ao_raw,
                resources.ao_blurred,
                resources.ray_query_scene)) {
            return false;
        }
        if (plan.rendersSsr() &&
            !callbacks.add_ssr(
                graph,
                resources.scene_color,
                resources.scene_depth,
                resources.ssr_raw_reflection,
                resources.ssr_hit_mask)) {
            return false;
        }
        if (plan.rendersRayTracedReflection() &&
            (!callbacks.add_ray_traced_reflection ||
             !callbacks.add_ray_traced_reflection(
                 graph,
                 resources.scene_depth,
                 resources.ssr_hit_mask,
                 resources.ray_query_scene,
                 resources.reflection))) {
            return false;
        }
        if (!callbacks.add_debug_draw(graph, resources.scene_color, resources.scene_depth) ||
            !callbacks.add_object_id(graph)) {
            return false;
        }

        post_process_input = resources.scene_color;
        if (plan.rendersBloom() &&
            !callbacks.add_bloom(graph, resources.scene_color, post_process_input)) {
            return false;
        }

        return post_process_input.valid();
    }

    bool VulkanFrameGraphBuilder::buildViewportOutputTail(
        VulkanPassGraph& graph,
        const VulkanRenderFeaturePlan& plan,
        const VulkanFrameGraphResources& resources,
        const VulkanFrameGraphCallbacks& callbacks,
        VulkanGraphImageHandle post_process_input) const {
        if (!buildFinalColor(graph, plan, resources, callbacks, post_process_input)) {
            return false;
        }

        graph.addPass("ImGuiComposite")
            .readImage(resources.final_color, VulkanGraphImageUsage::ShaderRead)
            .writeImage(resources.swapchain_color, VulkanGraphImageUsage::ColorAttachment)
            .execute(callbacks.record_imgui);
        addPresentTransition(graph, resources.swapchain_color);
        return true;
    }

    bool VulkanFrameGraphBuilder::buildDirectSwapchainOutputTail(
        VulkanPassGraph& graph,
        const VulkanRenderFeaturePlan& plan,
        const VulkanFrameGraphResources& resources,
        const VulkanFrameGraphCallbacks& callbacks,
        VulkanGraphImageHandle post_process_input) const {
        if (!buildFinalColor(graph, plan, resources, callbacks, post_process_input)) {
            return false;
        }

        addPresentTransition(graph, resources.swapchain_color);
        return true;
    }

    bool VulkanFrameGraphBuilder::buildFinalColor(
        VulkanPassGraph& graph,
        const VulkanRenderFeaturePlan& plan,
        const VulkanFrameGraphResources& resources,
        const VulkanFrameGraphCallbacks& callbacks,
        VulkanGraphImageHandle post_process_input) const {

        const VulkanGraphImageHandle post_process_output =
            plan.rendersSmaa() ? resources.smaa_source : resources.final_color;
        const bool ray_traced_reflection_debug =
            isVulkanRayTracedReflectionDebugView(
                plan.getPostProcessDebugSettings().view);
        if (!callbacks.add_post_process(
                graph,
                post_process_input,
                post_process_output,
                resources.scene_depth,
                resources.ao_raw,
                resources.ao_blurred,
                ray_traced_reflection_debug ?
                    resources.reflection.raw_reflection : resources.ssr_raw_reflection,
                ray_traced_reflection_debug ?
                    resources.reflection.hit_distance : resources.ssr_hit_mask)) {
            return false;
        }
        if (plan.rendersSmaa() &&
            !callbacks.add_smaa(graph, post_process_output, resources.final_color)) {
            return false;
        }

        return true;
    }

    void VulkanFrameGraphBuilder::addPresentTransition(
        VulkanPassGraph& graph,
        VulkanGraphImageHandle swapchain_color) const {
        graph.addPass("PresentTransition")
            .readImage(swapchain_color, VulkanGraphImageUsage::Present);
    }
} // namespace NexAur

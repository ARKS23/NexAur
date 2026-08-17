#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef NOMINMAX
    #define NOMINMAX
#endif
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/Log/log_system.h"
#include "Function/Renderer/data/render_settings.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/Renderer/data/render_data.h"
#include "Function/Renderer/renderer_service_types.h"
#include "Function/Renderer/frontend/render_scene_frame_builder.h"
#include "Function/Renderer/frontend/render_shadow_frame_builder.h"
#include "Function/Renderer/Vulkan/core/vulkan_retirement_queue.h"
#include "Function/Renderer/Vulkan/core/vulkan_device_context.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_constants.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_flight_tracker.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_graph_builder.h"
#include "Function/Renderer/Vulkan/frame/vulkan_render_feature_plan.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_render_data_translator.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_state_planner.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_ray_tracing_capabilities.h"
#include "Function/Renderer/Vulkan/reflection_probe_residency.h"
#include "Function/Renderer/Vulkan/upload/vulkan_upload_manager.h"

namespace {
    bool nearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool nearlyEqualVec3(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon = 0.001f) {
        return nearlyEqual(lhs.x, rhs.x, epsilon) &&
               nearlyEqual(lhs.y, rhs.y, epsilon) &&
               nearlyEqual(lhs.z, rhs.z, epsilon);
    }

    bool expectGameplay(bool condition, const std::string& message, std::string& failure) {
        if (condition) {
            return true;
        }

        failure = message;
        return false;
    }
} // namespace

int runShadowFrameBuilderSmoke() {
    NexAur::RenderView view;
    view.near_clip = 0.1f;
    view.far_clip = 120.0f;
    view.camera_position = glm::vec3{ 0.0f, 2.0f, 6.0f };
    view.view_matrix = glm::lookAt(
        view.camera_position,
        glm::vec3{ 0.0f, 1.0f, 0.0f },
        glm::vec3{ 0.0f, 1.0f, 0.0f });
    view.projection_matrix = glm::perspective(
        glm::radians(60.0f),
        16.0f / 9.0f,
        view.near_clip,
        view.far_clip);
    view.view_projection_matrix = view.projection_matrix * view.view_matrix;
    view.inverse_view_matrix = glm::inverse(view.view_matrix);
    view.inverse_projection_matrix = glm::inverse(view.projection_matrix);

    NexAur::RenderSettings settings;
    settings.shadow.cascade_count = NexAur::kMaxRenderShadowCascadeCount;
    settings.shadow.cascades_enabled = true;
    settings.shadow.distance = 64.0f;
    settings.shadow.cascade_split_lambda = 0.65f;
    settings.shadow.stabilize = true;
    settings.effects_debug.view = NexAur::RenderEffectDebugView::ShadowCascades;

    NexAur::RenderFrameDirectionalLight directional_light;
    directional_light.direction = glm::normalize(glm::vec3{ -0.3f, -1.0f, -0.2f });

    NexAur::RenderShadowFrameBuilder builder;
    const NexAur::RenderShadowCascadeFrame cascade_frame = builder.buildDirectionalShadowFrame(
        view,
        directional_light,
        settings.shadow,
        settings.effects_debug,
        2048.0f);

    bool success = true;
    std::string failure;
    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };
    auto matrixFinite = [](const glm::mat4& matrix) {
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                if (!std::isfinite(matrix[column][row])) {
                    return false;
                }
            }
        }
        return true;
    };
    auto matrixIsIdentity = [&](const glm::mat4& matrix) {
        return nearlyEqual(
                   matrix[0][0],
                   1.0f) && nearlyEqual(matrix[1][1], 1.0f) &&
               nearlyEqual(matrix[2][2], 1.0f) && nearlyEqual(matrix[3][3], 1.0f) &&
               nearlyEqual(matrix[0][1], 0.0f) && nearlyEqual(matrix[0][2], 0.0f) &&
               nearlyEqual(matrix[0][3], 0.0f) && nearlyEqual(matrix[1][0], 0.0f) &&
               nearlyEqual(matrix[1][2], 0.0f) && nearlyEqual(matrix[1][3], 0.0f) &&
               nearlyEqual(matrix[2][0], 0.0f) && nearlyEqual(matrix[2][1], 0.0f) &&
               nearlyEqual(matrix[2][3], 0.0f) && nearlyEqual(matrix[3][0], 0.0f) &&
               nearlyEqual(matrix[3][1], 0.0f) && nearlyEqual(matrix[3][2], 0.0f);
    };

    expect(
        cascade_frame.cascade_count == NexAur::kMaxRenderShadowCascadeCount &&
        cascade_frame.cascades_enabled && cascade_frame.debug_overlay,
        "Shadow frame builder smoke failed: cascade count or debug state is incorrect.");
    for (uint32_t index = 0; index < cascade_frame.cascade_count; ++index) {
        expect(
            std::isfinite(cascade_frame.split_depths[index]) &&
            matrixFinite(cascade_frame.light_view_projections[index]) &&
            (index == 0u || cascade_frame.split_depths[index] > cascade_frame.split_depths[index - 1u]),
            "Shadow frame builder smoke failed: cascade split or matrix is invalid.");
    }
    expect(
        nearlyEqual(cascade_frame.split_depths.back(), settings.shadow.distance),
        "Shadow frame builder smoke failed: final cascade does not reach shadow distance.");
    const float split_ratio = 1.0f / static_cast<float>(NexAur::kMaxRenderShadowCascadeCount);
    const float linear_split = view.near_clip +
        (settings.shadow.distance - view.near_clip) * split_ratio;
    const float logarithmic_split = view.near_clip *
        std::pow(settings.shadow.distance / view.near_clip, split_ratio);
    const float expected_split = linear_split * (1.0f - settings.shadow.cascade_split_lambda) +
        logarithmic_split * settings.shadow.cascade_split_lambda;
    expect(
        nearlyEqual(cascade_frame.split_depths[0], expected_split),
        "Shadow frame builder smoke failed: cascade split formula changed.");

    settings.shadow.stabilize = false;
    const NexAur::RenderShadowCascadeFrame unstable_frame = builder.buildDirectionalShadowFrame(
        view,
        directional_light,
        settings.shadow,
        settings.effects_debug,
        2048.0f);
    expect(
        !nearlyEqual(
            cascade_frame.light_view_projections[0][3][0],
            unstable_frame.light_view_projections[0][3][0],
            0.000001f) ||
        !nearlyEqual(
            cascade_frame.light_view_projections[0][3][1],
            unstable_frame.light_view_projections[0][3][1],
            0.000001f),
        "Shadow frame builder smoke failed: stabilization did not affect cascade projection.");

    NexAur::RenderFramePointLight point_light;
    point_light.position = glm::vec3{ 1.0f, 2.0f, 3.0f };
    point_light.shadow_slot = 0;
    point_light.cast_shadow = true;
    point_light.shadow_range = 12.0f;
    const NexAur::RenderPointShadowFrame point_frame = builder.buildPointShadowFrame(
        { point_light },
        settings.point_shadow,
        1u);
    const std::array<glm::vec3, NexAur::kRenderPointShadowCubeFaceCount> face_directions{
        glm::vec3{  1.0f,  0.0f,  0.0f },
        glm::vec3{ -1.0f,  0.0f,  0.0f },
        glm::vec3{  0.0f,  1.0f,  0.0f },
        glm::vec3{  0.0f, -1.0f,  0.0f },
        glm::vec3{  0.0f,  0.0f,  1.0f },
        glm::vec3{  0.0f,  0.0f, -1.0f }
    };
    const std::array<glm::vec3, NexAur::kRenderPointShadowCubeFaceCount> face_ups{
        glm::vec3{ 0.0f, -1.0f, 0.0f },
        glm::vec3{ 0.0f, -1.0f, 0.0f },
        glm::vec3{ 0.0f,  0.0f, 1.0f },
        glm::vec3{ 0.0f,  0.0f, -1.0f },
        glm::vec3{ 0.0f, -1.0f, 0.0f },
        glm::vec3{ 0.0f, -1.0f, 0.0f }
    };
    for (uint32_t face = 0; face < NexAur::kRenderPointShadowCubeFaceCount; ++face) {
        const glm::mat4& matrix = point_frame.light_view_projections[face];
        const glm::vec4 projected_face_center = matrix * glm::vec4{
            point_light.position + face_directions[face] * 2.0f,
            1.0f
        };
        const glm::vec4 projected_face_up = matrix * glm::vec4{
            point_light.position + face_ups[face] * 2.0f,
            1.0f
        };
        const glm::vec3 projected_ndc = glm::vec3{ projected_face_center } / projected_face_center.w;
        const glm::vec3 projected_up_ndc = glm::vec3{ projected_face_up } / projected_face_up.w;
        expect(
            point_frame.enabled && point_frame.face_count == NexAur::kRenderPointShadowCubeFaceCount &&
            point_frame.shadowed_light_count == 1u && matrixFinite(matrix) &&
            nearlyEqual(projected_ndc.x, 0.0f) && nearlyEqual(projected_ndc.y, 0.0f) &&
            projected_up_ndc.y < -0.1f,
            "Shadow frame builder smoke failed: point shadow cube face orientation is incorrect.");
    }

    NexAur::RenderFrameRectLight rect_light;
    rect_light.position = glm::vec3{ 0.0f, 4.0f, 0.0f };
    rect_light.size = glm::vec2{ 2.0f, 1.0f };
    rect_light.normal = glm::vec3{ 0.0f, -1.0f, 0.0f };
    rect_light.up = glm::vec3{ 0.0f, 0.0f, 1.0f };
    rect_light.shadow_slot = 0;
    rect_light.cast_shadow = true;
    rect_light.range = 10.0f;
    settings.rect_shadow.projection_margin = 0.0f;
    const NexAur::RenderRectShadowFrame rect_frame_without_margin = builder.buildRectShadowFrame(
        { rect_light },
        settings.rect_shadow,
        1u);
    settings.rect_shadow.projection_margin = 0.5f;
    const NexAur::RenderRectShadowFrame rect_frame = builder.buildRectShadowFrame(
        { rect_light },
        settings.rect_shadow,
        1u);
    const glm::vec4 rect_center = rect_frame.light_view_projections[0] * glm::vec4{ rect_light.position, 1.0f };
    const glm::vec4 rect_edge_without_margin = rect_frame_without_margin.light_view_projections[0] *
        glm::vec4{ rect_light.position + glm::vec3{ 1.0f, 0.0f, 0.0f }, 1.0f };
    const glm::vec4 rect_edge_with_margin = rect_frame.light_view_projections[0] *
        glm::vec4{ rect_light.position + glm::vec3{ 1.0f, 0.0f, 0.0f }, 1.0f };
    const float rect_edge_without_margin_x =
        std::abs(rect_edge_without_margin.x / rect_edge_without_margin.w);
    const float rect_edge_with_margin_x =
        std::abs(rect_edge_with_margin.x / rect_edge_with_margin.w);
    expect(
        rect_frame.enabled && rect_frame.shadowed_light_count == 1u && matrixFinite(rect_frame.light_view_projections[0]) &&
        nearlyEqual(rect_center.x, 0.0f) && nearlyEqual(rect_center.y, 0.0f) &&
        rect_edge_with_margin_x < rect_edge_without_margin_x,
        "Shadow frame builder smoke failed: rect shadow projection is not centered.");

    settings.shadow.enabled = false;
    const NexAur::RenderShadowCascadeFrame disabled_directional_frame =
        builder.buildDirectionalShadowFrame(
            view,
            directional_light,
            settings.shadow,
            settings.effects_debug,
            2048.0f);
    expect(
        disabled_directional_frame.cascade_count == 1u &&
        !disabled_directional_frame.cascades_enabled &&
        matrixIsIdentity(disabled_directional_frame.light_view_projections[0]),
        "Shadow frame builder smoke failed: disabled directional shadows produced an active frame.");
    settings.shadow.enabled = true;
    expect(
        !builder.buildPointShadowFrame({ point_light }, settings.point_shadow, 0u).enabled &&
        !builder.buildRectShadowFrame({ rect_light }, settings.rect_shadow, 0u).enabled,
        "Shadow frame builder smoke failed: target capacity was not respected.");

    if (!success) {
        std::cerr << failure << std::endl;
        return 1;
    }

    std::cout << "Shadow frame builder smoke passed." << std::endl;
    return 0;
}


int runRenderGraphStatePlannerSmoke() {
    struct UsageExpectation {
        NexAur::VulkanGraphImageUsage usage;
        NexAur::VulkanGraphAccessType access_type;
        VkImageLayout layout;
        VkAccessFlags2 access;
        VkPipelineStageFlags2 stage;
    };

    constexpr std::array<UsageExpectation, 5> kUsageExpectations{
        UsageExpectation{
            NexAur::VulkanGraphImageUsage::ColorAttachment,
            NexAur::VulkanGraphAccessType::Write,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
        },
        UsageExpectation{
            NexAur::VulkanGraphImageUsage::DepthStencilAttachment,
            NexAur::VulkanGraphAccessType::Write,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT
        },
        UsageExpectation{
            NexAur::VulkanGraphImageUsage::ShaderRead,
            NexAur::VulkanGraphAccessType::Read,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_2_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
        },
        UsageExpectation{
            NexAur::VulkanGraphImageUsage::TransferSource,
            NexAur::VulkanGraphAccessType::Read,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_ACCESS_2_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT
        },
        UsageExpectation{
            NexAur::VulkanGraphImageUsage::Present,
            NexAur::VulkanGraphAccessType::Read,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_NONE
        }
    };

    bool success = true;
    std::string failure;
    auto expect = [&](bool condition, const std::string& message) {
        if (!condition && success) {
            failure = message;
        }
        success = success && condition;
    };

    auto makeRange = [](
        VkImageAspectFlags aspect_mask,
        uint32_t base_mip_level = 0,
        uint32_t mip_count = 1,
        uint32_t base_array_layer = 0,
        uint32_t layer_count = 1) {
        NexAur::VulkanGraphImageSubresourceRange range;
        range.aspect_mask = aspect_mask;
        range.base_mip_level = base_mip_level;
        range.mip_count = mip_count;
        range.base_array_layer = base_array_layer;
        range.layer_count = layer_count;
        return range;
    };

    auto rangesEqual = [](
        const NexAur::VulkanGraphImageSubresourceRange& lhs,
        const NexAur::VulkanGraphImageSubresourceRange& rhs) {
        return lhs.aspect_mask == rhs.aspect_mask &&
               lhs.base_mip_level == rhs.base_mip_level &&
               lhs.mip_count == rhs.mip_count &&
               lhs.base_array_layer == rhs.base_array_layer &&
               lhs.layer_count == rhs.layer_count;
    };

    const NexAur::VulkanGraphImageSubresourceRange color_range =
        makeRange(VK_IMAGE_ASPECT_COLOR_BIT);
    const NexAur::VulkanGraphImageSubresourceRange depth_range =
        makeRange(VK_IMAGE_ASPECT_DEPTH_BIT);

    for (const UsageExpectation& expectation : kUsageExpectations) {
        const NexAur::VulkanGraphImageState state =
            NexAur::VulkanGraphStatePlanner::stateForUsage(
                expectation.usage,
                expectation.access_type,
                color_range);
        expect(state.layout == expectation.layout, "RenderGraph planner produced an unexpected usage layout.");
        expect(state.access == expectation.access, "RenderGraph planner produced an unexpected usage access mask.");
        expect(state.stage == expectation.stage, "RenderGraph planner produced an unexpected usage stage mask.");
        expect(state.last_access == expectation.access_type, "RenderGraph planner lost the latest access type.");
        expect(rangesEqual(state.subresource_range, color_range), "RenderGraph planner lost the subresource range.");
    }

    const NexAur::VulkanGraphImageState undefined_state =
        NexAur::VulkanGraphStatePlanner::stateForLayout(VK_IMAGE_LAYOUT_UNDEFINED, color_range);
    expect(
        undefined_state.access == VK_ACCESS_2_NONE &&
            undefined_state.stage == VK_PIPELINE_STAGE_2_NONE,
        "RenderGraph planner did not use synchronization2 NONE semantics for an undefined image.");
    expect(
        undefined_state.last_access == NexAur::VulkanGraphAccessType::None,
        "RenderGraph planner assigned an access type to an undefined image.");

    const NexAur::VulkanGraphImageState acquired_undefined_state =
        NexAur::VulkanGraphStatePlanner::stateForImport(
            VK_IMAGE_LAYOUT_UNDEFINED,
            color_range,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    expect(
        acquired_undefined_state.access == VK_ACCESS_2_NONE &&
            acquired_undefined_state.stage == VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        "RenderGraph planner lost the external acquire scope for an imported image.");

    const NexAur::VulkanGraphImageState present_layout_state =
        NexAur::VulkanGraphStatePlanner::stateForLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, color_range);
    expect(
        present_layout_state.access == VK_ACCESS_2_NONE &&
            present_layout_state.stage == VK_PIPELINE_STAGE_2_NONE,
        "RenderGraph planner did not use synchronization2 NONE semantics for a presentation layout.");

    const NexAur::VulkanGraphImageState acquired_present_state =
        NexAur::VulkanGraphStatePlanner::stateForImport(
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            color_range,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    expect(
        acquired_present_state.access == VK_ACCESS_2_NONE &&
            acquired_present_state.stage == VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        "RenderGraph planner did not align imported presentation state with the acquire wait scope.");

    const NexAur::VulkanGraphImageState conservative_state =
        NexAur::VulkanGraphStatePlanner::stateForLayout(VK_IMAGE_LAYOUT_GENERAL, color_range);
    expect(conservative_state.layout == VK_IMAGE_LAYOUT_GENERAL, "RenderGraph planner did not preserve an unknown layout.");
    expect(
        conservative_state.access == (VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT),
        "RenderGraph planner did not use conservative access for an unknown layout.");
    expect(
        conservative_state.stage == VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        "RenderGraph planner did not use a conservative stage for an unknown layout.");
    expect(
        conservative_state.last_access == NexAur::VulkanGraphAccessType::ReadWrite,
        "RenderGraph planner did not mark an unknown layout as conservative read/write access.");

    const NexAur::VulkanGraphImageState color_write =
        NexAur::VulkanGraphStatePlanner::stateForUsage(
            NexAur::VulkanGraphImageUsage::ColorAttachment,
            NexAur::VulkanGraphAccessType::Write,
            color_range);
    const NexAur::VulkanGraphImageTransitionPlan undefined_to_color_write =
        NexAur::VulkanGraphStatePlanner::planImageTransition(undefined_state, color_write);
    expect(
        undefined_to_color_write.requires_barrier,
        "RenderGraph planner skipped Undefined -> ColorWrite.");
    expect(
        undefined_to_color_write.source.stage == VK_PIPELINE_STAGE_2_NONE &&
            undefined_to_color_write.source.access == VK_ACCESS_2_NONE,
        "RenderGraph planner produced an invalid synchronization2 source scope for an undefined image.");

    const NexAur::VulkanGraphImageTransitionPlan acquired_undefined_to_color_write =
        NexAur::VulkanGraphStatePlanner::planImageTransition(acquired_undefined_state, color_write);
    expect(
        acquired_undefined_to_color_write.requires_barrier &&
            acquired_undefined_to_color_write.source.stage ==
                VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        "RenderGraph planner did not preserve the acquire scope for first swapchain image use.");

    const NexAur::VulkanGraphImageTransitionPlan present_to_color_write =
        NexAur::VulkanGraphStatePlanner::planImageTransition(acquired_present_state, color_write);
    expect(
        present_to_color_write.requires_barrier,
        "RenderGraph planner skipped Present -> ColorWrite.");
    expect(
        present_to_color_write.source.stage == VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT &&
            present_to_color_write.source.access == VK_ACCESS_2_NONE,
        "RenderGraph planner produced an invalid source scope for an acquired swapchain image.");

    const NexAur::VulkanGraphImageState present_destination =
        NexAur::VulkanGraphStatePlanner::stateForUsage(
            NexAur::VulkanGraphImageUsage::Present,
            NexAur::VulkanGraphAccessType::Read,
            color_range);
    const NexAur::VulkanGraphImageTransitionPlan color_write_to_present =
        NexAur::VulkanGraphStatePlanner::planImageTransition(color_write, present_destination);
    expect(
        color_write_to_present.requires_barrier,
        "RenderGraph planner skipped ColorWrite -> Present.");
    expect(
        color_write_to_present.destination.stage == VK_PIPELINE_STAGE_2_NONE &&
            color_write_to_present.destination.access == VK_ACCESS_2_NONE,
        "RenderGraph planner did not use synchronization2 NONE semantics when releasing for presentation.");

    const NexAur::VulkanGraphImageState color_read =
        NexAur::VulkanGraphStatePlanner::stateForUsage(
            NexAur::VulkanGraphImageUsage::ShaderRead,
            NexAur::VulkanGraphAccessType::Read,
            color_range);
    const NexAur::VulkanGraphImageTransitionPlan color_write_to_read =
        NexAur::VulkanGraphStatePlanner::planImageTransition(color_write, color_read);
    expect(
        color_write_to_read.requires_barrier,
        "RenderGraph planner skipped ColorWrite -> ShaderRead.");
    expect(
        color_write_to_read.source.access == VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        "RenderGraph planner produced an unexpected ColorWrite source access mask.");
    expect(
        color_write_to_read.destination.access == VK_ACCESS_2_SHADER_READ_BIT,
        "RenderGraph planner produced an unexpected ShaderRead destination access mask.");
    expect(
        rangesEqual(color_write_to_read.barrier_range, color_range),
        "RenderGraph planner produced an unexpected ColorWrite -> ShaderRead barrier range.");

    const NexAur::VulkanGraphImageState depth_write =
        NexAur::VulkanGraphStatePlanner::stateForUsage(
            NexAur::VulkanGraphImageUsage::DepthStencilAttachment,
            NexAur::VulkanGraphAccessType::Write,
            depth_range);
    const NexAur::VulkanGraphImageState depth_read =
        NexAur::VulkanGraphStatePlanner::stateForUsage(
            NexAur::VulkanGraphImageUsage::ShaderRead,
            NexAur::VulkanGraphAccessType::Read,
            depth_range);
    const NexAur::VulkanGraphImageTransitionPlan depth_write_to_read =
        NexAur::VulkanGraphStatePlanner::planImageTransition(depth_write, depth_read);
    expect(
        depth_write_to_read.requires_barrier,
        "RenderGraph planner skipped DepthWrite -> ShaderRead.");
    expect(
        rangesEqual(depth_write_to_read.barrier_range, depth_range),
        "RenderGraph planner produced an unexpected DepthWrite -> ShaderRead barrier range.");

    const NexAur::VulkanGraphImageTransitionPlan color_write_to_write =
        NexAur::VulkanGraphStatePlanner::planImageTransition(color_write, color_write);
    expect(
        color_write_to_write.requires_barrier,
        "RenderGraph planner skipped ColorWrite -> ColorWrite with an unchanged layout.");

    const NexAur::VulkanGraphImageTransitionPlan shader_read_to_color_write =
        NexAur::VulkanGraphStatePlanner::planImageTransition(color_read, color_write);
    expect(
        shader_read_to_color_write.requires_barrier,
        "RenderGraph planner skipped ShaderRead -> ColorWrite.");

    const NexAur::VulkanGraphImageTransitionPlan shader_read_to_shader_read =
        NexAur::VulkanGraphStatePlanner::planImageTransition(color_read, color_read);
    expect(
        !shader_read_to_shader_read.requires_barrier,
        "RenderGraph planner emitted a barrier for read-only access with an unchanged layout.");

    const NexAur::VulkanGraphImageState color_read_write =
        NexAur::VulkanGraphStatePlanner::stateForUsage(
            NexAur::VulkanGraphImageUsage::ColorAttachment,
            NexAur::VulkanGraphAccessType::ReadWrite,
            color_range);
    expect(
        color_read_write.access ==
            (VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT),
        "RenderGraph planner produced an incomplete color attachment ReadWrite access mask.");
    const NexAur::VulkanGraphImageTransitionPlan shader_read_to_read_write =
        NexAur::VulkanGraphStatePlanner::planImageTransition(color_read, color_read_write);
    expect(
        shader_read_to_read_write.requires_barrier,
        "RenderGraph planner skipped a Read -> ReadWrite hazard with an unchanged layout.");

    const NexAur::VulkanGraphImageSubresourceRange mip_zero =
        makeRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
    const NexAur::VulkanGraphImageSubresourceRange mip_one =
        makeRange(VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0, 1);
    const NexAur::VulkanGraphImageState mip_zero_write =
        NexAur::VulkanGraphStatePlanner::stateForUsage(
            NexAur::VulkanGraphImageUsage::ColorAttachment,
            NexAur::VulkanGraphAccessType::Write,
            mip_zero);
    const NexAur::VulkanGraphImageState mip_one_read =
        NexAur::VulkanGraphStatePlanner::stateForUsage(
            NexAur::VulkanGraphImageUsage::ShaderRead,
            NexAur::VulkanGraphAccessType::Read,
            mip_one);
    const NexAur::VulkanGraphImageTransitionPlan non_overlapping =
        NexAur::VulkanGraphStatePlanner::planImageTransition(mip_zero_write, mip_one_read);
    expect(
        !non_overlapping.requires_barrier,
        "RenderGraph planner emitted a dependency for non-overlapping mip ranges.");
    expect(
        !non_overlapping.barrier_range.valid(),
        "RenderGraph planner produced a barrier range for non-overlapping subresources.");

    const NexAur::VulkanGraphImageSubresourceRange source_range =
        makeRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 3, 0, 4);
    const NexAur::VulkanGraphImageSubresourceRange destination_range =
        makeRange(VK_IMAGE_ASPECT_COLOR_BIT, 1, 3, 2, 4);
    const NexAur::VulkanGraphImageSubresourceRange expected_intersection =
        makeRange(VK_IMAGE_ASPECT_COLOR_BIT, 1, 2, 2, 2);
    const NexAur::VulkanGraphImageTransitionPlan partial_overlap =
        NexAur::VulkanGraphStatePlanner::planImageTransition(
            NexAur::VulkanGraphStatePlanner::stateForUsage(
                NexAur::VulkanGraphImageUsage::ColorAttachment,
                NexAur::VulkanGraphAccessType::Write,
                source_range),
            NexAur::VulkanGraphStatePlanner::stateForUsage(
                NexAur::VulkanGraphImageUsage::ShaderRead,
                NexAur::VulkanGraphAccessType::Read,
                destination_range));
    expect(
        partial_overlap.requires_barrier,
        "RenderGraph planner skipped partially overlapping subresources.");
    expect(
        rangesEqual(partial_overlap.barrier_range, expected_intersection),
        "RenderGraph planner produced an incorrect subresource intersection.");

    if (!success) {
        std::cerr << "RenderGraph state planner smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "RenderGraph state planner smoke passed." << std::endl;
    return 0;
}

int runFrameFeaturePlanSmoke() {
    bool success = true;
    std::string failure;
    auto expect = [&](bool condition, const std::string& message) {
        if (!condition && success) {
            failure = message;
        }
        success = success && condition;
    };

    NexAur::VulkanRenderFeatureAvailability available;
    available.viewport_output = true;
    available.post_process = true;
    available.bloom = true;
    available.ao = true;
    available.ssr = true;
    available.smaa = true;
    available.directional_shadow = true;
    available.point_shadow = true;
    available.rect_shadow = true;

    NexAur::RenderSettings settings;
    const NexAur::VulkanRenderFeaturePlan default_plan =
        NexAur::VulkanRenderFeaturePlan::build(settings, available);
    expect(
        default_plan.getOutputRoute() == NexAur::VulkanFrameOutputRoute::Viewport,
        "Frame feature plan should select the available viewport output route.");
    expect(
        default_plan.rendersAo() &&
            !default_plan.rendersSsr() &&
            default_plan.rendersBloom() &&
            default_plan.rendersSmaa(),
        "Frame feature plan did not preserve default feature enable decisions.");

    settings.ao.enabled = false;
    settings.effects_debug.view = NexAur::RenderEffectDebugView::AoRaw;
    const NexAur::VulkanRenderFeaturePlan ao_debug_plan =
        NexAur::VulkanRenderFeaturePlan::build(settings, available);
    expect(
        ao_debug_plan.rendersAo() &&
            !ao_debug_plan.rendersBloom() &&
            !ao_debug_plan.rendersSmaa(),
        "AO debug view should force only the AO feature path.");

    settings.effects_debug.view = NexAur::RenderEffectDebugView::SsrHitMask;
    const NexAur::VulkanRenderFeaturePlan ssr_debug_plan =
        NexAur::VulkanRenderFeaturePlan::build(settings, available);
    expect(
        ssr_debug_plan.rendersSsr(),
        "SSR debug view should force SSR even when final-lit SSR is disabled.");

    settings.post_process.bloom_enabled = false;
    settings.post_process.bloom_intensity = 0.0f;
    settings.effects_debug.view = NexAur::RenderEffectDebugView::BloomDownsampleMip;
    const NexAur::VulkanRenderFeaturePlan bloom_debug_plan =
        NexAur::VulkanRenderFeaturePlan::build(settings, available);
    expect(
        bloom_debug_plan.rendersBloom(),
        "Bloom debug view should force bloom independently of final-lit settings.");

    settings.anti_aliasing.mode = NexAur::RenderAntiAliasingMode::None;
    settings.effects_debug.view = NexAur::RenderEffectDebugView::SmaaEdgeMask;
    const NexAur::VulkanRenderFeaturePlan smaa_debug_plan =
        NexAur::VulkanRenderFeaturePlan::build(settings, available);
    expect(
        smaa_debug_plan.rendersSmaa() &&
            smaa_debug_plan.getPostProcessDebugSettings().view ==
                NexAur::RenderEffectDebugView::ColorGraded,
        "SMAA debug should force SMAA and resolve its post-process source view.");

    settings = NexAur::RenderSettings{};
    settings.ibl_debug.mode = NexAur::RenderIblDebugMode::DiffuseIbl;
    const NexAur::VulkanRenderFeaturePlan isolated_plan =
        NexAur::VulkanRenderFeaturePlan::build(settings, available);
    expect(
        isolated_plan.isolatesForwardDebug() &&
            !isolated_plan.rendersAo() &&
            !isolated_plan.rendersSsr() &&
            !isolated_plan.rendersBloom() &&
            !isolated_plan.rendersSmaa(),
        "Forward debug isolation should suppress screen-space and post-process features.");

    settings = NexAur::RenderSettings{};
    settings.ssr.enabled = true;
    settings.effects_debug.view = NexAur::RenderEffectDebugView::SsrRawReflection;
    NexAur::VulkanRenderFeatureAvailability limited = available;
    limited.viewport_output = false;
    limited.ssr = false;
    const NexAur::VulkanRenderFeaturePlan fallback_plan =
        NexAur::VulkanRenderFeaturePlan::build(settings, limited);
    expect(
        fallback_plan.getOutputRoute() == NexAur::VulkanFrameOutputRoute::DirectSwapchain &&
            !fallback_plan.rendersSsr() &&
            fallback_plan.getDebugSettings().view == NexAur::RenderEffectDebugView::FinalLit,
        "Feature plan should resolve unavailable SSR debug output to direct final-lit output.");

    auto makeResources = []() {
        NexAur::VulkanFrameGraphResources resources;
        uint32_t index = 0;
        resources.directional_shadow_depth.index = index++;
        resources.point_shadow_depth.index = index++;
        resources.rect_shadow_depth.index = index++;
        resources.scene_color.index = index++;
        resources.scene_depth.index = index++;
        resources.ao_raw.index = index++;
        resources.ao_blurred.index = index++;
        resources.ssr_raw_reflection.index = index++;
        resources.ssr_hit_mask.index = index++;
        resources.final_color.index = index++;
        resources.swapchain_color.index = index++;
        resources.smaa_source.index = index++;
        return resources;
    };
    auto makeCallbacks = [](
        int& ao_calls,
        int& ssr_calls,
        int& bloom_calls,
        int& post_process_calls,
        int& smaa_calls) {
        NexAur::VulkanFrameGraphCallbacks callbacks;
        auto add_single_image = [](
            NexAur::VulkanPassGraph&,
            NexAur::VulkanGraphImageHandle) {
            return true;
        };
        callbacks.add_directional_shadow = add_single_image;
        callbacks.add_point_shadow = add_single_image;
        callbacks.add_rect_shadow = add_single_image;
        callbacks.add_skybox = add_single_image;
        callbacks.record_forward = [](VkCommandBuffer) { return true; };
        callbacks.add_ao = [&ao_calls](
            NexAur::VulkanPassGraph&,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle) {
            ++ao_calls;
            return true;
        };
        callbacks.add_ssr = [&ssr_calls](
            NexAur::VulkanPassGraph&,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle) {
            ++ssr_calls;
            return true;
        };
        callbacks.add_debug_draw = [](
            NexAur::VulkanPassGraph&,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle) {
            return true;
        };
        callbacks.add_object_id = [](NexAur::VulkanPassGraph&) { return true; };
        callbacks.add_bloom = [&bloom_calls](
            NexAur::VulkanPassGraph&,
            NexAur::VulkanGraphImageHandle scene_color,
            NexAur::VulkanGraphImageHandle& composite_color) {
            ++bloom_calls;
            composite_color = scene_color;
            return true;
        };
        callbacks.add_post_process = [&post_process_calls](
            NexAur::VulkanPassGraph&,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle) {
            ++post_process_calls;
            return true;
        };
        callbacks.add_smaa = [&smaa_calls](
            NexAur::VulkanPassGraph&,
            NexAur::VulkanGraphImageHandle,
            NexAur::VulkanGraphImageHandle) {
            ++smaa_calls;
            return true;
        };
        callbacks.record_imgui = [](VkCommandBuffer) { return true; };
        return callbacks;
    };

    int ao_calls = 0;
    int ssr_calls = 0;
    int bloom_calls = 0;
    int post_process_calls = 0;
    int smaa_calls = 0;
    NexAur::VulkanFrameGraphCallbacks viewport_callbacks = makeCallbacks(
        ao_calls,
        ssr_calls,
        bloom_calls,
        post_process_calls,
        smaa_calls);
    NexAur::VulkanPassGraph viewport_graph;
    NexAur::VulkanFrameGraphBuilder graph_builder;
    expect(
        graph_builder.build(
            viewport_graph,
            default_plan,
            makeResources(),
            viewport_callbacks),
        "Frame graph builder rejected a complete viewport graph context.");
    expect(
        ao_calls == 1 &&
            ssr_calls == 0 &&
            bloom_calls == 1 &&
            post_process_calls == 1 &&
            smaa_calls == 1,
        "Frame graph builder did not follow the feature plan for viewport output.");

    ao_calls = 0;
    ssr_calls = 0;
    bloom_calls = 0;
    post_process_calls = 0;
    smaa_calls = 0;
    NexAur::VulkanFrameGraphCallbacks direct_callbacks = makeCallbacks(
        ao_calls,
        ssr_calls,
        bloom_calls,
        post_process_calls,
        smaa_calls);
    direct_callbacks.record_imgui = {};
    NexAur::VulkanPassGraph direct_graph;
    expect(
        graph_builder.build(
            direct_graph,
            fallback_plan,
            makeResources(),
            direct_callbacks),
        "Frame graph builder should accept direct output without an ImGui tail callback.");
    expect(
        ssr_calls == 0 && post_process_calls == 1,
        "Frame graph builder did not use the resolved direct-output feature plan.");

    NexAur::VulkanFrameGraphCallbacks incomplete_viewport_callbacks = viewport_callbacks;
    incomplete_viewport_callbacks.record_imgui = {};
    NexAur::VulkanPassGraph incomplete_viewport_graph;
    expect(
        !graph_builder.build(
            incomplete_viewport_graph,
            default_plan,
            makeResources(),
            incomplete_viewport_callbacks),
        "Viewport graph should require its ImGui output tail callback.");

    if (!success) {
        std::cerr << "Frame feature plan smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "Frame feature plan smoke passed." << std::endl;
    return 0;
}

int runRetirementQueueSmoke() {
    struct MoveOnlySentinel {
        explicit MoveOnlySentinel(int& destruction_count)
            : destruction_count(&destruction_count) {}

        MoveOnlySentinel(const MoveOnlySentinel&) = delete;
        MoveOnlySentinel& operator=(const MoveOnlySentinel&) = delete;

        MoveOnlySentinel(MoveOnlySentinel&& other) noexcept
            : destruction_count(std::exchange(other.destruction_count, nullptr)) {}

        MoveOnlySentinel& operator=(MoveOnlySentinel&&) = delete;

        ~MoveOnlySentinel() {
            if (destruction_count != nullptr) {
                ++*destruction_count;
            }
        }

        int* destruction_count = nullptr;
    };

    bool success = true;
    std::string failure;
    auto expect = [&](bool condition, const std::string& message) {
        if (!condition && success) {
            failure = message;
        }
        success = success && condition;
    };

    NexAur::VulkanRetirementQueue queue;
    int immediate_destructions = 0;
    queue.retire(MoveOnlySentinel{ immediate_destructions });
    expect(
        immediate_destructions == 1,
        "Retirement queue should immediately collect resources with no in-flight submission.");

    const uint64_t first_serial = queue.markSubmitted();
    int first_submission_destructions = 0;
    queue.retire(MoveOnlySentinel{ first_submission_destructions });
    expect(
        first_submission_destructions == 0,
        "Retirement queue released an in-flight resource before its fence completed.");

    const uint64_t second_serial = queue.markSubmitted();
    int second_submission_destructions = 0;
    queue.retire(MoveOnlySentinel{ second_submission_destructions });
    queue.markCompleted(first_serial);
    expect(
        first_submission_destructions == 1 &&
            second_submission_destructions == 0,
        "Retirement queue did not collect exactly the completed submission.");

    queue.markCompleted(second_serial + 100u);
    expect(
        second_submission_destructions == 1,
        "Retirement queue did not clamp and collect a completed serial.");

    const NexAur::VulkanRetirementQueueStats completed_stats = queue.getStats();
    expect(
        completed_stats.submitted_serial == second_serial &&
            completed_stats.completed_serial == second_serial &&
            completed_stats.pending_count == 0 &&
            completed_stats.retired_count == 3 &&
            completed_stats.collected_count == 3,
        "Retirement queue reported incorrect completed statistics.");

    const uint64_t drain_serial = queue.markSubmitted();
    int drained_destructions = 0;
    queue.retire(MoveOnlySentinel{ drained_destructions });
    queue.drain();
    const NexAur::VulkanRetirementQueueStats drained_stats = queue.getStats();
    expect(
        drained_destructions == 1 &&
            drained_stats.completed_serial == drain_serial &&
            drained_stats.pending_count == 0 &&
            drained_stats.retired_count == 4 &&
            drained_stats.collected_count == 4,
        "Retirement queue drain did not release and account for pending resources.");

    queue.reset();
    const NexAur::VulkanRetirementQueueStats reset_stats = queue.getStats();
    expect(
        reset_stats.submitted_serial == 0 &&
            reset_stats.completed_serial == 0 &&
            reset_stats.pending_count == 0 &&
            reset_stats.retired_count == 0 &&
            reset_stats.collected_count == 0,
        "Retirement queue reset did not clear serials and statistics.");

    if (!success) {
        std::cerr << "Retirement queue smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "Retirement queue smoke passed." << std::endl;
    return 0;
}

int runFrameContextSmoke() {
    bool success = true;
    std::string failure;
    auto expect = [&](bool condition, const std::string& message) {
        if (!condition && success) {
            failure = message;
        }
        success = success && condition;
    };

    expect(
        NexAur::kVulkanFramesInFlight >= 2u &&
            NexAur::kVulkanFramesInFlight <= 3u,
        "Vulkan frame ring must use two or three frame slots.");

    NexAur::VulkanFrameSlotState frame_context;
    expect(
        !frame_context.isInFlight() &&
            frame_context.getSubmissionSerial() == 0,
        "Default Vulkan frame context reported an in-flight submission.");
    frame_context.markSubmitted(7u);
    expect(
        frame_context.isInFlight() &&
            frame_context.getSubmissionSerial() == 7u,
        "Vulkan frame context did not retain its submission serial.");
    frame_context.markCompleted();
    expect(
        !frame_context.isInFlight() &&
            frame_context.getSubmissionSerial() == 7u,
        "Vulkan frame context did not preserve completed submission identity.");

    NexAur::VulkanSwapchainImageFlightTracker tracker;
    tracker.reset(3u);
    expect(
        tracker.getImageCount() == 3u &&
            tracker.getInFlightImageCount() == 0u &&
            tracker.get(0u) == nullptr,
        "Swapchain image tracker did not initialize empty image ownership.");

    tracker.markSubmitted(0u, 0u, 11u);
    tracker.markSubmitted(1u, 1u, 12u);
    const NexAur::VulkanFrameSubmission* first = tracker.get(0u);
    const NexAur::VulkanFrameSubmission* second = tracker.get(1u);
    expect(
        tracker.getInFlightImageCount() == 2u &&
            first && first->frame_index == 0u && first->serial == 11u &&
            second && second->frame_index == 1u && second->serial == 12u,
        "Swapchain image tracker lost per-image frame ownership.");

    tracker.releaseFrame(0u, 99u);
    expect(
        tracker.getInFlightImageCount() == 2u,
        "Swapchain image tracker released a mismatched submission serial.");
    tracker.releaseFrame(0u, 11u);
    expect(
        tracker.get(0u) == nullptr &&
            tracker.get(1u) != nullptr &&
            tracker.getInFlightImageCount() == 1u,
        "Swapchain image tracker did not release exactly the completed frame.");

    tracker.markSubmitted(1u, 0u, 13u);
    const NexAur::VulkanFrameSubmission* replacement = tracker.get(1u);
    expect(
        replacement &&
            replacement->frame_index == 0u &&
            replacement->serial == 13u,
        "Swapchain image tracker did not replace reacquired image ownership.");
    tracker.reset();
    expect(
        tracker.getImageCount() == 0u &&
            tracker.getInFlightImageCount() == 0u,
        "Swapchain image tracker reset retained stale ownership.");

    if (!success) {
        std::cerr << "Frame context smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "Frame context smoke passed." << std::endl;
    return 0;
}

int runAsyncTransferStateSmoke() {
    struct NestedRetirement {
        NestedRetirement(
            NexAur::VulkanRetirementQueue& queue,
            int& outer_destructions,
            int& inner_destructions)
            : queue(&queue),
              outer_destructions(&outer_destructions),
              inner_destructions(&inner_destructions) {}

        NestedRetirement(const NestedRetirement&) = delete;
        NestedRetirement& operator=(const NestedRetirement&) = delete;

        NestedRetirement(NestedRetirement&& other) noexcept
            : queue(std::exchange(other.queue, nullptr)),
              outer_destructions(std::exchange(other.outer_destructions, nullptr)),
              inner_destructions(std::exchange(other.inner_destructions, nullptr)) {}

        ~NestedRetirement() {
            if (queue == nullptr) {
                return;
            }
            ++*outer_destructions;
            struct InnerRetirement {
                explicit InnerRetirement(int& destruction_count)
                    : destructions(&destruction_count) {}

                int* destructions = nullptr;

                InnerRetirement(const InnerRetirement&) = delete;
                InnerRetirement& operator=(const InnerRetirement&) = delete;

                InnerRetirement(InnerRetirement&& other) noexcept
                    : destructions(std::exchange(other.destructions, nullptr)) {}

                ~InnerRetirement() {
                    if (destructions != nullptr) {
                        ++*destructions;
                    }
                }
            };
            queue->retire(InnerRetirement{ *inner_destructions });
        }

        NexAur::VulkanRetirementQueue* queue = nullptr;
        int* outer_destructions = nullptr;
        int* inner_destructions = nullptr;
    };

    bool success = true;
    std::string failure;
    auto expect = [&](bool condition, const std::string& message) {
        if (!condition && success) {
            failure = message;
        }
        success = success && condition;
    };

    NexAur::VulkanUploadBudget budget;
    budget.max_bytes = 100;
    budget.max_requests = 2;
    NexAur::VulkanUploadBudgetTracker tracker(budget);
    expect(
        tracker.tryReserve(60),
        "Async upload budget rejected its first valid request.");
    expect(
        !tracker.tryReserve(41),
        "Async upload budget exceeded its byte limit.");
    expect(
        tracker.tryReserve(40) &&
            tracker.getReservedBytes() == 100 &&
            tracker.getReservedRequests() == 2,
        "Async upload budget did not accept its exact remaining capacity.");
    expect(
        !tracker.tryReserve(1),
        "Async upload budget exceeded its request limit.");

    NexAur::VulkanUploadTicket empty_ticket;
    expect(
        !empty_ticket.valid() &&
            empty_ticket.getStatus() == NexAur::VulkanUploadStatus::Failed &&
            empty_ticket.isTerminal(),
        "Default async upload ticket did not report an explicit failed state.");
    empty_ticket.cancel();
    expect(
        empty_ticket.getStatus() == NexAur::VulkanUploadStatus::Failed,
        "Cancelling an invalid upload ticket changed its terminal state.");

    NexAur::ViewportPickRequest pick_request;
    NexAur::ViewportPickResult pick_result;
    expect(
        pick_request.request_id == 0 &&
            pick_result.status == NexAur::ViewportPickStatus::Unsupported &&
            !pick_result.supported &&
            !pick_result.ready,
        "Viewport picking defaults did not represent a new unsupported request.");
    pick_result.status = NexAur::ViewportPickStatus::Cancelled;
    expect(
        pick_result.status != NexAur::ViewportPickStatus::Pending &&
            pick_result.status != NexAur::ViewportPickStatus::Ready,
        "Viewport picking cancellation was not a distinct terminal state.");

    NexAur::VulkanRetirementQueue retirement_queue;
    int outer_destructions = 0;
    int inner_destructions = 0;
    const uint64_t outer_serial = retirement_queue.markSubmitted();
    retirement_queue.retire(NestedRetirement{
        retirement_queue,
        outer_destructions,
        inner_destructions
    });
    const uint64_t inner_serial = retirement_queue.markSubmitted();
    retirement_queue.markCompleted(outer_serial);
    const NexAur::VulkanRetirementQueueStats nested_stats =
        retirement_queue.getStats();
    expect(
        outer_destructions == 1 &&
            inner_destructions == 0 &&
            nested_stats.pending_count == 1 &&
            nested_stats.retired_count == 2 &&
            nested_stats.collected_count == 1,
        "Nested async resource retirement did not remain queued for its serial.");
    retirement_queue.markCompleted(inner_serial);
    const NexAur::VulkanRetirementQueueStats retirement_stats =
        retirement_queue.getStats();
    expect(
        inner_destructions == 1 &&
            retirement_stats.pending_count == 0 &&
            retirement_stats.collected_count == 2,
        "Nested async resource retirement was not collected after completion.");

    if (!success) {
        std::cerr << "Async transfer state smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "Async transfer state smoke passed." << std::endl;
    return 0;
}


int runRenderFrameContractSmoke() {
    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };
    auto matrixExactlyEqual = [](const glm::mat4& lhs, const glm::mat4& rhs) {
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                if (lhs[column][row] != rhs[column][row]) {
                    return false;
                }
            }
        }
        return true;
    };
    auto projectNdc = [](const glm::mat4& projection, const glm::vec3& view_position) {
        const glm::vec4 clip = projection * glm::vec4{ view_position, 1.0f };
        return glm::vec3{ clip } / clip.w;
    };

    NexAur::RenderContext render_context;
    render_context.swapBuffers();
    const uint64_t first_frame_serial = render_context.getReadData().frame_serial;
    render_context.swapBuffers();
    const uint64_t second_frame_serial = render_context.getReadData().frame_serial;
    expect(
        first_frame_serial == 1u && second_frame_serial == 2u,
        "Render frame contract smoke failed: RenderContext frame serial did not advance per swap.");

    constexpr uint32_t viewport_width = 1280u;
    constexpr uint32_t viewport_height = 720u;
    constexpr float near_clip = 0.1f;
    constexpr float far_clip = 120.0f;

    NexAur::RenderDataPacket render_data;
    render_data.frame_serial = 37u;
    render_data.scene_id = 91u;
    render_data.render_settings.ssr.enabled = true;
    render_data.render_settings.ssr.max_steps = 47u;
    render_data.debug_visualization_options.enabled = true;
    render_data.debug_visualization_options.camera_frustum = false;
    render_data.camera_data.position = glm::vec3{ 3.0f, 2.0f, 5.0f };
    render_data.camera_data.near_clip = near_clip;
    render_data.camera_data.far_clip = far_clip;
    render_data.camera_data.view_matrix = glm::lookAt(
        render_data.camera_data.position,
        glm::vec3{ 0.0f },
        glm::vec3{ 0.0f, 1.0f, 0.0f });
    render_data.camera_data.projection_matrix = glm::perspective(
        glm::radians(60.0f),
        static_cast<float>(viewport_width) / static_cast<float>(viewport_height),
        near_clip,
        far_clip);

    NexAur::RenderObjectData opaque_object;
    opaque_object.model_asset = NexAur::AssetHandle{ NexAur::UUID{ 1001u } };
    render_data.opaque_objects.push_back(opaque_object);
    NexAur::RenderObjectData transparent_object = opaque_object;
    transparent_object.model_asset = NexAur::AssetHandle{ NexAur::UUID{ 1002u } };
    render_data.transparent_objects.push_back(transparent_object);

    NexAur::RendererPointLightData point_light;
    point_light.cast_shadow = true;
    render_data.point_lights_data.push_back(point_light);
    point_light.cast_shadow = false;
    render_data.point_lights_data.push_back(point_light);

    NexAur::RendererRectLightData rect_light;
    rect_light.cast_shadow = true;
    render_data.rect_lights_data.push_back(rect_light);

    NexAur::RendererReflectionProbeData active_probe;
    active_probe.entity_id = 201;
    active_probe.baked_environment_asset = NexAur::AssetHandle{ NexAur::UUID{ 2001u } };
    render_data.reflection_probes_data.push_back(active_probe);
    NexAur::RendererReflectionProbeData disabled_probe = active_probe;
    disabled_probe.entity_id = 202;
    disabled_probe.enabled = false;
    disabled_probe.baked_environment_asset = NexAur::AssetHandle{ NexAur::UUID{ 2002u } };
    render_data.reflection_probes_data.push_back(disabled_probe);
    NexAur::RendererReflectionProbeData zero_intensity_probe = active_probe;
    zero_intensity_probe.entity_id = 203;
    zero_intensity_probe.intensity = 0.0f;
    zero_intensity_probe.diffuse_intensity = 0.0f;
    zero_intensity_probe.baked_environment_asset = NexAur::AssetHandle{ NexAur::UUID{ 2003u } };
    render_data.reflection_probes_data.push_back(zero_intensity_probe);

    NexAur::RenderDebugLine debug_line;
    debug_line.end = glm::vec3{ 1.0f, 0.0f, 0.0f };
    render_data.debug_draw.lines.push_back(debug_line);

    NexAur::RenderSceneFrameBuilder frame_builder;
    const NexAur::RenderSceneFrame scene_frame = frame_builder.buildRenderSceneFrame(
        render_data,
        viewport_width,
        viewport_height);

    expect(
        scene_frame.frame_serial == render_data.frame_serial &&
        scene_frame.scene_id == render_data.scene_id &&
        scene_frame.render_settings.ssr.enabled &&
        scene_frame.render_settings.ssr.max_steps == 47u &&
        scene_frame.debug_visualization_options.enabled &&
        !scene_frame.debug_visualization_options.camera_frustum,
        "Render frame contract smoke failed: frame metadata did not survive canonical frame construction.");
    expect(
        scene_frame.source_counts.opaque_object_count == 1u &&
        scene_frame.source_counts.transparent_object_count == 1u &&
        scene_frame.source_counts.point_light_count == 2u &&
        scene_frame.source_counts.point_shadow_request_count == 1u &&
        scene_frame.source_counts.rect_light_count == 1u &&
        scene_frame.source_counts.rect_shadow_request_count == 1u &&
        scene_frame.source_counts.reflection_probe_count == 3u &&
        scene_frame.source_counts.debug_line_count == 1u,
        "Render frame contract smoke failed: source diagnostics counts were not preserved.");
    expect(
        scene_frame.reflection_probes.size() == 1u &&
        scene_frame.reflection_probe_references.size() == 3u &&
        scene_frame.reflection_probe_references[0].entity_id == 201 &&
        scene_frame.reflection_probe_references[1].entity_id == 202 &&
        scene_frame.reflection_probe_references[2].entity_id == 203 &&
        scene_frame.reflection_probe_references[1].baked_environment_asset ==
            disabled_probe.baked_environment_asset &&
        scene_frame.reflection_probe_references[2].baked_environment_asset ==
            zero_intensity_probe.baked_environment_asset,
        "Render frame contract smoke failed: complete reflection probe references were not preserved.");

    const glm::vec3 canonical_near = projectNdc(
        scene_frame.view.projection_matrix,
        glm::vec3{ 0.0f, 0.0f, -near_clip });
    const glm::vec3 canonical_far = projectNdc(
        scene_frame.view.projection_matrix,
        glm::vec3{ 0.0f, 0.0f, -far_clip });
    expect(
        nearlyEqual(canonical_near.z, -1.0f) && nearlyEqual(canonical_far.z, 1.0f),
        "Render frame contract smoke failed: canonical projection depth is not [-1, 1].");

    NexAur::VulkanRenderDataTranslator translator;
    const NexAur::VulkanRenderView vulkan_view = translator.buildRenderView(scene_frame.view);
    glm::mat4 legacy_clip_transform{ 1.0f };
    legacy_clip_transform[1][1] = -1.0f;
    legacy_clip_transform[2][2] = 0.5f;
    legacy_clip_transform[3][2] = 0.5f;
    const glm::mat4 legacy_vulkan_projection =
        legacy_clip_transform * render_data.camera_data.projection_matrix;
    expect(
        matrixExactlyEqual(vulkan_view.projection_matrix, legacy_vulkan_projection),
        "Render frame contract smoke failed: Vulkan projection changed from the renderer baseline.");

    const glm::vec3 vulkan_near = projectNdc(
        vulkan_view.projection_matrix,
        glm::vec3{ 0.0f, 0.0f, -near_clip });
    const glm::vec3 vulkan_far = projectNdc(
        vulkan_view.projection_matrix,
        glm::vec3{ 0.0f, 0.0f, -far_clip });
    const glm::vec3 canonical_y = projectNdc(
        scene_frame.view.projection_matrix,
        glm::vec3{ 0.0f, 0.5f, -2.0f });
    const glm::vec3 vulkan_y = projectNdc(
        vulkan_view.projection_matrix,
        glm::vec3{ 0.0f, 0.5f, -2.0f });
    expect(
        nearlyEqual(vulkan_near.z, 0.0f) &&
        nearlyEqual(vulkan_far.z, 1.0f) &&
        vulkan_view.projection_matrix[1][1] == -scene_frame.view.projection_matrix[1][1] &&
        nearlyEqual(vulkan_y.y, -canonical_y.y),
        "Render frame contract smoke failed: Vulkan depth remap or clip Y conversion is incorrect.");

    const glm::vec3 expected_view_position{ 0.7f, -0.4f, -6.0f };
    const glm::vec3 projected_view_position = projectNdc(
        vulkan_view.projection_matrix,
        expected_view_position);
    const glm::vec2 ssr_uv = glm::vec2{ projected_view_position } * 0.5f + 0.5f;
    glm::vec4 reconstructed_view_position =
        vulkan_view.inverse_projection_matrix *
        glm::vec4{ ssr_uv * 2.0f - 1.0f, projected_view_position.z, 1.0f };
    reconstructed_view_position /= reconstructed_view_position.w;
    expect(
        nearlyEqualVec3(glm::vec3{ reconstructed_view_position }, expected_view_position),
        "Render frame contract smoke failed: SSR depth reconstruction did not recover view space.");

    const glm::vec4 expected_world_position =
        vulkan_view.inverse_view_matrix * glm::vec4{ 0.45f, 0.3f, -4.0f, 1.0f };
    const glm::vec4 pick_clip = vulkan_view.view_projection_matrix * expected_world_position;
    const glm::vec3 pick_ndc = glm::vec3{ pick_clip } / pick_clip.w;
    const glm::vec2 pick_pixel = glm::vec2{
        (pick_ndc.x * 0.5f + 0.5f) * static_cast<float>(viewport_width),
        (pick_ndc.y * 0.5f + 0.5f) * static_cast<float>(viewport_height)
    };
    const glm::vec2 picking_ndc_xy{
        pick_pixel.x / static_cast<float>(viewport_width) * 2.0f - 1.0f,
        pick_pixel.y / static_cast<float>(viewport_height) * 2.0f - 1.0f
    };
    glm::vec4 reconstructed_world_position =
        glm::inverse(vulkan_view.view_projection_matrix) *
        glm::vec4{ picking_ndc_xy, pick_ndc.z, 1.0f };
    reconstructed_world_position /= reconstructed_world_position.w;
    expect(
        pick_pixel.y < static_cast<float>(viewport_height) * 0.5f &&
        nearlyEqualVec3(
            glm::vec3{ reconstructed_world_position },
            glm::vec3{ expected_world_position }),
        "Render frame contract smoke failed: top-left picking projection did not round-trip.");

    if (!success) {
        std::cerr << failure << std::endl;
        return 1;
    }

    std::cout << "Render frame contract smoke passed." << std::endl;
    return 0;
}


int runRenderSettingsSmoke() {
    NexAur::RenderContext render_context;

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    std::array<NexAur::ReflectionProbeResidencyCandidate, 6> residency_candidates{
        NexAur::ReflectionProbeResidencyCandidate{ 1, 1, 1, true, true, false },
        NexAur::ReflectionProbeResidencyCandidate{ 2, 2, 1, true, false, false },
        NexAur::ReflectionProbeResidencyCandidate{ 3, 3, 1, true, false, true },
        NexAur::ReflectionProbeResidencyCandidate{ 4, 10, 5, true, false, false },
        NexAur::ReflectionProbeResidencyCandidate{ 5, 10, 3, true, false, false },
        NexAur::ReflectionProbeResidencyCandidate{ 6, 0, 0, false, false, false }
    };
    expect(
        NexAur::selectReflectionProbeEvictionCandidate(residency_candidates, 2) == 5,
        "Reflection probe residency policy did not preserve pinned, active, and pending captures.");
    residency_candidates[4].pinned = true;
    residency_candidates[3].pinned = true;
    expect(
        NexAur::selectReflectionProbeEvictionCandidate(residency_candidates, 2) == -1,
        "Reflection probe residency policy should reject eviction when every resident capture is protected.");

    NexAur::RenderSettings settings;
    settings.lighting.preset = NexAur::RenderLightingPreset::Custom;
    settings.lighting.directional_light_intensity_scale = 0.25f;
    settings.lighting.point_light_intensity_scale = 1.5f;
    settings.lighting.rect_light_intensity_scale = 1.25f;
    settings.lighting.skybox_intensity_scale = 0.5f;
    settings.lighting.ibl_intensity_scale = 0.75f;
    settings.post_process.tone_mapping_mode = NexAur::RenderToneMappingMode::None;
    settings.post_process.exposure = 1.75f;
    settings.post_process.bloom_enabled = false;
    settings.post_process.bloom_intensity = 0.25f;
    settings.post_process.bloom_scatter = 0.5f;
    settings.post_process.bloom_radius = 1.25f;
    settings.post_process.color_grading_enabled = false;
    settings.post_process.color_grading_exposure_offset = 0.35f;
    settings.post_process.color_grading_contrast = 1.2f;
    settings.post_process.color_grading_saturation = 0.8f;
    settings.post_process.color_grading_temperature = 0.25f;
    settings.post_process.color_grading_tint = -0.15f;
    settings.post_process.color_grading_black_point = 0.02f;
    settings.post_process.color_grading_white_point = 1.1f;
    settings.post_process.vignette_intensity = 0.3f;
    settings.post_process.vignette_radius = 0.65f;
    settings.post_process.vignette_softness = 0.4f;
    settings.post_process.sharpen_intensity = 0.2f;
    settings.anti_aliasing.mode = NexAur::RenderAntiAliasingMode::None;
    settings.anti_aliasing.smaa_edge_threshold = 0.18f;
    settings.anti_aliasing.smaa_contrast_factor = 3.0f;
    settings.anti_aliasing.smaa_max_search_steps = 6u;
    settings.anti_aliasing.smaa_blend_strength = 0.55f;
    settings.ao.enabled = true;
    settings.ao.radius = 1.4f;
    settings.ao.intensity = 0.7f;
    settings.ao.bias = 0.03f;
    settings.ao.power = 1.35f;
    settings.ao.blur_enabled = false;
    settings.ao.half_resolution = false;
    settings.ssr.enabled = true;
    settings.ssr.max_distance = 24.0f;
    settings.ssr.max_steps = 40u;
    settings.ssr.thickness = 0.22f;
    settings.ssr.stride = 1.5f;
    settings.ssr.roughness_fade = 0.55f;
    settings.ssr.edge_fade = 0.18f;
    settings.ssr.intensity = 0.8f;
    settings.ibl_debug.mode = NexAur::RenderIblDebugMode::MaterialBaseColor;
    settings.ibl_debug.prefilter_mip = 3.0f;
    settings.effects_debug.view = NexAur::RenderEffectDebugView::SmaaEdgeMask;
    settings.effects_debug.bloom_mip = 3u;
    settings.effects_debug.shadow_cascade = 2u;
    settings.effects_debug.point_shadow_layer = 5u;
    settings.effects_debug.rect_shadow_layer = 2u;
    settings.shadow.enabled = false;
    settings.shadow.filter_mode = NexAur::RenderShadowFilterMode::PCSS;
    settings.shadow.strength = 0.45f;
    settings.shadow.constant_bias = 0.006f;
    settings.shadow.normal_bias = 0.05f;
    settings.shadow.slope_bias = 0.003f;
    settings.shadow.filter_radius = 2.0f;
    settings.shadow.pcss_light_radius = 1.25f;
    settings.shadow.pcss_search_radius = 5.5f;
    settings.shadow.pcss_min_filter_radius = 0.5f;
    settings.shadow.pcss_max_filter_radius = 8.0f;
    settings.shadow.distance = 64.0f;
    settings.shadow.map_resolution = 4096u;
    settings.shadow.stabilize = false;
    settings.shadow.cascades_enabled = true;
    settings.shadow.cascade_count = 4u;
    settings.shadow.cascade_split_lambda = 0.82f;
    settings.shadow.cascade_debug_overlay = true;
    settings.point_shadow.enabled = true;
    settings.point_shadow.max_shadowed_lights = 2u;
    settings.point_shadow.map_resolution = 1024u;
    settings.point_shadow.strength = 0.62f;
    settings.point_shadow.constant_bias = 0.012f;
    settings.point_shadow.normal_bias = 0.025f;
    settings.point_shadow.filter_radius = 1.5f;
    settings.contact_shadow.enabled = true;
    settings.contact_shadow.intensity = 0.4f;
    settings.contact_shadow.max_distance = 0.55f;
    settings.contact_shadow.thickness = 0.07f;
    settings.rect_shadow.enabled = false;
    settings.rect_shadow.max_shadowed_lights = 3u;
    settings.rect_shadow.map_resolution = 2048u;
    settings.rect_shadow.strength = 0.58f;
    settings.rect_shadow.constant_bias = 0.018f;
    settings.rect_shadow.normal_bias = 0.035f;
    settings.rect_shadow.filter_radius = 2.25f;
    settings.rect_shadow.projection_margin = 0.55f;
    settings.rect_shadow.soft_shadow_enabled = false;
    settings.rect_shadow.pcss_light_radius = 1.35f;
    settings.rect_shadow.pcss_search_radius = 6.25f;
    settings.rect_shadow.pcss_min_filter_radius = 0.75f;
    settings.rect_shadow.pcss_max_filter_radius = 9.5f;
    settings.rect_shadow.pcss_blocker_taps = 6u;
    settings.rect_shadow.pcss_filter_taps = 12u;
    settings.rect_light.enabled = false;
    settings.rect_light.max_lights = 7u;
    settings.rect_light.ltc_specular_enabled = false;
    settings.rect_light.specular_intensity_scale = 1.75f;
    settings.rect_light.debug_ltc_only = true;
    render_context.setRenderSettings(settings);
    render_context.getWriteData().render_settings = render_context.getRenderSettings();
    render_context.swapBuffers();

    const NexAur::RenderPostProcessSettings& first_post_process =
        render_context.getReadData().render_settings.post_process;
    const NexAur::RenderAntiAliasingSettings& first_anti_aliasing =
        render_context.getReadData().render_settings.anti_aliasing;
    const NexAur::RenderLightingCalibrationSettings& first_lighting =
        render_context.getReadData().render_settings.lighting;
    const NexAur::RenderAoSettings& first_ao =
        render_context.getReadData().render_settings.ao;
    const NexAur::RenderSsrSettings& first_ssr =
        render_context.getReadData().render_settings.ssr;
    const NexAur::RenderIblDebugSettings& first_ibl_debug =
        render_context.getReadData().render_settings.ibl_debug;
    const NexAur::RenderEffectDebugSettings& first_effects_debug =
        render_context.getReadData().render_settings.effects_debug;
    const NexAur::RenderShadowSettings& first_shadow =
        render_context.getReadData().render_settings.shadow;
    const NexAur::RenderPointShadowSettings& first_point_shadow =
        render_context.getReadData().render_settings.point_shadow;
    const NexAur::RenderContactShadowSettings& first_contact_shadow =
        render_context.getReadData().render_settings.contact_shadow;
    const NexAur::RenderRectShadowSettings& first_rect_shadow =
        render_context.getReadData().render_settings.rect_shadow;
    const NexAur::RenderRectLightSettings& first_rect_light =
        render_context.getReadData().render_settings.rect_light;
    expect(
        first_post_process.tone_mapping_mode == NexAur::RenderToneMappingMode::None,
        "RenderSettings smoke failed: tone mapping mode did not reach the read packet.");
    expect(
        nearlyEqual(first_post_process.exposure, 1.75f),
        "RenderSettings smoke failed: exposure did not reach the read packet.");
    expect(!first_post_process.bloom_enabled, "RenderSettings smoke failed: bloom enabled did not reach the read packet.");
    expect(
        nearlyEqual(first_post_process.bloom_intensity, 0.25f) &&
        nearlyEqual(first_post_process.bloom_scatter, 0.5f) &&
        nearlyEqual(first_post_process.bloom_radius, 1.25f),
        "RenderSettings smoke failed: bloom parameters did not reach the read packet.");
    expect(
        !first_post_process.color_grading_enabled &&
        nearlyEqual(first_post_process.color_grading_exposure_offset, 0.35f) &&
        nearlyEqual(first_post_process.color_grading_contrast, 1.2f) &&
        nearlyEqual(first_post_process.color_grading_saturation, 0.8f) &&
        nearlyEqual(first_post_process.color_grading_temperature, 0.25f) &&
        nearlyEqual(first_post_process.color_grading_tint, -0.15f) &&
        nearlyEqual(first_post_process.color_grading_black_point, 0.02f) &&
        nearlyEqual(first_post_process.color_grading_white_point, 1.1f) &&
        nearlyEqual(first_post_process.vignette_intensity, 0.3f) &&
        nearlyEqual(first_post_process.vignette_radius, 0.65f) &&
        nearlyEqual(first_post_process.vignette_softness, 0.4f) &&
        nearlyEqual(first_post_process.sharpen_intensity, 0.2f),
        "RenderSettings smoke failed: color grading parameters did not reach the read packet.");
    expect(
        first_anti_aliasing.mode == NexAur::RenderAntiAliasingMode::None &&
        nearlyEqual(first_anti_aliasing.smaa_edge_threshold, 0.18f) &&
        nearlyEqual(first_anti_aliasing.smaa_contrast_factor, 3.0f) &&
        first_anti_aliasing.smaa_max_search_steps == 6u &&
        nearlyEqual(first_anti_aliasing.smaa_blend_strength, 0.55f),
        "RenderSettings smoke failed: anti-aliasing parameters did not reach the read packet.");
    expect(
        first_lighting.preset == NexAur::RenderLightingPreset::Custom &&
        nearlyEqual(first_lighting.directional_light_intensity_scale, 0.25f) &&
        nearlyEqual(first_lighting.point_light_intensity_scale, 1.5f) &&
        nearlyEqual(first_lighting.rect_light_intensity_scale, 1.25f) &&
        nearlyEqual(first_lighting.skybox_intensity_scale, 0.5f) &&
        nearlyEqual(first_lighting.ibl_intensity_scale, 0.75f),
        "RenderSettings smoke failed: lighting calibration settings did not reach the read packet.");
    expect(
        first_ao.enabled &&
        nearlyEqual(first_ao.radius, 1.4f) &&
        nearlyEqual(first_ao.intensity, 0.7f) &&
        nearlyEqual(first_ao.bias, 0.03f) &&
        nearlyEqual(first_ao.power, 1.35f) &&
        !first_ao.blur_enabled &&
        !first_ao.half_resolution,
        "RenderSettings smoke failed: AO settings did not reach the read packet.");
    expect(
        first_ssr.enabled &&
        nearlyEqual(first_ssr.max_distance, 24.0f) &&
        first_ssr.max_steps == 40u &&
        nearlyEqual(first_ssr.thickness, 0.22f) &&
        nearlyEqual(first_ssr.stride, 1.5f) &&
        nearlyEqual(first_ssr.roughness_fade, 0.55f) &&
        nearlyEqual(first_ssr.edge_fade, 0.18f) &&
        nearlyEqual(first_ssr.intensity, 0.8f),
        "RenderSettings smoke failed: SSR settings did not reach the read packet.");
    expect(
        first_ibl_debug.mode == NexAur::RenderIblDebugMode::MaterialBaseColor &&
        nearlyEqual(first_ibl_debug.prefilter_mip, 3.0f),
        "RenderSettings smoke failed: IBL debug settings did not reach the read packet.");
    expect(
        first_effects_debug.view == NexAur::RenderEffectDebugView::SmaaEdgeMask &&
        first_effects_debug.bloom_mip == 3u &&
        first_effects_debug.shadow_cascade == 2u &&
        first_effects_debug.point_shadow_layer == 5u &&
        first_effects_debug.rect_shadow_layer == 2u,
        "RenderSettings smoke failed: effects debug settings did not reach the read packet.");
    expect(
        !first_shadow.enabled &&
        first_shadow.filter_mode == NexAur::RenderShadowFilterMode::PCSS &&
        nearlyEqual(first_shadow.strength, 0.45f) &&
        nearlyEqual(first_shadow.constant_bias, 0.006f) &&
        nearlyEqual(first_shadow.normal_bias, 0.05f) &&
        nearlyEqual(first_shadow.slope_bias, 0.003f) &&
        nearlyEqual(first_shadow.filter_radius, 2.0f) &&
        nearlyEqual(first_shadow.pcss_light_radius, 1.25f) &&
        nearlyEqual(first_shadow.pcss_search_radius, 5.5f) &&
        nearlyEqual(first_shadow.pcss_min_filter_radius, 0.5f) &&
        nearlyEqual(first_shadow.pcss_max_filter_radius, 8.0f) &&
        nearlyEqual(first_shadow.distance, 64.0f) &&
        first_shadow.map_resolution == 4096u &&
        !first_shadow.stabilize &&
        first_shadow.cascades_enabled &&
        first_shadow.cascade_count == 4u &&
        nearlyEqual(first_shadow.cascade_split_lambda, 0.82f) &&
        first_shadow.cascade_debug_overlay,
        "RenderSettings smoke failed: shadow settings did not reach the read packet.");
    expect(
        first_point_shadow.enabled &&
        first_point_shadow.max_shadowed_lights == 2u &&
        first_point_shadow.map_resolution == 1024u &&
        nearlyEqual(first_point_shadow.strength, 0.62f) &&
        nearlyEqual(first_point_shadow.constant_bias, 0.012f) &&
        nearlyEqual(first_point_shadow.normal_bias, 0.025f) &&
        nearlyEqual(first_point_shadow.filter_radius, 1.5f),
        "RenderSettings smoke failed: point shadow settings did not reach the read packet.");
    expect(
        first_contact_shadow.enabled &&
        nearlyEqual(first_contact_shadow.intensity, 0.4f) &&
        nearlyEqual(first_contact_shadow.max_distance, 0.55f) &&
        nearlyEqual(first_contact_shadow.thickness, 0.07f),
        "RenderSettings smoke failed: contact shadow settings did not reach the read packet.");
    expect(
        !first_rect_shadow.enabled &&
        first_rect_shadow.max_shadowed_lights == 3u &&
        first_rect_shadow.map_resolution == 2048u &&
        nearlyEqual(first_rect_shadow.strength, 0.58f) &&
        nearlyEqual(first_rect_shadow.constant_bias, 0.018f) &&
        nearlyEqual(first_rect_shadow.normal_bias, 0.035f) &&
        nearlyEqual(first_rect_shadow.filter_radius, 2.25f) &&
        nearlyEqual(first_rect_shadow.projection_margin, 0.55f) &&
        !first_rect_shadow.soft_shadow_enabled &&
        nearlyEqual(first_rect_shadow.pcss_light_radius, 1.35f) &&
        nearlyEqual(first_rect_shadow.pcss_search_radius, 6.25f) &&
        nearlyEqual(first_rect_shadow.pcss_min_filter_radius, 0.75f) &&
        nearlyEqual(first_rect_shadow.pcss_max_filter_radius, 9.5f) &&
        first_rect_shadow.pcss_blocker_taps == 6u &&
        first_rect_shadow.pcss_filter_taps == 12u,
        "RenderSettings smoke failed: rect shadow settings did not reach the read packet.");
    expect(
        !first_rect_light.enabled &&
        first_rect_light.max_lights == 7u &&
        !first_rect_light.ltc_specular_enabled &&
        nearlyEqual(first_rect_light.specular_intensity_scale, 1.75f) &&
        first_rect_light.debug_ltc_only,
        "RenderSettings smoke failed: rect light settings did not reach the read packet.");

    NexAur::applyRenderLightingPreset(settings, NexAur::RenderLightingPreset::Cornell);
    settings.ao.enabled = false;
    settings.ao.radius = 0.8f;
    settings.ao.intensity = 0.25f;
    settings.ao.bias = 0.01f;
    settings.ao.power = 2.0f;
    settings.ao.blur_enabled = true;
    settings.ao.half_resolution = true;
    settings.ssr.enabled = false;
    settings.ssr.max_distance = 18.0f;
    settings.ssr.max_steps = 32u;
    settings.ssr.thickness = 0.18f;
    settings.ssr.stride = 1.0f;
    settings.ssr.roughness_fade = 0.65f;
    settings.ssr.edge_fade = 0.12f;
    settings.ssr.intensity = 1.0f;
    settings.ibl_debug.mode = NexAur::RenderIblDebugMode::FinalLit;
    settings.ibl_debug.prefilter_mip = 0.0f;
    settings.post_process.color_grading_enabled = true;
    settings.post_process.color_grading_exposure_offset = 0.0f;
    settings.post_process.color_grading_contrast = 1.0f;
    settings.post_process.color_grading_saturation = 1.0f;
    settings.post_process.color_grading_temperature = 0.0f;
    settings.post_process.color_grading_tint = 0.0f;
    settings.post_process.color_grading_black_point = 0.0f;
    settings.post_process.color_grading_white_point = 1.0f;
    settings.post_process.vignette_intensity = 0.0f;
    settings.post_process.vignette_radius = 0.75f;
    settings.post_process.vignette_softness = 0.35f;
    settings.post_process.sharpen_intensity = 0.0f;
    settings.anti_aliasing.mode = NexAur::RenderAntiAliasingMode::SMAA;
    settings.anti_aliasing.smaa_edge_threshold = 0.08f;
    settings.anti_aliasing.smaa_contrast_factor = 2.0f;
    settings.anti_aliasing.smaa_max_search_steps = 12u;
    settings.anti_aliasing.smaa_blend_strength = 0.85f;
    settings.effects_debug.view = NexAur::RenderEffectDebugView::SmaaOutput;
    settings.effects_debug.bloom_mip = 0u;
    settings.effects_debug.shadow_cascade = 1u;
    settings.effects_debug.point_shadow_layer = 2u;
    settings.effects_debug.rect_shadow_layer = 0u;
    settings.shadow.enabled = true;
    settings.shadow.filter_mode = NexAur::RenderShadowFilterMode::PoissonPCF;
    settings.shadow.strength = 0.7f;
    settings.shadow.constant_bias = 0.003f;
    settings.shadow.normal_bias = 0.0f;
    settings.shadow.slope_bias = 0.001f;
    settings.shadow.filter_radius = 1.0f;
    settings.shadow.pcss_light_radius = 0.5f;
    settings.shadow.pcss_search_radius = 3.0f;
    settings.shadow.pcss_min_filter_radius = 0.75f;
    settings.shadow.pcss_max_filter_radius = 6.0f;
    settings.shadow.distance = 35.0f;
    settings.shadow.map_resolution = 2048u;
    settings.shadow.stabilize = true;
    settings.shadow.cascades_enabled = false;
    settings.shadow.cascade_count = 1u;
    settings.shadow.cascade_split_lambda = 0.65f;
    settings.shadow.cascade_debug_overlay = false;
    settings.point_shadow.enabled = true;
    settings.point_shadow.max_shadowed_lights = 1u;
    settings.point_shadow.map_resolution = 512u;
    settings.point_shadow.strength = 0.85f;
    settings.point_shadow.constant_bias = 0.015f;
    settings.point_shadow.normal_bias = 0.02f;
    settings.point_shadow.filter_radius = 1.0f;
    settings.contact_shadow.enabled = false;
    settings.contact_shadow.intensity = 0.35f;
    settings.contact_shadow.max_distance = 0.45f;
    settings.contact_shadow.thickness = 0.08f;
    settings.rect_shadow.enabled = true;
    settings.rect_shadow.max_shadowed_lights = 1u;
    settings.rect_shadow.map_resolution = 1024u;
    settings.rect_shadow.strength = 0.85f;
    settings.rect_shadow.constant_bias = 0.01f;
    settings.rect_shadow.normal_bias = 0.02f;
    settings.rect_shadow.filter_radius = 1.0f;
    settings.rect_shadow.projection_margin = 0.35f;
    settings.rect_shadow.soft_shadow_enabled = true;
    settings.rect_shadow.pcss_light_radius = 0.75f;
    settings.rect_shadow.pcss_search_radius = 3.0f;
    settings.rect_shadow.pcss_min_filter_radius = 0.5f;
    settings.rect_shadow.pcss_max_filter_radius = 5.0f;
    settings.rect_shadow.pcss_blocker_taps = 8u;
    settings.rect_shadow.pcss_filter_taps = 16u;
    settings.rect_light.enabled = true;
    settings.rect_light.max_lights = 1u;
    settings.rect_light.ltc_specular_enabled = true;
    settings.rect_light.specular_intensity_scale = 1.0f;
    settings.rect_light.debug_ltc_only = false;
    render_context.setRenderSettings(settings);
    render_context.getWriteData().render_settings = render_context.getRenderSettings();
    render_context.swapBuffers();

    const NexAur::RenderPostProcessSettings& second_post_process =
        render_context.getReadData().render_settings.post_process;
    const NexAur::RenderAntiAliasingSettings& second_anti_aliasing =
        render_context.getReadData().render_settings.anti_aliasing;
    const NexAur::RenderLightingCalibrationSettings& second_lighting =
        render_context.getReadData().render_settings.lighting;
    const NexAur::RenderAoSettings& second_ao =
        render_context.getReadData().render_settings.ao;
    const NexAur::RenderSsrSettings& second_ssr =
        render_context.getReadData().render_settings.ssr;
    const NexAur::RenderIblDebugSettings& second_ibl_debug =
        render_context.getReadData().render_settings.ibl_debug;
    const NexAur::RenderEffectDebugSettings& second_effects_debug =
        render_context.getReadData().render_settings.effects_debug;
    const NexAur::RenderShadowSettings& second_shadow =
        render_context.getReadData().render_settings.shadow;
    const NexAur::RenderPointShadowSettings& second_point_shadow =
        render_context.getReadData().render_settings.point_shadow;
    const NexAur::RenderContactShadowSettings& second_contact_shadow =
        render_context.getReadData().render_settings.contact_shadow;
    const NexAur::RenderRectShadowSettings& second_rect_shadow =
        render_context.getReadData().render_settings.rect_shadow;
    const NexAur::RenderRectLightSettings& second_rect_light =
        render_context.getReadData().render_settings.rect_light;
    expect(
        second_post_process.tone_mapping_mode == NexAur::RenderToneMappingMode::ACES,
        "RenderSettings smoke failed: ACES mode did not reach the read packet.");
    expect(
        nearlyEqual(second_post_process.exposure, 1.0f),
        "RenderSettings smoke failed: Cornell exposure did not reach the read packet.");
    expect(second_post_process.bloom_enabled, "RenderSettings smoke failed: updated bloom enabled did not reach the read packet.");
    expect(
        nearlyEqual(second_post_process.bloom_intensity, 0.02f) &&
        nearlyEqual(second_post_process.bloom_scatter, 0.7f) &&
        nearlyEqual(second_post_process.bloom_radius, 1.0f),
        "RenderSettings smoke failed: Cornell bloom parameters did not reach the read packet.");
    expect(
        second_post_process.color_grading_enabled &&
        nearlyEqual(second_post_process.color_grading_exposure_offset, 0.0f) &&
        nearlyEqual(second_post_process.color_grading_contrast, 1.0f) &&
        nearlyEqual(second_post_process.color_grading_saturation, 1.0f) &&
        nearlyEqual(second_post_process.color_grading_temperature, 0.0f) &&
        nearlyEqual(second_post_process.color_grading_tint, 0.0f) &&
        nearlyEqual(second_post_process.color_grading_black_point, 0.0f) &&
        nearlyEqual(second_post_process.color_grading_white_point, 1.0f) &&
        nearlyEqual(second_post_process.vignette_intensity, 0.0f) &&
        nearlyEqual(second_post_process.vignette_radius, 0.75f) &&
        nearlyEqual(second_post_process.vignette_softness, 0.35f) &&
        nearlyEqual(second_post_process.sharpen_intensity, 0.0f),
        "RenderSettings smoke failed: updated color grading parameters did not reach the read packet.");
    expect(
        second_anti_aliasing.mode == NexAur::RenderAntiAliasingMode::SMAA &&
        nearlyEqual(second_anti_aliasing.smaa_edge_threshold, 0.08f) &&
        nearlyEqual(second_anti_aliasing.smaa_contrast_factor, 2.0f) &&
        second_anti_aliasing.smaa_max_search_steps == 12u &&
        nearlyEqual(second_anti_aliasing.smaa_blend_strength, 0.85f),
        "RenderSettings smoke failed: updated anti-aliasing parameters did not reach the read packet.");
    expect(
        second_lighting.preset == NexAur::RenderLightingPreset::Cornell &&
        nearlyEqual(second_lighting.directional_light_intensity_scale, 0.0f) &&
        nearlyEqual(second_lighting.point_light_intensity_scale, 1.0f) &&
        nearlyEqual(second_lighting.rect_light_intensity_scale, 1.0f) &&
        nearlyEqual(second_lighting.skybox_intensity_scale, 0.0f) &&
        nearlyEqual(second_lighting.ibl_intensity_scale, 0.03f),
        "RenderSettings smoke failed: Cornell lighting preset did not reach the read packet.");
    expect(
        !second_ao.enabled &&
        nearlyEqual(second_ao.radius, 0.8f) &&
        nearlyEqual(second_ao.intensity, 0.25f) &&
        nearlyEqual(second_ao.bias, 0.01f) &&
        nearlyEqual(second_ao.power, 2.0f) &&
        second_ao.blur_enabled &&
        second_ao.half_resolution,
        "RenderSettings smoke failed: updated AO settings did not reach the read packet.");
    expect(
        !second_ssr.enabled &&
        nearlyEqual(second_ssr.max_distance, 18.0f) &&
        second_ssr.max_steps == 32u &&
        nearlyEqual(second_ssr.thickness, 0.18f) &&
        nearlyEqual(second_ssr.stride, 1.0f) &&
        nearlyEqual(second_ssr.roughness_fade, 0.65f) &&
        nearlyEqual(second_ssr.edge_fade, 0.12f) &&
        nearlyEqual(second_ssr.intensity, 1.0f),
        "RenderSettings smoke failed: updated SSR settings did not reach the read packet.");
    expect(
        second_ibl_debug.mode == NexAur::RenderIblDebugMode::FinalLit &&
        nearlyEqual(second_ibl_debug.prefilter_mip, 0.0f),
        "RenderSettings smoke failed: updated IBL debug settings did not reach the read packet.");
    expect(
        second_effects_debug.view == NexAur::RenderEffectDebugView::SmaaOutput &&
        second_effects_debug.bloom_mip == 0u &&
        second_effects_debug.shadow_cascade == 1u &&
        second_effects_debug.point_shadow_layer == 2u &&
        second_effects_debug.rect_shadow_layer == 0u,
        "RenderSettings smoke failed: updated effects debug settings did not reach the read packet.");
    expect(
        second_shadow.enabled &&
        second_shadow.filter_mode == NexAur::RenderShadowFilterMode::PoissonPCF &&
        nearlyEqual(second_shadow.strength, 0.7f) &&
        nearlyEqual(second_shadow.constant_bias, 0.003f) &&
        nearlyEqual(second_shadow.normal_bias, 0.0f) &&
        nearlyEqual(second_shadow.slope_bias, 0.001f) &&
        nearlyEqual(second_shadow.filter_radius, 1.0f) &&
        nearlyEqual(second_shadow.pcss_light_radius, 0.5f) &&
        nearlyEqual(second_shadow.pcss_search_radius, 3.0f) &&
        nearlyEqual(second_shadow.pcss_min_filter_radius, 0.75f) &&
        nearlyEqual(second_shadow.pcss_max_filter_radius, 6.0f) &&
        nearlyEqual(second_shadow.distance, 35.0f) &&
        second_shadow.map_resolution == 2048u &&
        second_shadow.stabilize &&
        !second_shadow.cascades_enabled &&
        second_shadow.cascade_count == 1u &&
        nearlyEqual(second_shadow.cascade_split_lambda, 0.65f) &&
        !second_shadow.cascade_debug_overlay,
        "RenderSettings smoke failed: updated shadow settings did not reach the read packet.");
    expect(
        second_point_shadow.enabled &&
        second_point_shadow.max_shadowed_lights == 1u &&
        second_point_shadow.map_resolution == 512u &&
        nearlyEqual(second_point_shadow.strength, 0.85f) &&
        nearlyEqual(second_point_shadow.constant_bias, 0.015f) &&
        nearlyEqual(second_point_shadow.normal_bias, 0.02f) &&
        nearlyEqual(second_point_shadow.filter_radius, 1.0f),
        "RenderSettings smoke failed: updated point shadow settings did not reach the read packet.");
    expect(
        !second_contact_shadow.enabled &&
        nearlyEqual(second_contact_shadow.intensity, 0.35f) &&
        nearlyEqual(second_contact_shadow.max_distance, 0.45f) &&
        nearlyEqual(second_contact_shadow.thickness, 0.08f),
        "RenderSettings smoke failed: updated contact shadow settings did not reach the read packet.");
    expect(
        second_rect_shadow.enabled &&
        second_rect_shadow.max_shadowed_lights == 1u &&
        second_rect_shadow.map_resolution == 1024u &&
        nearlyEqual(second_rect_shadow.strength, 0.85f) &&
        nearlyEqual(second_rect_shadow.constant_bias, 0.01f) &&
        nearlyEqual(second_rect_shadow.normal_bias, 0.02f) &&
        nearlyEqual(second_rect_shadow.filter_radius, 1.0f) &&
        nearlyEqual(second_rect_shadow.projection_margin, 0.35f) &&
        second_rect_shadow.soft_shadow_enabled &&
        nearlyEqual(second_rect_shadow.pcss_light_radius, 0.75f) &&
        nearlyEqual(second_rect_shadow.pcss_search_radius, 3.0f) &&
        nearlyEqual(second_rect_shadow.pcss_min_filter_radius, 0.5f) &&
        nearlyEqual(second_rect_shadow.pcss_max_filter_radius, 5.0f) &&
        second_rect_shadow.pcss_blocker_taps == 8u &&
        second_rect_shadow.pcss_filter_taps == 16u,
        "RenderSettings smoke failed: updated rect shadow settings did not reach the read packet.");
    expect(
        second_rect_light.enabled &&
        second_rect_light.max_lights == 1u &&
        second_rect_light.ltc_specular_enabled &&
        nearlyEqual(second_rect_light.specular_intensity_scale, 1.0f) &&
        !second_rect_light.debug_ltc_only,
        "RenderSettings smoke failed: updated rect light settings did not reach the read packet.");

    NexAur::RenderDataPacket render_data;
    render_data.render_settings = settings;
    render_data.directional_light_data.intensity = 2.0f;
    render_data.directional_light_data.cast_shadow = true;
    render_data.environment_data.skybox_intensity = 0.75f;
    render_data.environment_data.ibl_intensity = 0.65f;

    NexAur::RendererPointLightData cornell_light;
    cornell_light.intensity = 12.0f;
    cornell_light.cast_shadow = true;
    cornell_light.shadow_range = 7.0f;
    cornell_light.shadow_strength = 0.9f;
    render_data.point_lights_data.push_back(cornell_light);
    NexAur::RendererPointLightData budget_clipped_light = cornell_light;
    budget_clipped_light.position = glm::vec3{ 1.0f, 0.0f, 0.0f };
    render_data.point_lights_data.push_back(budget_clipped_light);
    NexAur::RendererRectLightData cornell_rect_light;
    cornell_rect_light.position = glm::vec3{ 0.0f, 2.8f, 0.0f };
    cornell_rect_light.size = glm::vec2{ 1.4f, 1.0f };
    cornell_rect_light.intensity = 8.0f;
    cornell_rect_light.range = 6.5f;
    cornell_rect_light.cast_shadow = true;
    cornell_rect_light.shadow_strength = 0.88f;
    render_data.rect_lights_data.push_back(cornell_rect_light);
    NexAur::RendererReflectionProbeData active_probe;
    active_probe.position = glm::vec3{ 0.0f, 1.0f, 0.0f };
    active_probe.box_extents = glm::vec3{ 3.0f, 2.0f, 4.0f };
    active_probe.intensity = 1.4f;
    active_probe.diffuse_intensity = 0.58f;
    active_probe.blend_distance = 0.6f;
    active_probe.capture_resolution = 256u;
    active_probe.capture_priority = 75u;
    active_probe.capture_near_clip = 0.05f;
    active_probe.capture_far_clip = 18.0f;
    active_probe.baked_environment_asset =
        NexAur::AssetHandle{ NexAur::UUID{ 4001u } };
    active_probe.entity_id = 41;
    active_probe.enabled = true;
    active_probe.diffuse_enabled = true;
    active_probe.box_projection = true;
    active_probe.capture_include_skybox = false;
    active_probe.capture_dirty = false;
    render_data.reflection_probes_data.push_back(active_probe);
    NexAur::RendererReflectionProbeData disabled_probe = active_probe;
    disabled_probe.enabled = false;
    disabled_probe.position = glm::vec3{ 8.0f, 0.0f, 0.0f };
    render_data.reflection_probes_data.push_back(disabled_probe);

    NexAur::RenderSceneFrameBuilder frame_builder;
    const NexAur::RenderSceneFrame cornell_frame =
        frame_builder.buildRenderSceneFrame(render_data, 1280u, 720u);
    expect(
        nearlyEqual(cornell_frame.directional_light.intensity, 0.0f) &&
        !cornell_frame.directional_light.cast_shadow,
        "RenderSettings smoke failed: Cornell preset did not suppress directional light.");
    expect(
        !cornell_frame.point_lights.empty() &&
        nearlyEqual(cornell_frame.point_lights.front().intensity, 12.0f),
        "RenderSettings smoke failed: Cornell preset should preserve local light intensity.");
    expect(
        cornell_frame.point_lights.size() == 2u &&
        cornell_frame.point_lights[0].shadow_requested &&
        cornell_frame.point_lights[0].cast_shadow &&
        cornell_frame.point_lights[0].shadow_slot == 0 &&
        nearlyEqual(cornell_frame.point_lights[0].shadow_range, 7.0f) &&
        nearlyEqual(cornell_frame.point_lights[0].shadow_strength, 0.9f) &&
        cornell_frame.point_lights[1].shadow_requested &&
        !cornell_frame.point_lights[1].cast_shadow &&
        cornell_frame.point_lights[1].shadow_slot == -1,
        "RenderSettings smoke failed: point shadow budget or slot assignment was incorrect.");
    expect(
        cornell_frame.rect_lights.size() == 1u &&
        nearlyEqual(cornell_frame.rect_lights.front().intensity, 8.0f) &&
        nearlyEqual(cornell_frame.rect_lights.front().size.x, 1.4f) &&
        nearlyEqual(cornell_frame.rect_lights.front().size.y, 1.0f) &&
        nearlyEqual(cornell_frame.rect_lights.front().range, 6.5f) &&
        cornell_frame.rect_lights.front().shadow_requested &&
        cornell_frame.rect_lights.front().cast_shadow &&
        cornell_frame.rect_lights.front().shadow_slot == 0 &&
        nearlyEqual(cornell_frame.rect_lights.front().shadow_strength, 0.88f),
        "RenderSettings smoke failed: rect light budget, calibration, or shadow slot was incorrect.");
    expect(
        cornell_frame.reflection_probes.size() == 1u &&
        nearlyEqual(cornell_frame.reflection_probes.front().position.y, 1.0f) &&
        nearlyEqual(cornell_frame.reflection_probes.front().box_extents.x, 3.0f) &&
        nearlyEqual(cornell_frame.reflection_probes.front().box_extents.y, 2.0f) &&
        nearlyEqual(cornell_frame.reflection_probes.front().box_extents.z, 4.0f) &&
        nearlyEqual(cornell_frame.reflection_probes.front().intensity, 1.4f) &&
        nearlyEqual(cornell_frame.reflection_probes.front().diffuse_intensity, 0.58f) &&
        nearlyEqual(cornell_frame.reflection_probes.front().blend_distance, 0.6f) &&
        cornell_frame.reflection_probes.front().capture_resolution == 256u &&
        cornell_frame.reflection_probes.front().capture_priority == 75u &&
        nearlyEqual(cornell_frame.reflection_probes.front().capture_near_clip, 0.05f) &&
        nearlyEqual(cornell_frame.reflection_probes.front().capture_far_clip, 18.0f) &&
        cornell_frame.reflection_probes.front().baked_environment_asset &&
        cornell_frame.reflection_probes.front().entity_id == 41 &&
        cornell_frame.reflection_probes.front().diffuse_enabled &&
        cornell_frame.reflection_probes.front().box_projection &&
        !cornell_frame.reflection_probes.front().capture_include_skybox &&
        !cornell_frame.reflection_probes.front().capture_dirty,
        "RenderSettings smoke failed: reflection probe extraction or sanitization was incorrect.");
    expect(
        nearlyEqual(cornell_frame.skybox_intensity, 0.0f) &&
        nearlyEqual(cornell_frame.ibl_intensity, 0.65f * 0.03f),
        "RenderSettings smoke failed: Cornell preset did not calibrate environment intensity.");

    if (!success) {
        std::cerr << failure << std::endl;
        return 1;
    }

    std::cout << "RenderSettings smoke passed." << std::endl;
    return 0;
}

int runRayTracingCapabilitiesSmoke() {
    NexAur::VulkanRayTracingDeviceSupport complete_support;
    complete_support.query_succeeded = true;
    complete_support.acceleration_structure_extension = true;
    complete_support.ray_query_extension = true;
    complete_support.deferred_host_operations_extension = true;
    complete_support.ray_tracing_pipeline_extension = true;
    complete_support.buffer_device_address_feature = true;
    complete_support.acceleration_structure_feature = true;
    complete_support.ray_query_feature = true;
    complete_support.ray_tracing_pipeline_feature = true;
    complete_support.min_scratch_alignment = 256;
    complete_support.max_geometry_count = 4096;
    complete_support.max_instance_count = 8192;

    bool success = true;
    std::string failure;
    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    const NexAur::VulkanRayTracingCapabilities automatic_capabilities =
        NexAur::negotiateVulkanRayTracingCapabilities(complete_support);
    expect(
        automatic_capabilities.supportsRayQuery() &&
        automatic_capabilities.ray_query_enabled &&
        automatic_capabilities.ray_tracing_pipeline &&
        automatic_capabilities.unavailable_reason.empty(),
        "Ray tracing capability smoke failed: complete Auto capability was not enabled.");
    expect(
        automatic_capabilities.min_scratch_alignment == 256u &&
        automatic_capabilities.max_geometry_count == 4096u &&
        automatic_capabilities.max_instance_count == 8192u,
        "Ray tracing capability smoke failed: acceleration structure properties were not preserved.");

    struct MissingRequirement {
        bool NexAur::VulkanRayTracingDeviceSupport::*field = nullptr;
        const char* reason_fragment = nullptr;
    };
    constexpr std::array<MissingRequirement, 6> missing_requirements{
        MissingRequirement{
            &NexAur::VulkanRayTracingDeviceSupport::acceleration_structure_extension,
            "VK_KHR_acceleration_structure" },
        MissingRequirement{
            &NexAur::VulkanRayTracingDeviceSupport::ray_query_extension,
            "VK_KHR_ray_query" },
        MissingRequirement{
            &NexAur::VulkanRayTracingDeviceSupport::deferred_host_operations_extension,
            "VK_KHR_deferred_host_operations" },
        MissingRequirement{
            &NexAur::VulkanRayTracingDeviceSupport::buffer_device_address_feature,
            "bufferDeviceAddress" },
        MissingRequirement{
            &NexAur::VulkanRayTracingDeviceSupport::acceleration_structure_feature,
            "accelerationStructure" },
        MissingRequirement{
            &NexAur::VulkanRayTracingDeviceSupport::ray_query_feature,
            "rayQuery" },
    };

    for (const MissingRequirement& missing_requirement : missing_requirements) {
        NexAur::VulkanRayTracingDeviceSupport incomplete_support = complete_support;
        incomplete_support.*(missing_requirement.field) = false;
        const NexAur::VulkanRayTracingCapabilities capabilities =
            NexAur::negotiateVulkanRayTracingCapabilities(incomplete_support);
        expect(
            !capabilities.supportsRayQuery() &&
            !capabilities.ray_query_enabled &&
            capabilities.unavailable_reason.find(missing_requirement.reason_fragment) !=
                std::string::npos,
            "Ray tracing capability smoke failed: an incomplete capability cluster was accepted.");
    }

    NexAur::VulkanRayTracingOptions disabled_options;
    disabled_options.ray_query_mode = NexAur::VulkanRayQueryMode::Disabled;
    const NexAur::VulkanRayTracingCapabilities disabled_capabilities =
        NexAur::negotiateVulkanRayTracingCapabilities(complete_support, disabled_options);
    expect(
        disabled_capabilities.supportsRayQuery() &&
        !disabled_capabilities.ray_query_enabled &&
        disabled_capabilities.unavailable_reason.find("configuration") != std::string::npos,
        "Ray tracing capability smoke failed: Disabled mode did not preserve support and select fallback.");

    NexAur::VulkanRayTracingOptions force_disabled_options;
    force_disabled_options.force_disable_ray_query = true;
    const NexAur::VulkanRayTracingCapabilities force_disabled_capabilities =
        NexAur::negotiateVulkanRayTracingCapabilities(
            complete_support,
            force_disabled_options);
    expect(
        force_disabled_capabilities.supportsRayQuery() &&
        !force_disabled_capabilities.ray_query_enabled &&
        force_disabled_capabilities.unavailable_reason.find("force-disabled") != std::string::npos,
        "Ray tracing capability smoke failed: force-disable did not select fallback.");

    NexAur::VulkanRayTracingDeviceSupport no_pipeline_support = complete_support;
    no_pipeline_support.ray_tracing_pipeline_extension = false;
    const NexAur::VulkanRayTracingCapabilities ray_query_only_capabilities =
        NexAur::negotiateVulkanRayTracingCapabilities(no_pipeline_support);
    expect(
        ray_query_only_capabilities.ray_query_enabled &&
        !ray_query_only_capabilities.ray_tracing_pipeline,
        "Ray tracing capability smoke failed: Ray Query incorrectly required the full RT pipeline.");

    NexAur::VulkanRayTracingDeviceSupport failed_query_support;
    failed_query_support.query_failure_reason = "Synthetic capability query failure.";
    const NexAur::VulkanRayTracingCapabilities failed_query_capabilities =
        NexAur::negotiateVulkanRayTracingCapabilities(failed_query_support);
    expect(
        !failed_query_capabilities.supportsRayQuery() &&
        !failed_query_capabilities.ray_query_enabled &&
        failed_query_capabilities.unavailable_reason == failed_query_support.query_failure_reason,
        "Ray tracing capability smoke failed: query failure did not produce an explicit fallback reason.");

    if (!success) {
        std::cerr << failure << std::endl;
        return 1;
    }

    std::cout << "Ray tracing capability smoke passed." << std::endl;
    return 0;
}

int runRayTracingDeviceSmoke(
    NexAur::VulkanRayQueryMode mode,
    bool force_disable_ray_query,
    const char* mode_name) {
    if (glfwInit() != GLFW_TRUE) {
        std::cerr << "Ray tracing device smoke failed: GLFW initialization failed." << std::endl;
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(64, 64, "NexAur RT-00 Smoke", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        std::cerr << "Ray tracing device smoke failed: hidden Vulkan window creation failed." << std::endl;
        return 1;
    }

    uint32_t extension_count = 0;
    const char** extension_names = glfwGetRequiredInstanceExtensions(&extension_count);
    std::vector<const char*> required_extensions;
    if (extension_names && extension_count > 0) {
        required_extensions.assign(extension_names, extension_names + extension_count);
    }

    NexAur::VulkanRayTracingOptions options;
    options.ray_query_mode = mode;
    options.force_disable_ray_query = force_disable_ray_query;

    NexAur::VulkanDeviceContext device_context;
    const bool initialized = device_context.init(window, required_extensions, options);
    bool success = initialized;
    std::string failure = "Ray tracing device smoke failed: VulkanDeviceContext initialization failed.";
    if (initialized) {
        const NexAur::VulkanRayTracingCapabilities& capabilities =
            device_context.getRayTracingCapabilities();
        if (force_disable_ray_query) {
            success = !capabilities.ray_query_enabled &&
                capabilities.unavailable_reason.find("force-disabled") != std::string::npos;
            failure = "Ray tracing device smoke failed: force-disable did not select Raster fallback.";
        } else if (mode == NexAur::VulkanRayQueryMode::Disabled) {
            success = !capabilities.ray_query_enabled &&
                capabilities.unavailable_reason.find("configuration") != std::string::npos;
            failure = "Ray tracing device smoke failed: Disabled mode enabled Ray Query.";
        } else if (capabilities.supportsRayQuery()) {
            success = capabilities.ray_query_enabled && capabilities.unavailable_reason.empty();
            failure = "Ray tracing device smoke failed: supported Auto mode did not enable Ray Query.";
        } else {
            success = !capabilities.ray_query_enabled && !capabilities.unavailable_reason.empty();
            failure = "Ray tracing device smoke failed: unsupported Auto mode has no fallback reason.";
        }
    }

    device_context.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    if (!success) {
        std::cerr << failure << std::endl;
        return 1;
    }

    std::cout << "Ray tracing device " << mode_name << " smoke passed." << std::endl;
    return 0;
}

int runRayTracingDeviceAutoSmoke() {
    return runRayTracingDeviceSmoke(NexAur::VulkanRayQueryMode::Auto, false, "Auto");
}

int runRayTracingDeviceDisabledSmoke() {
    return runRayTracingDeviceSmoke(NexAur::VulkanRayQueryMode::Disabled, false, "Disabled");
}

int runRayTracingDeviceForceDisabledSmoke() {
    return runRayTracingDeviceSmoke(NexAur::VulkanRayQueryMode::Auto, true, "force-disabled");
}


namespace {
    struct RendererTestEntry {
        const char* argument = nullptr;
        int (*run)() = nullptr;
    };

    constexpr RendererTestEntry kRendererTests[] = {
        { "--render-frame-contract", runRenderFrameContractSmoke },
        { "--render-settings", runRenderSettingsSmoke },
        { "--shadow-frame-builder", runShadowFrameBuilderSmoke },
        { "--render-graph-state-planner", runRenderGraphStatePlannerSmoke },
        { "--frame-feature-plan", runFrameFeaturePlanSmoke },
        { "--retirement-queue", runRetirementQueueSmoke },
        { "--frame-context", runFrameContextSmoke },
        { "--async-transfer-state", runAsyncTransferStateSmoke },
        { "--ray-tracing-capabilities", runRayTracingCapabilitiesSmoke },
        { "--ray-tracing-device-auto", runRayTracingDeviceAutoSmoke },
        { "--ray-tracing-device-disabled", runRayTracingDeviceDisabledSmoke },
        { "--ray-tracing-device-force-disabled", runRayTracingDeviceForceDisabledSmoke },
    };
} // namespace

int main(int argc, char** argv) {
    NexAur::LogSystem::init();

    if (argc != 2 || !argv || !argv[1]) {
        std::cerr << "Expected one renderer test selector." << std::endl;
        return 2;
    }

    const std::string_view argument{ argv[1] };
    for (const RendererTestEntry& test : kRendererTests) {
        if (argument == test.argument) {
            return test.run();
        }
    }

    std::cerr << "Unknown renderer test selector: " << argument << std::endl;
    return 2;
}

#include "pch.h"
#include "vulkan_forward_pass.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Resource/mesh.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"
#include "Function/Renderer/Vulkan/resources/vulkan_material_resource.h"
#include "Function/Renderer/Vulkan/resources/vulkan_mesh_resource.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"

#include <array>
#include <cstddef>

namespace NexAur {
    namespace {
        struct VulkanForwardPushConstants {
            glm::mat4 model{ 1.0f };
            std::array<glm::vec4, 3> previous_model_rows{
                glm::vec4{ 0.0f },
                glm::vec4{ 0.0f },
                glm::vec4{ 0.0f }
            };
        };

        static_assert(sizeof(VulkanForwardPushConstants) <= 128, "Forward pass push constants exceed Vulkan minimum limit.");

        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

        VkFormat findDepthFormat(VkPhysicalDevice physical_device) {
            constexpr VkFormatFeatureFlags required_features =
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
            const std::array<VkFormat, 3> candidates{
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT
            };

            for (VkFormat format : candidates) {
                VkFormatProperties properties{};
                vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
                if ((properties.optimalTilingFeatures & required_features) == required_features) {
                    return format;
                }
            }

            return VK_FORMAT_UNDEFINED;
        }

        VulkanForwardPassRenderOptions defaultRenderOptions() {
            VulkanForwardPassRenderOptions options;
            options.color_clear_value.color.float32[0] = 0.08f;
            options.color_clear_value.color.float32[1] = 0.10f;
            options.color_clear_value.color.float32[2] = 0.14f;
            options.color_clear_value.color.float32[3] = 1.0f;
            options.depth_clear_value.depthStencil.depth = 1.0f;
            options.depth_clear_value.depthStencil.stencil = 0;
            return options;
        }

        void transitionDepthImageToAttachment(
            VkCommandBuffer command_buffer,
            VkImage depth_image,
            VkImageLayout old_layout,
            VkImageLayout new_layout) {
            if (old_layout == new_layout) {
                return;
            }

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = old_layout;
            barrier.newLayout = new_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = depth_image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            vkCmdPipelineBarrier(
                command_buffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);
        }

    } // namespace

    VulkanForwardPass::~VulkanForwardPass() {
        shutdown();
    }

    bool VulkanForwardPass::recreateSwapchainResources(const VulkanForwardPassSwapchainContext& context) {
        cleanupSwapchainResources();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanForwardPass requires a valid swapchain context.");
            return false;
        }

        m_physical_device = context.physical_device;
        m_device = context.device;
        m_color_format = context.color_format;
        m_swapchain_color_format = context.swapchain_color_format;
        m_reflection_surface_format = context.reflection_surface_format;
        m_fallback_specular_format = context.fallback_specular_format;
        m_motion_vector_format = context.motion_vector_format;
        m_extent = context.extent;
        m_frame_descriptor_set_layout = context.frame_descriptor_set_layout;
        m_material_descriptor_set_layout = context.material_descriptor_set_layout;
        m_environment_descriptor_set_layout = context.environment_descriptor_set_layout;
        m_ray_tracing_scene_descriptor_set_layout =
            context.ray_tracing_scene_descriptor_set_layout;
        m_ray_query_enabled = context.ray_query_enabled;
        m_pipeline_cache = context.pipeline_cache;

        if (!createImageViews(context) || !createDepthResources(context) || !createPipeline()) {
            cleanupSwapchainResources();
            return false;
        }

        return true;
    }

    void VulkanForwardPass::cleanupSwapchainResources() {
        cleanupPipeline();
        cleanupDepthResources();

        for (VkImageView image_view : m_color_image_views) {
            if (image_view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, image_view, nullptr);
            }
        }
        m_color_image_views.clear();

        m_color_format = VK_FORMAT_UNDEFINED;
        m_swapchain_color_format = VK_FORMAT_UNDEFINED;
        m_reflection_surface_format = VK_FORMAT_UNDEFINED;
        m_fallback_specular_format = VK_FORMAT_UNDEFINED;
        m_motion_vector_format = VK_FORMAT_UNDEFINED;
        m_depth_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_frame_descriptor_set_layout = VK_NULL_HANDLE;
        m_material_descriptor_set_layout = VK_NULL_HANDLE;
        m_environment_descriptor_set_layout = VK_NULL_HANDLE;
        m_ray_tracing_scene_descriptor_set_layout = VK_NULL_HANDLE;
        m_ray_query_enabled = false;
        m_pipeline_cache = nullptr;
    }

    void VulkanForwardPass::shutdown() {
        cleanupSwapchainResources();
        m_physical_device = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
    }

    bool VulkanForwardPass::record(
        VkCommandBuffer command_buffer,
        uint32_t image_index,
        const VulkanDrawList& draw_list,
        VkDescriptorSet frame_descriptor_set,
        VkDescriptorSet environment_descriptor_set,
        VkDescriptorSet reflection_probe_descriptor_set) {
        return record(
            command_buffer,
            image_index,
            draw_list,
            frame_descriptor_set,
            environment_descriptor_set,
            reflection_probe_descriptor_set,
            defaultRenderOptions());
    }

    bool VulkanForwardPass::record(
        VkCommandBuffer command_buffer,
        uint32_t image_index,
        const VulkanDrawList& draw_list,
        VkDescriptorSet frame_descriptor_set,
        VkDescriptorSet environment_descriptor_set,
        VkDescriptorSet reflection_probe_descriptor_set,
        const VulkanForwardPassRenderOptions& options) {
        if (command_buffer == VK_NULL_HANDLE || image_index >= m_color_image_views.size()) {
            return false;
        }

        transitionDepthImageToAttachment(
            command_buffer,
            m_depth_image.getImage(),
            m_depth_image.getLayout(),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        m_depth_image.setLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

        VulkanRenderTarget target;
        target.color_view = m_color_image_views[image_index];
        target.color_format = m_color_format;
        target.depth_view = m_depth_image.getImageView();
        target.depth_format = m_depth_format;
        target.extent = m_extent;
        return record(
            command_buffer,
            target,
            draw_list,
            frame_descriptor_set,
            environment_descriptor_set,
            reflection_probe_descriptor_set,
            options);
    }

    bool VulkanForwardPass::record(
        VkCommandBuffer command_buffer,
        const VulkanRenderTarget& target,
        const VulkanDrawList& draw_list,
        VkDescriptorSet frame_descriptor_set,
        VkDescriptorSet environment_descriptor_set,
        VkDescriptorSet reflection_probe_descriptor_set) {
        return record(
            command_buffer,
            target,
            draw_list,
            frame_descriptor_set,
            environment_descriptor_set,
            reflection_probe_descriptor_set,
            defaultRenderOptions());
    }

    bool VulkanForwardPass::record(
        VkCommandBuffer command_buffer,
        const VulkanRenderTarget& target,
        const VulkanDrawList& draw_list,
        VkDescriptorSet frame_descriptor_set,
        VkDescriptorSet environment_descriptor_set,
        VkDescriptorSet reflection_probe_descriptor_set,
        const VulkanForwardPassRenderOptions& options) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid()) {
            return false;
        }

        if (target.color_format != m_color_format || target.depth_format != m_depth_format) {
            NX_CORE_ERROR("VulkanForwardPass target format does not match the current pipeline.");
            return false;
        }

        std::array<VkRenderingAttachmentInfo, 1 + kVulkanAuxiliaryColorAttachmentCount>
            color_attachments{};
        color_attachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachments[0].imageView = target.color_view;
        color_attachments[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachments[0].loadOp = options.color_load_op;
        color_attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachments[0].clearValue = options.color_clear_value;
        for (uint32_t index = 0;
             index < target.auxiliary_color_attachment_count;
             ++index) {
            VkRenderingAttachmentInfo& attachment = color_attachments[index + 1u];
            attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            attachment.imageView = target.auxiliary_color_views[index];
            attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.loadOp = options.auxiliary_color_load_op;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.clearValue = options.auxiliary_color_clear_value;
        }

        VkRenderingAttachmentInfo depth_attachment{};
        depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.imageView = target.depth_view;
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.loadOp = options.depth_load_op;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment.clearValue = options.depth_clear_value;

        VkRenderingInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.offset = { 0, 0 };
        rendering_info.renderArea.extent = target.extent;
        rendering_info.layerCount = 1;
        rendering_info.pColorAttachments = color_attachments.data();
        rendering_info.pDepthAttachment = &depth_attachment;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(target.extent.width);
        viewport.height = static_cast<float>(target.extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = target.extent;
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);

        const bool use_ray_query_debug =
            options.ray_query_debug &&
            options.ray_tracing_scene_descriptor_set != VK_NULL_HANDLE &&
            isRayQueryReady();
        const bool use_ray_query_shadow =
            !use_ray_query_debug &&
            options.ray_query_shadow &&
            options.ray_tracing_scene_descriptor_set != VK_NULL_HANDLE &&
            isRayQueryShadowReady();
        const bool use_ray_query_pipeline =
            use_ray_query_debug || use_ray_query_shadow;
        const bool use_ray_query_shadow_mrt =
            use_ray_query_shadow &&
            target.auxiliary_color_attachment_count == kVulkanAuxiliaryColorAttachmentCount &&
            isRayQueryShadowMrtReady();
        const bool use_mrt_pipeline =
            target.auxiliary_color_attachment_count == kVulkanAuxiliaryColorAttachmentCount &&
            ((use_ray_query_shadow_mrt) ||
             (!use_ray_query_pipeline &&
              m_mrt_pipeline != VK_NULL_HANDLE &&
              m_mrt_pipeline_layout != VK_NULL_HANDLE));
        if (target.auxiliary_color_attachment_count > 0 &&
            !use_mrt_pipeline &&
            !use_ray_query_debug &&
            !use_ray_query_shadow) {
            NX_CORE_ERROR(
                "VulkanForwardPass received MRT attachments without a compatible raster pipeline.");
            return false;
        }
        rendering_info.colorAttachmentCount = use_mrt_pipeline ?
            1u + kVulkanAuxiliaryColorAttachmentCount : 1u;

        vkCmdBeginRendering(command_buffer, &rendering_info);

        const VkPipeline pipeline = use_ray_query_debug ?
            m_ray_query_pipeline :
            (use_ray_query_shadow ?
             (use_ray_query_shadow_mrt ? m_ray_query_shadow_mrt_pipeline :
              m_ray_query_shadow_pipeline) :
             (use_mrt_pipeline ? m_mrt_pipeline : m_pipeline));
        const VkPipelineLayout pipeline_layout = use_ray_query_debug ?
            m_ray_query_pipeline_layout :
            (use_ray_query_shadow ?
             (use_ray_query_shadow_mrt ? m_ray_query_shadow_mrt_pipeline_layout :
              m_ray_query_shadow_pipeline_layout) :
             (use_mrt_pipeline ? m_mrt_pipeline_layout : m_pipeline_layout));
        if (pipeline != VK_NULL_HANDLE &&
            pipeline_layout != VK_NULL_HANDLE &&
            frame_descriptor_set != VK_NULL_HANDLE &&
            environment_descriptor_set != VK_NULL_HANDLE &&
            reflection_probe_descriptor_set != VK_NULL_HANDLE &&
            !draw_list.opaque_items.empty()) {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            vkCmdBindDescriptorSets(
                command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline_layout,
                0,
                1,
                &frame_descriptor_set,
                0,
                nullptr);

            vkCmdBindDescriptorSets(
                command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline_layout,
                2,
                1,
                &environment_descriptor_set,
                0,
                nullptr);

            vkCmdBindDescriptorSets(
                command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline_layout,
                3,
                1,
                &reflection_probe_descriptor_set,
                0,
                nullptr);

            if (use_ray_query_pipeline) {
                vkCmdBindDescriptorSets(
                    command_buffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline_layout,
                    4,
                    1,
                    &options.ray_tracing_scene_descriptor_set,
                    0,
                    nullptr);
            }

            for (const VulkanMeshDrawItem& item : draw_list.opaque_items) {
                if (!item.mesh || !item.mesh->isReady() || !item.material || !item.material->isReady()) {
                    continue;
                }

                const VkBuffer vertex_buffer = item.mesh->getVertexBuffer();
                const VkDeviceSize vertex_offset = 0;
                vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &vertex_offset);
                vkCmdBindIndexBuffer(command_buffer, item.mesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

                VulkanForwardPushConstants push_constants;
                push_constants.model = item.transform;
                for (uint32_t row = 0; row < 3; ++row) {
                    push_constants.previous_model_rows[row] = glm::vec4(
                        item.previous_transform[0][row],
                        item.previous_transform[1][row],
                        item.previous_transform[2][row],
                        item.previous_transform[3][row]);
                }
                vkCmdPushConstants(
                    command_buffer,
                    pipeline_layout,
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0,
                    sizeof(VulkanForwardPushConstants),
                    &push_constants);

                const VkDescriptorSet material_descriptor_set = item.material->getDescriptorSet();
                vkCmdBindDescriptorSets(
                    command_buffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipeline_layout,
                    1,
                    1,
                    &material_descriptor_set,
                    0,
                    nullptr);

                vkCmdDrawIndexed(command_buffer, item.mesh->getIndexCount(), 1, 0, 0, 0);
            }
        }

        vkCmdEndRendering(command_buffer);
        return true;
    }

    VkImageView VulkanForwardPass::getSwapchainColorImageView(uint32_t image_index) const {
        if (image_index >= m_color_image_views.size()) {
            return VK_NULL_HANDLE;
        }

        return m_color_image_views[image_index];
    }

    VulkanRenderTarget VulkanForwardPass::getSwapchainRenderTarget(uint32_t image_index) const {
        VulkanRenderTarget target;
        target.color_view = getSwapchainColorImageView(image_index);
        target.color_format = m_swapchain_color_format;
        target.depth_view = m_depth_image.getImageView();
        target.depth_format = m_depth_format;
        target.extent = m_extent;
        return target;
    }

    bool VulkanForwardPass::createDepthResources(const VulkanForwardPassSwapchainContext& context) {
        m_depth_format = findDepthFormat(context.physical_device);
        if (m_depth_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanForwardPass failed to find a supported depth format.");
            return false;
        }

        VulkanOwnedImageCreateInfo create_info;
        create_info.extent = { context.extent.width, context.extent.height, 1 };
        create_info.format = m_depth_format;
        create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        create_info.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
        create_info.debug_name = "VulkanForwardPass depth image";
        if (!m_depth_image.create(*context.gpu_allocator, create_info)) {
            return false;
        }

        return true;
    }

    bool VulkanForwardPass::createImageViews(const VulkanForwardPassSwapchainContext& context) {
        m_color_image_views.reserve(context.color_images.size());

        for (VkImage image : context.color_images) {
            VkImageViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = image;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = context.swapchain_color_format;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.baseMipLevel = 0;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = 1;

            VkImageView image_view = VK_NULL_HANDLE;
            if (!checkVk(vkCreateImageView(context.device, &view_info, nullptr, &image_view), "vkCreateImageView(swapchain color)")) {
                return false;
            }

            m_color_image_views.push_back(image_view);
        }

        return true;
    }

    bool VulkanForwardPass::createPipeline() {
        if (!m_pipeline_cache) {
            NX_CORE_ERROR("VulkanForwardPass requires a pipeline cache.");
            return false;
        }

        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(VulkanForwardPushConstants);

        VulkanGraphicsPipelineDesc desc;
        desc.debug_name = "ForwardOpaque";
        desc.shader_program = VulkanShaderProgramId::Forward;
        desc.color_format = m_color_format;
        desc.depth_format = m_depth_format;
        desc.descriptor_set_layouts = {
            m_frame_descriptor_set_layout,
            m_material_descriptor_set_layout,
            m_environment_descriptor_set_layout,
            m_environment_descriptor_set_layout
        };
        desc.push_constant_ranges = { push_constant_range };

        const VulkanGraphicsPipelineState pipeline_state = m_pipeline_cache->getOrCreateGraphicsPipeline(desc);
        if (!pipeline_state.valid()) {
            return false;
        }

        m_pipeline = pipeline_state.pipeline;
        m_pipeline_layout = pipeline_state.layout;

        if (m_reflection_surface_format != VK_FORMAT_UNDEFINED &&
            m_fallback_specular_format != VK_FORMAT_UNDEFINED &&
            m_motion_vector_format != VK_FORMAT_UNDEFINED) {
            VulkanGraphicsPipelineDesc mrt_desc = desc;
            mrt_desc.debug_name = "ForwardMrt";
            mrt_desc.shader_program = VulkanShaderProgramId::ForwardMrt;
            mrt_desc.color_attachment_formats = {
                m_color_format,
                m_reflection_surface_format,
                m_fallback_specular_format,
                m_motion_vector_format
            };
            const VulkanGraphicsPipelineState mrt_pipeline_state =
                m_pipeline_cache->getOrCreateGraphicsPipeline(mrt_desc);
            if (!mrt_pipeline_state.valid()) {
                NX_CORE_ERROR("Failed to create Forward MRT pipeline.");
                return false;
            }
            m_mrt_pipeline = mrt_pipeline_state.pipeline;
            m_mrt_pipeline_layout = mrt_pipeline_state.layout;
        }

        if (m_ray_query_enabled) {
            VulkanGraphicsPipelineDesc ray_query_desc = desc;
            ray_query_desc.debug_name = "ForwardRayQueryDebug";
            ray_query_desc.shader_program = VulkanShaderProgramId::ForwardRayQuery;
            ray_query_desc.descriptor_set_layouts.push_back(
                m_ray_tracing_scene_descriptor_set_layout);
            const VulkanGraphicsPipelineState ray_query_pipeline_state =
                m_pipeline_cache->getOrCreateGraphicsPipeline(ray_query_desc);
            if (ray_query_pipeline_state.valid()) {
                m_ray_query_pipeline = ray_query_pipeline_state.pipeline;
                m_ray_query_pipeline_layout = ray_query_pipeline_state.layout;
            } else {
                NX_CORE_WARN(
                    "Forward Ray Query debug pipeline is unavailable; Raster fallback remains active.");
            }

            VulkanGraphicsPipelineDesc ray_query_shadow_desc = desc;
            ray_query_shadow_desc.debug_name = "ForwardRayQueryShadow";
            ray_query_shadow_desc.shader_program = VulkanShaderProgramId::ForwardRayQueryShadow;
            ray_query_shadow_desc.descriptor_set_layouts.push_back(
                m_ray_tracing_scene_descriptor_set_layout);
            const VulkanGraphicsPipelineState ray_query_shadow_pipeline_state =
                m_pipeline_cache->getOrCreateGraphicsPipeline(ray_query_shadow_desc);
            if (ray_query_shadow_pipeline_state.valid()) {
                m_ray_query_shadow_pipeline = ray_query_shadow_pipeline_state.pipeline;
                m_ray_query_shadow_pipeline_layout = ray_query_shadow_pipeline_state.layout;
            } else {
                NX_CORE_WARN(
                    "Forward Ray Query shadow pipeline is unavailable; CSM / PCSS fallback remains active.");
            }

            VulkanGraphicsPipelineDesc ray_query_shadow_mrt_desc = ray_query_shadow_desc;
            ray_query_shadow_mrt_desc.debug_name = "ForwardRayQueryShadowMrt";
            ray_query_shadow_mrt_desc.shader_program =
                VulkanShaderProgramId::ForwardRayQueryShadowMrt;
            ray_query_shadow_mrt_desc.color_attachment_formats = {
                m_color_format,
                m_reflection_surface_format,
                m_fallback_specular_format,
                m_motion_vector_format
            };
            const VulkanGraphicsPipelineState ray_query_shadow_mrt_pipeline_state =
                m_pipeline_cache->getOrCreateGraphicsPipeline(ray_query_shadow_mrt_desc);
            if (ray_query_shadow_mrt_pipeline_state.valid()) {
                m_ray_query_shadow_mrt_pipeline = ray_query_shadow_mrt_pipeline_state.pipeline;
                m_ray_query_shadow_mrt_pipeline_layout = ray_query_shadow_mrt_pipeline_state.layout;
            } else {
                NX_CORE_WARN(
                    "Forward Ray Query shadow MRT pipeline is unavailable; reflection surface writes will be skipped on the Ray Query shadow path.");
            }
        }

        return true;
    }

    void VulkanForwardPass::cleanupDepthResources() {
        m_depth_image.reset();
    }

    void VulkanForwardPass::cleanupPipeline() {
        m_pipeline = VK_NULL_HANDLE;
        m_pipeline_layout = VK_NULL_HANDLE;
        m_mrt_pipeline = VK_NULL_HANDLE;
        m_mrt_pipeline_layout = VK_NULL_HANDLE;
        m_ray_query_pipeline = VK_NULL_HANDLE;
        m_ray_query_pipeline_layout = VK_NULL_HANDLE;
        m_ray_query_shadow_pipeline = VK_NULL_HANDLE;
        m_ray_query_shadow_pipeline_layout = VK_NULL_HANDLE;
        m_ray_query_shadow_mrt_pipeline = VK_NULL_HANDLE;
        m_ray_query_shadow_mrt_pipeline_layout = VK_NULL_HANDLE;
    }
} // namespace NexAur

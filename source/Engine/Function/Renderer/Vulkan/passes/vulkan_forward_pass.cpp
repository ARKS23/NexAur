#include "pch.h"
#include "vulkan_forward_pass.h"

#include "Function/Resource/mesh.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"
#include "Function/Renderer/Vulkan/resources/vulkan_material_resource.h"
#include "Function/Renderer/Vulkan/resources/vulkan_mesh_resource.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"

#include <array>
#include <cstddef>
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>

namespace NexAur {
    namespace {
        struct VulkanForwardPushConstants {
            glm::mat4 model{ 1.0f };
            glm::mat4 normal_matrix{ 1.0f };
        };

        static_assert(sizeof(VulkanForwardPushConstants) <= 128, "Forward pass push constants exceed Vulkan minimum limit.");

        glm::mat4 buildNormalMatrix(const glm::mat4& model) {
            if (std::abs(glm::determinant(model)) <= 0.000001f) {
                return glm::mat4{ 1.0f };
            }

            return glm::transpose(glm::inverse(model));
        }

        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

        uint32_t findMemoryType(
            VkPhysicalDevice physical_device,
            uint32_t type_filter,
            VkMemoryPropertyFlags properties) {
            VkPhysicalDeviceMemoryProperties memory_properties{};
            vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

            for (uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
                const bool type_matches = (type_filter & (1u << index)) != 0;
                const bool properties_match = (memory_properties.memoryTypes[index].propertyFlags & properties) == properties;
                if (type_matches && properties_match) {
                    return index;
                }
            }

            return UINT32_MAX;
        }

        VkFormat findDepthFormat(VkPhysicalDevice physical_device) {
            const std::array<VkFormat, 3> candidates{
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT
            };

            for (VkFormat format : candidates) {
                VkFormatProperties properties{};
                vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
                if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
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
        m_extent = context.extent;
        m_frame_descriptor_set_layout = context.frame_descriptor_set_layout;
        m_material_descriptor_set_layout = context.material_descriptor_set_layout;
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
        m_depth_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_frame_descriptor_set_layout = VK_NULL_HANDLE;
        m_material_descriptor_set_layout = VK_NULL_HANDLE;
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
        VkDescriptorSet frame_descriptor_set) {
        return record(command_buffer, image_index, draw_list, frame_descriptor_set, defaultRenderOptions());
    }

    bool VulkanForwardPass::record(
        VkCommandBuffer command_buffer,
        uint32_t image_index,
        const VulkanDrawList& draw_list,
        VkDescriptorSet frame_descriptor_set,
        const VulkanForwardPassRenderOptions& options) {
        if (command_buffer == VK_NULL_HANDLE || image_index >= m_color_image_views.size()) {
            return false;
        }

        transitionDepthImageToAttachment(
            command_buffer,
            m_depth_image,
            m_depth_image_layout,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        m_depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VulkanRenderTarget target;
        target.color_view = m_color_image_views[image_index];
        target.color_format = m_color_format;
        target.depth_view = m_depth_image_view;
        target.depth_format = m_depth_format;
        target.extent = m_extent;
        return record(command_buffer, target, draw_list, frame_descriptor_set, options);
    }

    bool VulkanForwardPass::record(
        VkCommandBuffer command_buffer,
        const VulkanRenderTarget& target,
        const VulkanDrawList& draw_list,
        VkDescriptorSet frame_descriptor_set) {
        return record(command_buffer, target, draw_list, frame_descriptor_set, defaultRenderOptions());
    }

    bool VulkanForwardPass::record(
        VkCommandBuffer command_buffer,
        const VulkanRenderTarget& target,
        const VulkanDrawList& draw_list,
        VkDescriptorSet frame_descriptor_set,
        const VulkanForwardPassRenderOptions& options) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid()) {
            return false;
        }

        if (target.color_format != m_color_format || target.depth_format != m_depth_format) {
            NX_CORE_ERROR("VulkanForwardPass target format does not match the current pipeline.");
            return false;
        }

        VkRenderingAttachmentInfo color_attachment{};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment.imageView = target.color_view;
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.loadOp = options.color_load_op;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.clearValue = options.color_clear_value;

        VkRenderingAttachmentInfo depth_attachment{};
        depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.imageView = target.depth_view;
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.loadOp = options.depth_load_op;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.clearValue = options.depth_clear_value;

        VkRenderingInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.renderArea.offset = { 0, 0 };
        rendering_info.renderArea.extent = target.extent;
        rendering_info.layerCount = 1;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment;
        rendering_info.pDepthAttachment = &depth_attachment;

        vkCmdBeginRendering(command_buffer, &rendering_info);

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

        if (m_pipeline != VK_NULL_HANDLE &&
            m_pipeline_layout != VK_NULL_HANDLE &&
            frame_descriptor_set != VK_NULL_HANDLE &&
            !draw_list.opaque_items.empty()) {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

            vkCmdBindDescriptorSets(
                command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipeline_layout,
                0,
                1,
                &frame_descriptor_set,
                0,
                nullptr);

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
                push_constants.normal_matrix = buildNormalMatrix(item.transform);
                vkCmdPushConstants(
                    command_buffer,
                    m_pipeline_layout,
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0,
                    sizeof(VulkanForwardPushConstants),
                    &push_constants);

                const VkDescriptorSet material_descriptor_set = item.material->getDescriptorSet();
                vkCmdBindDescriptorSets(
                    command_buffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_pipeline_layout,
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

    bool VulkanForwardPass::createDepthResources(const VulkanForwardPassSwapchainContext& context) {
        m_depth_format = findDepthFormat(context.physical_device);
        if (m_depth_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanForwardPass failed to find a supported depth format.");
            return false;
        }

        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = context.extent.width;
        image_info.extent.height = context.extent.height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = m_depth_format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (!checkVk(vkCreateImage(context.device, &image_info, nullptr, &m_depth_image), "vkCreateImage(depth)")) {
            return false;
        }

        VkMemoryRequirements memory_requirements{};
        vkGetImageMemoryRequirements(context.device, m_depth_image, &memory_requirements);

        const uint32_t memory_type = findMemoryType(
            context.physical_device,
            memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory_type == UINT32_MAX) {
            NX_CORE_ERROR("VulkanForwardPass failed to find device-local depth image memory.");
            return false;
        }

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;

        if (!checkVk(vkAllocateMemory(context.device, &allocate_info, nullptr, &m_depth_memory), "vkAllocateMemory(depth)") ||
            !checkVk(vkBindImageMemory(context.device, m_depth_image, m_depth_memory, 0), "vkBindImageMemory(depth)")) {
            return false;
        }

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_depth_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = m_depth_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (!checkVk(vkCreateImageView(context.device, &view_info, nullptr, &m_depth_image_view), "vkCreateImageView(depth)")) {
            return false;
        }

        m_depth_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        return true;
    }

    bool VulkanForwardPass::createImageViews(const VulkanForwardPassSwapchainContext& context) {
        m_color_image_views.reserve(context.color_images.size());

        for (VkImage image : context.color_images) {
            VkImageViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = image;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = context.color_format;
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
            m_material_descriptor_set_layout
        };
        desc.push_constant_ranges = { push_constant_range };

        const VulkanGraphicsPipelineState pipeline_state = m_pipeline_cache->getOrCreateGraphicsPipeline(desc);
        if (!pipeline_state.valid()) {
            return false;
        }

        m_pipeline = pipeline_state.pipeline;
        m_pipeline_layout = pipeline_state.layout;
        return true;
    }

    void VulkanForwardPass::cleanupDepthResources() {
        if (m_depth_image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_depth_image_view, nullptr);
            m_depth_image_view = VK_NULL_HANDLE;
        }

        if (m_depth_image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_depth_image, nullptr);
            m_depth_image = VK_NULL_HANDLE;
        }

        if (m_depth_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_depth_memory, nullptr);
            m_depth_memory = VK_NULL_HANDLE;
        }

        m_depth_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    void VulkanForwardPass::cleanupPipeline() {
        m_pipeline = VK_NULL_HANDLE;
        m_pipeline_layout = VK_NULL_HANDLE;
    }
} // namespace NexAur

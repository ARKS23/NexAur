#include "pch.h"
#include "vulkan_forward_pass.h"

#include "Function/Resource/mesh.h"
#include "Function/Renderer/Vulkan/resources/vulkan_mesh_resource.h"
#include "Function/Renderer/Vulkan/vulkan_draw_list.h"

#ifdef NX_PLATFORM_WINDOWS
    #include <Windows.h>
#endif

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>

namespace NexAur {
    namespace {
        struct VulkanForwardPushConstants {
            glm::mat4 view_projection{ 1.0f };
            glm::mat4 model{ 1.0f };
        };

        static_assert(sizeof(VulkanForwardPushConstants) <= 128, "D8 forward pass push constants exceed Vulkan minimum limit.");

        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

        std::filesystem::path executableDirectory() {
#ifdef NX_PLATFORM_WINDOWS
            std::array<char, MAX_PATH> buffer{};
            const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length > 0 && length < buffer.size()) {
                return std::filesystem::path(buffer.data()).parent_path();
            }
#endif
            return std::filesystem::current_path();
        }

        std::filesystem::path shaderPath(const char* file_name) {
            return executableDirectory() / "shaders" / "Renderer" / "Vulkan" / file_name;
        }

        std::vector<char> readBinaryFile(const std::filesystem::path& path) {
            std::ifstream file(path, std::ios::ate | std::ios::binary);
            if (!file.is_open()) {
                NX_CORE_ERROR("Failed to open Vulkan shader: {}", path.string());
                return {};
            }

            const std::streamsize file_size = file.tellg();
            if (file_size <= 0 || file_size % 4 != 0) {
                NX_CORE_ERROR("Invalid Vulkan shader bytecode size: {}", path.string());
                return {};
            }

            std::vector<char> buffer(static_cast<size_t>(file_size));
            file.seekg(0);
            file.read(buffer.data(), file_size);
            return buffer;
        }

        VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& bytecode, const char* debug_name) {
            if (device == VK_NULL_HANDLE || bytecode.empty()) {
                return VK_NULL_HANDLE;
            }

            VkShaderModuleCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            create_info.codeSize = bytecode.size();
            create_info.pCode = reinterpret_cast<const uint32_t*>(bytecode.data());

            VkShaderModule shader_module = VK_NULL_HANDLE;
            if (!checkVk(vkCreateShaderModule(device, &create_info, nullptr, &shader_module), debug_name)) {
                return VK_NULL_HANDLE;
            }

            return shader_module;
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

        VkVertexInputBindingDescription vertexBindingDescription() {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(Vertex);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return binding;
        }

        std::array<VkVertexInputAttributeDescription, 3> vertexAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 3> attributes{};

            attributes[0].location = 0;
            attributes[0].binding = 0;
            attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[0].offset = offsetof(Vertex, position);

            attributes[1].location = 1;
            attributes[1].binding = 0;
            attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[1].offset = offsetof(Vertex, normal);

            attributes[2].location = 2;
            attributes[2].binding = 0;
            attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributes[2].offset = offsetof(Vertex, texCoords);

            return attributes;
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
    }

    void VulkanForwardPass::shutdown() {
        cleanupSwapchainResources();
        m_physical_device = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
    }

    bool VulkanForwardPass::record(VkCommandBuffer command_buffer, uint32_t image_index, const VulkanDrawList& draw_list) {
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
        return record(command_buffer, target, draw_list);
    }

    bool VulkanForwardPass::record(VkCommandBuffer command_buffer, const VulkanRenderTarget& target, const VulkanDrawList& draw_list) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid()) {
            return false;
        }

        if (target.color_format != m_color_format || target.depth_format != m_depth_format) {
            NX_CORE_ERROR("VulkanForwardPass target format does not match the current pipeline.");
            return false;
        }

        VkClearValue clear_value{};
        clear_value.color.float32[0] = 0.08f;
        clear_value.color.float32[1] = 0.10f;
        clear_value.color.float32[2] = 0.14f;
        clear_value.color.float32[3] = 1.0f;

        VkRenderingAttachmentInfo color_attachment{};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment.imageView = target.color_view;
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.clearValue = clear_value;

        VkClearValue depth_clear{};
        depth_clear.depthStencil.depth = 1.0f;
        depth_clear.depthStencil.stencil = 0;

        VkRenderingAttachmentInfo depth_attachment{};
        depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment.imageView = target.depth_view;
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.clearValue = depth_clear;

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

        if (m_pipeline != VK_NULL_HANDLE && m_pipeline_layout != VK_NULL_HANDLE && !draw_list.opaque_items.empty()) {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

            for (const VulkanMeshDrawItem& item : draw_list.opaque_items) {
                if (!item.mesh || !item.mesh->isReady()) {
                    continue;
                }

                const VkBuffer vertex_buffer = item.mesh->getVertexBuffer();
                const VkDeviceSize vertex_offset = 0;
                vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &vertex_offset);
                vkCmdBindIndexBuffer(command_buffer, item.mesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

                VulkanForwardPushConstants push_constants;
                push_constants.view_projection = draw_list.view.view_projection_matrix;
                push_constants.model = item.transform;
                vkCmdPushConstants(
                    command_buffer,
                    m_pipeline_layout,
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0,
                    sizeof(VulkanForwardPushConstants),
                    &push_constants);

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
        const std::vector<char> vertex_shader = readBinaryFile(shaderPath("vulkan_forward.vert.spv"));
        const std::vector<char> fragment_shader = readBinaryFile(shaderPath("vulkan_forward.frag.spv"));

        VkShaderModule vertex_module = createShaderModule(m_device, vertex_shader, "vkCreateShaderModule(forward vertex)");
        VkShaderModule fragment_module = createShaderModule(m_device, fragment_shader, "vkCreateShaderModule(forward fragment)");

        auto cleanup_shader_modules = [&]() {
            if (fragment_module != VK_NULL_HANDLE) {
                vkDestroyShaderModule(m_device, fragment_module, nullptr);
            }
            if (vertex_module != VK_NULL_HANDLE) {
                vkDestroyShaderModule(m_device, vertex_module, nullptr);
            }
        };

        if (vertex_module == VK_NULL_HANDLE || fragment_module == VK_NULL_HANDLE) {
            cleanup_shader_modules();
            return false;
        }

        VkPipelineShaderStageCreateInfo vertex_stage{};
        vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_stage.module = vertex_module;
        vertex_stage.pName = "VSMain";

        VkPipelineShaderStageCreateInfo fragment_stage{};
        fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragment_stage.module = fragment_module;
        fragment_stage.pName = "PSMain";

        const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{ vertex_stage, fragment_stage };

        const VkVertexInputBindingDescription binding = vertexBindingDescription();
        const std::array<VkVertexInputAttributeDescription, 3> attributes = vertexAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertex_input{};
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input.vertexBindingDescriptionCount = 1;
        vertex_input.pVertexBindingDescriptions = &binding;
        vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vertex_input.pVertexAttributeDescriptions = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = VK_TRUE;
        depth_stencil.depthWriteEnable = VK_TRUE;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo color_blend{};
        color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend.attachmentCount = 1;
        color_blend.pAttachments = &color_blend_attachment;

        const std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates = dynamic_states.data();

        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constant_range.offset = 0;
        push_constant_range.size = sizeof(VulkanForwardPushConstants);

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_constant_range;

        if (!checkVk(vkCreatePipelineLayout(m_device, &layout_info, nullptr, &m_pipeline_layout), "vkCreatePipelineLayout(forward)")) {
            cleanup_shader_modules();
            return false;
        }

        VkPipelineRenderingCreateInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachmentFormats = &m_color_format;
        rendering_info.depthAttachmentFormat = m_depth_format;

        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.pNext = &rendering_info;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stages.size());
        pipeline_info.pStages = shader_stages.data();
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterization;
        pipeline_info.pMultisampleState = &multisample;
        pipeline_info.pDepthStencilState = &depth_stencil;
        pipeline_info.pColorBlendState = &color_blend;
        pipeline_info.pDynamicState = &dynamic_state;
        pipeline_info.layout = m_pipeline_layout;

        const bool created_pipeline = checkVk(
            vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline),
            "vkCreateGraphicsPipelines(forward)");

        cleanup_shader_modules();
        return created_pipeline;
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
        if (m_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
        }

        if (m_pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
            m_pipeline_layout = VK_NULL_HANDLE;
        }
    }
} // namespace NexAur

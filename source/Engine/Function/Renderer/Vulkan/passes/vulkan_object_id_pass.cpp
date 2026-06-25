#include "pch.h"
#include "vulkan_object_id_pass.h"

#include "Function/Resource/mesh.h"
#include "Function/Renderer/Vulkan/resources/vulkan_mesh_resource.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"

#ifdef NX_PLATFORM_WINDOWS
    #include <Windows.h>
#endif

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>

namespace NexAur {
    namespace {
        struct VulkanObjectIdPushConstants {
            glm::mat4 view_projection{ 1.0f };
            glm::mat4 model{ 1.0f };
        };

        static_assert(sizeof(VulkanObjectIdPushConstants) <= 128, "ObjectId pass push constants exceed Vulkan minimum limit.");

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

    VulkanObjectIdPass::~VulkanObjectIdPass() {
        shutdown();
    }

    bool VulkanObjectIdPass::init(const VulkanObjectIdPassContext& context) {
        shutdown();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanObjectIdPass requires a valid context.");
            return false;
        }

        m_device = context.device;
        m_object_id_format = context.object_id_format;
        m_depth_format = context.depth_format;

        if (!createPipeline()) {
            shutdown();
            return false;
        }

        return true;
    }

    void VulkanObjectIdPass::shutdown() {
        cleanupPipeline();
        m_device = VK_NULL_HANDLE;
        m_object_id_format = VK_FORMAT_UNDEFINED;
        m_depth_format = VK_FORMAT_UNDEFINED;
    }

    bool VulkanObjectIdPass::record(VkCommandBuffer command_buffer, const VulkanRenderTarget& target, const VulkanDrawList& draw_list) {
        if (command_buffer == VK_NULL_HANDLE || !target.valid()) {
            return false;
        }

        if (target.color_format != m_object_id_format || target.depth_format != m_depth_format) {
            NX_CORE_ERROR("VulkanObjectIdPass target format does not match the current pipeline.");
            return false;
        }

        VkClearValue object_id_clear{};
        object_id_clear.color.int32[0] = -1;

        VkRenderingAttachmentInfo color_attachment{};
        color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment.imageView = target.color_view;
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.clearValue = object_id_clear;

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
                if (!item.mesh || !item.mesh->isReady() || item.entity_id < 0) {
                    continue;
                }

                const VkBuffer vertex_buffer = item.mesh->getVertexBuffer();
                const VkDeviceSize vertex_offset = 0;
                vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &vertex_offset);
                vkCmdBindIndexBuffer(command_buffer, item.mesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

                VulkanObjectIdPushConstants push_constants;
                push_constants.view_projection = draw_list.view.view_projection_matrix;
                push_constants.model = item.transform;
                vkCmdPushConstants(
                    command_buffer,
                    m_pipeline_layout,
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0,
                    sizeof(VulkanObjectIdPushConstants),
                    &push_constants);

                vkCmdDrawIndexed(
                    command_buffer,
                    item.mesh->getIndexCount(),
                    1,
                    0,
                    0,
                    static_cast<uint32_t>(item.entity_id));
            }
        }

        vkCmdEndRendering(command_buffer);
        return true;
    }

    bool VulkanObjectIdPass::createPipeline() {
        const std::vector<char> vertex_shader = readBinaryFile(shaderPath("vulkan_object_id.vert.spv"));
        const std::vector<char> fragment_shader = readBinaryFile(shaderPath("vulkan_object_id.frag.spv"));

        VkShaderModule vertex_module = createShaderModule(m_device, vertex_shader, "vkCreateShaderModule(object id vertex)");
        VkShaderModule fragment_module = createShaderModule(m_device, fragment_shader, "vkCreateShaderModule(object id fragment)");

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
        color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;

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
        push_constant_range.size = sizeof(VulkanObjectIdPushConstants);

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_constant_range;

        if (!checkVk(vkCreatePipelineLayout(m_device, &layout_info, nullptr, &m_pipeline_layout), "vkCreatePipelineLayout(object id)")) {
            cleanup_shader_modules();
            return false;
        }

        VkPipelineRenderingCreateInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachmentFormats = &m_object_id_format;
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
            "vkCreateGraphicsPipelines(object id)");

        cleanup_shader_modules();
        return created_pipeline;
    }

    void VulkanObjectIdPass::cleanupPipeline() {
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

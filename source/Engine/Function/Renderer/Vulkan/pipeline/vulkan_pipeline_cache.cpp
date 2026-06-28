#include "pch.h"
#include "vulkan_pipeline_cache.h"

#include "Function/Resource/mesh.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_debug_draw_types.h"
#include "Function/Renderer/Vulkan/shaders/vulkan_shader_library.h"

#include <array>
#include <cstddef>
#include <vector>

namespace NexAur {
    namespace {
        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

        VkVertexInputBindingDescription meshVertexBindingDescription() {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(Vertex);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return binding;
        }

        std::array<VkVertexInputAttributeDescription, 5> meshVertexAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 5> attributes{};

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

            attributes[3].location = 3;
            attributes[3].binding = 0;
            attributes[3].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[3].offset = offsetof(Vertex, tangent);

            attributes[4].location = 4;
            attributes[4].binding = 0;
            attributes[4].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[4].offset = offsetof(Vertex, bitangent);

            return attributes;
        }

        VkVertexInputBindingDescription debugDrawVertexBindingDescription() {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(VulkanDebugDrawVertex);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return binding;
        }

        std::array<VkVertexInputAttributeDescription, 2> debugDrawVertexAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 2> attributes{};

            attributes[0].location = 0;
            attributes[0].binding = 0;
            attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[0].offset = offsetof(VulkanDebugDrawVertex, position);

            attributes[1].location = 1;
            attributes[1].binding = 0;
            attributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributes[1].offset = offsetof(VulkanDebugDrawVertex, color);

            return attributes;
        }
    } // namespace

    VulkanPipelineCache::~VulkanPipelineCache() {
        shutdown();
    }

    bool VulkanPipelineCache::init(VkDevice device, VulkanShaderLibrary& shader_library) {
        shutdown();

        if (device == VK_NULL_HANDLE || !shader_library.isInitialized()) {
            NX_CORE_ERROR("VulkanPipelineCache requires a valid device and shader library.");
            return false;
        }

        m_device = device;
        m_shader_library = &shader_library;

        if (!createNativePipelineCache()) {
            shutdown();
            return false;
        }

        return true;
    }

    void VulkanPipelineCache::shutdown() {
        if (m_device != VK_NULL_HANDLE) {
            for (auto& [_, state] : m_graphics_pipelines) {
                if (state.pipeline != VK_NULL_HANDLE) {
                    vkDestroyPipeline(m_device, state.pipeline, nullptr);
                }
                if (state.layout != VK_NULL_HANDLE) {
                    vkDestroyPipelineLayout(m_device, state.layout, nullptr);
                }
            }
            if (m_pipeline_cache != VK_NULL_HANDLE) {
                vkDestroyPipelineCache(m_device, m_pipeline_cache, nullptr);
            }
        }

        m_graphics_pipelines.clear();
        m_pipeline_cache = VK_NULL_HANDLE;
        m_shader_library = nullptr;
        m_device = VK_NULL_HANDLE;
    }

    VulkanGraphicsPipelineState VulkanPipelineCache::getOrCreateGraphicsPipeline(const VulkanGraphicsPipelineDesc& desc) {
        if (!isInitialized()) {
            NX_CORE_ERROR("VulkanPipelineCache is not initialized.");
            return {};
        }

        auto cached_it = m_graphics_pipelines.find(desc);
        if (cached_it != m_graphics_pipelines.end()) {
            return cached_it->second;
        }

        VulkanGraphicsPipelineState state = createGraphicsPipeline(desc);
        if (!state.valid()) {
            return {};
        }

        m_graphics_pipelines.emplace(desc, state);
        return state;
    }

    bool VulkanPipelineCache::createNativePipelineCache() {
        VkPipelineCacheCreateInfo cache_info{};
        cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        return checkVk(
            vkCreatePipelineCache(m_device, &cache_info, nullptr, &m_pipeline_cache),
            "vkCreatePipelineCache");
    }

    VulkanGraphicsPipelineState VulkanPipelineCache::createGraphicsPipeline(const VulkanGraphicsPipelineDesc& desc) {
        if (!desc.valid()) {
            NX_CORE_ERROR("Invalid Vulkan graphics pipeline desc: {}", desc.debug_name);
            return {};
        }

        VulkanShaderProgram shader_program = m_shader_library->getProgram(desc.shader_program);
        if (!shader_program.valid()) {
            NX_CORE_ERROR("Failed to resolve shader program for pipeline: {}", desc.debug_name);
            return {};
        }

        VkPipelineShaderStageCreateInfo vertex_stage{};
        vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_stage.module = shader_program.vertex_module;
        vertex_stage.pName = shader_program.vertex_entry;

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
        shader_stages.push_back(vertex_stage);
        if (shader_program.has_fragment_stage) {
            VkPipelineShaderStageCreateInfo fragment_stage{};
            fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragment_stage.module = shader_program.fragment_module;
            fragment_stage.pName = shader_program.fragment_entry;
            shader_stages.push_back(fragment_stage);
        }

        VkVertexInputBindingDescription binding{};
        std::array<VkVertexInputAttributeDescription, 5> attributes{};
        std::array<VkVertexInputAttributeDescription, 2> debug_attributes{};

        VkPipelineVertexInputStateCreateInfo vertex_input{};
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        if (desc.vertex_layout == VulkanPipelineVertexLayout::MeshPositionNormalTexcoordTangent) {
            binding = meshVertexBindingDescription();
            attributes = meshVertexAttributeDescriptions();
            vertex_input.vertexBindingDescriptionCount = 1;
            vertex_input.pVertexBindingDescriptions = &binding;
            vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
            vertex_input.pVertexAttributeDescriptions = attributes.data();
        } else if (desc.vertex_layout == VulkanPipelineVertexLayout::DebugPositionColor) {
            binding = debugDrawVertexBindingDescription();
            debug_attributes = debugDrawVertexAttributeDescriptions();
            vertex_input.vertexBindingDescriptionCount = 1;
            vertex_input.pVertexBindingDescriptions = &binding;
            vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(debug_attributes.size());
            vertex_input.pVertexAttributeDescriptions = debug_attributes.data();
        }

        VkPipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly.topology = desc.topology;

        VkPipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = desc.cull_mode;
        rasterization.frontFace = desc.front_face;
        rasterization.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth_stencil{};
        depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_stencil.depthTestEnable = desc.depth_test_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.depthWriteEnable = desc.depth_write_enable ? VK_TRUE : VK_FALSE;
        depth_stencil.depthCompareOp = desc.depth_compare_op;

        VkPipelineColorBlendAttachmentState color_blend_attachment{};
        color_blend_attachment.blendEnable = desc.blend_enable ? VK_TRUE : VK_FALSE;
        color_blend_attachment.colorWriteMask = desc.color_write_mask;

        VkPipelineColorBlendStateCreateInfo color_blend{};
        color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        const bool has_color_attachment = desc.color_format != VK_FORMAT_UNDEFINED;
        color_blend.attachmentCount = has_color_attachment ? 1u : 0u;
        color_blend.pAttachments = has_color_attachment ? &color_blend_attachment : nullptr;

        const std::array<VkDynamicState, 2> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
        dynamic_state.pDynamicStates = dynamic_states.data();

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = static_cast<uint32_t>(desc.descriptor_set_layouts.size());
        layout_info.pSetLayouts = desc.descriptor_set_layouts.empty() ? nullptr : desc.descriptor_set_layouts.data();
        layout_info.pushConstantRangeCount = static_cast<uint32_t>(desc.push_constant_ranges.size());
        layout_info.pPushConstantRanges = desc.push_constant_ranges.empty() ? nullptr : desc.push_constant_ranges.data();

        VulkanGraphicsPipelineState state;
        if (!checkVk(vkCreatePipelineLayout(m_device, &layout_info, nullptr, &state.layout), "vkCreatePipelineLayout(graphics pipeline cache)")) {
            return {};
        }

        VkPipelineRenderingCreateInfo rendering_info{};
        rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_info.colorAttachmentCount = has_color_attachment ? 1u : 0u;
        rendering_info.pColorAttachmentFormats = has_color_attachment ? &desc.color_format : nullptr;
        rendering_info.depthAttachmentFormat = desc.depth_format;

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
        pipeline_info.layout = state.layout;

        if (!checkVk(
                vkCreateGraphicsPipelines(m_device, m_pipeline_cache, 1, &pipeline_info, nullptr, &state.pipeline),
                "vkCreateGraphicsPipelines(graphics pipeline cache)")) {
            vkDestroyPipelineLayout(m_device, state.layout, nullptr);
            return {};
        }

        return state;
    }
} // namespace NexAur

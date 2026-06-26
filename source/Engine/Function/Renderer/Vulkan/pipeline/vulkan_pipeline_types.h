#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "Function/Renderer/Vulkan/shaders/vulkan_shader_library.h"

namespace NexAur {
    enum class VulkanPipelineVertexLayout {
        None = 0,
        MeshPositionNormalTexcoord
    };

    struct VulkanGraphicsPipelineDesc {
        VulkanShaderProgramId shader_program = VulkanShaderProgramId::Forward;
        VkFormat color_format = VK_FORMAT_UNDEFINED;
        VkFormat depth_format = VK_FORMAT_UNDEFINED;
        std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
        std::vector<VkPushConstantRange> push_constant_ranges;
        VulkanPipelineVertexLayout vertex_layout = VulkanPipelineVertexLayout::MeshPositionNormalTexcoord;
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkCullModeFlags cull_mode = VK_CULL_MODE_NONE;
        VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        bool depth_test_enable = true;
        bool depth_write_enable = true;
        VkCompareOp depth_compare_op = VK_COMPARE_OP_LESS_OR_EQUAL;
        VkColorComponentFlags color_write_mask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        bool blend_enable = false;
        std::string debug_name;

        bool valid() const {
            return (color_format != VK_FORMAT_UNDEFINED || depth_format != VK_FORMAT_UNDEFINED) &&
                   (!(depth_test_enable || depth_write_enable) || depth_format != VK_FORMAT_UNDEFINED);
        }
    };

    inline bool operator==(const VulkanGraphicsPipelineDesc& lhs, const VulkanGraphicsPipelineDesc& rhs) {
        auto pushConstantsEqual = [](const std::vector<VkPushConstantRange>& a, const std::vector<VkPushConstantRange>& b) {
            if (a.size() != b.size()) {
                return false;
            }
            for (size_t index = 0; index < a.size(); ++index) {
                if (a[index].stageFlags != b[index].stageFlags ||
                    a[index].offset != b[index].offset ||
                    a[index].size != b[index].size) {
                    return false;
                }
            }
            return true;
        };

        return lhs.shader_program == rhs.shader_program &&
               lhs.color_format == rhs.color_format &&
               lhs.depth_format == rhs.depth_format &&
               lhs.descriptor_set_layouts == rhs.descriptor_set_layouts &&
               pushConstantsEqual(lhs.push_constant_ranges, rhs.push_constant_ranges) &&
               lhs.vertex_layout == rhs.vertex_layout &&
               lhs.topology == rhs.topology &&
               lhs.cull_mode == rhs.cull_mode &&
               lhs.front_face == rhs.front_face &&
               lhs.depth_test_enable == rhs.depth_test_enable &&
               lhs.depth_write_enable == rhs.depth_write_enable &&
               lhs.depth_compare_op == rhs.depth_compare_op &&
               lhs.color_write_mask == rhs.color_write_mask &&
               lhs.blend_enable == rhs.blend_enable;
    }

    struct VulkanGraphicsPipelineDescHash {
        size_t operator()(const VulkanGraphicsPipelineDesc& desc) const {
            size_t seed = 0;
            auto combine = [&seed](size_t value) {
                seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
            };

            combine(std::hash<int>{}(static_cast<int>(desc.shader_program)));
            combine(std::hash<int>{}(static_cast<int>(desc.color_format)));
            combine(std::hash<int>{}(static_cast<int>(desc.depth_format)));
            for (VkDescriptorSetLayout layout : desc.descriptor_set_layouts) {
                combine(std::hash<VkDescriptorSetLayout>{}(layout));
            }
            for (const VkPushConstantRange& range : desc.push_constant_ranges) {
                combine(std::hash<uint32_t>{}(range.stageFlags));
                combine(std::hash<uint32_t>{}(range.offset));
                combine(std::hash<uint32_t>{}(range.size));
            }
            combine(std::hash<int>{}(static_cast<int>(desc.vertex_layout)));
            combine(std::hash<int>{}(static_cast<int>(desc.topology)));
            combine(std::hash<uint32_t>{}(desc.cull_mode));
            combine(std::hash<int>{}(static_cast<int>(desc.front_face)));
            combine(std::hash<bool>{}(desc.depth_test_enable));
            combine(std::hash<bool>{}(desc.depth_write_enable));
            combine(std::hash<int>{}(static_cast<int>(desc.depth_compare_op)));
            combine(std::hash<uint32_t>{}(desc.color_write_mask));
            combine(std::hash<bool>{}(desc.blend_enable));
            return seed;
        }
    };

    struct VulkanGraphicsPipelineState {
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;

        bool valid() const {
            return pipeline != VK_NULL_HANDLE &&
                   layout != VK_NULL_HANDLE;
        }
    };
} // namespace NexAur

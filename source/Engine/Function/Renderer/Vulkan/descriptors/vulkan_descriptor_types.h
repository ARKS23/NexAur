#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <vulkan/vulkan.h>

namespace NexAur {
    enum class VulkanDescriptorSetLayoutId : uint8_t {
        FrameGlobal = 0,
        Material,
        PostProcessInput,
        BloomDualInput,
        Environment,
        AoInput
    };

    struct VulkanDescriptorBindingDesc {
        uint32_t binding = 0;
        VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        uint32_t descriptor_count = 1;
        VkShaderStageFlags stage_flags = 0;
        VkDescriptorBindingFlags binding_flags = 0;

        bool operator==(const VulkanDescriptorBindingDesc& other) const {
            return binding == other.binding &&
                   descriptor_type == other.descriptor_type &&
                   descriptor_count == other.descriptor_count &&
                   stage_flags == other.stage_flags &&
                   binding_flags == other.binding_flags;
        }
    };

    struct VulkanDescriptorSetLayoutDesc {
        VkDescriptorSetLayoutCreateFlags flags = 0;
        std::vector<VulkanDescriptorBindingDesc> bindings;

        void normalize() {
            std::sort(
                bindings.begin(),
                bindings.end(),
                [](const VulkanDescriptorBindingDesc& lhs, const VulkanDescriptorBindingDesc& rhs) {
                    return lhs.binding < rhs.binding;
                });
        }

        bool operator==(const VulkanDescriptorSetLayoutDesc& other) const {
            return flags == other.flags &&
                   bindings == other.bindings;
        }
    };

    struct VulkanDescriptorSetLayoutDescHash {
        size_t operator()(const VulkanDescriptorSetLayoutDesc& desc) const {
            size_t seed = 0;
            auto combine = [&seed](size_t value) {
                seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
            };

            combine(std::hash<uint32_t>{}(desc.flags));
            for (const VulkanDescriptorBindingDesc& binding : desc.bindings) {
                combine(std::hash<uint32_t>{}(binding.binding));
                combine(std::hash<int>{}(static_cast<int>(binding.descriptor_type)));
                combine(std::hash<uint32_t>{}(binding.descriptor_count));
                combine(std::hash<uint32_t>{}(binding.stage_flags));
                combine(std::hash<uint32_t>{}(binding.binding_flags));
            }

            return seed;
        }
    };
} // namespace NexAur

#pragma once

#include <unordered_map>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_types.h"

namespace NexAur {
    class VulkanShaderLibrary;

    class NEXAUR_API VulkanPipelineCache {
    public:
        VulkanPipelineCache() = default;
        ~VulkanPipelineCache();

        VulkanPipelineCache(const VulkanPipelineCache&) = delete;
        VulkanPipelineCache& operator=(const VulkanPipelineCache&) = delete;

        bool init(VkDevice device, VulkanShaderLibrary& shader_library);
        void shutdown();

        VulkanGraphicsPipelineState getOrCreateGraphicsPipeline(const VulkanGraphicsPipelineDesc& desc);
        VkDescriptorSetLayout getEmptyDescriptorSetLayout() const { return m_empty_descriptor_set_layout; }
        bool isInitialized() const { return m_device != VK_NULL_HANDLE && m_shader_library != nullptr; }

    private:
        bool createNativePipelineCache();
        bool createEmptyDescriptorSetLayout();
        VulkanGraphicsPipelineState createGraphicsPipeline(const VulkanGraphicsPipelineDesc& desc);

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VulkanShaderLibrary* m_shader_library = nullptr;
        VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_empty_descriptor_set_layout = VK_NULL_HANDLE;
        std::unordered_map<VulkanGraphicsPipelineDesc, VulkanGraphicsPipelineState, VulkanGraphicsPipelineDescHash> m_graphics_pipelines;
    };
} // namespace NexAur

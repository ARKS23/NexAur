#pragma once

#include <unordered_map>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_types.h"

namespace NexAur {
    class VulkanShaderLibrary;

    class VulkanPipelineCache {
    public:
        VulkanPipelineCache() = default;
        ~VulkanPipelineCache();

        VulkanPipelineCache(const VulkanPipelineCache&) = delete;
        VulkanPipelineCache& operator=(const VulkanPipelineCache&) = delete;

        bool init(VkDevice device, VulkanShaderLibrary& shader_library);
        void shutdown();

        VulkanGraphicsPipelineState getOrCreateGraphicsPipeline(const VulkanGraphicsPipelineDesc& desc);
        VulkanComputePipelineState getOrCreateComputePipeline(const VulkanComputePipelineDesc& desc);
        bool isInitialized() const { return m_device != VK_NULL_HANDLE && m_shader_library != nullptr; }

    private:
        bool createNativePipelineCache();
        VulkanGraphicsPipelineState createGraphicsPipeline(const VulkanGraphicsPipelineDesc& desc);
        VulkanComputePipelineState createComputePipeline(const VulkanComputePipelineDesc& desc);

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VulkanShaderLibrary* m_shader_library = nullptr;
        VkPipelineCache m_pipeline_cache = VK_NULL_HANDLE;
        std::unordered_map<VulkanGraphicsPipelineDesc, VulkanGraphicsPipelineState, VulkanGraphicsPipelineDescHash> m_graphics_pipelines;
        std::unordered_map<VulkanComputePipelineDesc, VulkanComputePipelineState, VulkanComputePipelineDescHash> m_compute_pipelines;
    };
} // namespace NexAur

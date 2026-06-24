#pragma once

#include <memory>
#include <unordered_map>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
#include "Function/RendererV2/resources/vulkan_model_resource.h"
#include "Function/RendererV2/vulkan_resource_context.h"

namespace NexAur {
    class AssetManager;

    class NEXAUR_API VulkanRenderResourceCache {
    public:
        VulkanRenderResourceCache() = default;
        ~VulkanRenderResourceCache();

        VulkanRenderResourceCache(const VulkanRenderResourceCache&) = delete;
        VulkanRenderResourceCache& operator=(const VulkanRenderResourceCache&) = delete;

        bool init(const VulkanResourceContext& context);
        // 调用方需要保证没有正在使用缓存资源的 GPU 命令；Backend::shutdown() 会先等待 device idle。
        void clear();
        void shutdown();

        VulkanModelResource* getOrCreateModel(AssetHandle model_asset, AssetManager& asset_manager);
        VulkanModelResource* getModel(AssetHandle model_asset) const;

        bool isInitialized() const { return m_initialized; }

    private:
        bool createAllocator(const VulkanResourceContext& context);
        bool createUploadCommandPool(const VulkanResourceContext& context);
        VulkanResourceUploadContext createUploadContext() const;

    private:
        VmaAllocator m_allocator = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkQueue m_graphics_queue = VK_NULL_HANDLE;
        VkCommandPool m_upload_command_pool = VK_NULL_HANDLE;
        std::unordered_map<AssetHandle, std::unique_ptr<VulkanModelResource>> m_model_cache;
        bool m_initialized = false;
    };
} // namespace NexAur

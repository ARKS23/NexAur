#pragma once

#include <memory>
#include <unordered_map>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
#include "Function/Renderer/Vulkan/resources/vulkan_model_resource.h"
#include "Function/Renderer/Vulkan/resources/vulkan_texture_resource.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class AssetManager;

    class NEXAUR_API VulkanRenderResourceCache {
    public:
        VulkanRenderResourceCache() = default;
        ~VulkanRenderResourceCache();

        VulkanRenderResourceCache(const VulkanRenderResourceCache&) = delete;
        VulkanRenderResourceCache& operator=(const VulkanRenderResourceCache&) = delete;

        bool init(const VulkanResourceContext& context);
        // Caller must make sure cached GPU resources are no longer referenced by in-flight commands.
        void clear();
        void shutdown();

        VulkanModelResource* getOrCreateModel(AssetHandle model_asset, AssetManager& asset_manager);
        VulkanModelResource* getModel(AssetHandle model_asset) const;
        VulkanTextureResource* getOrCreateTexture(AssetHandle texture_asset, AssetManager& asset_manager);
        VulkanTextureResource* getTexture(AssetHandle texture_asset) const;
        VulkanTextureResource* getFallbackWhiteTexture() const { return m_fallback_white_texture.get(); }

        bool isInitialized() const { return m_initialized; }

    private:
        bool createAllocator(const VulkanResourceContext& context);
        bool createUploadCommandPool(const VulkanResourceContext& context);
        bool createFallbackTexture();
        VulkanResourceUploadContext createUploadContext() const;

    private:
        VmaAllocator m_allocator = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkQueue m_graphics_queue = VK_NULL_HANDLE;
        VkCommandPool m_upload_command_pool = VK_NULL_HANDLE;
        std::unordered_map<AssetHandle, std::unique_ptr<VulkanModelResource>> m_model_cache;
        std::unordered_map<AssetHandle, std::unique_ptr<VulkanTextureResource>> m_texture_cache;
        std::unique_ptr<VulkanTextureResource> m_fallback_white_texture;
        bool m_initialized = false;
    };
} // namespace NexAur

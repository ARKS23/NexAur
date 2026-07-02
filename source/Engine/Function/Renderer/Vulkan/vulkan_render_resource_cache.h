#pragma once

#include <cstddef>
#include <memory>
#include <unordered_map>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
#include "Function/Renderer/Vulkan/resources/vulkan_environment_resource.h"
#include "Function/Renderer/Vulkan/resources/vulkan_model_resource.h"
#include "Function/Renderer/Vulkan/resources/vulkan_texture_resource.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class AssetManager;
    class MaterialAsset;
    class VulkanDescriptorAllocator;
    class VulkanDescriptorLayoutCache;

    class NEXAUR_API VulkanRenderResourceCache {
    public:
        VulkanRenderResourceCache() = default;
        ~VulkanRenderResourceCache();

        VulkanRenderResourceCache(const VulkanRenderResourceCache&) = delete;
        VulkanRenderResourceCache& operator=(const VulkanRenderResourceCache&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VulkanDescriptorLayoutCache& descriptor_layout_cache,
            VulkanDescriptorAllocator& descriptor_allocator);
        // Caller must make sure cached GPU resources are no longer referenced by in-flight commands.
        void clear();
        void shutdown();

        VulkanModelResource* getOrCreateModel(AssetHandle model_asset, AssetManager& asset_manager);
        VulkanModelResource* getModel(AssetHandle model_asset) const;
        VulkanTextureResource* getOrCreateTexture(AssetHandle texture_asset, AssetManager& asset_manager);
        VulkanTextureResource* getTexture(AssetHandle texture_asset) const;
        VulkanEnvironmentResource* getOrCreateEnvironment(AssetHandle environment_asset, AssetManager& asset_manager);
        VulkanEnvironmentResource* getEnvironment(AssetHandle environment_asset) const;
        std::unique_ptr<VulkanEnvironmentResource> createRuntimeEnvironment(
            AssetHandle environment_asset,
            AssetManager& asset_manager,
            const glm::vec3& fallback_color,
            const VulkanEnvironmentResourceBuildSettings& settings);
        VulkanTextureResource* getFallbackWhiteTexture() const { return m_fallback_white_texture.get(); }
        VulkanMaterialResource* getFallbackMaterial() const { return m_fallback_material.get(); }
        VulkanEnvironmentResource* getFallbackEnvironment() const { return m_fallback_environment.get(); }
        size_t getModelCount() const { return m_model_cache.size(); }
        size_t getTextureCount() const { return m_texture_cache.size(); }
        size_t getEnvironmentCount() const { return m_environment_cache.size(); }
        size_t getMeshCount() const;
        size_t getMaterialCount() const;
        bool hasFallbackWhiteTexture() const;
        bool hasFallbackMaterial() const;
        bool hasFallbackEnvironment() const;

        bool createMaterialResource(
            VulkanMaterialResource& material_resource,
            const MaterialAsset& material_asset,
            AssetManager& asset_manager);

        bool isInitialized() const { return m_initialized; }

    private:
        bool createAllocator(const VulkanResourceContext& context);
        bool createUploadCommandPool(const VulkanResourceContext& context);
        bool resolveDescriptorLayouts();
        bool createFallbackTexture();
        bool createFallbackMaterial();
        bool createFallbackEnvironment();
        VulkanResourceUploadContext createUploadContext() const;
        VulkanMaterialResourceCreateContext createMaterialContext() const;
        VulkanEnvironmentResourceCreateContext createEnvironmentContext() const;

    private:
        VmaAllocator m_allocator = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkQueue m_graphics_queue = VK_NULL_HANDLE;
        VkCommandPool m_upload_command_pool = VK_NULL_HANDLE;
        VulkanDescriptorLayoutCache* m_descriptor_layout_cache = nullptr;
        VulkanDescriptorAllocator* m_descriptor_allocator = nullptr;
        VkDescriptorSetLayout m_material_descriptor_set_layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_environment_descriptor_set_layout = VK_NULL_HANDLE;
        std::unordered_map<AssetHandle, std::unique_ptr<VulkanModelResource>> m_model_cache;
        std::unordered_map<AssetHandle, std::unique_ptr<VulkanTextureResource>> m_texture_cache;
        std::unordered_map<AssetHandle, std::unique_ptr<VulkanEnvironmentResource>> m_environment_cache;
        std::unique_ptr<VulkanTextureResource> m_fallback_white_texture;
        std::unique_ptr<VulkanMaterialResource> m_fallback_material;
        std::unique_ptr<VulkanEnvironmentResource> m_fallback_environment;
        bool m_initialized = false;
    };
} // namespace NexAur

#include "pch.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "vulkan_render_resource_cache.h"

#include "Function/Resource/asset_manager.h"
#include "Function/Resource/environment_map_asset.h"
#include "Function/Resource/material_asset.h"
#include "Function/Resource/material_types.h"
#include "Function/Resource/model.h"
#include "Function/Resource/texture_asset.h"
#include "Function/Resource/texture_loader.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/resources/vulkan_model_resource.h"

#include <string>

namespace NexAur {
    namespace {
        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }
    } // namespace

    VulkanRenderResourceCache::~VulkanRenderResourceCache() {
        shutdown();
    }

    bool VulkanRenderResourceCache::init(
        const VulkanResourceContext& context,
        VulkanDescriptorLayoutCache& descriptor_layout_cache,
        VulkanDescriptorAllocator& descriptor_allocator) {
        if (m_initialized) {
            return true;
        }
        if (!context.valid()) {
            NX_CORE_ERROR("VulkanRenderResourceCache init failed: invalid VulkanResourceContext.");
            return false;
        }

        m_device = context.device;
        m_graphics_queue = context.graphics_queue;
        m_descriptor_layout_cache = &descriptor_layout_cache;
        m_descriptor_allocator = &descriptor_allocator;
        if (!createAllocator(context) ||
            !createUploadCommandPool(context) ||
            !resolveDescriptorLayouts()) {
            shutdown();
            return false;
        }

        m_initialized = true;
        if (!createFallbackTexture() ||
            !createFallbackMaterial() ||
            !createFallbackEnvironment()) {
            shutdown();
            return false;
        }

        NX_CORE_INFO("VulkanRenderResourceCache initialized.");
        return true;
    }

    void VulkanRenderResourceCache::clear() {
        m_model_cache.clear();
        m_texture_cache.clear();
        m_environment_cache.clear();
    }

    void VulkanRenderResourceCache::shutdown() {
        if (!m_initialized &&
            m_allocator == VK_NULL_HANDLE &&
            m_upload_command_pool == VK_NULL_HANDLE) {
            return;
        }

        clear();
        m_fallback_environment.reset();
        m_fallback_material.reset();
        m_fallback_white_texture.reset();
        if (m_upload_command_pool != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, m_upload_command_pool, nullptr);
            m_upload_command_pool = VK_NULL_HANDLE;
        }
        if (m_allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(m_allocator);
            m_allocator = VK_NULL_HANDLE;
        }

        m_device = VK_NULL_HANDLE;
        m_graphics_queue = VK_NULL_HANDLE;
        m_descriptor_layout_cache = nullptr;
        m_descriptor_allocator = nullptr;
        m_material_descriptor_set_layout = VK_NULL_HANDLE;
        m_environment_descriptor_set_layout = VK_NULL_HANDLE;
        m_initialized = false;
        NX_CORE_INFO("VulkanRenderResourceCache shutdown.");
    }

    VulkanModelResource* VulkanRenderResourceCache::getOrCreateModel(AssetHandle model_asset, AssetManager& asset_manager) {
        if (!m_initialized || m_allocator == VK_NULL_HANDLE) {
            NX_CORE_WARN("VulkanRenderResourceCache is not initialized.");
            return nullptr;
        }
        if (!model_asset) {
            NX_CORE_WARN("Attempted to resolve Vulkan model with invalid AssetHandle.");
            return nullptr;
        }

        if (VulkanModelResource* cached_model = getModel(model_asset)) {
            return cached_model;
        }

        const AssetType asset_type = asset_manager.getAssetType(model_asset);
        if (asset_type != AssetType::Model) {
            NX_CORE_WARN(
                "AssetHandle is not a Model asset: {} ({})",
                static_cast<uint64_t>(model_asset.id),
                assetTypeToString(asset_type));
            return nullptr;
        }

        std::shared_ptr<Model> cpu_model = asset_manager.loadModelCPU(model_asset);
        if (!cpu_model || !cpu_model->isLoaded()) {
            NX_CORE_ERROR("Failed to load CPU model for Vulkan resource: {}", static_cast<uint64_t>(model_asset.id));
            return nullptr;
        }

        std::string debug_name = asset_manager.getPath(model_asset);
        if (debug_name.empty()) {
            debug_name = "VulkanModel." + std::to_string(static_cast<uint64_t>(model_asset.id));
        }

        auto model_resource = std::make_unique<VulkanModelResource>();
        if (!model_resource->create(createUploadContext(), *cpu_model, debug_name, *this, asset_manager)) {
            NX_CORE_ERROR("Failed to create Vulkan model resource: {}", debug_name);
            return nullptr;
        }

        VulkanModelResource* model_resource_ptr = model_resource.get();
        m_model_cache.emplace(model_asset, std::move(model_resource));
        return model_resource_ptr;
    }

    VulkanModelResource* VulkanRenderResourceCache::getModel(AssetHandle model_asset) const {
        if (!model_asset) {
            return nullptr;
        }

        auto cached_it = m_model_cache.find(model_asset);
        return cached_it != m_model_cache.end() ? cached_it->second.get() : nullptr;
    }

    VulkanTextureResource* VulkanRenderResourceCache::getOrCreateTexture(AssetHandle texture_asset, AssetManager& asset_manager) {
        if (!m_initialized || m_allocator == VK_NULL_HANDLE) {
            NX_CORE_WARN("VulkanRenderResourceCache is not initialized.");
            return nullptr;
        }
        if (!texture_asset) {
            NX_CORE_WARN("Attempted to resolve Vulkan texture with invalid AssetHandle.");
            return getFallbackWhiteTexture();
        }

        if (VulkanTextureResource* cached_texture = getTexture(texture_asset)) {
            return cached_texture;
        }

        const AssetType asset_type = asset_manager.getAssetType(texture_asset);
        if (asset_type != AssetType::Texture2D) {
            NX_CORE_WARN(
                "AssetHandle is not a Texture2D asset: {} ({})",
                static_cast<uint64_t>(texture_asset.id),
                assetTypeToString(asset_type));
            return getFallbackWhiteTexture();
        }

        std::shared_ptr<TextureAsset> cpu_texture = asset_manager.loadTextureCPU(texture_asset);
        if (!cpu_texture || !cpu_texture->isLoaded()) {
            NX_CORE_ERROR("Failed to load CPU texture for Vulkan resource: {}", static_cast<uint64_t>(texture_asset.id));
            return getFallbackWhiteTexture();
        }

        auto texture_resource = std::make_unique<VulkanTextureResource>();
        if (!texture_resource->create(createUploadContext(), *cpu_texture)) {
            NX_CORE_ERROR("Failed to create Vulkan texture resource: {}", asset_manager.getPath(texture_asset));
            return getFallbackWhiteTexture();
        }

        VulkanTextureResource* texture_resource_ptr = texture_resource.get();
        m_texture_cache.emplace(texture_asset, std::move(texture_resource));
        return texture_resource_ptr;
    }

    VulkanTextureResource* VulkanRenderResourceCache::getTexture(AssetHandle texture_asset) const {
        if (!texture_asset) {
            return nullptr;
        }

        auto cached_it = m_texture_cache.find(texture_asset);
        return cached_it != m_texture_cache.end() ? cached_it->second.get() : nullptr;
    }

    VulkanEnvironmentResource* VulkanRenderResourceCache::getOrCreateEnvironment(
        AssetHandle environment_asset,
        AssetManager& asset_manager) {
        if (!m_initialized || m_allocator == VK_NULL_HANDLE) {
            NX_CORE_WARN("VulkanRenderResourceCache is not initialized.");
            return getFallbackEnvironment();
        }
        if (!environment_asset) {
            return getFallbackEnvironment();
        }

        if (VulkanEnvironmentResource* cached_environment = getEnvironment(environment_asset)) {
            return cached_environment;
        }

        const AssetType asset_type = asset_manager.getAssetType(environment_asset);
        if (asset_type != AssetType::EnvironmentMap) {
            NX_CORE_WARN(
                "AssetHandle is not an EnvironmentMap asset: {} ({})",
                static_cast<uint64_t>(environment_asset.id),
                assetTypeToString(asset_type));
            return getFallbackEnvironment();
        }

        std::shared_ptr<EnvironmentMapAsset> cpu_environment = asset_manager.loadEnvironmentMapCPU(environment_asset);
        if (!cpu_environment || !cpu_environment->isLoaded()) {
            NX_CORE_ERROR("Failed to load CPU environment map for Vulkan resource: {}", static_cast<uint64_t>(environment_asset.id));
            return getFallbackEnvironment();
        }

        auto environment_resource = std::make_unique<VulkanEnvironmentResource>();
        if (!environment_resource->create(createEnvironmentContext(), *cpu_environment)) {
            NX_CORE_ERROR("Failed to create Vulkan environment resource: {}", asset_manager.getPath(environment_asset));
            return getFallbackEnvironment();
        }

        VulkanEnvironmentResource* environment_resource_ptr = environment_resource.get();
        m_environment_cache.emplace(environment_asset, std::move(environment_resource));
        return environment_resource_ptr;
    }

    VulkanEnvironmentResource* VulkanRenderResourceCache::getEnvironment(AssetHandle environment_asset) const {
        if (!environment_asset) {
            return nullptr;
        }

        auto cached_it = m_environment_cache.find(environment_asset);
        return cached_it != m_environment_cache.end() ? cached_it->second.get() : nullptr;
    }

    std::unique_ptr<VulkanEnvironmentResource> VulkanRenderResourceCache::createRuntimeEnvironment(
        AssetHandle environment_asset,
        AssetManager& asset_manager,
        const glm::vec3& fallback_color,
        const VulkanEnvironmentResourceBuildSettings& settings) {
        if (!m_initialized || m_allocator == VK_NULL_HANDLE) {
            NX_CORE_WARN("VulkanRenderResourceCache is not initialized.");
            return nullptr;
        }

        auto environment_resource = std::make_unique<VulkanEnvironmentResource>();
        if (environment_asset && asset_manager.getAssetType(environment_asset) == AssetType::EnvironmentMap) {
            std::shared_ptr<EnvironmentMapAsset> cpu_environment =
                asset_manager.loadEnvironmentMapCPU(environment_asset);
            if (cpu_environment &&
                cpu_environment->isLoaded() &&
                environment_resource->create(createEnvironmentContext(), *cpu_environment, settings)) {
                return environment_resource;
            }

            NX_CORE_WARN(
                "Runtime reflection probe could not create environment from asset {}; using fallback color.",
                static_cast<uint64_t>(environment_asset.id));
            environment_resource = std::make_unique<VulkanEnvironmentResource>();
        }

        if (!environment_resource->createFallback(createEnvironmentContext(), fallback_color, settings)) {
            NX_CORE_ERROR("Failed to create runtime reflection probe fallback environment.");
            return nullptr;
        }

        return environment_resource;
    }

    std::unique_ptr<VulkanEnvironmentResource> VulkanRenderResourceCache::createRuntimeEnvironmentFromCubePixels(
        uint32_t cube_size,
        const std::vector<float>& rgba_pixels,
        const VulkanEnvironmentResourceBuildSettings& settings) {
        if (!m_initialized || m_allocator == VK_NULL_HANDLE) {
            NX_CORE_WARN("VulkanRenderResourceCache is not initialized.");
            return nullptr;
        }

        auto environment_resource = std::make_unique<VulkanEnvironmentResource>();
        if (!environment_resource->createFromCubePixels(
                createEnvironmentContext(),
                cube_size,
                rgba_pixels,
                settings)) {
            NX_CORE_ERROR("Failed to create runtime reflection probe environment from captured cubemap.");
            return nullptr;
        }

        return environment_resource;
    }

    size_t VulkanRenderResourceCache::getMeshCount() const {
        size_t mesh_count = 0;
        for (const auto& [asset_handle, model_resource] : m_model_cache) {
            (void)asset_handle;
            if (model_resource) {
                mesh_count += model_resource->getMeshes().size();
            }
        }
        return mesh_count;
    }

    size_t VulkanRenderResourceCache::getMaterialCount() const {
        size_t material_count = 0;
        for (const auto& [asset_handle, model_resource] : m_model_cache) {
            (void)asset_handle;
            if (model_resource) {
                material_count += model_resource->getMaterials().size();
            }
        }

        if (hasFallbackMaterial()) {
            ++material_count;
        }
        return material_count;
    }

    bool VulkanRenderResourceCache::hasFallbackWhiteTexture() const {
        return m_fallback_white_texture && m_fallback_white_texture->isReady();
    }

    bool VulkanRenderResourceCache::hasFallbackMaterial() const {
        return m_fallback_material && m_fallback_material->isReady();
    }

    bool VulkanRenderResourceCache::hasFallbackEnvironment() const {
        return m_fallback_environment && m_fallback_environment->isReady();
    }

    bool VulkanRenderResourceCache::createMaterialResource(
        VulkanMaterialResource& material_resource,
        const MaterialAsset& material_asset,
        AssetManager& asset_manager) {
        VulkanTextureResource* fallback_texture = getFallbackWhiteTexture();
        if (!fallback_texture || !fallback_texture->isReady()) {
            NX_CORE_ERROR("Vulkan material resource requires a valid fallback texture.");
            return false;
        }

        auto resolve_texture = [&](AssetHandle texture_asset) {
            if (!texture_asset) {
                return fallback_texture;
            }

            VulkanTextureResource* texture = getOrCreateTexture(texture_asset, asset_manager);
            return texture && texture->isReady() ? texture : fallback_texture;
        };

        VulkanMaterialTextureSet textures;
        textures.base_color = resolve_texture(material_asset.getBaseColorTexture());
        textures.normal = resolve_texture(material_asset.getNormalTexture());
        textures.metallic = resolve_texture(material_asset.getMetallicTexture());
        textures.roughness = resolve_texture(material_asset.getRoughnessTexture());
        textures.metallic_roughness = resolve_texture(material_asset.getMetallicRoughnessTexture());
        textures.ao = resolve_texture(material_asset.getAOTexture());
        textures.emissive = resolve_texture(material_asset.getEmissiveTexture());

        return material_resource.create(createMaterialContext(), material_asset, textures);
    }

    bool VulkanRenderResourceCache::createAllocator(const VulkanResourceContext& context) {
        VmaAllocatorCreateInfo allocator_info{};
        allocator_info.vulkanApiVersion = context.api_version;
        allocator_info.instance = context.instance;
        allocator_info.physicalDevice = context.physical_device;
        allocator_info.device = context.device;

        const VkResult result = vmaCreateAllocator(&allocator_info, &m_allocator);
        if (result != VK_SUCCESS) {
            NX_CORE_ERROR("Failed to create VMA allocator: {}", static_cast<int>(result));
            m_allocator = VK_NULL_HANDLE;
            return false;
        }

        return true;
    }

    bool VulkanRenderResourceCache::createUploadCommandPool(const VulkanResourceContext& context) {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = context.graphics_queue_family;

        return checkVk(
            vkCreateCommandPool(context.device, &pool_info, nullptr, &m_upload_command_pool),
            "vkCreateCommandPool(resource upload)");
    }

    bool VulkanRenderResourceCache::resolveDescriptorLayouts() {
        if (!m_descriptor_layout_cache || !m_descriptor_layout_cache->isInitialized()) {
            NX_CORE_ERROR("VulkanRenderResourceCache requires an initialized descriptor layout cache.");
            return false;
        }
        if (!m_descriptor_allocator || !m_descriptor_allocator->isInitialized()) {
            NX_CORE_ERROR("VulkanRenderResourceCache requires an initialized descriptor allocator.");
            return false;
        }

        m_material_descriptor_set_layout = m_descriptor_layout_cache->getBuiltinLayout(VulkanDescriptorSetLayoutId::Material);
        if (m_material_descriptor_set_layout == VK_NULL_HANDLE) {
            NX_CORE_ERROR("Failed to resolve material descriptor set layout.");
            return false;
        }

        m_environment_descriptor_set_layout = m_descriptor_layout_cache->getBuiltinLayout(VulkanDescriptorSetLayoutId::Environment);
        if (m_environment_descriptor_set_layout == VK_NULL_HANDLE) {
            NX_CORE_ERROR("Failed to resolve environment descriptor set layout.");
            return false;
        }

        return true;
    }

    bool VulkanRenderResourceCache::createFallbackTexture() {
        std::shared_ptr<TextureAsset> fallback_texture = TextureLoader::createSolidColorRGBA8(
            255,
            255,
            255,
            255,
            TextureColorSpace::SRGB,
            "FallbackWhiteTexture");
        if (!fallback_texture || !fallback_texture->isLoaded()) {
            NX_CORE_ERROR("Failed to create CPU fallback white texture.");
            return false;
        }

        auto fallback_resource = std::make_unique<VulkanTextureResource>();
        if (!fallback_resource->create(createUploadContext(), *fallback_texture)) {
            NX_CORE_ERROR("Failed to create Vulkan fallback white texture.");
            return false;
        }

        m_fallback_white_texture = std::move(fallback_resource);
        return true;
    }

    bool VulkanRenderResourceCache::createFallbackMaterial() {
        MaterialImportData import_data;
        import_data.name = "FallbackMaterial";
        MaterialAsset fallback_material(import_data, AssetHandle());

        auto material_resource = std::make_unique<VulkanMaterialResource>();
        if (!createMaterialResource(*material_resource, fallback_material, AssetManager::getInstance())) {
            NX_CORE_ERROR("Failed to create Vulkan fallback material.");
            return false;
        }

        m_fallback_material = std::move(material_resource);
        return true;
    }

    bool VulkanRenderResourceCache::createFallbackEnvironment() {
        auto environment_resource = std::make_unique<VulkanEnvironmentResource>();
        if (!environment_resource->createFallback(createEnvironmentContext(), glm::vec3{ 0.08f, 0.10f, 0.14f })) {
            NX_CORE_ERROR("Failed to create Vulkan fallback environment.");
            return false;
        }

        m_fallback_environment = std::move(environment_resource);
        return true;
    }

    VulkanResourceUploadContext VulkanRenderResourceCache::createUploadContext() const {
        VulkanResourceUploadContext context;
        context.allocator = m_allocator;
        context.device = m_device;
        context.graphics_queue = m_graphics_queue;
        context.command_pool = m_upload_command_pool;
        return context;
    }

    VulkanMaterialResourceCreateContext VulkanRenderResourceCache::createMaterialContext() const {
        VulkanMaterialResourceCreateContext context;
        context.upload_context = createUploadContext();
        context.descriptor_allocator = m_descriptor_allocator;
        context.descriptor_set_layout = m_material_descriptor_set_layout;
        return context;
    }

    VulkanEnvironmentResourceCreateContext VulkanRenderResourceCache::createEnvironmentContext() const {
        VulkanEnvironmentResourceCreateContext context;
        context.upload_context = createUploadContext();
        context.descriptor_allocator = m_descriptor_allocator;
        context.descriptor_set_layout = m_environment_descriptor_set_layout;
        return context;
    }
} // namespace NexAur

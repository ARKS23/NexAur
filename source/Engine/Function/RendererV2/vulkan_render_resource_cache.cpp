#include "pch.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "vulkan_render_resource_cache.h"

#include "Function/Resource/asset_manager.h"
#include "Function/Resource/model.h"
#include "Function/RendererV2/resources/vulkan_model_resource.h"

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

    bool VulkanRenderResourceCache::init(const VulkanResourceContext& context) {
        if (m_initialized) {
            return true;
        }
        if (!context.valid()) {
            NX_CORE_ERROR("VulkanRenderResourceCache init failed: invalid VulkanResourceContext.");
            return false;
        }

        m_device = context.device;
        m_graphics_queue = context.graphics_queue;
        if (!createAllocator(context) || !createUploadCommandPool(context)) {
            shutdown();
            return false;
        }

        m_initialized = true;
        NX_CORE_INFO("VulkanRenderResourceCache initialized.");
        return true;
    }

    void VulkanRenderResourceCache::clear() {
        m_model_cache.clear();
    }

    void VulkanRenderResourceCache::shutdown() {
        if (!m_initialized &&
            m_allocator == VK_NULL_HANDLE &&
            m_upload_command_pool == VK_NULL_HANDLE) {
            return;
        }

        clear();
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
        if (!model_resource->create(createUploadContext(), *cpu_model, debug_name)) {
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

    bool VulkanRenderResourceCache::createAllocator(const VulkanResourceContext& context) {
        VmaAllocatorCreateInfo allocator_info{};
        allocator_info.vulkanApiVersion = VK_API_VERSION_1_1;
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

    VulkanResourceUploadContext VulkanRenderResourceCache::createUploadContext() const {
        VulkanResourceUploadContext context;
        context.allocator = m_allocator;
        context.device = m_device;
        context.graphics_queue = m_graphics_queue;
        context.command_pool = m_upload_command_pool;
        return context;
    }
} // namespace NexAur

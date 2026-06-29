#include "pch.h"
#include "vulkan_shadow_map_target.h"

#include <algorithm>
#include <array>

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

    VulkanShadowMapTarget::~VulkanShadowMapTarget() {
        shutdown();
    }

    bool VulkanShadowMapTarget::init(const VulkanResourceContext& context, uint32_t resolution, uint32_t layer_count) {
        shutdown();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanShadowMapTarget requires a valid Vulkan context.");
            return false;
        }

        m_physical_device = context.physical_device;
        m_device = context.device;
        m_depth_format = findDepthFormat();
        if (m_depth_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanShadowMapTarget failed to find a supported depth format.");
            shutdown();
            return false;
        }

        resolution = std::max(1u, resolution);
        layer_count = std::max(1u, layer_count);
        if (!createImage(resolution, layer_count) || !createSampler()) {
            shutdown();
            return false;
        }

        m_ready = true;
        return true;
    }

    void VulkanShadowMapTarget::shutdown() {
        cleanupImage();
        cleanupSampler();

        m_physical_device = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_depth_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_layer_count = 1;
        m_ready = false;
    }

    VulkanDepthRenderTarget VulkanShadowMapTarget::getRenderTarget() const {
        return getRenderTarget(0);
    }

    VulkanDepthRenderTarget VulkanShadowMapTarget::getRenderTarget(uint32_t layer_index) const {
        VulkanDepthRenderTarget target;
        if (layer_index >= m_depth_layer_views.size()) {
            return target;
        }

        target.depth_view = m_depth_layer_views[layer_index];
        target.depth_format = m_depth_format;
        target.extent = m_extent;
        return target;
    }

    bool VulkanShadowMapTarget::createImage(uint32_t resolution, uint32_t layer_count) {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = resolution;
        image_info.extent.height = resolution;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = layer_count;
        image_info.format = m_depth_format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (!checkVk(vkCreateImage(m_device, &image_info, nullptr, &m_depth_image), "vkCreateImage(shadow map)")) {
            return false;
        }

        VkMemoryRequirements memory_requirements{};
        vkGetImageMemoryRequirements(m_device, m_depth_image, &memory_requirements);

        const uint32_t memory_type = findMemoryType(
            memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory_type == UINT32_MAX) {
            NX_CORE_ERROR("VulkanShadowMapTarget failed to find device-local image memory.");
            return false;
        }

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;

        if (!checkVk(vkAllocateMemory(m_device, &allocate_info, nullptr, &m_depth_memory), "vkAllocateMemory(shadow map)") ||
            !checkVk(vkBindImageMemory(m_device, m_depth_image, m_depth_memory, 0), "vkBindImageMemory(shadow map)")) {
            return false;
        }

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_depth_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        view_info.format = m_depth_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = layer_count;

        if (!checkVk(vkCreateImageView(m_device, &view_info, nullptr, &m_depth_image_view), "vkCreateImageView(shadow map sampled array)")) {
            return false;
        }

        m_depth_layer_views.resize(layer_count, VK_NULL_HANDLE);
        for (uint32_t layer_index = 0; layer_index < layer_count; ++layer_index) {
            VkImageViewCreateInfo layer_view_info = view_info;
            layer_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            layer_view_info.subresourceRange.baseArrayLayer = layer_index;
            layer_view_info.subresourceRange.layerCount = 1;

            if (!checkVk(
                    vkCreateImageView(m_device, &layer_view_info, nullptr, &m_depth_layer_views[layer_index]),
                    "vkCreateImageView(shadow map layer)")) {
                return false;
            }
        }

        m_extent = { resolution, resolution };
        m_layer_count = layer_count;
        m_depth_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        return true;
    }

    bool VulkanShadowMapTarget::createSampler() {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 1.0f;
        sampler_info.maxAnisotropy = 1.0f;

        return checkVk(vkCreateSampler(m_device, &sampler_info, nullptr, &m_sampler), "vkCreateSampler(shadow map)");
    }

    void VulkanShadowMapTarget::cleanupImage() {
        for (VkImageView layer_view : m_depth_layer_views) {
            if (layer_view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, layer_view, nullptr);
            }
        }
        m_depth_layer_views.clear();

        if (m_depth_image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_depth_image_view, nullptr);
            m_depth_image_view = VK_NULL_HANDLE;
        }
        if (m_depth_image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_depth_image, nullptr);
            m_depth_image = VK_NULL_HANDLE;
        }
        if (m_depth_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_depth_memory, nullptr);
            m_depth_memory = VK_NULL_HANDLE;
        }

        m_depth_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_extent = {};
        m_layer_count = 1;
        m_ready = false;
    }

    void VulkanShadowMapTarget::cleanupSampler() {
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device, m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
        }
    }

    uint32_t VulkanShadowMapTarget::findMemoryType(
        uint32_t type_filter,
        VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memory_properties{};
        vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memory_properties);

        for (uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
            const bool type_matches = (type_filter & (1u << index)) != 0;
            const bool properties_match = (memory_properties.memoryTypes[index].propertyFlags & properties) == properties;
            if (type_matches && properties_match) {
                return index;
            }
        }

        return UINT32_MAX;
    }

    VkFormat VulkanShadowMapTarget::findDepthFormat() const {
        const std::array<VkFormat, 3> candidates{
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        for (VkFormat format : candidates) {
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &properties);
            if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0 &&
                (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0) {
                return format;
            }
        }

        return VK_FORMAT_UNDEFINED;
    }
} // namespace NexAur

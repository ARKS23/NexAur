#include "pch.h"
#include "vulkan_scene_color_target.h"

#include <algorithm>

namespace NexAur {
    namespace {
        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

        uint32_t findMemoryType(
            VkPhysicalDevice physical_device,
            uint32_t type_filter,
            VkMemoryPropertyFlags properties) {
            VkPhysicalDeviceMemoryProperties memory_properties{};
            vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

            for (uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
                const bool type_matches = (type_filter & (1u << index)) != 0;
                const bool properties_match = (memory_properties.memoryTypes[index].propertyFlags & properties) == properties;
                if (type_matches && properties_match) {
                    return index;
                }
            }

            return UINT32_MAX;
        }
    } // namespace

    VulkanSceneColorTarget::~VulkanSceneColorTarget() {
        shutdown();
    }

    bool VulkanSceneColorTarget::init(
        const VulkanResourceContext& context,
        VkFormat color_format,
        uint32_t width,
        uint32_t height) {
        shutdown();

        if (!context.valid() || color_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanSceneColorTarget requires a valid Vulkan context and color format.");
            return false;
        }

        m_physical_device = context.physical_device;
        m_device = context.device;
        m_color_format = color_format;

        width = std::max(1u, width);
        height = std::max(1u, height);
        if (!recreateImage(width, height) || !createSampler()) {
            shutdown();
            return false;
        }

        return true;
    }

    bool VulkanSceneColorTarget::resize(uint32_t width, uint32_t height) {
        if (m_device == VK_NULL_HANDLE || m_color_format == VK_FORMAT_UNDEFINED) {
            return false;
        }

        width = std::max(1u, width);
        height = std::max(1u, height);
        if (m_ready && m_extent.width == width && m_extent.height == height) {
            return true;
        }

        return recreateImage(width, height);
    }

    void VulkanSceneColorTarget::shutdown() {
        cleanupImage();
        cleanupSampler();
        m_color_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_physical_device = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
    }

    bool VulkanSceneColorTarget::recreateImage(uint32_t width, uint32_t height) {
        cleanupImage();
        if (!createImage(width, height)) {
            cleanupImage();
            return false;
        }

        m_extent = { width, height };
        m_color_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_ready = true;
        return true;
    }

    bool VulkanSceneColorTarget::createImage(uint32_t width, uint32_t height) {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = m_color_format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (!checkVk(vkCreateImage(m_device, &image_info, nullptr, &m_color_image), "vkCreateImage(scene color)")) {
            return false;
        }

        VkMemoryRequirements memory_requirements{};
        vkGetImageMemoryRequirements(m_device, m_color_image, &memory_requirements);

        const uint32_t memory_type = findMemoryType(
            m_physical_device,
            memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory_type == UINT32_MAX) {
            NX_CORE_ERROR("VulkanSceneColorTarget failed to find device-local image memory.");
            return false;
        }

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;

        if (!checkVk(vkAllocateMemory(m_device, &allocate_info, nullptr, &m_color_memory), "vkAllocateMemory(scene color)") ||
            !checkVk(vkBindImageMemory(m_device, m_color_image, m_color_memory, 0), "vkBindImageMemory(scene color)")) {
            return false;
        }

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_color_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = m_color_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        return checkVk(vkCreateImageView(m_device, &view_info, nullptr, &m_color_image_view), "vkCreateImageView(scene color)");
    }

    bool VulkanSceneColorTarget::createSampler() {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;
        sampler_info.maxAnisotropy = 1.0f;

        return checkVk(vkCreateSampler(m_device, &sampler_info, nullptr, &m_sampler), "vkCreateSampler(scene color)");
    }

    void VulkanSceneColorTarget::cleanupImage() {
        m_ready = false;

        if (m_color_image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_color_image_view, nullptr);
            m_color_image_view = VK_NULL_HANDLE;
        }

        if (m_color_image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_color_image, nullptr);
            m_color_image = VK_NULL_HANDLE;
        }

        if (m_color_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_color_memory, nullptr);
            m_color_memory = VK_NULL_HANDLE;
        }

        m_color_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_extent = {};
    }

    void VulkanSceneColorTarget::cleanupSampler() {
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device, m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
        }
    }
} // namespace NexAur

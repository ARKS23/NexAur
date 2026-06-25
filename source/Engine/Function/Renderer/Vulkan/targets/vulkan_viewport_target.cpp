#include "pch.h"
#include "vulkan_viewport_target.h"

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

        VkFormat findDepthFormat(VkPhysicalDevice physical_device) {
            const std::array<VkFormat, 3> candidates{
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT
            };

            for (VkFormat format : candidates) {
                VkFormatProperties properties{};
                vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
                if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
                    return format;
                }
            }

            return VK_FORMAT_UNDEFINED;
        }
    } // namespace

    VulkanViewportTarget::~VulkanViewportTarget() {
        shutdown();
    }

    bool VulkanViewportTarget::init(
        const VulkanResourceContext& context,
        VkFormat color_format,
        uint32_t width,
        uint32_t height) {
        shutdown();

        if (!context.valid() || color_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanViewportTarget requires a valid Vulkan context and color format.");
            return false;
        }

        m_physical_device = context.physical_device;
        m_device = context.device;
        m_color_format = color_format;
        m_depth_format = findDepthFormat(context.physical_device);
        if (m_depth_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanViewportTarget failed to find a supported depth format.");
            shutdown();
            return false;
        }

        if (!createSampler() || !recreateImages(width, height)) {
            shutdown();
            return false;
        }

        return true;
    }

    bool VulkanViewportTarget::resize(uint32_t width, uint32_t height) {
        if (m_device == VK_NULL_HANDLE) {
            return false;
        }

        width = std::max(1u, width);
        height = std::max(1u, height);
        if (m_ready && m_extent.width == width && m_extent.height == height) {
            return true;
        }

        cleanupImages();
        return recreateImages(width, height);
    }

    void VulkanViewportTarget::shutdown() {
        cleanupImages();
        cleanupSampler();
        m_physical_device = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_color_format = VK_FORMAT_UNDEFINED;
        m_depth_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_ready = false;
    }

    VulkanRenderTarget VulkanViewportTarget::getRenderTarget() const {
        VulkanRenderTarget target;
        target.color_view = m_color_image_view;
        target.color_format = m_color_format;
        target.depth_view = m_depth_image_view;
        target.depth_format = m_depth_format;
        target.extent = m_extent;
        return target;
    }

    bool VulkanViewportTarget::recreateImages(uint32_t width, uint32_t height) {
        width = std::max(1u, width);
        height = std::max(1u, height);

        const VkImageUsageFlags color_usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if (!createImage(
                width,
                height,
                m_color_format,
                color_usage,
                VK_IMAGE_ASPECT_COLOR_BIT,
                m_color_image,
                m_color_memory,
                m_color_image_view)) {
            cleanupImages();
            return false;
        }

        if (!createImage(
                width,
                height,
                m_depth_format,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                m_depth_image,
                m_depth_memory,
                m_depth_image_view)) {
            cleanupImages();
            return false;
        }

        m_extent = { width, height };
        m_color_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_depth_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_ready = true;
        return true;
    }

    bool VulkanViewportTarget::createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImageAspectFlags aspect,
        VkImage& image,
        VkDeviceMemory& memory,
        VkImageView& image_view) {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = usage;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (!checkVk(vkCreateImage(m_device, &image_info, nullptr, &image), "vkCreateImage(viewport target)")) {
            return false;
        }

        VkMemoryRequirements memory_requirements{};
        vkGetImageMemoryRequirements(m_device, image, &memory_requirements);

        const uint32_t memory_type = findMemoryType(
            m_physical_device,
            memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory_type == UINT32_MAX) {
            NX_CORE_ERROR("VulkanViewportTarget failed to find device-local image memory.");
            return false;
        }

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;

        if (!checkVk(vkAllocateMemory(m_device, &allocate_info, nullptr, &memory), "vkAllocateMemory(viewport target)") ||
            !checkVk(vkBindImageMemory(m_device, image, memory, 0), "vkBindImageMemory(viewport target)")) {
            return false;
        }

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = format;
        view_info.subresourceRange.aspectMask = aspect;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        return checkVk(vkCreateImageView(m_device, &view_info, nullptr, &image_view), "vkCreateImageView(viewport target)");
    }

    bool VulkanViewportTarget::createSampler() {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 1.0f;
        sampler_info.maxAnisotropy = 1.0f;

        return checkVk(vkCreateSampler(m_device, &sampler_info, nullptr, &m_sampler), "vkCreateSampler(viewport target)");
    }

    void VulkanViewportTarget::cleanupImages() {
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

        m_color_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_depth_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_extent = {};
        m_ready = false;
    }

    void VulkanViewportTarget::cleanupSampler() {
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device, m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
        }
    }
} // namespace NexAur

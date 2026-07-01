#include "pch.h"
#include "vulkan_ao_target.h"

#include <algorithm>
#include <limits>

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
                const bool properties_match =
                    (memory_properties.memoryTypes[index].propertyFlags & properties) == properties;
                if (type_matches && properties_match) {
                    return index;
                }
            }

            return std::numeric_limits<uint32_t>::max();
        }

        VkExtent2D resolveAoExtent(uint32_t width, uint32_t height, bool half_resolution) {
            width = std::max(1u, width);
            height = std::max(1u, height);
            if (half_resolution) {
                width = std::max(1u, width / 2u);
                height = std::max(1u, height / 2u);
            }

            return { width, height };
        }
    } // namespace

    VulkanAoTarget::~VulkanAoTarget() {
        shutdown();
    }

    bool VulkanAoTarget::init(
        const VulkanResourceContext& context,
        VkFormat color_format,
        uint32_t width,
        uint32_t height,
        bool half_resolution) {
        shutdown();

        if (!context.valid() || color_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanAoTarget requires a valid Vulkan context and color format.");
            return false;
        }

        m_physical_device = context.physical_device;
        m_device = context.device;
        m_color_format = color_format;

        if (!createSampler() || !recreateImages(width, height, half_resolution)) {
            shutdown();
            return false;
        }

        return true;
    }

    bool VulkanAoTarget::resize(uint32_t width, uint32_t height, bool half_resolution) {
        if (m_device == VK_NULL_HANDLE || m_color_format == VK_FORMAT_UNDEFINED) {
            return false;
        }

        const VkExtent2D next_extent = resolveAoExtent(width, height, half_resolution);
        if (m_ready &&
            m_extent.width == next_extent.width &&
            m_extent.height == next_extent.height &&
            m_half_resolution == half_resolution) {
            return true;
        }

        return recreateImages(width, height, half_resolution);
    }

    void VulkanAoTarget::shutdown() {
        cleanupImages();
        cleanupSampler();
        m_physical_device = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_color_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_half_resolution = true;
    }

    VulkanAoRenderTarget VulkanAoTarget::getRawRenderTarget() const {
        return {
            m_raw_image.view.view,
            m_raw_image.view.format,
            m_raw_image.view.extent
        };
    }

    VulkanAoRenderTarget VulkanAoTarget::getBlurredRenderTarget() const {
        return {
            m_blurred_image.view.view,
            m_blurred_image.view.format,
            m_blurred_image.view.extent
        };
    }

    bool VulkanAoTarget::recreateImages(uint32_t width, uint32_t height, bool half_resolution) {
        cleanupImages();

        const VkExtent2D extent = resolveAoExtent(width, height, half_resolution);
        if (!createImage(extent.width, extent.height, m_raw_image) ||
            !createImage(extent.width, extent.height, m_blurred_image)) {
            cleanupImages();
            return false;
        }

        m_extent = extent;
        m_half_resolution = half_resolution;
        m_ready = true;
        return true;
    }

    bool VulkanAoTarget::createImage(uint32_t width, uint32_t height, OwnedImage& image) {
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
        image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (!checkVk(vkCreateImage(m_device, &image_info, nullptr, &image.view.image), "vkCreateImage(ao target)")) {
            return false;
        }

        VkMemoryRequirements memory_requirements{};
        vkGetImageMemoryRequirements(m_device, image.view.image, &memory_requirements);

        const uint32_t memory_type = findMemoryType(
            m_physical_device,
            memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory_type == std::numeric_limits<uint32_t>::max()) {
            NX_CORE_ERROR("VulkanAoTarget failed to find device-local image memory.");
            return false;
        }

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;

        if (!checkVk(vkAllocateMemory(m_device, &allocate_info, nullptr, &image.memory), "vkAllocateMemory(ao target)") ||
            !checkVk(vkBindImageMemory(m_device, image.view.image, image.memory, 0), "vkBindImageMemory(ao target)")) {
            return false;
        }

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = image.view.image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = m_color_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (!checkVk(vkCreateImageView(m_device, &view_info, nullptr, &image.view.view), "vkCreateImageView(ao target)")) {
            return false;
        }

        image.view.format = m_color_format;
        image.view.extent = { width, height };
        image.view.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        return true;
    }

    bool VulkanAoTarget::createSampler() {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;
        sampler_info.maxAnisotropy = 1.0f;

        return checkVk(vkCreateSampler(m_device, &sampler_info, nullptr, &m_sampler), "vkCreateSampler(ao target)");
    }

    void VulkanAoTarget::cleanupImages() {
        auto cleanup_image = [this](OwnedImage& image) {
            if (image.view.view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, image.view.view, nullptr);
                image.view.view = VK_NULL_HANDLE;
            }
            if (image.view.image != VK_NULL_HANDLE) {
                vkDestroyImage(m_device, image.view.image, nullptr);
                image.view.image = VK_NULL_HANDLE;
            }
            if (image.memory != VK_NULL_HANDLE) {
                vkFreeMemory(m_device, image.memory, nullptr);
                image.memory = VK_NULL_HANDLE;
            }

            image.view = {};
        };

        cleanup_image(m_raw_image);
        cleanup_image(m_blurred_image);
        m_extent = {};
        m_ready = false;
    }

    void VulkanAoTarget::cleanupSampler() {
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device, m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
        }
    }
} // namespace NexAur

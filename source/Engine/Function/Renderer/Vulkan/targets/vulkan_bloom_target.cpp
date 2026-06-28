#include "pch.h"
#include "vulkan_bloom_target.h"

#include <algorithm>
#include <array>
#include <limits>

namespace NexAur {
    namespace {
        constexpr uint32_t kMaxBloomMipCount = 6;

        const VulkanBloomImageView kInvalidBloomImage{};

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
    } // namespace

    VulkanBloomTarget::~VulkanBloomTarget() {
        shutdown();
    }

    bool VulkanBloomTarget::init(
        const VulkanResourceContext& context,
        VkFormat color_format,
        uint32_t width,
        uint32_t height) {
        shutdown();

        if (!context.valid() || color_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanBloomTarget requires a valid Vulkan context and color format.");
            return false;
        }

        m_physical_device = context.physical_device;
        m_device = context.device;
        m_color_format = color_format;

        if (!createSampler() || !recreateImages(width, height)) {
            shutdown();
            return false;
        }

        return true;
    }

    bool VulkanBloomTarget::resize(uint32_t width, uint32_t height) {
        if (m_device == VK_NULL_HANDLE || m_color_format == VK_FORMAT_UNDEFINED) {
            return false;
        }

        width = std::max(1u, width);
        height = std::max(1u, height);
        if (m_ready && m_extent.width == width && m_extent.height == height) {
            return true;
        }

        return recreateImages(width, height);
    }

    void VulkanBloomTarget::shutdown() {
        cleanupImages();
        cleanupSampler();
        m_physical_device = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_color_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
    }

    const VulkanBloomImageView& VulkanBloomTarget::getDownsampleImage(uint32_t index) const {
        if (index >= m_downsample_images.size()) {
            return kInvalidBloomImage;
        }

        return m_downsample_images[index].view;
    }

    const VulkanBloomImageView& VulkanBloomTarget::getUpsampleImage(uint32_t index) const {
        if (index >= m_upsample_images.size()) {
            return kInvalidBloomImage;
        }

        return m_upsample_images[index].view;
    }

    VulkanBloomRenderTarget VulkanBloomTarget::getDownsampleRenderTarget(uint32_t index) const {
        const VulkanBloomImageView& image = getDownsampleImage(index);
        return { image.view, image.format, image.extent };
    }

    VulkanBloomRenderTarget VulkanBloomTarget::getUpsampleRenderTarget(uint32_t index) const {
        const VulkanBloomImageView& image = getUpsampleImage(index);
        return { image.view, image.format, image.extent };
    }

    VulkanBloomRenderTarget VulkanBloomTarget::getCompositeRenderTarget() const {
        return {
            m_composite_image.view.view,
            m_composite_image.view.format,
            m_composite_image.view.extent
        };
    }

    void VulkanBloomTarget::setDownsampleLayout(uint32_t index, VkImageLayout layout) {
        if (index < m_downsample_images.size()) {
            m_downsample_images[index].view.layout = layout;
        }
    }

    void VulkanBloomTarget::setUpsampleLayout(uint32_t index, VkImageLayout layout) {
        if (index < m_upsample_images.size()) {
            m_upsample_images[index].view.layout = layout;
        }
    }

    bool VulkanBloomTarget::recreateImages(uint32_t width, uint32_t height) {
        cleanupImages();

        width = std::max(1u, width);
        height = std::max(1u, height);
        m_extent = { width, height };

        if (!createImage(width, height, m_composite_image)) {
            cleanupImages();
            return false;
        }

        const uint32_t mip_count = computeMipCount(width, height);
        m_downsample_images.resize(mip_count);
        m_upsample_images.resize(mip_count > 1 ? mip_count - 1 : 0);

        uint32_t mip_width = std::max(1u, width / 2u);
        uint32_t mip_height = std::max(1u, height / 2u);
        for (uint32_t mip_index = 0; mip_index < mip_count; ++mip_index) {
            if (!createImage(mip_width, mip_height, m_downsample_images[mip_index])) {
                cleanupImages();
                return false;
            }

            if (mip_index < m_upsample_images.size() &&
                !createImage(mip_width, mip_height, m_upsample_images[mip_index])) {
                cleanupImages();
                return false;
            }

            mip_width = std::max(1u, mip_width / 2u);
            mip_height = std::max(1u, mip_height / 2u);
        }

        m_ready = true;
        return true;
    }

    bool VulkanBloomTarget::createImage(uint32_t width, uint32_t height, OwnedImage& image) {
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

        if (!checkVk(vkCreateImage(m_device, &image_info, nullptr, &image.view.image), "vkCreateImage(bloom target)")) {
            return false;
        }

        VkMemoryRequirements memory_requirements{};
        vkGetImageMemoryRequirements(m_device, image.view.image, &memory_requirements);

        const uint32_t memory_type = findMemoryType(
            m_physical_device,
            memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory_type == std::numeric_limits<uint32_t>::max()) {
            NX_CORE_ERROR("VulkanBloomTarget failed to find device-local image memory.");
            return false;
        }

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;

        if (!checkVk(vkAllocateMemory(m_device, &allocate_info, nullptr, &image.memory), "vkAllocateMemory(bloom target)") ||
            !checkVk(vkBindImageMemory(m_device, image.view.image, image.memory, 0), "vkBindImageMemory(bloom target)")) {
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

        if (!checkVk(vkCreateImageView(m_device, &view_info, nullptr, &image.view.view), "vkCreateImageView(bloom target)")) {
            return false;
        }

        image.view.format = m_color_format;
        image.view.extent = { width, height };
        image.view.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        return true;
    }

    bool VulkanBloomTarget::createSampler() {
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

        return checkVk(vkCreateSampler(m_device, &sampler_info, nullptr, &m_sampler), "vkCreateSampler(bloom target)");
    }

    void VulkanBloomTarget::cleanupImages() {
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

        for (OwnedImage& image : m_downsample_images) {
            cleanup_image(image);
        }
        for (OwnedImage& image : m_upsample_images) {
            cleanup_image(image);
        }

        cleanup_image(m_composite_image);

        m_downsample_images.clear();
        m_upsample_images.clear();
        m_extent = {};
        m_ready = false;
    }

    void VulkanBloomTarget::cleanupSampler() {
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device, m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
        }
    }

    uint32_t VulkanBloomTarget::computeMipCount(uint32_t width, uint32_t height) const {
        uint32_t mip_count = 0;
        uint32_t mip_width = std::max(1u, width / 2u);
        uint32_t mip_height = std::max(1u, height / 2u);

        while (mip_count < kMaxBloomMipCount && mip_width > 1 && mip_height > 1) {
            ++mip_count;
            mip_width = std::max(1u, mip_width / 2u);
            mip_height = std::max(1u, mip_height / 2u);
        }

        return std::max(1u, mip_count);
    }
} // namespace NexAur

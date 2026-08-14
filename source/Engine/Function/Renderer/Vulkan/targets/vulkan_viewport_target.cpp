#include "pch.h"
#include "vulkan_viewport_target.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"

#include <algorithm>
#include <array>

namespace NexAur {
    namespace {
        VkFormat findDepthFormat(VkPhysicalDevice physical_device) {
            constexpr VkFormatFeatureFlags required_features =
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
            const std::array<VkFormat, 3> candidates{
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT
            };

            for (VkFormat format : candidates) {
                VkFormatProperties properties{};
                vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
                if ((properties.optimalTilingFeatures & required_features) == required_features) {
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

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() ||
            color_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanViewportTarget requires a valid Vulkan context and color format.");
            return false;
        }

        m_physical_device = context.physical_device;
        m_gpu_allocator = context.gpu_allocator;
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
        m_gpu_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
        m_color_format = VK_FORMAT_UNDEFINED;
        m_depth_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_ready = false;
    }

    VulkanRenderTarget VulkanViewportTarget::getRenderTarget() const {
        VulkanRenderTarget target;
        target.color_view = m_color_image.getImageView();
        target.color_format = m_color_format;
        target.depth_view = m_depth_image.getImageView();
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
                m_color_image)) {
            cleanupImages();
            return false;
        }

        if (!createImage(
                width,
                height,
                m_depth_format,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                m_depth_image)) {
            cleanupImages();
            return false;
        }

        m_extent = { width, height };
        m_ready = true;
        return true;
    }

    bool VulkanViewportTarget::createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImageAspectFlags aspect,
        VulkanOwnedImage& image) {
        VulkanOwnedImageCreateInfo create_info;
        create_info.extent = { width, height, 1 };
        create_info.format = format;
        create_info.usage = usage;
        create_info.aspect_mask = aspect;
        create_info.debug_name = aspect == VK_IMAGE_ASPECT_DEPTH_BIT ?
            "VulkanViewportTarget depth image" :
            "VulkanViewportTarget color image";
        return image.create(*m_gpu_allocator, create_info);
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

        return m_sampler.create(*m_gpu_allocator, sampler_info, "vkCreateSampler(viewport target)");
    }

    void VulkanViewportTarget::cleanupImages() {
        m_color_image.reset();
        m_depth_image.reset();

        m_extent = {};
        m_ready = false;
    }

    void VulkanViewportTarget::cleanupSampler() {
        m_sampler.reset();
    }
} // namespace NexAur

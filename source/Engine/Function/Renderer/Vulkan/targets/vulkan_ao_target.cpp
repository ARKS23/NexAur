#include "pch.h"
#include "vulkan_ao_target.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"

#include <algorithm>
namespace NexAur {
    namespace {
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

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() ||
            color_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanAoTarget requires a valid Vulkan context and color format.");
            return false;
        }

        m_gpu_allocator = context.gpu_allocator;
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
        m_gpu_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
        m_color_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_half_resolution = true;
    }

    VulkanAoRenderTarget VulkanAoTarget::getRawRenderTarget() const {
        return {
            m_raw_image.getImageView(),
            m_raw_image.getFormat(),
            m_raw_image.getExtent()
        };
    }

    VulkanAoRenderTarget VulkanAoTarget::getBlurredRenderTarget() const {
        return {
            m_blurred_image.getImageView(),
            m_blurred_image.getFormat(),
            m_blurred_image.getExtent()
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

    bool VulkanAoTarget::createImage(uint32_t width, uint32_t height, VulkanOwnedImage& image) {
        VulkanOwnedImageCreateInfo create_info;
        create_info.extent = { width, height, 1 };
        create_info.format = m_color_format;
        create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        create_info.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.debug_name = "VulkanAoTarget image";
        return image.create(*m_gpu_allocator, create_info);
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

        return m_sampler.create(*m_gpu_allocator, sampler_info, "vkCreateSampler(ao target)");
    }

    void VulkanAoTarget::cleanupImages() {
        m_raw_image.reset();
        m_blurred_image.reset();
        m_extent = {};
        m_ready = false;
    }

    void VulkanAoTarget::cleanupSampler() {
        m_sampler.reset();
    }
} // namespace NexAur

#include "pch.h"
#include "vulkan_ssr_target.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"

#include <algorithm>
namespace NexAur {
    VulkanSsrTarget::~VulkanSsrTarget() {
        shutdown();
    }

    bool VulkanSsrTarget::init(
        const VulkanResourceContext& context,
        VkFormat reflection_format,
        VkFormat hit_mask_format,
        uint32_t width,
        uint32_t height) {
        shutdown();

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() ||
            reflection_format == VK_FORMAT_UNDEFINED ||
            hit_mask_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanSsrTarget requires a valid Vulkan context and color formats.");
            return false;
        }

        m_gpu_allocator = context.gpu_allocator;
        m_device = context.device;
        m_reflection_format = reflection_format;
        m_hit_mask_format = hit_mask_format;

        if (!createSampler() || !recreateImages(width, height)) {
            shutdown();
            return false;
        }

        return true;
    }

    bool VulkanSsrTarget::resize(uint32_t width, uint32_t height) {
        if (m_device == VK_NULL_HANDLE ||
            m_reflection_format == VK_FORMAT_UNDEFINED ||
            m_hit_mask_format == VK_FORMAT_UNDEFINED) {
            return false;
        }

        width = std::max(1u, width);
        height = std::max(1u, height);
        if (m_ready && m_extent.width == width && m_extent.height == height) {
            return true;
        }

        return recreateImages(width, height);
    }

    void VulkanSsrTarget::shutdown() {
        cleanupImages();
        cleanupSampler();
        m_reflection_format = VK_FORMAT_UNDEFINED;
        m_hit_mask_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_gpu_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
    }

    VulkanSsrRenderTarget VulkanSsrTarget::getRawReflectionRenderTarget() const {
        return {
            m_raw_reflection_image.getImageView(),
            m_raw_reflection_image.getFormat(),
            m_raw_reflection_image.getExtent()
        };
    }

    VulkanSsrRenderTarget VulkanSsrTarget::getHitMaskRenderTarget() const {
        return {
            m_hit_mask_image.getImageView(),
            m_hit_mask_image.getFormat(),
            m_hit_mask_image.getExtent()
        };
    }

    bool VulkanSsrTarget::recreateImages(uint32_t width, uint32_t height) {
        cleanupImages();

        width = std::max(1u, width);
        height = std::max(1u, height);
        if (!createImage(width, height, m_reflection_format, m_raw_reflection_image) ||
            !createImage(width, height, m_hit_mask_format, m_hit_mask_image)) {
            cleanupImages();
            return false;
        }

        m_extent = { width, height };
        m_ready = true;
        return true;
    }

    bool VulkanSsrTarget::createImage(uint32_t width, uint32_t height, VkFormat format, VulkanOwnedImage& image) {
        VulkanOwnedImageCreateInfo create_info;
        create_info.extent = { width, height, 1 };
        create_info.format = format;
        create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        create_info.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.debug_name = "VulkanSsrTarget image";
        return image.create(*m_gpu_allocator, create_info);
    }

    bool VulkanSsrTarget::createSampler() {
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

        return m_sampler.create(*m_gpu_allocator, sampler_info, "vkCreateSampler(ssr target)");
    }

    void VulkanSsrTarget::cleanupImages() {
        m_raw_reflection_image.reset();
        m_hit_mask_image.reset();
        m_extent = {};
        m_ready = false;
    }

    void VulkanSsrTarget::cleanupSampler() {
        m_sampler.reset();
    }
} // namespace NexAur

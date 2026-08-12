#include "pch.h"
#include "vulkan_smaa_target.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"

#include <algorithm>
namespace NexAur {
    VulkanSmaaTarget::~VulkanSmaaTarget() {
        shutdown();
    }

    bool VulkanSmaaTarget::init(
        const VulkanResourceContext& context,
        VkFormat source_format,
        VkFormat mask_format,
        uint32_t width,
        uint32_t height) {
        shutdown();

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() ||
            source_format == VK_FORMAT_UNDEFINED ||
            mask_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanSmaaTarget requires a valid Vulkan context and color formats.");
            return false;
        }

        m_gpu_allocator = context.gpu_allocator;
        m_device = context.device;
        m_source_format = source_format;
        m_mask_format = mask_format;

        if (!createSampler() || !recreateImages(width, height)) {
            shutdown();
            return false;
        }

        return true;
    }

    bool VulkanSmaaTarget::resize(uint32_t width, uint32_t height) {
        if (m_device == VK_NULL_HANDLE ||
            m_source_format == VK_FORMAT_UNDEFINED ||
            m_mask_format == VK_FORMAT_UNDEFINED) {
            return false;
        }

        width = std::max(1u, width);
        height = std::max(1u, height);
        if (m_ready && m_extent.width == width && m_extent.height == height) {
            return true;
        }

        return recreateImages(width, height);
    }

    void VulkanSmaaTarget::shutdown() {
        cleanupImages();
        cleanupSampler();
        m_source_format = VK_FORMAT_UNDEFINED;
        m_mask_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_gpu_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
    }

    VulkanSmaaRenderTarget VulkanSmaaTarget::getSourceRenderTarget() const {
        return {
            m_source_image.getImageView(),
            m_source_image.getFormat(),
            m_source_image.getExtent()
        };
    }

    VulkanSmaaRenderTarget VulkanSmaaTarget::getEdgeRenderTarget() const {
        return {
            m_edge_image.getImageView(),
            m_edge_image.getFormat(),
            m_edge_image.getExtent()
        };
    }

    VulkanSmaaRenderTarget VulkanSmaaTarget::getBlendRenderTarget() const {
        return {
            m_blend_image.getImageView(),
            m_blend_image.getFormat(),
            m_blend_image.getExtent()
        };
    }

    bool VulkanSmaaTarget::recreateImages(uint32_t width, uint32_t height) {
        cleanupImages();

        width = std::max(1u, width);
        height = std::max(1u, height);
        if (!createImage(width, height, m_source_format, m_source_image) ||
            !createImage(width, height, m_mask_format, m_edge_image) ||
            !createImage(width, height, m_mask_format, m_blend_image)) {
            cleanupImages();
            return false;
        }

        m_extent = { width, height };
        m_ready = true;
        return true;
    }

    bool VulkanSmaaTarget::createImage(uint32_t width, uint32_t height, VkFormat format, VulkanOwnedImage& image) {
        VulkanOwnedImageCreateInfo create_info;
        create_info.extent = { width, height, 1 };
        create_info.format = format;
        create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        create_info.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.debug_name = "VulkanSmaaTarget image";
        return image.create(*m_gpu_allocator, create_info);
    }

    bool VulkanSmaaTarget::createSampler() {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.minFilter = VK_FILTER_NEAREST;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;
        sampler_info.maxAnisotropy = 1.0f;

        return m_sampler.create(m_device, sampler_info, "vkCreateSampler(smaa target)");
    }

    void VulkanSmaaTarget::cleanupImages() {
        m_source_image.reset();
        m_edge_image.reset();
        m_blend_image.reset();
        m_extent = {};
        m_ready = false;
    }

    void VulkanSmaaTarget::cleanupSampler() {
        m_sampler.reset();
    }
} // namespace NexAur

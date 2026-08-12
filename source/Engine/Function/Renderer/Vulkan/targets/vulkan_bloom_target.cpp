#include "pch.h"
#include "vulkan_bloom_target.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"

#include <algorithm>

namespace NexAur {
    namespace {
        constexpr uint32_t kMaxBloomMipCount = 6;

        const VulkanBloomImageView kInvalidBloomImage{};

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

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() ||
            color_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanBloomTarget requires a valid Vulkan context and color format.");
            return false;
        }

        m_gpu_allocator = context.gpu_allocator;
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
        m_gpu_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
        m_color_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
    }

    const VulkanBloomImageView& VulkanBloomTarget::getDownsampleImage(uint32_t index) const {
        if (index >= m_downsample_images.size()) {
            return kInvalidBloomImage;
        }

        return m_downsample_images[index].getView();
    }

    const VulkanBloomImageView& VulkanBloomTarget::getUpsampleImage(uint32_t index) const {
        if (index >= m_upsample_images.size()) {
            return kInvalidBloomImage;
        }

        return m_upsample_images[index].getView();
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
            m_composite_image.getImageView(),
            m_composite_image.getFormat(),
            m_composite_image.getExtent()
        };
    }

    void VulkanBloomTarget::setDownsampleLayout(uint32_t index, VkImageLayout layout) {
        if (index < m_downsample_images.size()) {
            m_downsample_images[index].setLayout(layout);
        }
    }

    void VulkanBloomTarget::setUpsampleLayout(uint32_t index, VkImageLayout layout) {
        if (index < m_upsample_images.size()) {
            m_upsample_images[index].setLayout(layout);
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

    bool VulkanBloomTarget::createImage(uint32_t width, uint32_t height, VulkanOwnedImage& image) {
        VulkanOwnedImageCreateInfo create_info;
        create_info.extent = { width, height, 1 };
        create_info.format = m_color_format;
        create_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        create_info.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.debug_name = "VulkanBloomTarget image";
        return image.create(*m_gpu_allocator, create_info);
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

        return m_sampler.create(m_device, sampler_info, "vkCreateSampler(bloom target)");
    }

    void VulkanBloomTarget::cleanupImages() {
        m_downsample_images.clear();
        m_upsample_images.clear();
        m_composite_image.reset();
        m_extent = {};
        m_ready = false;
    }

    void VulkanBloomTarget::cleanupSampler() {
        m_sampler.reset();
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

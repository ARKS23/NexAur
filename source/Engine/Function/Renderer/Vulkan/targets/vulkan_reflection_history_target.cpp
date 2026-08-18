#include "pch.h"
#include "vulkan_reflection_history_target.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"

#include <algorithm>

namespace NexAur {
    VulkanReflectionHistoryTarget::~VulkanReflectionHistoryTarget() {
        shutdown();
    }

    bool VulkanReflectionHistoryTarget::init(
        const VulkanResourceContext& context,
        VkFormat format,
        uint32_t width,
        uint32_t height) {
        shutdown();

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() ||
            format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanReflectionHistoryTarget requires a valid context and format.");
            return false;
        }

        m_gpu_allocator = context.gpu_allocator;
        m_device = context.device;
        m_format = format;
        if (!createSampler() || !recreateImages(width, height)) {
            shutdown();
            return false;
        }
        return true;
    }

    bool VulkanReflectionHistoryTarget::resize(uint32_t width, uint32_t height) {
        if (m_device == VK_NULL_HANDLE || m_format == VK_FORMAT_UNDEFINED) {
            return false;
        }

        width = std::max(1u, width);
        height = std::max(1u, height);
        if (m_ready && m_extent.width == width && m_extent.height == height) {
            return true;
        }
        return recreateImages(width, height);
    }

    void VulkanReflectionHistoryTarget::shutdown() {
        cleanupImages();
        cleanupSampler();
        m_gpu_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
        m_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_read_index = 0;
        m_write_index = 1;
    }

    const VulkanImageViewState* VulkanReflectionHistoryTarget::getImage(
        VulkanReflectionHistoryImage image,
        uint32_t ping_pong_index) const {
        const VulkanOwnedImage* owned_image = getOwnedImage(image, ping_pong_index);
        return owned_image != nullptr && owned_image->isReady() ?
            &owned_image->getView() : nullptr;
    }

    void VulkanReflectionHistoryTarget::setImageLayout(
        VulkanReflectionHistoryImage image,
        uint32_t ping_pong_index,
        VkImageLayout layout) {
        if (VulkanOwnedImage* owned_image = getOwnedImage(image, ping_pong_index)) {
            owned_image->setLayout(layout);
        }
    }

    bool VulkanReflectionHistoryTarget::recreateImages(uint32_t width, uint32_t height) {
        cleanupImages();
        width = std::max(1u, width);
        height = std::max(1u, height);

        if (!createImage(width, height, "Reflection raw history", m_raw_reflection) ||
            !createImage(width, height, "Reflection hit distance history", m_hit_distance)) {
            cleanupImages();
            return false;
        }

        const std::array<const char*, kPingPongImageCount> names{
            "Reflection filtered radiance history",
            "Reflection moments history",
            "Reflection length history",
            "Reflection depth history",
            "Reflection surface history"
        };
        for (uint32_t index = 0; index < kPingPongCount; ++index) {
            if (!createImage(width, height, names[0], m_filtered_radiance[index]) ||
                !createImage(width, height, names[1], m_moments[index]) ||
                !createImage(width, height, names[2], m_history_length[index]) ||
                !createImage(width, height, names[3], m_depth[index]) ||
                !createImage(width, height, names[4], m_surface[index])) {
                cleanupImages();
                return false;
            }
        }

        m_extent = { width, height };
        m_read_index = 0;
        m_write_index = 1;
        m_ready = true;
        return true;
    }

    bool VulkanReflectionHistoryTarget::createImage(
        uint32_t width,
        uint32_t height,
        const char* debug_name,
        VulkanOwnedImage& image) {
        VulkanOwnedImageCreateInfo create_info;
        create_info.extent = { width, height, 1 };
        create_info.format = m_format;
        create_info.usage =
            VK_IMAGE_USAGE_STORAGE_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT;
        create_info.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.debug_name = debug_name != nullptr ? debug_name : "Reflection history image";
        return image.create(*m_gpu_allocator, create_info);
    }

    bool VulkanReflectionHistoryTarget::createSampler() {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.maxAnisotropy = 1.0f;
        return m_sampler.create(*m_gpu_allocator, sampler_info, "vkCreateSampler(reflection history)");
    }

    void VulkanReflectionHistoryTarget::cleanupImages() {
        m_raw_reflection.reset();
        m_hit_distance.reset();
        for (VulkanOwnedImage& image : m_filtered_radiance) {
            image.reset();
        }
        for (VulkanOwnedImage& image : m_moments) {
            image.reset();
        }
        for (VulkanOwnedImage& image : m_history_length) {
            image.reset();
        }
        for (VulkanOwnedImage& image : m_depth) {
            image.reset();
        }
        for (VulkanOwnedImage& image : m_surface) {
            image.reset();
        }
        m_extent = {};
        m_ready = false;
    }

    void VulkanReflectionHistoryTarget::cleanupSampler() {
        m_sampler.reset();
    }

    VulkanOwnedImage* VulkanReflectionHistoryTarget::getOwnedImage(
        VulkanReflectionHistoryImage image,
        uint32_t ping_pong_index) {
        if (ping_pong_index >= kPingPongCount) {
            return nullptr;
        }

        switch (image) {
            case VulkanReflectionHistoryImage::RawReflection:
                return ping_pong_index == 0 ? &m_raw_reflection : nullptr;
            case VulkanReflectionHistoryImage::HitDistance:
                return ping_pong_index == 0 ? &m_hit_distance : nullptr;
            case VulkanReflectionHistoryImage::FilteredRadiance:
                return &m_filtered_radiance[ping_pong_index];
            case VulkanReflectionHistoryImage::Moments:
                return &m_moments[ping_pong_index];
            case VulkanReflectionHistoryImage::HistoryLength:
                return &m_history_length[ping_pong_index];
            case VulkanReflectionHistoryImage::Depth:
                return &m_depth[ping_pong_index];
            case VulkanReflectionHistoryImage::Surface:
                return &m_surface[ping_pong_index];
            case VulkanReflectionHistoryImage::Count:
            default:
                return nullptr;
        }
    }

    const VulkanOwnedImage* VulkanReflectionHistoryTarget::getOwnedImage(
        VulkanReflectionHistoryImage image,
        uint32_t ping_pong_index) const {
        return const_cast<VulkanReflectionHistoryTarget*>(this)->getOwnedImage(
            image,
            ping_pong_index);
    }
} // namespace NexAur

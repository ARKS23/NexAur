#include "pch.h"
#include "vulkan_scene_color_target.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"

#include <algorithm>

namespace NexAur {
    VulkanSceneColorTarget::~VulkanSceneColorTarget() {
        shutdown();
    }

    bool VulkanSceneColorTarget::init(
        const VulkanResourceContext& context,
        VkFormat color_format,
        uint32_t width,
        uint32_t height) {
        shutdown();

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() ||
            color_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanSceneColorTarget requires a valid Vulkan context and color format.");
            return false;
        }

        m_gpu_allocator = context.gpu_allocator;
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
        m_gpu_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
    }

    bool VulkanSceneColorTarget::recreateImage(uint32_t width, uint32_t height) {
        cleanupImage();
        if (!createImage(width, height)) {
            cleanupImage();
            return false;
        }

        m_extent = { width, height };
        m_ready = true;
        return true;
    }

    bool VulkanSceneColorTarget::createImage(uint32_t width, uint32_t height) {
        VulkanOwnedImageCreateInfo create_info;
        create_info.extent = { width, height, 1 };
        create_info.format = m_color_format;
        create_info.usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        create_info.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.debug_name = "VulkanSceneColorTarget image";
        return m_color_image.create(*m_gpu_allocator, create_info);
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

        return m_sampler.create(m_device, sampler_info, "vkCreateSampler(scene color)");
    }

    void VulkanSceneColorTarget::cleanupImage() {
        m_ready = false;

        m_color_image.reset();

        m_extent = {};
    }

    void VulkanSceneColorTarget::cleanupSampler() {
        m_sampler.reset();
    }
} // namespace NexAur

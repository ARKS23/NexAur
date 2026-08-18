#include "pch.h"
#include "vulkan_reflection_surface_target.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"

#include <algorithm>

namespace NexAur {
    VulkanReflectionSurfaceTarget::~VulkanReflectionSurfaceTarget() {
        shutdown();
    }

    bool VulkanReflectionSurfaceTarget::init(
        const VulkanResourceContext& context,
        VkFormat reflection_surface_format,
        VkFormat fallback_specular_format,
        VkFormat motion_vector_format,
        uint32_t width,
        uint32_t height) {
        shutdown();

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() ||
            reflection_surface_format == VK_FORMAT_UNDEFINED ||
            fallback_specular_format == VK_FORMAT_UNDEFINED ||
            motion_vector_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanReflectionSurfaceTarget requires valid color formats and a Vulkan context.");
            return false;
        }

        m_gpu_allocator = context.gpu_allocator;
        m_device = context.device;
        m_reflection_surface_format = reflection_surface_format;
        m_fallback_specular_format = fallback_specular_format;
        m_motion_vector_format = motion_vector_format;

        if (!createSampler() || !recreateImages(width, height)) {
            shutdown();
            return false;
        }
        return true;
    }

    bool VulkanReflectionSurfaceTarget::resize(uint32_t width, uint32_t height) {
        if (m_device == VK_NULL_HANDLE ||
            m_reflection_surface_format == VK_FORMAT_UNDEFINED) {
            return false;
        }

        width = std::max(1u, width);
        height = std::max(1u, height);
        if (m_ready && m_extent.width == width && m_extent.height == height) {
            return true;
        }
        return recreateImages(width, height);
    }

    void VulkanReflectionSurfaceTarget::shutdown() {
        cleanupImages();
        cleanupSampler();
        m_gpu_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
        m_reflection_surface_format = VK_FORMAT_UNDEFINED;
        m_fallback_specular_format = VK_FORMAT_UNDEFINED;
        m_motion_vector_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
    }

    void VulkanReflectionSurfaceTarget::applyToRenderTarget(VulkanRenderTarget& target) const {
        target.auxiliary_color_attachment_count = 0;
        if (!m_ready) {
            return;
        }

        target.auxiliary_color_views[0] = m_reflection_surface_image.getImageView();
        target.auxiliary_color_formats[0] = m_reflection_surface_format;
        target.auxiliary_color_views[1] = m_fallback_specular_image.getImageView();
        target.auxiliary_color_formats[1] = m_fallback_specular_format;
        target.auxiliary_color_views[2] = m_motion_vector_image.getImageView();
        target.auxiliary_color_formats[2] = m_motion_vector_format;
        target.auxiliary_color_attachment_count = kVulkanAuxiliaryColorAttachmentCount;
    }

    bool VulkanReflectionSurfaceTarget::recreateImages(uint32_t width, uint32_t height) {
        cleanupImages();

        width = std::max(1u, width);
        height = std::max(1u, height);
        if (!createImage(
                width,
                height,
                m_reflection_surface_format,
                "VulkanReflectionSurfaceTarget reflection surface",
                m_reflection_surface_image) ||
            !createImage(
                width,
                height,
                m_fallback_specular_format,
                "VulkanReflectionSurfaceTarget fallback specular",
                m_fallback_specular_image) ||
            !createImage(
                width,
                height,
                m_motion_vector_format,
                "VulkanReflectionSurfaceTarget motion vector",
                m_motion_vector_image)) {
            cleanupImages();
            return false;
        }

        m_extent = { width, height };
        m_ready = true;
        return true;
    }

    bool VulkanReflectionSurfaceTarget::createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        const char* debug_name,
        VulkanOwnedImage& image) {
        VulkanOwnedImageCreateInfo create_info;
        create_info.extent = { width, height, 1 };
        create_info.format = format;
        create_info.usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT;
        create_info.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.debug_name = debug_name != nullptr ? debug_name : "VulkanReflectionSurfaceTarget image";
        return image.create(*m_gpu_allocator, create_info);
    }

    bool VulkanReflectionSurfaceTarget::createSampler() {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.maxAnisotropy = 1.0f;
        return m_sampler.create(*m_gpu_allocator, sampler_info, "vkCreateSampler(reflection surface)");
    }

    void VulkanReflectionSurfaceTarget::cleanupImages() {
        m_reflection_surface_image.reset();
        m_fallback_specular_image.reset();
        m_motion_vector_image.reset();
        m_extent = {};
        m_ready = false;
    }

    void VulkanReflectionSurfaceTarget::cleanupSampler() {
        m_sampler.reset();
    }
} // namespace NexAur

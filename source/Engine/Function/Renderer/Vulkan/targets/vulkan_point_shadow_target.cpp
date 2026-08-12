#include "pch.h"
#include "vulkan_point_shadow_target.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/data/render_settings.h"

#include <algorithm>
#include <array>

namespace NexAur {
    VulkanPointShadowTarget::~VulkanPointShadowTarget() {
        shutdown();
    }

    bool VulkanPointShadowTarget::init(
        const VulkanResourceContext& context,
        uint32_t resolution,
        uint32_t shadowed_light_capacity) {
        shutdown();

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized()) {
            NX_CORE_ERROR("VulkanPointShadowTarget requires a valid Vulkan context.");
            return false;
        }

        m_physical_device = context.physical_device;
        m_gpu_allocator = context.gpu_allocator;
        m_device = context.device;
        m_depth_format = findDepthFormat();
        if (m_depth_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanPointShadowTarget failed to find a supported depth format.");
            shutdown();
            return false;
        }

        resolution = std::max(1u, resolution);
        shadowed_light_capacity = std::clamp(shadowed_light_capacity, 1u, kMaxRenderPointShadowLights);
        if (!createImage(resolution, shadowed_light_capacity) || !createSampler()) {
            shutdown();
            return false;
        }

        m_ready = true;
        return true;
    }

    void VulkanPointShadowTarget::shutdown() {
        cleanupImage();
        cleanupSampler();

        m_physical_device = VK_NULL_HANDLE;
        m_gpu_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
        m_depth_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_shadowed_light_capacity = 1;
        m_layer_count = kRenderPointShadowCubeFaceCount;
        m_ready = false;
    }

    VulkanDepthRenderTarget VulkanPointShadowTarget::getFaceRenderTarget(
        uint32_t shadow_slot,
        uint32_t face_index) const {
        const uint32_t layer_index =
            shadow_slot * kRenderPointShadowCubeFaceCount + face_index;
        return getRenderTarget(layer_index);
    }

    VulkanDepthRenderTarget VulkanPointShadowTarget::getRenderTarget(uint32_t layer_index) const {
        VulkanDepthRenderTarget target;
        if (layer_index >= m_depth_layer_views.size()) {
            return target;
        }

        target.depth_view = m_depth_layer_views[layer_index];
        target.depth_format = m_depth_format;
        target.extent = m_extent;
        return target;
    }

    bool VulkanPointShadowTarget::createImage(uint32_t resolution, uint32_t shadowed_light_capacity) {
        const uint32_t layer_count = shadowed_light_capacity * kRenderPointShadowCubeFaceCount;

        VulkanOwnedImageCreateInfo create_info;
        create_info.extent = { resolution, resolution, 1 };
        create_info.format = m_depth_format;
        create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        create_info.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
        create_info.view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        create_info.array_layers = layer_count;
        create_info.view_layer_count = layer_count;
        create_info.debug_name = "VulkanPointShadowTarget image";
        if (!m_depth_image.create(*m_gpu_allocator, create_info)) {
            return false;
        }

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_depth_image.getImage();
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = m_depth_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        m_depth_layer_views.resize(layer_count, VK_NULL_HANDLE);
        for (uint32_t layer_index = 0; layer_index < layer_count; ++layer_index) {
            view_info.subresourceRange.baseArrayLayer = layer_index;

            if (!VulkanDiagnosticsCollector::checkVk(
                    vkCreateImageView(m_device, &view_info, nullptr, &m_depth_layer_views[layer_index]),
                    "vkCreateImageView(point shadow face)")) {
                return false;
            }
        }

        m_extent = { resolution, resolution };
        m_shadowed_light_capacity = shadowed_light_capacity;
        m_layer_count = layer_count;
        return true;
    }

    bool VulkanPointShadowTarget::createSampler() {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 1.0f;
        sampler_info.maxAnisotropy = 1.0f;

        return m_sampler.create(m_device, sampler_info, "vkCreateSampler(point shadow)");
    }

    void VulkanPointShadowTarget::cleanupImage() {
        for (VkImageView layer_view : m_depth_layer_views) {
            if (layer_view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, layer_view, nullptr);
            }
        }
        m_depth_layer_views.clear();

        m_depth_image.reset();
    }

    void VulkanPointShadowTarget::cleanupSampler() {
        m_sampler.reset();
    }

    VkFormat VulkanPointShadowTarget::findDepthFormat() const {
        constexpr std::array<VkFormat, 3> candidates{
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        for (VkFormat format : candidates) {
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &properties);
            if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0 &&
                (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0) {
                return format;
            }
        }

        return VK_FORMAT_UNDEFINED;
    }
} // namespace NexAur

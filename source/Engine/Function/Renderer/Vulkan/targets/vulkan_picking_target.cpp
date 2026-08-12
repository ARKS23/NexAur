#include "pch.h"
#include "vulkan_picking_target.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace NexAur {
    namespace {
        VkFormat findDepthFormat(VkPhysicalDevice physical_device) {
            const std::array<VkFormat, 3> candidates{
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT
            };

            for (VkFormat format : candidates) {
                VkFormatProperties properties{};
                vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
                if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
                    return format;
                }
            }

            return VK_FORMAT_UNDEFINED;
        }
    } // namespace

    VulkanPickingTarget::~VulkanPickingTarget() {
        shutdown();
    }

    bool VulkanPickingTarget::init(const VulkanResourceContext& context, uint32_t width, uint32_t height) {
        shutdown();

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized()) {
            NX_CORE_ERROR("VulkanPickingTarget requires a valid Vulkan context.");
            return false;
        }

        m_physical_device = context.physical_device;
        m_gpu_allocator = context.gpu_allocator;
        m_device = context.device;
        m_depth_format = findDepthFormat(context.physical_device);
        if (m_depth_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanPickingTarget failed to find a supported depth format.");
            shutdown();
            return false;
        }

        if (!createReadbackBuffer() || !recreateImages(width, height)) {
            shutdown();
            return false;
        }

        return true;
    }

    bool VulkanPickingTarget::resize(uint32_t width, uint32_t height) {
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

    void VulkanPickingTarget::shutdown() {
        cleanupImages();
        cleanupReadbackBuffer();
        m_physical_device = VK_NULL_HANDLE;
        m_gpu_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
        m_extent = {};
        m_depth_format = VK_FORMAT_UNDEFINED;
        m_ready = false;
    }

    int32_t VulkanPickingTarget::readbackEntityId() const {
        if (m_device == VK_NULL_HANDLE || !m_readback_buffer.isReady()) {
            return -1;
        }

        void* mapped = nullptr;
        if (!m_readback_buffer.map(mapped)) {
            return -1;
        }

        if (!m_readback_buffer.isHostCoherent() && !m_readback_buffer.invalidate()) {
            m_readback_buffer.unmap();
            return -1;
        }

        int32_t entity_id = -1;
        std::memcpy(&entity_id, mapped, sizeof(entity_id));
        m_readback_buffer.unmap();
        return entity_id;
    }

    VulkanRenderTarget VulkanPickingTarget::getRenderTarget() const {
        VulkanRenderTarget target;
        target.color_view = m_object_id_image.getImageView();
        target.color_format = m_object_id_format;
        target.depth_view = m_depth_image.getImageView();
        target.depth_format = m_depth_format;
        target.extent = m_extent;
        return target;
    }

    bool VulkanPickingTarget::recreateImages(uint32_t width, uint32_t height) {
        width = std::max(1u, width);
        height = std::max(1u, height);

        const VkImageUsageFlags object_id_usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if (!createImage(
                width,
                height,
                m_object_id_format,
                object_id_usage,
                VK_IMAGE_ASPECT_COLOR_BIT,
                m_object_id_image)) {
            cleanupImages();
            return false;
        }

        if (!createImage(
                width,
                height,
                m_depth_format,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                m_depth_image)) {
            cleanupImages();
            return false;
        }

        m_extent = { width, height };
        m_ready = true;
        return true;
    }

    bool VulkanPickingTarget::createImage(
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
            "VulkanPickingTarget depth image" :
            "VulkanPickingTarget object id image";
        return image.create(*m_gpu_allocator, create_info);
    }

    bool VulkanPickingTarget::createReadbackBuffer() {
        return m_readback_buffer.create(
            *m_gpu_allocator,
            sizeof(int32_t),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            "VulkanPickingTarget readback buffer");
    }

    void VulkanPickingTarget::cleanupImages() {
        m_object_id_image.reset();
        m_depth_image.reset();
        m_extent = {};
        m_ready = false;
    }

    void VulkanPickingTarget::cleanupReadbackBuffer() {
        m_readback_buffer.reset();
    }
} // namespace NexAur

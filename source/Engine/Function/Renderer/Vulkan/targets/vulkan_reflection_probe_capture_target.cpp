#include "pch.h"
#include "vulkan_reflection_probe_capture_target.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace NexAur {
    namespace {
        float halfToFloat(uint16_t value) {
            const uint32_t sign = static_cast<uint32_t>(value & 0x8000u) << 16u;
            uint32_t exponent = (value >> 10u) & 0x1fu;
            uint32_t mantissa = value & 0x03ffu;

            uint32_t bits = 0;
            if (exponent == 0u) {
                if (mantissa == 0u) {
                    bits = sign;
                } else {
                    exponent = 1u;
                    while ((mantissa & 0x0400u) == 0u) {
                        mantissa <<= 1u;
                        --exponent;
                    }
                    mantissa &= 0x03ffu;
                    bits = sign | ((exponent + 112u) << 23u) | (mantissa << 13u);
                }
            } else if (exponent == 31u) {
                bits = sign | 0x7f800000u | (mantissa << 13u);
            } else {
                bits = sign | ((exponent + 112u) << 23u) | (mantissa << 13u);
            }

            float result = 0.0f;
            std::memcpy(&result, &bits, sizeof(float));
            return result;
        }

        float unsignedFloatToFloat(uint32_t value, uint32_t mantissa_bits) {
            const uint32_t mantissa_mask = (1u << mantissa_bits) - 1u;
            const uint32_t mantissa = value & mantissa_mask;
            const uint32_t exponent = value >> mantissa_bits;
            if (exponent == 0u) {
                return std::ldexp(static_cast<float>(mantissa), -14 - static_cast<int>(mantissa_bits));
            }
            if (exponent == 31u) {
                return std::numeric_limits<float>::infinity();
            }

            const float significand =
                1.0f +
                static_cast<float>(mantissa) /
                    static_cast<float>(1u << mantissa_bits);
            return std::ldexp(significand, static_cast<int>(exponent) - 15);
        }
    } // namespace

    VulkanReflectionProbeCaptureTarget::~VulkanReflectionProbeCaptureTarget() {
        shutdown();
    }

    bool VulkanReflectionProbeCaptureTarget::init(
        const VulkanResourceContext& context,
        VkFormat color_format,
        VkFormat depth_format,
        uint32_t resolution) {
        shutdown();

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() ||
            color_format == VK_FORMAT_UNDEFINED ||
            depth_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanReflectionProbeCaptureTarget requires a valid Vulkan context and formats.");
            return false;
        }

        m_physical_device = context.physical_device;
        m_gpu_allocator = context.gpu_allocator;
        m_device = context.device;
        m_color_format = color_format;
        m_depth_format = depth_format;

        if (colorBytesPerTexel() == 0) {
            NX_CORE_ERROR("Reflection probe capture readback does not support color format {}.", static_cast<int>(m_color_format));
            shutdown();
            return false;
        }

        if (!recreateImages(std::max(1u, resolution))) {
            shutdown();
            return false;
        }

        m_ready = true;
        return true;
    }

    bool VulkanReflectionProbeCaptureTarget::resize(uint32_t resolution) {
        if (m_device == VK_NULL_HANDLE ||
            m_color_format == VK_FORMAT_UNDEFINED ||
            m_depth_format == VK_FORMAT_UNDEFINED) {
            return false;
        }

        resolution = std::max(1u, resolution);
        if (m_ready && m_extent.width == resolution && m_extent.height == resolution) {
            return true;
        }

        if (!recreateImages(resolution)) {
            return false;
        }

        m_ready = true;
        return true;
    }

    void VulkanReflectionProbeCaptureTarget::shutdown() {
        cleanupImages();
        cleanupReadbackBuffer();
        m_physical_device = VK_NULL_HANDLE;
        m_gpu_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
        m_color_format = VK_FORMAT_UNDEFINED;
        m_depth_format = VK_FORMAT_UNDEFINED;
        m_extent = {};
        m_ready = false;
    }

    VulkanRenderTarget VulkanReflectionProbeCaptureTarget::getFaceRenderTarget(uint32_t face_index) const {
        VulkanRenderTarget target;
        if (face_index >= m_color_face_views.size()) {
            return target;
        }

        target.color_view = m_color_face_views[face_index];
        target.color_format = m_color_format;
        target.depth_view = m_depth_view;
        target.depth_format = m_depth_format;
        target.extent = m_extent;
        return target;
    }

    bool VulkanReflectionProbeCaptureTarget::recordCopyToReadback(VkCommandBuffer command_buffer) const {
        if (command_buffer == VK_NULL_HANDLE ||
            !m_color_image.isReady() ||
            !m_readback_buffer.isReady()) {
            return false;
        }

        const VkDeviceSize face_size =
            static_cast<VkDeviceSize>(m_extent.width) *
            static_cast<VkDeviceSize>(m_extent.height) *
            colorBytesPerTexel();

        std::vector<VkBufferImageCopy> copy_regions;
        copy_regions.reserve(kFaceCount);
        for (uint32_t face = 0; face < kFaceCount; ++face) {
            VkBufferImageCopy region{};
            region.bufferOffset = face_size * face;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = face;
            region.imageSubresource.layerCount = 1;
            region.imageExtent = { m_extent.width, m_extent.height, 1 };
            copy_regions.push_back(region);
        }

        vkCmdCopyImageToBuffer(
            command_buffer,
            m_color_image.getImage(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_readback_buffer.get(),
            static_cast<uint32_t>(copy_regions.size()),
            copy_regions.data());
        return true;
    }

    bool VulkanReflectionProbeCaptureTarget::readColorPixels(std::vector<float>& pixels) const {
        pixels.clear();
        if (!m_ready || !m_readback_buffer.isReady() || m_readback_size == 0) {
            return false;
        }

        void* mapped_data = nullptr;
        if (!m_readback_buffer.map(mapped_data)) {
            return false;
        }

        if (!m_readback_buffer.isHostCoherent() && !m_readback_buffer.invalidate()) {
                m_readback_buffer.unmap();
                return false;
        }

        const bool decoded = decodeReadback(mapped_data, pixels);
        m_readback_buffer.unmap();
        return decoded;
    }

    bool VulkanReflectionProbeCaptureTarget::recreateImages(uint32_t resolution) {
        cleanupImages();
        cleanupReadbackBuffer();

        if (!createColorImage(resolution) ||
            !createDepthImage(resolution) ||
            !createReadbackBuffer(resolution)) {
            cleanupImages();
            cleanupReadbackBuffer();
            return false;
        }

        m_extent = { resolution, resolution };
        return true;
    }

    bool VulkanReflectionProbeCaptureTarget::createColorImage(uint32_t resolution) {
        VulkanOwnedImageCreateInfo create_info;
        create_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        create_info.extent = { resolution, resolution, 1 };
        create_info.format = m_color_format;
        create_info.usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        create_info.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
        create_info.array_layers = kFaceCount;
        create_info.view_layer_count = kFaceCount;
        create_info.debug_name = "VulkanReflectionProbeCaptureTarget color image";
        if (!m_color_image.create(*m_gpu_allocator, create_info)) {
            return false;
        }

        m_color_face_views.resize(kFaceCount, VK_NULL_HANDLE);
        VkImageViewCreateInfo face_view_info{};
        face_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        face_view_info.image = m_color_image.getImage();
        face_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        face_view_info.format = m_color_format;
        face_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        face_view_info.subresourceRange.levelCount = 1;
        face_view_info.subresourceRange.layerCount = 1;
        for (uint32_t face = 0; face < kFaceCount; ++face) {
            face_view_info.subresourceRange.baseArrayLayer = face;
            if (!VulkanDiagnosticsCollector::checkVk(
                    vkCreateImageView(m_device, &face_view_info, nullptr, &m_color_face_views[face]),
                    "vkCreateImageView(reflection probe capture face)")) {
                return false;
            }
        }

        return true;
    }

    bool VulkanReflectionProbeCaptureTarget::createDepthImage(uint32_t resolution) {
        VulkanOwnedImageCreateInfo create_info;
        create_info.extent = { resolution, resolution, 1 };
        create_info.format = m_depth_format;
        create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        create_info.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
        create_info.debug_name = "VulkanReflectionProbeCaptureTarget depth image";
        if (!m_depth_image.create(*m_gpu_allocator, create_info)) {
            return false;
        }

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_depth_image.getImage();
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = m_depth_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        return VulkanDiagnosticsCollector::checkVk(
            vkCreateImageView(m_device, &view_info, nullptr, &m_depth_view),
            "vkCreateImageView(reflection probe capture depth)");
    }

    bool VulkanReflectionProbeCaptureTarget::createReadbackBuffer(uint32_t resolution) {
        m_readback_size =
            static_cast<VkDeviceSize>(resolution) *
            static_cast<VkDeviceSize>(resolution) *
            colorBytesPerTexel() *
            kFaceCount;

        return m_readback_buffer.create(
            *m_gpu_allocator,
            m_readback_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
            "VulkanReflectionProbeCaptureTarget readback buffer");
    }

    void VulkanReflectionProbeCaptureTarget::cleanupImages() {
        m_ready = false;

        for (VkImageView face_view : m_color_face_views) {
            if (face_view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, face_view, nullptr);
            }
        }
        m_color_face_views.clear();

        m_color_image.reset();

        if (m_depth_view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_depth_view, nullptr);
            m_depth_view = VK_NULL_HANDLE;
        }
        m_depth_image.reset();

        m_extent = {};
    }

    void VulkanReflectionProbeCaptureTarget::cleanupReadbackBuffer() {
        m_readback_buffer.reset();
        m_readback_size = 0;
    }

    VkDeviceSize VulkanReflectionProbeCaptureTarget::colorBytesPerTexel() const {
        switch (m_color_format) {
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return sizeof(uint16_t) * 4u;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return sizeof(float) * 4u;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
            return sizeof(uint32_t);
        default:
            return 0;
        }
    }

    bool VulkanReflectionProbeCaptureTarget::decodeReadback(const void* data, std::vector<float>& pixels) const {
        const uint32_t resolution = getResolution();
        if (resolution == 0 || data == nullptr) {
            return false;
        }

        const size_t pixel_count =
            static_cast<size_t>(resolution) *
            static_cast<size_t>(resolution) *
            kFaceCount;
        pixels.resize(pixel_count * 4u);

        if (m_color_format == VK_FORMAT_R32G32B32A32_SFLOAT) {
            std::memcpy(pixels.data(), data, pixels.size() * sizeof(float));
            return true;
        }

        if (m_color_format == VK_FORMAT_R16G16B16A16_SFLOAT) {
            const uint16_t* source = static_cast<const uint16_t*>(data);
            for (size_t index = 0; index < pixel_count * 4u; ++index) {
                pixels[index] = halfToFloat(source[index]);
            }
            return true;
        }

        if (m_color_format == VK_FORMAT_B10G11R11_UFLOAT_PACK32) {
            const uint32_t* source = static_cast<const uint32_t*>(data);
            for (size_t index = 0; index < pixel_count; ++index) {
                const uint32_t packed = source[index];
                pixels[index * 4u + 0u] = unsignedFloatToFloat(packed & 0x7ffu, 6u);
                pixels[index * 4u + 1u] = unsignedFloatToFloat((packed >> 11u) & 0x7ffu, 6u);
                pixels[index * 4u + 2u] = unsignedFloatToFloat((packed >> 22u) & 0x3ffu, 5u);
                pixels[index * 4u + 3u] = 1.0f;
            }
            return true;
        }

        return false;
    }
} // namespace NexAur

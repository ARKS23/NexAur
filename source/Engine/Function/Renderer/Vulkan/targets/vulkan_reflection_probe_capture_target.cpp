#include "pch.h"
#include "vulkan_reflection_probe_capture_target.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace NexAur {
    namespace {
        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

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
            color_format == VK_FORMAT_UNDEFINED ||
            depth_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanReflectionProbeCaptureTarget requires a valid Vulkan context and formats.");
            return false;
        }

        m_physical_device = context.physical_device;
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
            m_color_image == VK_NULL_HANDLE ||
            m_readback_buffer == VK_NULL_HANDLE) {
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
            m_color_image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_readback_buffer,
            static_cast<uint32_t>(copy_regions.size()),
            copy_regions.data());
        return true;
    }

    bool VulkanReflectionProbeCaptureTarget::readColorPixels(std::vector<float>& pixels) const {
        pixels.clear();
        if (!m_ready || m_readback_memory == VK_NULL_HANDLE || m_readback_size == 0) {
            return false;
        }

        void* mapped_data = nullptr;
        if (!checkVk(vkMapMemory(m_device, m_readback_memory, 0, m_readback_size, 0, &mapped_data), "vkMapMemory(reflection probe capture)")) {
            return false;
        }

        if (!m_readback_memory_coherent) {
            VkMappedMemoryRange range{};
            range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range.memory = m_readback_memory;
            range.offset = 0;
            range.size = VK_WHOLE_SIZE;
            if (!checkVk(vkInvalidateMappedMemoryRanges(m_device, 1, &range), "vkInvalidateMappedMemoryRanges(reflection probe capture)")) {
                vkUnmapMemory(m_device, m_readback_memory);
                return false;
            }
        }

        const bool decoded = decodeReadback(mapped_data, pixels);
        vkUnmapMemory(m_device, m_readback_memory);
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
        m_color_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_depth_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        return true;
    }

    bool VulkanReflectionProbeCaptureTarget::createColorImage(uint32_t resolution) {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent = { resolution, resolution, 1 };
        image_info.mipLevels = 1;
        image_info.arrayLayers = kFaceCount;
        image_info.format = m_color_format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage =
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (!checkVk(vkCreateImage(m_device, &image_info, nullptr, &m_color_image), "vkCreateImage(reflection probe capture color)")) {
            return false;
        }

        VkMemoryRequirements memory_requirements{};
        vkGetImageMemoryRequirements(m_device, m_color_image, &memory_requirements);
        const uint32_t memory_type = findMemoryType(
            memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory_type == UINT32_MAX) {
            NX_CORE_ERROR("VulkanReflectionProbeCaptureTarget failed to find color image memory.");
            return false;
        }

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;
        if (!checkVk(vkAllocateMemory(m_device, &allocate_info, nullptr, &m_color_memory), "vkAllocateMemory(reflection probe capture color)") ||
            !checkVk(vkBindImageMemory(m_device, m_color_image, m_color_memory, 0), "vkBindImageMemory(reflection probe capture color)")) {
            return false;
        }

        VkImageViewCreateInfo cube_view_info{};
        cube_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        cube_view_info.image = m_color_image;
        cube_view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        cube_view_info.format = m_color_format;
        cube_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        cube_view_info.subresourceRange.levelCount = 1;
        cube_view_info.subresourceRange.layerCount = kFaceCount;
        if (!checkVk(vkCreateImageView(m_device, &cube_view_info, nullptr, &m_color_cube_view), "vkCreateImageView(reflection probe capture cube)")) {
            return false;
        }

        m_color_face_views.resize(kFaceCount, VK_NULL_HANDLE);
        for (uint32_t face = 0; face < kFaceCount; ++face) {
            VkImageViewCreateInfo face_view_info = cube_view_info;
            face_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            face_view_info.subresourceRange.baseArrayLayer = face;
            face_view_info.subresourceRange.layerCount = 1;
            if (!checkVk(
                    vkCreateImageView(m_device, &face_view_info, nullptr, &m_color_face_views[face]),
                    "vkCreateImageView(reflection probe capture face)")) {
                return false;
            }
        }

        return true;
    }

    bool VulkanReflectionProbeCaptureTarget::createDepthImage(uint32_t resolution) {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent = { resolution, resolution, 1 };
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = m_depth_format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (!checkVk(vkCreateImage(m_device, &image_info, nullptr, &m_depth_image), "vkCreateImage(reflection probe capture depth)")) {
            return false;
        }

        VkMemoryRequirements memory_requirements{};
        vkGetImageMemoryRequirements(m_device, m_depth_image, &memory_requirements);
        const uint32_t memory_type = findMemoryType(
            memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory_type == UINT32_MAX) {
            NX_CORE_ERROR("VulkanReflectionProbeCaptureTarget failed to find depth image memory.");
            return false;
        }

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;
        if (!checkVk(vkAllocateMemory(m_device, &allocate_info, nullptr, &m_depth_memory), "vkAllocateMemory(reflection probe capture depth)") ||
            !checkVk(vkBindImageMemory(m_device, m_depth_image, m_depth_memory, 0), "vkBindImageMemory(reflection probe capture depth)")) {
            return false;
        }

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_depth_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = m_depth_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        return checkVk(vkCreateImageView(m_device, &view_info, nullptr, &m_depth_view), "vkCreateImageView(reflection probe capture depth)");
    }

    bool VulkanReflectionProbeCaptureTarget::createReadbackBuffer(uint32_t resolution) {
        m_readback_size =
            static_cast<VkDeviceSize>(resolution) *
            static_cast<VkDeviceSize>(resolution) *
            colorBytesPerTexel() *
            kFaceCount;

        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = m_readback_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (!checkVk(vkCreateBuffer(m_device, &buffer_info, nullptr, &m_readback_buffer), "vkCreateBuffer(reflection probe capture readback)")) {
            return false;
        }

        VkMemoryRequirements memory_requirements{};
        vkGetBufferMemoryRequirements(m_device, m_readback_buffer, &memory_requirements);

        VkMemoryPropertyFlags properties =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        uint32_t memory_type = findMemoryType(memory_requirements.memoryTypeBits, properties);
        m_readback_memory_coherent = memory_type != UINT32_MAX;
        if (memory_type == UINT32_MAX) {
            properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            memory_type = findMemoryType(memory_requirements.memoryTypeBits, properties);
        }
        if (memory_type == UINT32_MAX) {
            NX_CORE_ERROR("VulkanReflectionProbeCaptureTarget failed to find readback buffer memory.");
            return false;
        }

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;
        if (!checkVk(vkAllocateMemory(m_device, &allocate_info, nullptr, &m_readback_memory), "vkAllocateMemory(reflection probe capture readback)") ||
            !checkVk(vkBindBufferMemory(m_device, m_readback_buffer, m_readback_memory, 0), "vkBindBufferMemory(reflection probe capture readback)")) {
            return false;
        }

        return true;
    }

    void VulkanReflectionProbeCaptureTarget::cleanupImages() {
        m_ready = false;

        for (VkImageView face_view : m_color_face_views) {
            if (face_view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, face_view, nullptr);
            }
        }
        m_color_face_views.clear();

        if (m_color_cube_view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_color_cube_view, nullptr);
            m_color_cube_view = VK_NULL_HANDLE;
        }
        if (m_color_image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_color_image, nullptr);
            m_color_image = VK_NULL_HANDLE;
        }
        if (m_color_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_color_memory, nullptr);
            m_color_memory = VK_NULL_HANDLE;
        }

        if (m_depth_view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_depth_view, nullptr);
            m_depth_view = VK_NULL_HANDLE;
        }
        if (m_depth_image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_depth_image, nullptr);
            m_depth_image = VK_NULL_HANDLE;
        }
        if (m_depth_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_depth_memory, nullptr);
            m_depth_memory = VK_NULL_HANDLE;
        }

        m_color_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_depth_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_extent = {};
    }

    void VulkanReflectionProbeCaptureTarget::cleanupReadbackBuffer() {
        if (m_readback_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_readback_buffer, nullptr);
            m_readback_buffer = VK_NULL_HANDLE;
        }
        if (m_readback_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_readback_memory, nullptr);
            m_readback_memory = VK_NULL_HANDLE;
        }
        m_readback_size = 0;
        m_readback_memory_coherent = false;
    }

    uint32_t VulkanReflectionProbeCaptureTarget::findMemoryType(
        uint32_t type_filter,
        VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memory_properties{};
        vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memory_properties);

        for (uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
            const bool type_matches = (type_filter & (1u << index)) != 0;
            const bool properties_match =
                (memory_properties.memoryTypes[index].propertyFlags & properties) == properties;
            if (type_matches && properties_match) {
                return index;
            }
        }

        return UINT32_MAX;
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

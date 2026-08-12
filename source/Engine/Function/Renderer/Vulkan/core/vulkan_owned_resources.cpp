#include "pch.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"

namespace NexAur {
    VulkanOwnedImage::~VulkanOwnedImage() {
        reset();
    }

    VulkanOwnedImage::VulkanOwnedImage(VulkanOwnedImage&& other) noexcept {
        moveFrom(std::move(other));
    }

    VulkanOwnedImage& VulkanOwnedImage::operator=(VulkanOwnedImage&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(std::move(other));
        }
        return *this;
    }

    bool VulkanOwnedImage::create(
        const VulkanGpuAllocator& allocator,
        const VulkanOwnedImageCreateInfo& create_info) {
        reset();

        if (!allocator.isInitialized() || !create_info.valid()) {
            NX_CORE_ERROR("VulkanOwnedImage requires an initialized allocator and valid create info.");
            return false;
        }

        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.flags = create_info.flags;
        image_info.imageType = create_info.image_type;
        image_info.format = create_info.format;
        image_info.extent = create_info.extent;
        image_info.mipLevels = create_info.mip_levels;
        image_info.arrayLayers = create_info.array_layers;
        image_info.samples = create_info.samples;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = create_info.usage;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        const char* debug_name = create_info.debug_name.empty() ?
            "VulkanOwnedImage" :
            create_info.debug_name.c_str();
        if (!allocator.createImage(
                image_info,
                create_info.memory_usage,
                m_view.image,
                m_allocation,
                debug_name)) {
            return false;
        }

        m_allocator = &allocator;
        m_device = allocator.getDevice();
        m_view.format = create_info.format;
        m_view.extent = {
            create_info.extent.width,
            create_info.extent.height
        };
        m_view.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_view.debug_name = debug_name;

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_view.image;
        view_info.viewType = create_info.view_type;
        view_info.format = create_info.format;
        view_info.subresourceRange.aspectMask = create_info.aspect_mask;
        view_info.subresourceRange.baseMipLevel = create_info.view_base_mip_level;
        view_info.subresourceRange.levelCount = create_info.view_mip_count > 0 ?
            create_info.view_mip_count :
            create_info.mip_levels - create_info.view_base_mip_level;
        view_info.subresourceRange.baseArrayLayer = create_info.view_base_array_layer;
        view_info.subresourceRange.layerCount = create_info.view_layer_count > 0 ?
            create_info.view_layer_count :
            create_info.array_layers - create_info.view_base_array_layer;

        if (!VulkanDiagnosticsCollector::checkVk(
                vkCreateImageView(m_device, &view_info, nullptr, &m_view.view),
                create_info.debug_name.empty() ? "vkCreateImageView" : create_info.debug_name.c_str())) {
            reset();
            return false;
        }

        return true;
    }

    void VulkanOwnedImage::reset() {
        if (m_device != VK_NULL_HANDLE && m_view.view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_view.view, nullptr);
        }
        if (m_allocator != nullptr) {
            m_allocator->destroyImage(m_view.image, m_allocation);
        }

        m_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
        m_view = {};
        m_allocation = VK_NULL_HANDLE;
    }

    void VulkanOwnedImage::moveFrom(VulkanOwnedImage&& other) noexcept {
        m_allocator = other.m_allocator;
        m_device = other.m_device;
        m_view = std::move(other.m_view);
        m_allocation = other.m_allocation;

        other.m_allocator = nullptr;
        other.m_device = VK_NULL_HANDLE;
        other.m_view = {};
        other.m_allocation = VK_NULL_HANDLE;
    }

    VulkanOwnedBuffer::~VulkanOwnedBuffer() {
        reset();
    }

    VulkanOwnedBuffer::VulkanOwnedBuffer(VulkanOwnedBuffer&& other) noexcept {
        moveFrom(std::move(other));
    }

    VulkanOwnedBuffer& VulkanOwnedBuffer::operator=(VulkanOwnedBuffer&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(std::move(other));
        }
        return *this;
    }

    bool VulkanOwnedBuffer::create(
        const VulkanGpuAllocator& allocator,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memory_usage,
        VmaAllocationCreateFlags allocation_flags,
        const char* debug_name) {
        reset();

        if (!allocator.isInitialized() || size == 0 || usage == 0) {
            NX_CORE_ERROR("VulkanOwnedBuffer requires an initialized allocator, size, and usage.");
            return false;
        }

        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (!allocator.createBuffer(
                buffer_info,
                memory_usage,
                allocation_flags,
                m_buffer,
                m_allocation,
                debug_name ? debug_name : "VulkanOwnedBuffer")) {
            return false;
        }

        m_allocator = &allocator;
        m_size = size;
        m_debug_name = debug_name ? debug_name : "VulkanOwnedBuffer";
        return true;
    }

    void VulkanOwnedBuffer::reset() {
        if (m_allocator != nullptr) {
            m_allocator->destroyBuffer(m_buffer, m_allocation);
        }

        m_allocator = nullptr;
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
        m_size = 0;
        m_debug_name.clear();
    }

    bool VulkanOwnedBuffer::isHostCoherent() const {
        if (m_allocator == nullptr || m_allocation == VK_NULL_HANDLE) {
            return false;
        }

        VkMemoryPropertyFlags properties = 0;
        vmaGetAllocationMemoryProperties(m_allocator->getHandle(), m_allocation, &properties);
        return (properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
    }

    bool VulkanOwnedBuffer::map(void*& mapped_data) const {
        mapped_data = nullptr;
        if (m_allocator == nullptr || m_allocation == VK_NULL_HANDLE) {
            return false;
        }

        return VulkanDiagnosticsCollector::checkVk(
            vmaMapMemory(m_allocator->getHandle(), m_allocation, &mapped_data),
            m_debug_name.empty() ? "vmaMapMemory" : m_debug_name.c_str());
    }

    void VulkanOwnedBuffer::unmap() const {
        if (m_allocator != nullptr && m_allocation != VK_NULL_HANDLE) {
            vmaUnmapMemory(m_allocator->getHandle(), m_allocation);
        }
    }

    bool VulkanOwnedBuffer::invalidate(VkDeviceSize offset, VkDeviceSize size) const {
        if (m_allocator == nullptr || m_allocation == VK_NULL_HANDLE) {
            return false;
        }

        return VulkanDiagnosticsCollector::checkVk(
            vmaInvalidateAllocation(m_allocator->getHandle(), m_allocation, offset, size),
            m_debug_name.empty() ? "vmaInvalidateAllocation" : m_debug_name.c_str());
    }

    bool VulkanOwnedBuffer::flush(VkDeviceSize offset, VkDeviceSize size) const {
        if (m_allocator == nullptr || m_allocation == VK_NULL_HANDLE) {
            return false;
        }

        return VulkanDiagnosticsCollector::checkVk(
            vmaFlushAllocation(m_allocator->getHandle(), m_allocation, offset, size),
            m_debug_name.empty() ? "vmaFlushAllocation" : m_debug_name.c_str());
    }

    void VulkanOwnedBuffer::moveFrom(VulkanOwnedBuffer&& other) noexcept {
        m_allocator = other.m_allocator;
        m_buffer = other.m_buffer;
        m_allocation = other.m_allocation;
        m_size = other.m_size;
        m_debug_name = std::move(other.m_debug_name);

        other.m_allocator = nullptr;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
        other.m_size = 0;
        other.m_debug_name.clear();
    }

    VulkanOwnedSampler::~VulkanOwnedSampler() {
        reset();
    }

    VulkanOwnedSampler::VulkanOwnedSampler(VulkanOwnedSampler&& other) noexcept {
        moveFrom(std::move(other));
    }

    VulkanOwnedSampler& VulkanOwnedSampler::operator=(VulkanOwnedSampler&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(std::move(other));
        }
        return *this;
    }

    bool VulkanOwnedSampler::create(
        VkDevice device,
        const VkSamplerCreateInfo& create_info,
        const char* debug_name) {
        reset();
        if (device == VK_NULL_HANDLE) {
            NX_CORE_ERROR("VulkanOwnedSampler requires a valid device.");
            return false;
        }

        if (!VulkanDiagnosticsCollector::checkVk(
                vkCreateSampler(device, &create_info, nullptr, &m_sampler),
                debug_name ? debug_name : "vkCreateSampler")) {
            return false;
        }

        m_device = device;
        m_debug_name = debug_name ? debug_name : "VulkanOwnedSampler";
        return true;
    }

    void VulkanOwnedSampler::reset() {
        if (m_device != VK_NULL_HANDLE && m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device, m_sampler, nullptr);
        }

        m_device = VK_NULL_HANDLE;
        m_sampler = VK_NULL_HANDLE;
        m_debug_name.clear();
    }

    void VulkanOwnedSampler::moveFrom(VulkanOwnedSampler&& other) noexcept {
        m_device = other.m_device;
        m_sampler = other.m_sampler;
        m_debug_name = std::move(other.m_debug_name);

        other.m_device = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_debug_name.clear();
    }
} // namespace NexAur

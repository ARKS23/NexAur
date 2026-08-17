#include "pch.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/core/vulkan_retirement_queue.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"

namespace NexAur {
    namespace {
        struct RetiredImage {
            const VulkanGpuAllocator* allocator = nullptr;
            VkDevice device = VK_NULL_HANDLE;
            VkImage image = VK_NULL_HANDLE;
            VkImageView view = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;

            RetiredImage() = default;
            RetiredImage(const RetiredImage&) = delete;
            RetiredImage& operator=(const RetiredImage&) = delete;

            RetiredImage(RetiredImage&& other) noexcept
                : allocator(other.allocator),
                  device(other.device),
                  image(other.image),
                  view(other.view),
                  allocation(other.allocation) {
                other.allocator = nullptr;
                other.device = VK_NULL_HANDLE;
                other.image = VK_NULL_HANDLE;
                other.view = VK_NULL_HANDLE;
                other.allocation = VK_NULL_HANDLE;
            }

            RetiredImage& operator=(RetiredImage&& other) noexcept {
                if (this != &other) {
                    destroy();
                    allocator = other.allocator;
                    device = other.device;
                    image = other.image;
                    view = other.view;
                    allocation = other.allocation;
                    other.allocator = nullptr;
                    other.device = VK_NULL_HANDLE;
                    other.image = VK_NULL_HANDLE;
                    other.view = VK_NULL_HANDLE;
                    other.allocation = VK_NULL_HANDLE;
                }
                return *this;
            }

            ~RetiredImage() { destroy(); }

        private:
            void destroy() {
                if (device != VK_NULL_HANDLE && view != VK_NULL_HANDLE) {
                    vkDestroyImageView(device, view, nullptr);
                }
                if (allocator != nullptr) {
                    allocator->destroyImage(image, allocation);
                }
                allocator = nullptr;
                device = VK_NULL_HANDLE;
                image = VK_NULL_HANDLE;
                view = VK_NULL_HANDLE;
                allocation = VK_NULL_HANDLE;
            }
        };

        struct RetiredBuffer {
            const VulkanGpuAllocator* allocator = nullptr;
            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation allocation = VK_NULL_HANDLE;

            RetiredBuffer() = default;
            RetiredBuffer(const RetiredBuffer&) = delete;
            RetiredBuffer& operator=(const RetiredBuffer&) = delete;

            RetiredBuffer(RetiredBuffer&& other) noexcept
                : allocator(other.allocator),
                  buffer(other.buffer),
                  allocation(other.allocation) {
                other.allocator = nullptr;
                other.buffer = VK_NULL_HANDLE;
                other.allocation = VK_NULL_HANDLE;
            }

            RetiredBuffer& operator=(RetiredBuffer&& other) noexcept {
                if (this != &other) {
                    destroy();
                    allocator = other.allocator;
                    buffer = other.buffer;
                    allocation = other.allocation;
                    other.allocator = nullptr;
                    other.buffer = VK_NULL_HANDLE;
                    other.allocation = VK_NULL_HANDLE;
                }
                return *this;
            }

            ~RetiredBuffer() { destroy(); }

        private:
            void destroy() {
                if (allocator != nullptr) {
                    allocator->destroyBuffer(buffer, allocation);
                }
                allocator = nullptr;
                buffer = VK_NULL_HANDLE;
                allocation = VK_NULL_HANDLE;
            }
        };

        struct RetiredSampler {
            VkDevice device = VK_NULL_HANDLE;
            VkSampler sampler = VK_NULL_HANDLE;

            RetiredSampler() = default;
            RetiredSampler(const RetiredSampler&) = delete;
            RetiredSampler& operator=(const RetiredSampler&) = delete;

            RetiredSampler(RetiredSampler&& other) noexcept
                : device(other.device), sampler(other.sampler) {
                other.device = VK_NULL_HANDLE;
                other.sampler = VK_NULL_HANDLE;
            }

            RetiredSampler& operator=(RetiredSampler&& other) noexcept {
                if (this != &other) {
                    destroy();
                    device = other.device;
                    sampler = other.sampler;
                    other.device = VK_NULL_HANDLE;
                    other.sampler = VK_NULL_HANDLE;
                }
                return *this;
            }

            ~RetiredSampler() { destroy(); }

        private:
            void destroy() {
                if (device != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE) {
                    vkDestroySampler(device, sampler, nullptr);
                }
                device = VK_NULL_HANDLE;
                sampler = VK_NULL_HANDLE;
            }
        };
    } // namespace

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
        m_retirement_queue = allocator.getRetirementQueue();
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
        RetiredImage retired;
        retired.allocator = m_allocator;
        retired.device = m_device;
        retired.image = m_view.image;
        retired.view = m_view.view;
        retired.allocation = m_allocation;
        if (m_retirement_queue != nullptr &&
            (retired.view != VK_NULL_HANDLE || retired.image != VK_NULL_HANDLE)) {
            m_retirement_queue->retire(std::move(retired));
        }

        m_allocator = nullptr;
        m_retirement_queue = nullptr;
        m_device = VK_NULL_HANDLE;
        m_view = {};
        m_allocation = VK_NULL_HANDLE;
    }

    void VulkanOwnedImage::moveFrom(VulkanOwnedImage&& other) noexcept {
        m_allocator = other.m_allocator;
        m_retirement_queue = other.m_retirement_queue;
        m_device = other.m_device;
        m_view = std::move(other.m_view);
        m_allocation = other.m_allocation;

        other.m_allocator = nullptr;
        other.m_retirement_queue = nullptr;
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
        const VulkanOwnedBufferCreateInfo& create_info) {
        reset();

        if (!allocator.isInitialized() || !create_info.valid()) {
            NX_CORE_ERROR("VulkanOwnedBuffer requires an initialized allocator and valid create info.");
            return false;
        }
        if (create_info.requiresDeviceAddress() &&
            !allocator.isBufferDeviceAddressEnabled()) {
            NX_CORE_ERROR(
                "VulkanOwnedBuffer '{}' requires buffer device address support.",
                create_info.debug_name.empty() ? "Unnamed" : create_info.debug_name);
            return false;
        }

        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = create_info.size;
        buffer_info.usage = create_info.usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        const char* operation = create_info.debug_name.empty() ?
            "VulkanOwnedBuffer" : create_info.debug_name.c_str();
        if (!allocator.createBuffer(
                buffer_info,
                create_info.memory_usage,
                create_info.allocation_flags,
                create_info.minimum_alignment,
                buffer,
                allocation,
                operation)) {
            return false;
        }

        VkDeviceAddress device_address = 0;
        if (create_info.requiresDeviceAddress()) {
            device_address = allocator.getBufferDeviceAddress(buffer, operation);
            if (device_address == 0 ||
                device_address % create_info.minimum_alignment != 0) {
                if (device_address != 0) {
                    NX_CORE_ERROR(
                        "{} failed: device address {} does not satisfy alignment {}.",
                        operation,
                        device_address,
                        create_info.minimum_alignment);
                }
                allocator.destroyBuffer(buffer, allocation);
                return false;
            }
        }

        m_allocator = &allocator;
        m_retirement_queue = allocator.getRetirementQueue();
        m_buffer = buffer;
        m_allocation = allocation;
        m_size = create_info.size;
        m_usage = create_info.usage;
        m_device_address = device_address;
        m_debug_name = operation;
        return true;
    }

    bool VulkanOwnedBuffer::create(
        const VulkanGpuAllocator& allocator,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memory_usage,
        VmaAllocationCreateFlags allocation_flags,
        const char* debug_name) {
        VulkanOwnedBufferCreateInfo create_info;
        create_info.size = size;
        create_info.usage = usage;
        create_info.memory_usage = memory_usage;
        create_info.allocation_flags = allocation_flags;
        create_info.debug_name = debug_name ? debug_name : "VulkanOwnedBuffer";
        return create(allocator, create_info);
    }

    void VulkanOwnedBuffer::reset() {
        RetiredBuffer retired;
        retired.allocator = m_allocator;
        retired.buffer = m_buffer;
        retired.allocation = m_allocation;
        if (m_retirement_queue != nullptr && retired.buffer != VK_NULL_HANDLE) {
            m_retirement_queue->retire(std::move(retired));
        }

        m_allocator = nullptr;
        m_retirement_queue = nullptr;
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
        m_size = 0;
        m_usage = 0;
        m_device_address = 0;
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
        m_retirement_queue = other.m_retirement_queue;
        m_buffer = other.m_buffer;
        m_allocation = other.m_allocation;
        m_size = other.m_size;
        m_usage = other.m_usage;
        m_device_address = other.m_device_address;
        m_debug_name = std::move(other.m_debug_name);

        other.m_allocator = nullptr;
        other.m_retirement_queue = nullptr;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_allocation = VK_NULL_HANDLE;
        other.m_size = 0;
        other.m_usage = 0;
        other.m_device_address = 0;
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
        const VulkanGpuAllocator& allocator,
        const VkSamplerCreateInfo& create_info,
        const char* debug_name) {
        reset();
        const VkDevice device = allocator.getDevice();
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
        m_retirement_queue = allocator.getRetirementQueue();
        m_debug_name = debug_name ? debug_name : "VulkanOwnedSampler";
        return true;
    }

    void VulkanOwnedSampler::reset() {
        RetiredSampler retired;
        retired.device = m_device;
        retired.sampler = m_sampler;
        if (m_retirement_queue != nullptr && retired.sampler != VK_NULL_HANDLE) {
            m_retirement_queue->retire(std::move(retired));
        }

        m_device = VK_NULL_HANDLE;
        m_retirement_queue = nullptr;
        m_sampler = VK_NULL_HANDLE;
        m_debug_name.clear();
    }

    void VulkanOwnedSampler::moveFrom(VulkanOwnedSampler&& other) noexcept {
        m_device = other.m_device;
        m_retirement_queue = other.m_retirement_queue;
        m_sampler = other.m_sampler;
        m_debug_name = std::move(other.m_debug_name);

        other.m_device = VK_NULL_HANDLE;
        other.m_retirement_queue = nullptr;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_debug_name.clear();
    }
} // namespace NexAur

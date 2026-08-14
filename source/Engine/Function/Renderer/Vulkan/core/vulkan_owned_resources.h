#pragma once

#include <cstdint>
#include <string>

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace NexAur {
    class VulkanGpuAllocator;
    class VulkanRetirementQueue;

    struct VulkanImageViewState {
        VkImage image = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        std::string debug_name;

        bool valid() const {
            return image != VK_NULL_HANDLE &&
                   view != VK_NULL_HANDLE &&
                   format != VK_FORMAT_UNDEFINED &&
                   extent.width > 0 &&
                   extent.height > 0;
        }
    };

    struct VulkanOwnedImageCreateInfo {
        VkImageType image_type = VK_IMAGE_TYPE_2D;
        VkExtent3D extent{ 1, 1, 1 };
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags usage = 0;
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
        VkImageCreateFlags flags = 0;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        uint32_t mip_levels = 1;
        uint32_t array_layers = 1;
        uint32_t view_base_mip_level = 0;
        uint32_t view_mip_count = 0;
        uint32_t view_base_array_layer = 0;
        uint32_t view_layer_count = 0;
        VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        std::string debug_name;

        bool valid() const {
            return format != VK_FORMAT_UNDEFINED &&
                   usage != 0 &&
                   aspect_mask != 0 &&
                   extent.width > 0 &&
                   extent.height > 0 &&
                   extent.depth > 0 &&
                   mip_levels > 0 &&
                   array_layers > 0 &&
                   view_base_mip_level < mip_levels &&
                   view_base_array_layer < array_layers;
        }
    };

    class VulkanOwnedImage final {
    public:
        VulkanOwnedImage() = default;
        ~VulkanOwnedImage();

        VulkanOwnedImage(const VulkanOwnedImage&) = delete;
        VulkanOwnedImage& operator=(const VulkanOwnedImage&) = delete;

        VulkanOwnedImage(VulkanOwnedImage&& other) noexcept;
        VulkanOwnedImage& operator=(VulkanOwnedImage&& other) noexcept;

        bool create(const VulkanGpuAllocator& allocator, const VulkanOwnedImageCreateInfo& create_info);
        void reset();

        bool isReady() const { return m_view.valid(); }
        VkImage getImage() const { return m_view.image; }
        VkImageView getImageView() const { return m_view.view; }
        VkFormat getFormat() const { return m_view.format; }
        VkExtent2D getExtent() const { return m_view.extent; }
        VkImageLayout getLayout() const { return m_view.layout; }
        void setLayout(VkImageLayout layout) { m_view.layout = layout; }
        const VulkanImageViewState& getView() const { return m_view; }

    private:
        void moveFrom(VulkanOwnedImage&& other) noexcept;

        const VulkanGpuAllocator* m_allocator = nullptr;
        VulkanRetirementQueue* m_retirement_queue = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;
        VulkanImageViewState m_view;
        VmaAllocation m_allocation = VK_NULL_HANDLE;
    };

    class VulkanOwnedBuffer final {
    public:
        VulkanOwnedBuffer() = default;
        ~VulkanOwnedBuffer();

        VulkanOwnedBuffer(const VulkanOwnedBuffer&) = delete;
        VulkanOwnedBuffer& operator=(const VulkanOwnedBuffer&) = delete;

        VulkanOwnedBuffer(VulkanOwnedBuffer&& other) noexcept;
        VulkanOwnedBuffer& operator=(VulkanOwnedBuffer&& other) noexcept;

        bool create(
            const VulkanGpuAllocator& allocator,
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VmaMemoryUsage memory_usage,
            VmaAllocationCreateFlags allocation_flags,
            const char* debug_name);
        void reset();

        bool isReady() const { return m_buffer != VK_NULL_HANDLE; }
        VkBuffer get() const { return m_buffer; }
        VkDeviceSize getSize() const { return m_size; }
        bool isHostCoherent() const;
        bool map(void*& mapped_data) const;
        void unmap() const;
        bool invalidate(
            VkDeviceSize offset = 0,
            VkDeviceSize size = VK_WHOLE_SIZE) const;
        bool flush(
            VkDeviceSize offset = 0,
            VkDeviceSize size = VK_WHOLE_SIZE) const;

    private:
        void moveFrom(VulkanOwnedBuffer&& other) noexcept;

        const VulkanGpuAllocator* m_allocator = nullptr;
        VulkanRetirementQueue* m_retirement_queue = nullptr;
        VkBuffer m_buffer = VK_NULL_HANDLE;
        VmaAllocation m_allocation = VK_NULL_HANDLE;
        VkDeviceSize m_size = 0;
        std::string m_debug_name;
    };

    class VulkanOwnedSampler final {
    public:
        VulkanOwnedSampler() = default;
        ~VulkanOwnedSampler();

        VulkanOwnedSampler(const VulkanOwnedSampler&) = delete;
        VulkanOwnedSampler& operator=(const VulkanOwnedSampler&) = delete;

        VulkanOwnedSampler(VulkanOwnedSampler&& other) noexcept;
        VulkanOwnedSampler& operator=(VulkanOwnedSampler&& other) noexcept;

        bool create(
            const VulkanGpuAllocator& allocator,
            const VkSamplerCreateInfo& create_info,
            const char* debug_name);
        void reset();

        bool isReady() const { return m_sampler != VK_NULL_HANDLE; }
        VkSampler get() const { return m_sampler; }

    private:
        void moveFrom(VulkanOwnedSampler&& other) noexcept;

        VkDevice m_device = VK_NULL_HANDLE;
        VulkanRetirementQueue* m_retirement_queue = nullptr;
        VkSampler m_sampler = VK_NULL_HANDLE;
        std::string m_debug_name;
    };
} // namespace NexAur

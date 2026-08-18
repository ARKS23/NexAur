#pragma once

#include <array>
#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    enum class VulkanReflectionHistoryImage : uint8_t {
        RawReflection = 0,
        HitDistance,
        FilteredRadiance,
        Moments,
        HistoryLength,
        Depth,
        Surface,
        Count
    };

    class VulkanReflectionHistoryTarget final {
    public:
        VulkanReflectionHistoryTarget() = default;
        ~VulkanReflectionHistoryTarget();

        VulkanReflectionHistoryTarget(const VulkanReflectionHistoryTarget&) = delete;
        VulkanReflectionHistoryTarget& operator=(const VulkanReflectionHistoryTarget&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VkFormat format,
            uint32_t width,
            uint32_t height);
        bool resize(uint32_t width, uint32_t height);
        void shutdown();

        bool isReady() const { return m_ready; }
        VkExtent2D getExtent() const { return m_extent; }
        VkFormat getFormat() const { return m_format; }
        VkSampler getSampler() const { return m_sampler.get(); }
        uint32_t getReadIndex() const { return m_read_index; }
        uint32_t getWriteIndex() const { return m_write_index; }
        void swapPingPong() {
            std::swap(m_read_index, m_write_index);
        }

        const VulkanImageViewState* getImage(
            VulkanReflectionHistoryImage image,
            uint32_t ping_pong_index = 0) const;
        void setImageLayout(
            VulkanReflectionHistoryImage image,
            uint32_t ping_pong_index,
            VkImageLayout layout);

    private:
        static constexpr uint32_t kPingPongCount = 2;
        static constexpr uint32_t kSingleImageCount = 2;
        static constexpr uint32_t kPingPongImageCount =
            static_cast<uint32_t>(VulkanReflectionHistoryImage::Count) - kSingleImageCount;

        bool recreateImages(uint32_t width, uint32_t height);
        bool createImage(
            uint32_t width,
            uint32_t height,
            const char* debug_name,
            VulkanOwnedImage& image);
        bool createSampler();
        void cleanupImages();
        void cleanupSampler();

        VulkanOwnedImage* getOwnedImage(
            VulkanReflectionHistoryImage image,
            uint32_t ping_pong_index);
        const VulkanOwnedImage* getOwnedImage(
            VulkanReflectionHistoryImage image,
            uint32_t ping_pong_index) const;

    private:
        const VulkanGpuAllocator* m_gpu_allocator = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};
        VulkanOwnedImage m_raw_reflection;
        VulkanOwnedImage m_hit_distance;
        std::array<VulkanOwnedImage, kPingPongCount> m_filtered_radiance;
        std::array<VulkanOwnedImage, kPingPongCount> m_moments;
        std::array<VulkanOwnedImage, kPingPongCount> m_history_length;
        std::array<VulkanOwnedImage, kPingPongCount> m_depth;
        std::array<VulkanOwnedImage, kPingPongCount> m_surface;
        VulkanOwnedSampler m_sampler;
        uint32_t m_read_index = 0;
        uint32_t m_write_index = 1;
        bool m_ready = false;
    };
} // namespace NexAur

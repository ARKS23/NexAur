#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace NexAur {
    struct VulkanGpuTimestampQueryStats {
        bool supported = false;
        bool pending = false;
        uint64_t sample_count = 0;
        double last_duration_ms = 0.0;
    };

    class VulkanGpuTimestampQuery final {
    public:
        VulkanGpuTimestampQuery() = default;
        ~VulkanGpuTimestampQuery();

        VulkanGpuTimestampQuery(const VulkanGpuTimestampQuery&) = delete;
        VulkanGpuTimestampQuery& operator=(const VulkanGpuTimestampQuery&) = delete;

        bool init(
            VkPhysicalDevice physical_device,
            VkDevice device,
            uint32_t queue_family_index);
        void shutdown();

        bool begin(VkCommandBuffer command_buffer);
        bool end(VkCommandBuffer command_buffer);
        bool resolve();
        // Only discard timestamps recorded into a command buffer that was not submitted.
        void discard();

        bool isSupported() const { return m_query_pool != VK_NULL_HANDLE; }
        bool isPending() const { return m_pending; }
        VulkanGpuTimestampQueryStats getStats() const;

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkQueryPool m_query_pool = VK_NULL_HANDLE;
        double m_timestamp_period_ns = 0.0;
        uint32_t m_timestamp_valid_bits = 0;
        uint64_t m_sample_count = 0;
        double m_last_duration_ms = 0.0;
        bool m_recording = false;
        bool m_pending = false;
    };
} // namespace NexAur

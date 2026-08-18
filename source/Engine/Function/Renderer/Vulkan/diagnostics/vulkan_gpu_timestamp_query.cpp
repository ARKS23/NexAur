#include "pch.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_gpu_timestamp_query.h"

#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"

#include <array>
#include <vector>

namespace NexAur {
    VulkanGpuTimestampQuery::~VulkanGpuTimestampQuery() {
        shutdown();
    }

    bool VulkanGpuTimestampQuery::init(
        VkPhysicalDevice physical_device,
        VkDevice device,
        uint32_t queue_family_index) {
        shutdown();
        if (physical_device == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
            return false;
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical_device, &properties);

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device,
            &queue_family_count,
            nullptr);
        if (queue_family_index >= queue_family_count ||
            properties.limits.timestampPeriod <= 0.0f) {
            return true;
        }

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device,
            &queue_family_count,
            queue_families.data());
        const uint32_t valid_bits =
            queue_families[queue_family_index].timestampValidBits;
        if (valid_bits == 0) {
            return true;
        }

        VkQueryPoolCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        create_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
        create_info.queryCount = 2;

        VkQueryPool query_pool = VK_NULL_HANDLE;
        if (!VulkanDiagnosticsCollector::checkVk(
                vkCreateQueryPool(device, &create_info, nullptr, &query_pool),
                "vkCreateQueryPool(GPU timestamp)")) {
            return false;
        }

        m_device = device;
        m_query_pool = query_pool;
        m_timestamp_period_ns = properties.limits.timestampPeriod;
        m_timestamp_valid_bits = valid_bits;
        return true;
    }

    void VulkanGpuTimestampQuery::shutdown() {
        if (m_device != VK_NULL_HANDLE && m_query_pool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(m_device, m_query_pool, nullptr);
        }
        m_device = VK_NULL_HANDLE;
        m_query_pool = VK_NULL_HANDLE;
        m_timestamp_period_ns = 0.0;
        m_timestamp_valid_bits = 0;
        m_sample_count = 0;
        m_last_duration_ms = 0.0;
        m_recording = false;
        m_pending = false;
    }

    bool VulkanGpuTimestampQuery::begin(VkCommandBuffer command_buffer) {
        if (!isSupported() ||
            command_buffer == VK_NULL_HANDLE ||
            m_recording) {
            return false;
        }
        if (m_pending && !resolve()) {
            return false;
        }

        vkCmdResetQueryPool(command_buffer, m_query_pool, 0, 2);
        vkCmdWriteTimestamp2(
            command_buffer,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            m_query_pool,
            0);
        m_recording = true;
        return true;
    }

    bool VulkanGpuTimestampQuery::end(VkCommandBuffer command_buffer) {
        if (!isSupported() ||
            command_buffer == VK_NULL_HANDLE ||
            !m_recording ||
            m_pending) {
            return false;
        }

        vkCmdWriteTimestamp2(
            command_buffer,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            m_query_pool,
            1);
        m_recording = false;
        m_pending = true;
        return true;
    }

    bool VulkanGpuTimestampQuery::resolve() {
        if (!isSupported() || !m_pending || m_recording) {
            return false;
        }

        std::array<uint64_t, 2> timestamps{};
        const VkResult result = vkGetQueryPoolResults(
            m_device,
            m_query_pool,
            0,
            static_cast<uint32_t>(timestamps.size()),
            sizeof(timestamps),
            timestamps.data(),
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT);
        if (result == VK_NOT_READY) {
            return false;
        }
        if (!VulkanDiagnosticsCollector::checkVk(
                result,
                "vkGetQueryPoolResults(GPU timestamp)")) {
            m_pending = false;
            return false;
        }

        uint64_t elapsed_ticks = 0;
        if (m_timestamp_valid_bits >= 64) {
            elapsed_ticks = timestamps[1] - timestamps[0];
        } else {
            const uint64_t mask =
                (uint64_t{ 1 } << m_timestamp_valid_bits) - 1u;
            elapsed_ticks = (timestamps[1] - timestamps[0]) & mask;
        }

        m_last_duration_ms =
            static_cast<double>(elapsed_ticks) * m_timestamp_period_ns * 1.0e-6;
        ++m_sample_count;
        m_pending = false;
        return true;
    }

    void VulkanGpuTimestampQuery::discard() {
        m_recording = false;
        m_pending = false;
    }

    VulkanGpuTimestampQueryStats VulkanGpuTimestampQuery::getStats() const {
        VulkanGpuTimestampQueryStats stats;
        stats.supported = isSupported();
        stats.pending = m_pending;
        stats.sample_count = m_sample_count;
        stats.last_duration_ms = m_last_duration_ms;
        return stats;
    }
} // namespace NexAur

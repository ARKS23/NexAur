#include "pch.h"
#include "Function/Renderer/Vulkan/core/vulkan_retirement_queue.h"

#include <algorithm>

namespace NexAur {
    uint64_t VulkanRetirementQueue::markSubmitted() {
        return ++m_submitted_serial;
    }

    void VulkanRetirementQueue::markCompleted(uint64_t completed_serial) {
        m_completed_serial = std::max(
            m_completed_serial,
            std::min(completed_serial, m_submitted_serial));
        collectCompleted();
    }

    void VulkanRetirementQueue::drain() {
        while (!m_entries.empty()) {
            std::vector<std::unique_ptr<RetiredResourceBase>> resources;
            resources.reserve(m_entries.size());
            for (Entry& entry : m_entries) {
                resources.push_back(std::move(entry.resource));
            }
            m_collected_count += static_cast<uint64_t>(resources.size());
            m_entries.clear();
            resources.clear();
        }
        m_completed_serial = m_submitted_serial;
    }

    void VulkanRetirementQueue::reset() {
        drain();
        m_submitted_serial = 0;
        m_completed_serial = 0;
        m_retired_count = 0;
        m_collected_count = 0;
    }

    VulkanRetirementQueueStats VulkanRetirementQueue::getStats() const {
        VulkanRetirementQueueStats stats;
        stats.submitted_serial = m_submitted_serial;
        stats.completed_serial = m_completed_serial;
        stats.pending_count = m_entries.size();
        stats.retired_count = m_retired_count;
        stats.collected_count = m_collected_count;
        return stats;
    }

    void VulkanRetirementQueue::enqueue(
        uint64_t retire_serial,
        std::unique_ptr<RetiredResourceBase> resource) {
        if (!resource) {
            return;
        }

        ++m_retired_count;
        if (retire_serial <= m_completed_serial) {
            ++m_collected_count;
            return;
        }

        m_entries.push_back({ retire_serial, std::move(resource) });
    }

    void VulkanRetirementQueue::collectCompleted() {
        std::vector<std::unique_ptr<RetiredResourceBase>> resources;
        auto entry = m_entries.begin();
        while (entry != m_entries.end()) {
            if (entry->retire_serial > m_completed_serial) {
                ++entry;
                continue;
            }

            resources.push_back(std::move(entry->resource));
            entry = m_entries.erase(entry);
        }

        m_collected_count += static_cast<uint64_t>(resources.size());
        // Destruction may retire child Vulkan resources back into this queue.
        resources.clear();
    }
} // namespace NexAur

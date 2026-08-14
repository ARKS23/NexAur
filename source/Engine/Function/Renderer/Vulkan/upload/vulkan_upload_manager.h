#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Base.h"

namespace NexAur {
    struct VulkanResourceContext;

    enum class VulkanUploadStatus : uint8_t {
        Pending = 0,
        Submitted,
        Ready,
        Failed,
        Cancelled
    };

    struct VulkanUploadBudget {
        size_t max_bytes = 16u * 1024u * 1024u;
        uint32_t max_requests = 32;

        bool valid() const { return max_bytes > 0 && max_requests > 0; }
    };

    class VulkanUploadBudgetTracker final {
    public:
        explicit VulkanUploadBudgetTracker(VulkanUploadBudget budget)
            : m_budget(budget) {}

        bool tryReserve(size_t byte_count);

        size_t getReservedBytes() const { return m_reserved_bytes; }
        uint32_t getReservedRequests() const { return m_reserved_requests; }

    private:
        VulkanUploadBudget m_budget;
        size_t m_reserved_bytes = 0;
        uint32_t m_reserved_requests = 0;
    };

    namespace Detail {
        struct VulkanUploadTicketState;
    }

    class VulkanUploadTicket final {
    public:
        VulkanUploadTicket() = default;

        bool valid() const { return m_state != nullptr; }
        uint64_t getId() const;
        uint64_t getSubmissionSerial() const;
        size_t getByteCount() const;
        VulkanUploadStatus getStatus() const;
        bool isReady() const { return getStatus() == VulkanUploadStatus::Ready; }
        bool isTerminal() const;
        void cancel();

    private:
        friend class VulkanUploadManager;

        explicit VulkanUploadTicket(
            std::shared_ptr<Detail::VulkanUploadTicketState> state)
            : m_state(std::move(state)) {}

        std::shared_ptr<Detail::VulkanUploadTicketState> m_state;
    };

    struct VulkanUploadManagerConfig {
        VulkanUploadBudget budget;
        size_t staging_segment_bytes = 16u * 1024u * 1024u;
        uint32_t batch_count = 2;

        bool valid() const {
            return budget.valid() &&
                   staging_segment_bytes >= budget.max_bytes &&
                   batch_count >= 2 &&
                   batch_count <= 3;
        }
    };

    struct VulkanUploadManagerStats {
        size_t pending_bytes = 0;
        size_t submitted_bytes_this_frame = 0;
        size_t byte_budget_per_frame = 0;
        uint32_t pending_requests = 0;
        uint32_t in_flight_requests = 0;
        uint32_t submitted_requests_this_frame = 0;
        uint32_t request_budget_per_frame = 0;
        uint64_t ready_requests = 0;
        uint64_t failed_requests = 0;
        uint64_t cancelled_requests = 0;
    };

    class VulkanUploadManager final {
    public:
        using RecordCallback =
            std::function<void(VkCommandBuffer, VkBuffer, VkDeviceSize)>;

        VulkanUploadManager();
        ~VulkanUploadManager();

        VulkanUploadManager(const VulkanUploadManager&) = delete;
        VulkanUploadManager& operator=(const VulkanUploadManager&) = delete;

        bool init(
            const VulkanResourceContext& context,
            const VulkanUploadManagerConfig& config = {});
        void shutdown();

        VulkanUploadTicket enqueue(
            std::vector<uint8_t> data,
            std::string debug_name,
            RecordCallback record_callback);

        bool processFrame();
        uint64_t collectCompletedSerial() const;
        void onSubmissionsCompleted(uint64_t completed_serial);
        bool waitUntilReady(const VulkanUploadTicket& ticket);

        bool isInitialized() const;
        VulkanUploadManagerStats getStats() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
} // namespace NexAur

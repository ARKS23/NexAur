#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Core/Base.h"

namespace NexAur {
    struct VulkanFrameSubmission {
        uint32_t frame_index = 0;
        uint64_t serial = 0;

        bool valid() const { return serial != 0; }
    };

    class VulkanFrameSlotState final {
    public:
        void markSubmitted(uint64_t serial);
        void markCompleted();
        void reset();

        bool isInFlight() const { return m_in_flight; }
        uint64_t getSubmissionSerial() const { return m_submission_serial; }

    private:
        uint64_t m_submission_serial = 0;
        bool m_in_flight = false;
    };

    class VulkanSwapchainImageFlightTracker final {
    public:
        void reset(size_t image_count = 0);

        const VulkanFrameSubmission* get(uint32_t image_index) const;
        void markSubmitted(
            uint32_t image_index,
            uint32_t frame_index,
            uint64_t serial);
        void releaseFrame(uint32_t frame_index, uint64_t serial);

        size_t getInFlightImageCount() const;
        size_t getImageCount() const { return m_submissions.size(); }

    private:
        std::vector<VulkanFrameSubmission> m_submissions;
    };
} // namespace NexAur

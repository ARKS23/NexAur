#include "pch.h"
#include "vulkan_frame_flight_tracker.h"

#include <algorithm>

namespace NexAur {
    void VulkanFrameSlotState::markSubmitted(uint64_t serial) {
        m_submission_serial = serial;
        m_in_flight = serial != 0;
    }

    void VulkanFrameSlotState::markCompleted() {
        m_in_flight = false;
    }

    void VulkanFrameSlotState::reset() {
        m_submission_serial = 0;
        m_in_flight = false;
    }

    void VulkanSwapchainImageFlightTracker::reset(size_t image_count) {
        m_submissions.assign(image_count, {});
    }

    const VulkanFrameSubmission* VulkanSwapchainImageFlightTracker::get(
        uint32_t image_index) const {
        if (image_index >= m_submissions.size() ||
            !m_submissions[image_index].valid()) {
            return nullptr;
        }
        return &m_submissions[image_index];
    }

    void VulkanSwapchainImageFlightTracker::markSubmitted(
        uint32_t image_index,
        uint32_t frame_index,
        uint64_t serial) {
        if (image_index >= m_submissions.size() || serial == 0) {
            return;
        }
        m_submissions[image_index] = { frame_index, serial };
    }

    void VulkanSwapchainImageFlightTracker::releaseFrame(
        uint32_t frame_index,
        uint64_t serial) {
        if (serial == 0) {
            return;
        }
        for (VulkanFrameSubmission& submission : m_submissions) {
            if (submission.valid() &&
                submission.frame_index == frame_index &&
                submission.serial == serial) {
                submission = {};
            }
        }
    }

    size_t VulkanSwapchainImageFlightTracker::getInFlightImageCount() const {
        return static_cast<size_t>(std::count_if(
            m_submissions.begin(),
            m_submissions.end(),
            [](const VulkanFrameSubmission& submission) {
                return submission.valid();
            }));
    }
} // namespace NexAur

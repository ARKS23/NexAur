#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "Core/Base.h"

namespace NexAur {
    struct VulkanRetirementQueueStats {
        uint64_t submitted_serial = 0;
        uint64_t completed_serial = 0;
        size_t pending_count = 0;
        uint64_t retired_count = 0;
        uint64_t collected_count = 0;
    };

    class VulkanRetirementQueue final {
    public:
        VulkanRetirementQueue() = default;
        ~VulkanRetirementQueue() = default;

        VulkanRetirementQueue(const VulkanRetirementQueue&) = delete;
        VulkanRetirementQueue& operator=(const VulkanRetirementQueue&) = delete;

        uint64_t markSubmitted();
        void markCompleted(uint64_t completed_serial);
        // The caller must establish GPU idle before forcing pending resources out.
        void drain();
        void reset();

        uint64_t getSubmittedSerial() const { return m_submitted_serial; }
        uint64_t getCompletedSerial() const { return m_completed_serial; }
        VulkanRetirementQueueStats getStats() const;

        template<typename T>
        void retire(T&& resource) {
            static_assert(
                !std::is_lvalue_reference_v<T>,
                "VulkanRetirementQueue requires ownership to be moved into the queue.");
            using Resource = std::decay_t<T>;
            static_assert(
                std::is_move_constructible_v<Resource>,
                "Retired resources must be move constructible.");

            // A resource retired after submission N remains alive until N's fence completes.
            enqueue(
                m_submitted_serial,
                std::make_unique<RetiredResource<Resource>>(std::forward<T>(resource)));
        }

    private:
        class RetiredResourceBase {
        public:
            virtual ~RetiredResourceBase() = default;
        };

        template<typename T>
        class RetiredResource final : public RetiredResourceBase {
        public:
            explicit RetiredResource(T&& resource)
                : m_resource(std::move(resource)) {}

        private:
            T m_resource;
        };

        struct Entry {
            uint64_t retire_serial = 0;
            std::unique_ptr<RetiredResourceBase> resource;
        };

        void enqueue(
            uint64_t retire_serial,
            std::unique_ptr<RetiredResourceBase> resource);
        void collectCompleted();

    private:
        std::vector<Entry> m_entries;
        uint64_t m_submitted_serial = 0;
        uint64_t m_completed_serial = 0;
        uint64_t m_retired_count = 0;
        uint64_t m_collected_count = 0;
    };
} // namespace NexAur

#include "pch.h"
#include "vulkan_upload_manager.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/core/vulkan_retirement_queue.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

#include <algorithm>
#include <cstring>
#include <deque>
#include <limits>
#include <utility>

namespace NexAur {
    namespace Detail {
        struct VulkanUploadTicketState {
            uint64_t id = 0;
            uint64_t submission_serial = 0;
            size_t byte_count = 0;
            VulkanUploadStatus status = VulkanUploadStatus::Failed;
        };
    } // namespace Detail

    namespace {
        constexpr VkDeviceSize kUploadAlignment = 16;

        VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) {
            return (value + alignment - 1) & ~(alignment - 1);
        }

        bool checkVk(VkResult result, const char* operation) {
            return VulkanDiagnosticsCollector::checkVk(result, operation);
        }
    } // namespace

    bool VulkanUploadBudgetTracker::tryReserve(size_t byte_count) {
        if (!m_budget.valid() || byte_count == 0 ||
            m_reserved_requests >= m_budget.max_requests ||
            byte_count > m_budget.max_bytes - m_reserved_bytes) {
            return false;
        }

        m_reserved_bytes += byte_count;
        ++m_reserved_requests;
        return true;
    }

    uint64_t VulkanUploadTicket::getId() const {
        return m_state ? m_state->id : 0;
    }

    uint64_t VulkanUploadTicket::getSubmissionSerial() const {
        return m_state ? m_state->submission_serial : 0;
    }

    size_t VulkanUploadTicket::getByteCount() const {
        return m_state ? m_state->byte_count : 0;
    }

    VulkanUploadStatus VulkanUploadTicket::getStatus() const {
        return m_state ? m_state->status : VulkanUploadStatus::Failed;
    }

    bool VulkanUploadTicket::isTerminal() const {
        const VulkanUploadStatus status = getStatus();
        return status == VulkanUploadStatus::Ready ||
               status == VulkanUploadStatus::Failed ||
               status == VulkanUploadStatus::Cancelled;
    }

    void VulkanUploadTicket::cancel() {
        if (!m_state) {
            return;
        }

        if (m_state->status == VulkanUploadStatus::Pending ||
            m_state->status == VulkanUploadStatus::Submitted) {
            m_state->status = VulkanUploadStatus::Cancelled;
        }
    }

    struct VulkanUploadManager::Impl {
        struct Request {
            std::vector<uint8_t> data;
            std::string debug_name;
            RecordCallback record;
            std::shared_ptr<Detail::VulkanUploadTicketState> state;
        };

        struct Batch {
            VkCommandBuffer command_buffer = VK_NULL_HANDLE;
            VkFence fence = VK_NULL_HANDLE;
            uint64_t submission_serial = 0;
            std::vector<std::shared_ptr<Detail::VulkanUploadTicketState>> tickets;

            bool inFlight() const { return submission_serial != 0; }
        };

        bool initialized = false;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue graphics_queue = VK_NULL_HANDLE;
        VkCommandPool command_pool = VK_NULL_HANDLE;
        const VulkanGpuAllocator* gpu_allocator = nullptr;
        VulkanRetirementQueue* retirement_queue = nullptr;
        VulkanOwnedBuffer staging_ring;
        void* mapped_staging = nullptr;
        VulkanUploadManagerConfig config;
        std::vector<Batch> batches;
        std::deque<Request> pending;
        uint64_t next_ticket_id = 1;
        uint64_t ready_requests = 0;
        uint64_t failed_requests = 0;
        uint64_t cancelled_requests = 0;
        size_t submitted_bytes_this_frame = 0;
        uint32_t submitted_requests_this_frame = 0;

        void failRequests(std::vector<Request>& requests) {
            for (Request& request : requests) {
                if (request.state->status != VulkanUploadStatus::Cancelled) {
                    request.state->status = VulkanUploadStatus::Failed;
                    ++failed_requests;
                } else {
                    ++cancelled_requests;
                }
            }
        }

        void completeBatch(Batch& batch) {
            for (const auto& ticket : batch.tickets) {
                if (ticket->status == VulkanUploadStatus::Submitted) {
                    ticket->status = VulkanUploadStatus::Ready;
                    ++ready_requests;
                } else if (ticket->status == VulkanUploadStatus::Cancelled) {
                    ++cancelled_requests;
                }
            }
            batch.tickets.clear();
            batch.submission_serial = 0;
        }

        Batch* findIdleBatch() {
            auto it = std::find_if(
                batches.begin(),
                batches.end(),
                [](const Batch& batch) { return !batch.inFlight(); });
            return it != batches.end() ? &*it : nullptr;
        }

        Batch* findBatch(const VulkanUploadTicket& ticket) {
            if (!ticket.m_state) {
                return nullptr;
            }
            auto it = std::find_if(
                batches.begin(),
                batches.end(),
                [&ticket](const Batch& batch) {
                    return std::find(
                               batch.tickets.begin(),
                               batch.tickets.end(),
                               ticket.m_state) != batch.tickets.end();
                });
            return it != batches.end() ? &*it : nullptr;
        }
    };

    VulkanUploadManager::VulkanUploadManager()
        : m_impl(std::make_unique<Impl>()) {}

    VulkanUploadManager::~VulkanUploadManager() {
        shutdown();
    }

    bool VulkanUploadManager::init(
        const VulkanResourceContext& context,
        const VulkanUploadManagerConfig& config) {
        shutdown();
        if (!context.valid() || context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() || !config.valid()) {
            NX_CORE_ERROR("VulkanUploadManager requires a valid context and configuration.");
            return false;
        }

        Impl& impl = *m_impl;
        impl.device = context.device;
        impl.graphics_queue = context.graphics_queue;
        impl.gpu_allocator = context.gpu_allocator;
        impl.retirement_queue = context.retirement_queue;
        impl.config = config;

        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = context.graphics_queue_family;
        if (!checkVk(
                vkCreateCommandPool(
                    impl.device,
                    &pool_info,
                    nullptr,
                    &impl.command_pool),
                "vkCreateCommandPool(async upload)")) {
            shutdown();
            return false;
        }

        impl.batches.resize(config.batch_count);
        std::vector<VkCommandBuffer> command_buffers(config.batch_count);
        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = impl.command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = config.batch_count;
        if (!checkVk(
                vkAllocateCommandBuffers(
                    impl.device,
                    &allocate_info,
                    command_buffers.data()),
                "vkAllocateCommandBuffers(async upload)")) {
            shutdown();
            return false;
        }

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (uint32_t index = 0; index < config.batch_count; ++index) {
            impl.batches[index].command_buffer = command_buffers[index];
            if (!checkVk(
                    vkCreateFence(
                        impl.device,
                        &fence_info,
                        nullptr,
                        &impl.batches[index].fence),
                    "vkCreateFence(async upload)")) {
                shutdown();
                return false;
            }
        }

        const VkDeviceSize ring_size =
            static_cast<VkDeviceSize>(config.staging_segment_bytes) *
            config.batch_count;
        if (!impl.staging_ring.create(
                *context.gpu_allocator,
                ring_size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                "Vulkan async upload staging ring") ||
            !impl.staging_ring.map(impl.mapped_staging)) {
            shutdown();
            return false;
        }

        impl.initialized = true;
        return true;
    }

    void VulkanUploadManager::shutdown() {
        if (!m_impl) {
            return;
        }

        Impl& impl = *m_impl;
        for (Impl::Request& request : impl.pending) {
            request.state->status = VulkanUploadStatus::Cancelled;
            ++impl.cancelled_requests;
        }
        impl.pending.clear();

        if (impl.device != VK_NULL_HANDLE) {
            for (Impl::Batch& batch : impl.batches) {
                if (batch.inFlight() && batch.fence != VK_NULL_HANDLE) {
                    const VkFence fence = batch.fence;
                    if (checkVk(
                            vkWaitForFences(
                                impl.device,
                                1,
                                &fence,
                                VK_TRUE,
                                UINT64_MAX),
                            "vkWaitForFences(async upload shutdown)")) {
                        if (impl.retirement_queue != nullptr) {
                            impl.retirement_queue->markCompleted(
                                batch.submission_serial);
                        }
                        impl.completeBatch(batch);
                    }
                }
            }
        }

        if (impl.mapped_staging != nullptr) {
            impl.staging_ring.unmap();
            impl.mapped_staging = nullptr;
        }
        impl.staging_ring.reset();

        if (impl.device != VK_NULL_HANDLE) {
            for (Impl::Batch& batch : impl.batches) {
                if (batch.fence != VK_NULL_HANDLE) {
                    vkDestroyFence(impl.device, batch.fence, nullptr);
                }
            }
            if (impl.command_pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(impl.device, impl.command_pool, nullptr);
            }
        }

        impl = Impl{};
    }

    VulkanUploadTicket VulkanUploadManager::enqueue(
        std::vector<uint8_t> data,
        std::string debug_name,
        RecordCallback record_callback) {
        Impl& impl = *m_impl;
        auto state = std::make_shared<Detail::VulkanUploadTicketState>();
        state->id = impl.next_ticket_id++;
        state->byte_count = data.size();

        if (!impl.initialized || data.empty() || !record_callback ||
            data.size() > impl.config.staging_segment_bytes ||
            data.size() > impl.config.budget.max_bytes) {
            state->status = VulkanUploadStatus::Failed;
            ++impl.failed_requests;
            NX_CORE_ERROR(
                "Vulkan upload request '{}' does not fit the staging budget ({} bytes).",
                debug_name,
                data.size());
            return VulkanUploadTicket(std::move(state));
        }

        state->status = VulkanUploadStatus::Pending;
        impl.pending.push_back({
            std::move(data),
            std::move(debug_name),
            std::move(record_callback),
            state
        });
        return VulkanUploadTicket(std::move(state));
    }

    bool VulkanUploadManager::processFrame() {
        Impl& impl = *m_impl;
        impl.submitted_bytes_this_frame = 0;
        impl.submitted_requests_this_frame = 0;
        if (!impl.initialized) {
            return false;
        }

        while (!impl.pending.empty() &&
               impl.pending.front().state->status == VulkanUploadStatus::Cancelled) {
            ++impl.cancelled_requests;
            impl.pending.pop_front();
        }
        if (impl.pending.empty()) {
            return true;
        }

        Impl::Batch* batch = impl.findIdleBatch();
        if (batch == nullptr) {
            return true;
        }

        VulkanUploadBudgetTracker budget(impl.config.budget);
        VkDeviceSize segment_cursor = 0;
        std::vector<Impl::Request> selected;
        while (!impl.pending.empty()) {
            Impl::Request& request = impl.pending.front();
            if (request.state->status == VulkanUploadStatus::Cancelled) {
                ++impl.cancelled_requests;
                impl.pending.pop_front();
                continue;
            }

            const VkDeviceSize aligned_offset =
                alignUp(segment_cursor, kUploadAlignment);
            if (aligned_offset + request.data.size() >
                    impl.config.staging_segment_bytes ||
                !budget.tryReserve(request.data.size())) {
                break;
            }

            segment_cursor = aligned_offset + request.data.size();
            selected.push_back(std::move(request));
            impl.pending.pop_front();
        }
        if (selected.empty()) {
            return true;
        }

        if (!checkVk(
                vkResetCommandBuffer(batch->command_buffer, 0),
                "vkResetCommandBuffer(async upload)")) {
            impl.failRequests(selected);
            return false;
        }

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (!checkVk(
                vkBeginCommandBuffer(batch->command_buffer, &begin_info),
                "vkBeginCommandBuffer(async upload)")) {
            impl.failRequests(selected);
            return false;
        }

        const size_t batch_index = static_cast<size_t>(batch - impl.batches.data());
        const VkDeviceSize segment_base =
            static_cast<VkDeviceSize>(batch_index) *
            impl.config.staging_segment_bytes;
        VkDeviceSize request_cursor = 0;
        for (Impl::Request& request : selected) {
            request_cursor = alignUp(request_cursor, kUploadAlignment);
            const VkDeviceSize staging_offset = segment_base + request_cursor;
            std::memcpy(
                static_cast<uint8_t*>(impl.mapped_staging) + staging_offset,
                request.data.data(),
                request.data.size());
            request.record(
                batch->command_buffer,
                impl.staging_ring.get(),
                staging_offset);
            request_cursor += request.data.size();
        }

        if (!impl.staging_ring.flush(segment_base, request_cursor) ||
            !checkVk(
                vkEndCommandBuffer(batch->command_buffer),
                "vkEndCommandBuffer(async upload)")) {
            impl.failRequests(selected);
            return false;
        }

        const VkFence fence = batch->fence;
        if (!checkVk(
                vkResetFences(impl.device, 1, &fence),
                "vkResetFences(async upload)")) {
            impl.failRequests(selected);
            return false;
        }

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &batch->command_buffer;
        if (!checkVk(
                vkQueueSubmit(
                    impl.graphics_queue,
                    1,
                    &submit_info,
                    batch->fence),
                "vkQueueSubmit(async upload)")) {
            impl.failRequests(selected);
            return false;
        }

        batch->submission_serial = impl.retirement_queue->markSubmitted();
        batch->tickets.reserve(selected.size());
        for (Impl::Request& request : selected) {
            request.state->submission_serial = batch->submission_serial;
            request.state->status = VulkanUploadStatus::Submitted;
            batch->tickets.push_back(std::move(request.state));
        }
        impl.submitted_bytes_this_frame = budget.getReservedBytes();
        impl.submitted_requests_this_frame = budget.getReservedRequests();
        return true;
    }

    uint64_t VulkanUploadManager::collectCompletedSerial() const {
        const Impl& impl = *m_impl;
        if (!impl.initialized) {
            return 0;
        }

        uint64_t completed_serial = 0;
        for (const Impl::Batch& batch : impl.batches) {
            if (!batch.inFlight()) {
                continue;
            }
            const VkResult result = vkGetFenceStatus(impl.device, batch.fence);
            if (result == VK_SUCCESS) {
                completed_serial = std::max(
                    completed_serial,
                    batch.submission_serial);
            } else if (result != VK_NOT_READY) {
                checkVk(result, "vkGetFenceStatus(async upload)");
            }
        }
        return completed_serial;
    }

    void VulkanUploadManager::onSubmissionsCompleted(uint64_t completed_serial) {
        if (completed_serial == 0) {
            return;
        }

        for (Impl::Batch& batch : m_impl->batches) {
            if (batch.inFlight() &&
                batch.submission_serial <= completed_serial) {
                m_impl->completeBatch(batch);
            }
        }
    }

    bool VulkanUploadManager::waitUntilReady(const VulkanUploadTicket& ticket) {
        Impl& impl = *m_impl;
        if (!impl.initialized || !ticket.valid()) {
            return false;
        }

        while (!ticket.isTerminal()) {
            if (ticket.getStatus() == VulkanUploadStatus::Pending &&
                !processFrame()) {
                return false;
            }

            Impl::Batch* batch = impl.findBatch(ticket);
            if (batch == nullptr) {
                if (ticket.getStatus() == VulkanUploadStatus::Pending) {
                    auto first_in_flight = std::find_if(
                        impl.batches.begin(),
                        impl.batches.end(),
                        [](const Impl::Batch& candidate) {
                            return candidate.inFlight();
                        });
                    if (first_in_flight == impl.batches.end()) {
                        return false;
                    }
                    batch = &*first_in_flight;
                } else {
                    return false;
                }
            }

            const VkFence fence = batch->fence;
            if (!checkVk(
                    vkWaitForFences(
                        impl.device,
                        1,
                        &fence,
                        VK_TRUE,
                        UINT64_MAX),
                    "vkWaitForFences(async upload bootstrap)")) {
                return false;
            }

            const uint64_t completed_serial = batch->submission_serial;
            impl.retirement_queue->markCompleted(completed_serial);
            onSubmissionsCompleted(completed_serial);
        }

        return ticket.isReady();
    }

    bool VulkanUploadManager::isInitialized() const {
        return m_impl && m_impl->initialized;
    }

    VulkanUploadManagerStats VulkanUploadManager::getStats() const {
        VulkanUploadManagerStats stats;
        if (!m_impl) {
            return stats;
        }

        const Impl& impl = *m_impl;
        stats.byte_budget_per_frame = impl.config.budget.max_bytes;
        stats.request_budget_per_frame = impl.config.budget.max_requests;
        stats.submitted_bytes_this_frame = impl.submitted_bytes_this_frame;
        stats.submitted_requests_this_frame = impl.submitted_requests_this_frame;
        stats.ready_requests = impl.ready_requests;
        stats.failed_requests = impl.failed_requests;
        stats.cancelled_requests = impl.cancelled_requests;
        for (const Impl::Request& request : impl.pending) {
            if (request.state->status == VulkanUploadStatus::Pending) {
                stats.pending_bytes += request.data.size();
                ++stats.pending_requests;
            }
        }
        for (const Impl::Batch& batch : impl.batches) {
            stats.in_flight_requests +=
                static_cast<uint32_t>(batch.tickets.size());
        }
        return stats;
    }
} // namespace NexAur

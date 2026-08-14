#include "pch.h"
#include "vulkan_picking_manager.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace NexAur {
    namespace {
        const char* pickStatusToText(ViewportPickStatus status) {
            switch (status) {
            case ViewportPickStatus::Pending:
                return "Pending";
            case ViewportPickStatus::Ready:
                return "Ready";
            case ViewportPickStatus::Failed:
                return "Failed";
            case ViewportPickStatus::Cancelled:
                return "Cancelled";
            case ViewportPickStatus::Unsupported:
            default:
                return "Unsupported";
            }
        }
    } // namespace

    VulkanPickingManager::~VulkanPickingManager() {
        shutdown();
    }

    bool VulkanPickingManager::init(
        const VulkanResourceContext& context,
        uint32_t width,
        uint32_t height) {
        shutdown();
        if (!context.valid() || context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized()) {
            NX_CORE_ERROR("VulkanPickingManager requires a valid Vulkan context.");
            return false;
        }

        if (!createReadbackSlots(context) ||
            !m_target.init(context, width, height)) {
            shutdown();
            return false;
        }
        return true;
    }

    bool VulkanPickingManager::resize(uint32_t width, uint32_t height) {
        cancelPendingRequests();
        releaseUnsubmittedRecording();
        if (!m_target.resize(width, height)) {
            return false;
        }

        m_frame_ready = false;
        m_recorded_this_frame = false;
        return true;
    }

    void VulkanPickingManager::shutdown() {
        cancelPendingRequests();
        m_target.shutdown();
        for (ReadbackSlot& slot : m_readback_slots) {
            slot.buffer.reset();
            slot.clearTracking();
        }
        m_requests.clear();
        m_next_request_id = 1;
        m_recording_frame_index = 0;
        m_frame_ready = false;
        m_recorded_this_frame = false;
    }

    bool VulkanPickingManager::createReadbackSlots(
        const VulkanResourceContext& context) {
        for (uint32_t frame_index = 0;
             frame_index < static_cast<uint32_t>(m_readback_slots.size());
             ++frame_index) {
            if (!m_readback_slots[frame_index].buffer.create(
                    *context.gpu_allocator,
                    sizeof(int32_t),
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                    "Vulkan picking frame readback")) {
                return false;
            }
        }
        return true;
    }

    VulkanGraphImageHandle VulkanPickingManager::addObjectIdImage(
        VulkanPassGraph& graph) {
        if (!m_target.isReady()) {
            return {};
        }

        VulkanGraphImageDesc desc;
        desc.name = "PickingObjectId";
        desc.image = m_target.getObjectIdImage();
        desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.initial_layout = m_target.getObjectIdLayout();
        desc.commit_layout = [this](VkImageLayout layout) {
            m_target.setObjectIdLayout(layout);
        };
        return graph.addImage(std::move(desc));
    }

    VulkanGraphImageHandle VulkanPickingManager::addDepthImage(
        VulkanPassGraph& graph) {
        if (!m_target.isReady()) {
            return {};
        }

        VulkanGraphImageDesc desc;
        desc.name = "PickingDepth";
        desc.image = m_target.getDepthImage();
        desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
        desc.initial_layout = m_target.getDepthLayout();
        desc.commit_layout = [this](VkImageLayout layout) {
            m_target.setDepthLayout(layout);
        };
        return graph.addImage(std::move(desc));
    }

    bool VulkanPickingManager::addReadbackPass(
        VulkanPassGraph& graph,
        VulkanGraphImageHandle object_id_image,
        uint32_t frame_index) {
        if (!object_id_image.valid() || frame_index >= m_readback_slots.size()) {
            return false;
        }

        ReadbackSlot& slot = m_readback_slots[frame_index];
        RequestState* request = findUnscheduledRequest();
        if (request == nullptr || slot.inFlight()) {
            return true;
        }

        request->scheduled = true;
        request->frame_index = frame_index;
        slot.request_id = request->id;
        slot.recorded = false;
        const uint64_t request_id = request->id;
        graph.addPass("ObjectIdPickingReadback")
            .readImage(object_id_image, VulkanGraphImageUsage::TransferSource)
            .execute([this, frame_index, request_id](
                VkCommandBuffer command_buffer) {
                return recordReadback(
                    command_buffer,
                    frame_index,
                    request_id);
            });
        return true;
    }

    void VulkanPickingManager::beginFrameRecording(uint32_t frame_index) {
        releaseUnsubmittedRecording();
        m_recording_frame_index = frame_index;
        m_recorded_this_frame = false;
    }

    void VulkanPickingManager::markPassRecorded() {
        m_recorded_this_frame = true;
    }

    void VulkanPickingManager::onFrameSubmitted(
        uint32_t frame_index,
        uint64_t submission_serial) {
        if (m_recorded_this_frame) {
            m_frame_ready = true;
        }
        if (frame_index >= m_readback_slots.size()) {
            return;
        }

        ReadbackSlot& slot = m_readback_slots[frame_index];
        if (slot.request_id == 0) {
            return;
        }
        RequestState* request = findRequest(slot.request_id);
        if (!slot.recorded || request == nullptr ||
            request->status != ViewportPickStatus::Pending) {
            if (request != nullptr &&
                request->status == ViewportPickStatus::Pending) {
                request->scheduled = false;
            }
            slot.clearTracking();
            return;
        }

        slot.submission_serial = submission_serial;
        request->submission_serial = submission_serial;
    }

    void VulkanPickingManager::onSubmissionsCompleted(
        uint64_t completed_serial) {
        for (ReadbackSlot& slot : m_readback_slots) {
            if (slot.inFlight() &&
                slot.submission_serial <= completed_serial) {
                resolveReadback(slot);
                slot.clearTracking();
            }
        }
    }

    ViewportPickResult VulkanPickingManager::pickViewport(
        const ViewportPickRequest& request) {
        ViewportPickResult result;
        result.supported = true;
        result.status = ViewportPickStatus::Pending;

        if (!m_target.isReady()) {
            result.status = ViewportPickStatus::Failed;
            return result;
        }

        if (request.request_id != 0) {
            const RequestState* existing = findRequest(request.request_id);
            if (existing == nullptr) {
                result.request_id = request.request_id;
                result.status = ViewportPickStatus::Failed;
                return result;
            }
            return buildResult(*existing);
        }

        cancelPendingRequests();
        pruneRequestHistory();

        RequestState state;
        state.id = m_next_request_id++;
        state.x = request.x;
        state.y = request.y;
        const VkExtent2D extent = m_target.getExtent();
        if (request.x < 0 || request.y < 0 ||
            request.x >= static_cast<int>(extent.width) ||
            request.y >= static_cast<int>(extent.height)) {
            state.status = ViewportPickStatus::Ready;
        }
        m_requests.push_back(state);
        return buildResult(m_requests.back());
    }

    RendererDebugPickingTargetStats VulkanPickingManager::buildDebugStats() const {
        RendererDebugPickingTargetStats stats;
        stats.ready = m_target.isReady();
        stats.frame_ready = m_frame_ready;
        if (stats.ready) {
            const VkExtent2D extent = m_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
            stats.object_id_format =
                VulkanDiagnosticsCollector::vkFormatToString(
                    m_target.getObjectIdFormat());
            stats.depth_format =
                VulkanDiagnosticsCollector::vkFormatToString(
                    m_target.getDepthFormat());
        }

        for (const RequestState& request : m_requests) {
            if (request.status == ViewportPickStatus::Pending) {
                ++stats.pending_request_count;
            }
        }
        for (const ReadbackSlot& slot : m_readback_slots) {
            if (slot.inFlight()) {
                ++stats.readback_in_flight_count;
            }
        }
        if (!m_requests.empty()) {
            stats.last_request_status =
                pickStatusToText(m_requests.back().status);
        }
        return stats;
    }

    void VulkanPickingManager::cancelPendingRequests() {
        for (RequestState& request : m_requests) {
            if (request.status == ViewportPickStatus::Pending) {
                request.status = ViewportPickStatus::Cancelled;
            }
        }
    }

    void VulkanPickingManager::releaseUnsubmittedRecording() {
        if (m_recording_frame_index >= m_readback_slots.size()) {
            return;
        }

        ReadbackSlot& slot = m_readback_slots[m_recording_frame_index];
        if (slot.request_id == 0 || slot.inFlight()) {
            return;
        }
        RequestState* request = findRequest(slot.request_id);
        if (request != nullptr &&
            request->status == ViewportPickStatus::Pending) {
            request->scheduled = false;
        }
        slot.clearTracking();
    }

    bool VulkanPickingManager::recordReadback(
        VkCommandBuffer command_buffer,
        uint32_t frame_index,
        uint64_t request_id) {
        if (frame_index >= m_readback_slots.size() ||
            command_buffer == VK_NULL_HANDLE) {
            return false;
        }

        RequestState* request = findRequest(request_id);
        ReadbackSlot& slot = m_readback_slots[frame_index];
        if (request == nullptr ||
            request->status != ViewportPickStatus::Pending ||
            slot.request_id != request_id || !slot.buffer.isReady()) {
            return false;
        }

        VkBufferImageCopy copy_region{};
        copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_region.imageSubresource.layerCount = 1;
        copy_region.imageOffset = { request->x, request->y, 0 };
        copy_region.imageExtent = { 1, 1, 1 };
        vkCmdCopyImageToBuffer(
            command_buffer,
            m_target.getObjectIdImage(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            slot.buffer.get(),
            1,
            &copy_region);
        slot.recorded = true;
        return true;
    }

    bool VulkanPickingManager::resolveReadback(ReadbackSlot& slot) {
        RequestState* request = findRequest(slot.request_id);
        if (request == nullptr ||
            request->status != ViewportPickStatus::Pending) {
            return true;
        }

        void* mapped = nullptr;
        if (!slot.buffer.map(mapped)) {
            request->status = ViewportPickStatus::Failed;
            return false;
        }
        if (!slot.buffer.isHostCoherent() && !slot.buffer.invalidate()) {
            slot.buffer.unmap();
            request->status = ViewportPickStatus::Failed;
            return false;
        }

        std::memcpy(&request->entity_id, mapped, sizeof(request->entity_id));
        slot.buffer.unmap();
        request->status = ViewportPickStatus::Ready;
        return true;
    }

    VulkanPickingManager::RequestState* VulkanPickingManager::findRequest(
        uint64_t request_id) {
        auto it = std::find_if(
            m_requests.begin(),
            m_requests.end(),
            [request_id](const RequestState& request) {
                return request.id == request_id;
            });
        return it != m_requests.end() ? &*it : nullptr;
    }

    const VulkanPickingManager::RequestState* VulkanPickingManager::findRequest(
        uint64_t request_id) const {
        auto it = std::find_if(
            m_requests.begin(),
            m_requests.end(),
            [request_id](const RequestState& request) {
                return request.id == request_id;
            });
        return it != m_requests.end() ? &*it : nullptr;
    }

    VulkanPickingManager::RequestState*
    VulkanPickingManager::findUnscheduledRequest() {
        auto it = std::find_if(
            m_requests.rbegin(),
            m_requests.rend(),
            [](const RequestState& request) {
                return request.status == ViewportPickStatus::Pending &&
                       !request.scheduled;
            });
        return it != m_requests.rend() ? &*it : nullptr;
    }

    ViewportPickResult VulkanPickingManager::buildResult(
        const RequestState& request) const {
        ViewportPickResult result;
        result.request_id = request.id;
        result.status = request.status;
        result.supported = true;
        result.ready = request.status == ViewportPickStatus::Ready;
        result.entity_id = request.entity_id;
        return result;
    }

    void VulkanPickingManager::pruneRequestHistory() {
        constexpr size_t kMaxRequestHistory = 16;
        while (m_requests.size() >= kMaxRequestHistory &&
               m_requests.front().status != ViewportPickStatus::Pending) {
            m_requests.pop_front();
        }
    }
} // namespace NexAur

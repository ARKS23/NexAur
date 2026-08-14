#pragma once

#include <array>
#include <cstdint>
#include <deque>

#include <vulkan/vulkan.h>

#include "Function/Renderer/renderer_debug_service.h"
#include "Function/Renderer/renderer_service_types.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_constants.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"
#include "Function/Renderer/Vulkan/targets/vulkan_picking_target.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanPassGraph;

    class VulkanPickingManager final {
    public:
        VulkanPickingManager() = default;
        ~VulkanPickingManager();

        VulkanPickingManager(const VulkanPickingManager&) = delete;
        VulkanPickingManager& operator=(const VulkanPickingManager&) = delete;

        bool init(
            const VulkanResourceContext& context,
            uint32_t width,
            uint32_t height);
        bool resize(uint32_t width, uint32_t height);
        void shutdown();

        bool isReady() const { return m_target.isReady(); }
        VkFormat getObjectIdFormat() const { return m_target.getObjectIdFormat(); }
        VkFormat getDepthFormat() const { return m_target.getDepthFormat(); }
        VulkanRenderTarget getRenderTarget() const { return m_target.getRenderTarget(); }

        VulkanGraphImageHandle addObjectIdImage(VulkanPassGraph& graph);
        VulkanGraphImageHandle addDepthImage(VulkanPassGraph& graph);
        bool addReadbackPass(
            VulkanPassGraph& graph,
            VulkanGraphImageHandle object_id_image,
            uint32_t frame_index);

        void beginFrameRecording(uint32_t frame_index);
        void markPassRecorded();
        void onFrameSubmitted(uint32_t frame_index, uint64_t submission_serial);
        void onSubmissionsCompleted(uint64_t completed_serial);

        ViewportPickResult pickViewport(const ViewportPickRequest& request);
        RendererDebugPickingTargetStats buildDebugStats() const;

    private:
        struct RequestState {
            uint64_t id = 0;
            uint64_t submission_serial = 0;
            uint32_t frame_index = 0;
            int x = 0;
            int y = 0;
            int entity_id = -1;
            ViewportPickStatus status = ViewportPickStatus::Pending;
            bool scheduled = false;
        };

        struct ReadbackSlot {
            VulkanOwnedBuffer buffer;
            uint64_t request_id = 0;
            uint64_t submission_serial = 0;
            bool recorded = false;

            bool inFlight() const { return submission_serial != 0; }
            void clearTracking() {
                request_id = 0;
                submission_serial = 0;
                recorded = false;
            }
        };

        bool createReadbackSlots(const VulkanResourceContext& context);
        void cancelPendingRequests();
        void releaseUnsubmittedRecording();
        bool recordReadback(
            VkCommandBuffer command_buffer,
            uint32_t frame_index,
            uint64_t request_id);
        bool resolveReadback(ReadbackSlot& slot);
        RequestState* findRequest(uint64_t request_id);
        const RequestState* findRequest(uint64_t request_id) const;
        RequestState* findUnscheduledRequest();
        ViewportPickResult buildResult(const RequestState& request) const;
        void pruneRequestHistory();

    private:
        VulkanPickingTarget m_target;
        std::array<ReadbackSlot, kVulkanFramesInFlight> m_readback_slots;
        std::deque<RequestState> m_requests;
        uint64_t m_next_request_id = 1;
        uint32_t m_recording_frame_index = 0;
        bool m_frame_ready = false;
        bool m_recorded_this_frame = false;
    };
} // namespace NexAur

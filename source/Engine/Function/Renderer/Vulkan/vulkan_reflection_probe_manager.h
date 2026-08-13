#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

#include "Function/Renderer/renderer_service_types.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_prepared_frame.h"
#include "Function/Renderer/Vulkan/resources/vulkan_environment_resource.h"
#include "Function/Renderer/Vulkan/targets/vulkan_reflection_probe_capture_target.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class AssetManager;
    class VulkanRenderResourceCache;

    struct VulkanReflectionProbeCaptureCallbacks {
        std::function<bool(const RenderSettings&, std::string&)> prepare;
        std::function<bool(
            VkCommandBuffer,
            const RenderView&,
            const VulkanDrawList&,
            const RenderSettings&,
            std::string&)> record_shadows;
        std::function<bool(
            VkCommandBuffer,
            const VulkanDrawList&,
            VulkanRenderTarget,
            bool,
            std::string&)> record_face;

        bool valid() const {
            return static_cast<bool>(prepare) &&
                   static_cast<bool>(record_shadows) &&
                   static_cast<bool>(record_face);
        }
    };

    class VulkanReflectionProbeManager final {
    public:
        static constexpr uint32_t kCaptureBudgetPerFrame = 1;
        static constexpr uint32_t kResidentCaptureLimit = 8;

        VulkanReflectionProbeManager() = default;
        ~VulkanReflectionProbeManager();

        VulkanReflectionProbeManager(const VulkanReflectionProbeManager&) = delete;
        VulkanReflectionProbeManager& operator=(const VulkanReflectionProbeManager&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VkCommandPool command_pool,
            VkFormat color_format,
            VkFormat depth_format);
        void shutdown();

        bool requestCapture(const ReflectionProbeCaptureRequest& request);
        bool clearCapture(int entity_id);
        ReflectionProbeCaptureState getCaptureState(int entity_id) const;
        ReflectionProbeCaptureQueueState getQueueState() const;

        void processFrame(
            VulkanPreparedFrame& prepared_frame,
            VulkanRenderResourceCache& resource_cache,
            AssetManager& asset_manager,
            const VulkanReflectionProbeCaptureCallbacks& callbacks);

    private:
        struct RuntimeCapture {
            ReflectionProbeCaptureState state;
            std::unique_ptr<VulkanEnvironmentResource> environment;
            AssetHandle baked_asset;
            uint64_t generation = 0;
            uint64_t last_used_frame = 0;
            uint64_t bake_pin_until_frame = 0;
        };

        ReflectionProbeCaptureState buildCaptureState(
            const ReflectionProbeCaptureRequest& request,
            ReflectionProbeCaptureStatus status,
            bool runtime_resource_ready,
            std::string message) const;
        void setCaptureFailure(
            RuntimeCapture& capture,
            const ReflectionProbeCaptureRequest& request,
            std::string message) const;

        void syncScene(uint64_t scene_id);
        void pruneCaptures(const RenderSceneFrame& scene_frame);
        void enqueueCapture(const ReflectionProbeCaptureRequest& request);
        bool erasePendingCapture(int entity_id);
        bool hasPendingCapture(int entity_id) const;

        uint32_t countResidentCaptures() const;
        bool isCapturePinned(
            const RenderSceneFrame& scene_frame,
            int entity_id,
            const RuntimeCapture& capture) const;
        uint32_t countPinnedCaptures(const RenderSceneFrame& scene_frame) const;
        bool evictCapture(const RenderSceneFrame& scene_frame, int protected_entity_id);
        bool canAcquireResidentSlot(
            const RenderSceneFrame& scene_frame,
            int requested_entity_id,
            int protected_entity_id) const;
        bool ensureResidentSlot(
            const RenderSceneFrame& scene_frame,
            int requested_entity_id,
            int protected_entity_id);
        void enforceResidentBudget(
            const RenderSceneFrame& scene_frame,
            int protected_entity_id);

        void processPendingCaptures(
            const VulkanPreparedFrame& prepared_frame,
            VulkanRenderResourceCache& resource_cache,
            AssetManager& asset_manager,
            const VulkanReflectionProbeCaptureCallbacks& callbacks);
        std::unique_ptr<VulkanEnvironmentResource> captureScene(
            const VulkanDrawList& source_draw_list,
            const RenderFrameReflectionProbe& probe,
            const ReflectionProbeCaptureRequest& request,
            const RenderSettings& render_settings,
            VulkanRenderResourceCache& resource_cache,
            const VulkanReflectionProbeCaptureCallbacks& callbacks,
            std::string& error_message);
        void bindRuntimeCapture(VulkanPreparedFrame& prepared_frame);

        bool ensureCaptureTarget(uint32_t resolution, std::string& error_message);
        bool submitImmediateCommands(
            const char* operation,
            const std::function<bool(VkCommandBuffer)>& record_commands) const;
        void transitionCaptureImagesToAttachment(VkCommandBuffer command_buffer);
        bool copyCaptureToReadback();

    private:
        VulkanResourceContext m_resource_context;
        VkCommandPool m_command_pool = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
        VulkanReflectionProbeCaptureTarget m_capture_target;
        std::vector<ReflectionProbeCaptureRequest> m_pending_captures;
        std::unordered_map<int, RuntimeCapture> m_captures;
        uint64_t m_active_scene_id = 0;
        uint64_t m_capture_generation = 0;
        uint32_t m_pinned_capture_count = 0;
        int m_last_captured_entity_id = -1;
        bool m_initialized = false;
    };
} // namespace NexAur

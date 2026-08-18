#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "Function/Renderer/data/render_settings.h"
#include "Function/Renderer/Vulkan/frame/vulkan_render_feature_plan.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"

namespace NexAur {
    struct VulkanReflectionHistoryKey {
        uint64_t scene_id = 0;
        uint64_t frame_serial = 0;
        uint64_t surface_generation = 0;
        uint64_t tlas_generation = 0;
        uint64_t settings_signature = 0;
        uint32_t viewport_width = 0;
        uint32_t viewport_height = 0;
        VulkanFrameOutputRoute output_route = VulkanFrameOutputRoute::DirectSwapchain;
        bool reflection_enabled = false;
        bool half_resolution = true;

        bool operator==(const VulkanReflectionHistoryKey& other) const {
            return scene_id == other.scene_id &&
                   frame_serial == other.frame_serial &&
                   surface_generation == other.surface_generation &&
                   tlas_generation == other.tlas_generation &&
                   settings_signature == other.settings_signature &&
                   viewport_width == other.viewport_width &&
                   viewport_height == other.viewport_height &&
                   output_route == other.output_route &&
                   reflection_enabled == other.reflection_enabled &&
                   half_resolution == other.half_resolution;
        }
    };

    struct VulkanReflectionHistoryDecision {
        bool reset = true;
        const char* reason = "First frame";
    };

    uint64_t hashVulkanReflectionSettings(
        const RenderRayTracedReflectionSettings& settings);

    VulkanReflectionHistoryDecision decideVulkanReflectionHistoryReset(
        bool has_previous,
        const VulkanReflectionHistoryKey& previous,
        const VulkanReflectionHistoryKey& current,
        bool camera_cut);

    class VulkanReflectionHistoryState final {
    public:
        VulkanReflectionHistoryState() = default;

        void reset();
        void prepareFrame(
            VulkanDrawList& draw_list,
            const VulkanReflectionHistoryKey& key);
        void onFrameSubmitted();

        bool isValid() const { return m_has_previous && !m_pending_reset; }
        bool isPendingReset() const { return m_pending_reset; }
        const std::string& getLastResetReason() const { return m_last_reset_reason; }
        const VulkanReflectionHistoryKey& getCommittedKey() const { return m_committed_key; }

    private:
        static bool isCameraCut(
            const VulkanDrawList& draw_list,
            const glm::vec3& previous_camera_position,
            bool has_previous);

    private:
        VulkanReflectionHistoryKey m_committed_key;
        VulkanReflectionHistoryKey m_pending_key;
        glm::mat4 m_previous_view_projection{ 1.0f };
        glm::mat4 m_pending_view_projection{ 1.0f };
        glm::vec3 m_previous_camera_position{ 0.0f };
        glm::vec3 m_pending_camera_position{ 0.0f };
        std::unordered_map<int, glm::mat4> m_previous_object_transforms;
        std::unordered_map<int, glm::mat4> m_pending_object_transforms;
        std::string m_last_reset_reason = "First frame";
        bool m_has_previous = false;
        bool m_pending_reset = true;
    };
} // namespace NexAur

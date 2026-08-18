#include "pch.h"
#include "vulkan_reflection_history.h"

#include <bit>
#include <cstdint>
#include <utility>

namespace NexAur {
    namespace {
        uint64_t hashCombine(uint64_t seed, uint64_t value) {
            seed ^= value + 0x9e3779b97f4a7c15ull +
                (seed << 6u) + (seed >> 2u);
            return seed;
        }

        uint64_t hashFloat(uint64_t seed, float value) {
            return hashCombine(seed, std::bit_cast<uint32_t>(value));
        }
    } // namespace

    uint64_t hashVulkanReflectionSettings(
        const RenderRayTracedReflectionSettings& settings) {
        uint64_t hash = 0xcbf29ce484222325ull;
        hash = hashCombine(hash, settings.enabled ? 1u : 0u);
        hash = hashCombine(hash, settings.half_resolution ? 1u : 0u);
        hash = hashFloat(hash, settings.max_distance);
        hash = hashFloat(hash, settings.max_roughness);
        hash = hashFloat(hash, settings.normal_bias);
        return hash;
    }

    VulkanReflectionHistoryDecision decideVulkanReflectionHistoryReset(
        bool has_previous,
        const VulkanReflectionHistoryKey& previous,
        const VulkanReflectionHistoryKey& current,
        bool camera_cut) {
        if (!has_previous) {
            return { true, "First frame" };
        }
        if (previous.scene_id != current.scene_id) {
            return { true, "Scene changed" };
        }
        if (current.frame_serial != previous.frame_serial + 1u) {
            return { true, "Frame serial discontinuity" };
        }
        if (previous.viewport_width != current.viewport_width ||
            previous.viewport_height != current.viewport_height) {
            return { true, "Viewport extent changed" };
        }
        if (previous.output_route != current.output_route) {
            return { true, "Output route changed" };
        }
        if (previous.reflection_enabled != current.reflection_enabled ||
            previous.half_resolution != current.half_resolution) {
            return { true, "Reflection mode changed" };
        }
        if (previous.surface_generation != current.surface_generation) {
            return { true, "Surface generation changed" };
        }
        if (previous.tlas_generation != current.tlas_generation) {
            return { true, "TLAS generation changed" };
        }
        if (previous.settings_signature != current.settings_signature) {
            return { true, "Reflection settings changed" };
        }
        if (camera_cut) {
            return { true, "Camera cut or teleport" };
        }
        return { false, "Valid" };
    }

    void VulkanReflectionHistoryState::reset() {
        m_committed_key = {};
        m_pending_key = {};
        m_previous_view_projection = glm::mat4{ 1.0f };
        m_pending_view_projection = glm::mat4{ 1.0f };
        m_previous_camera_position = glm::vec3{ 0.0f };
        m_pending_camera_position = glm::vec3{ 0.0f };
        m_previous_object_transforms.clear();
        m_pending_object_transforms.clear();
        m_last_reset_reason = "First frame";
        m_has_previous = false;
        m_pending_reset = true;
    }

    void VulkanReflectionHistoryState::prepareFrame(
        VulkanDrawList& draw_list,
        const VulkanReflectionHistoryKey& key) {
        const bool camera_cut = isCameraCut(
            draw_list,
            m_previous_camera_position,
            m_has_previous);
        const VulkanReflectionHistoryDecision decision =
            decideVulkanReflectionHistoryReset(
                m_has_previous,
                m_committed_key,
                key,
                camera_cut);

        m_pending_key = key;
        m_pending_view_projection = draw_list.view.view_projection_matrix;
        m_pending_camera_position = draw_list.view.camera_position;
        m_pending_reset = decision.reset;
        m_last_reset_reason = decision.reason;
        draw_list.previous_view_projection = m_has_previous ?
            m_previous_view_projection : draw_list.view.view_projection_matrix;
        draw_list.reflection_history_valid = m_has_previous && !decision.reset;
        draw_list.reflection_history_reset = decision.reset;

        m_pending_object_transforms.clear();
        for (VulkanMeshDrawItem& item : draw_list.opaque_items) {
            item.previous_transform = item.transform;
            if (!decision.reset && item.entity_id >= 0) {
                const auto previous = m_previous_object_transforms.find(item.entity_id);
                if (previous != m_previous_object_transforms.end()) {
                    item.previous_transform = previous->second;
                }
            }
            if (item.entity_id >= 0) {
                m_pending_object_transforms[item.entity_id] = item.transform;
            }
        }
        for (VulkanMeshDrawItem& item : draw_list.transparent_items) {
            if (item.entity_id >= 0) {
                m_pending_object_transforms[item.entity_id] = item.transform;
            }
        }
    }

    void VulkanReflectionHistoryState::onFrameSubmitted() {
        m_committed_key = m_pending_key;
        m_previous_view_projection = m_pending_view_projection;
        m_previous_camera_position = m_pending_camera_position;
        m_previous_object_transforms = std::move(m_pending_object_transforms);
        m_has_previous = true;
        m_pending_reset = false;
    }

    bool VulkanReflectionHistoryState::isCameraCut(
        const VulkanDrawList& draw_list,
        const glm::vec3& previous_camera_position,
        bool has_previous) {
        if (!has_previous) {
            return false;
        }

        return glm::length(draw_list.view.camera_position - previous_camera_position) > 25.0f;
    }
} // namespace NexAur

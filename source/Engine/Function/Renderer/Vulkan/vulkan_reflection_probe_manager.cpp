#include "pch.h"
#include "vulkan_reflection_probe_manager.h"

#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/core/vulkan_retirement_queue.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_render_data_translator.h"
#include "Function/Renderer/Vulkan/reflection_probe_residency.h"
#include "Function/Renderer/Vulkan/vulkan_render_resource_cache.h"
#include "Function/Resource/asset_manager.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace NexAur {
    namespace {
        float sanitizeMin(float value, float fallback, float minimum) {
            return std::isfinite(value) && value >= minimum ? value : fallback;
        }

        uint32_t sanitizeCaptureResolution(uint32_t resolution) {
            if (resolution >= 1024u) {
                return 1024u;
            }
            if (resolution >= 512u) {
                return 512u;
            }
            if (resolution >= 256u) {
                return 256u;
            }
            if (resolution >= 128u) {
                return 128u;
            }
            if (resolution >= 64u) {
                return 64u;
            }
            return 32u;
        }

        RenderView buildCaptureView(
            const RenderFrameReflectionProbe& probe,
            uint32_t face_index,
            uint32_t resolution,
            float near_clip,
            float far_clip) {
            constexpr std::array<glm::vec3, VulkanReflectionProbeCaptureTarget::kFaceCount>
                kFaceDirections{
                    glm::vec3{ 1.0f, 0.0f, 0.0f },
                    glm::vec3{ -1.0f, 0.0f, 0.0f },
                    glm::vec3{ 0.0f, 1.0f, 0.0f },
                    glm::vec3{ 0.0f, -1.0f, 0.0f },
                    glm::vec3{ 0.0f, 0.0f, 1.0f },
                    glm::vec3{ 0.0f, 0.0f, -1.0f }
                };
            constexpr std::array<glm::vec3, VulkanReflectionProbeCaptureTarget::kFaceCount>
                kFaceUps{
                    glm::vec3{ 0.0f, -1.0f, 0.0f },
                    glm::vec3{ 0.0f, -1.0f, 0.0f },
                    glm::vec3{ 0.0f, 0.0f, 1.0f },
                    glm::vec3{ 0.0f, 0.0f, -1.0f },
                    glm::vec3{ 0.0f, -1.0f, 0.0f },
                    glm::vec3{ 0.0f, -1.0f, 0.0f }
                };

            const uint32_t safe_face =
                std::min(face_index, VulkanReflectionProbeCaptureTarget::kFaceCount - 1u);
            const float safe_near =
                sanitizeMin(near_clip, probe.capture_near_clip, 0.001f);
            const float safe_far = std::max(
                safe_near + 0.01f,
                sanitizeMin(far_clip, probe.capture_far_clip, safe_near + 0.01f));

            RenderView view;
            view.viewport_width = std::max(1u, resolution);
            view.viewport_height = std::max(1u, resolution);
            view.near_clip = safe_near;
            view.far_clip = safe_far;
            view.camera_position = probe.position;
            view.view_matrix = glm::lookAt(
                probe.position,
                probe.position + kFaceDirections[safe_face],
                kFaceUps[safe_face]);
            view.projection_matrix = glm::perspective(
                glm::radians(90.0f),
                1.0f,
                safe_near,
                safe_far);
            view.view_projection_matrix = view.projection_matrix * view.view_matrix;
            view.inverse_view_matrix = glm::inverse(view.view_matrix);
            view.inverse_projection_matrix = glm::inverse(view.projection_matrix);
            return view;
        }

        const RenderFrameReflectionProbe* findProbe(
            const RenderSceneFrame& scene_frame,
            int entity_id) {
            const auto probe_it = std::find_if(
                scene_frame.reflection_probes.begin(),
                scene_frame.reflection_probes.end(),
                [entity_id](const RenderFrameReflectionProbe& probe) {
                    return probe.entity_id == entity_id;
                });
            return probe_it != scene_frame.reflection_probes.end() ? &*probe_it : nullptr;
        }

        const RenderFrameReflectionProbeReference* findProbeReference(
            const RenderSceneFrame& scene_frame,
            int entity_id) {
            const auto probe_it = std::find_if(
                scene_frame.reflection_probe_references.begin(),
                scene_frame.reflection_probe_references.end(),
                [entity_id](const RenderFrameReflectionProbeReference& reference) {
                    return reference.entity_id == entity_id;
                });
            return probe_it != scene_frame.reflection_probe_references.end() ? &*probe_it : nullptr;
        }

        VulkanEnvironmentResourceBuildSettings buildEnvironmentSettings(
            const ReflectionProbeCaptureRequest& request) {
            VulkanEnvironmentResourceBuildSettings settings;
            settings.environment_size = sanitizeCaptureResolution(request.resolution);
            settings.irradiance_size = std::clamp(settings.environment_size / 4u, 16u, 64u);
            settings.prefilter_size = std::clamp(settings.environment_size, 32u, 512u);
            settings.brdf_lut_size = 256u;
            settings.debug_name_override =
                "RuntimeReflectionProbe." + std::to_string(request.entity_id);
            return settings;
        }

        void transitionImageLayout(
            VkCommandBuffer command_buffer,
            VkImage image,
            VkImageLayout old_layout,
            VkImageLayout new_layout,
            VkImageAspectFlags aspect_mask,
            VkAccessFlags src_access,
            VkAccessFlags dst_access,
            VkPipelineStageFlags src_stage,
            VkPipelineStageFlags dst_stage,
            uint32_t layer_count) {
            if (old_layout == new_layout) {
                return;
            }

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = src_access;
            barrier.dstAccessMask = dst_access;
            barrier.oldLayout = old_layout;
            barrier.newLayout = new_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = aspect_mask;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = layer_count;

            vkCmdPipelineBarrier(
                command_buffer,
                src_stage,
                dst_stage,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);
        }
    } // namespace

    VulkanReflectionProbeManager::~VulkanReflectionProbeManager() {
        shutdown();
    }

    bool VulkanReflectionProbeManager::init(
        const VulkanResourceContext& context,
        VkFormat color_format,
        VkFormat depth_format) {
        shutdown();

        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            color_format == VK_FORMAT_UNDEFINED ||
            depth_format == VK_FORMAT_UNDEFINED) {
            NX_CORE_ERROR("VulkanReflectionProbeManager requires a valid Vulkan context and formats.");
            return false;
        }

        m_resource_context = context;
        m_retirement_queue = context.retirement_queue;
        m_color_format = color_format;
        m_depth_format = depth_format;
        if (!createCommandPool(context.graphics_queue_family)) {
            shutdown();
            return false;
        }
        m_initialized = true;
        return true;
    }

    void VulkanReflectionProbeManager::shutdown() {
        m_pending_captures.clear();
        clearCaptures();
        m_capture_target.shutdown();
        if (m_resource_context.device != VK_NULL_HANDLE &&
            m_command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(
                m_resource_context.device,
                m_command_pool,
                nullptr);
        }
        m_resource_context = {};
        m_retirement_queue = nullptr;
        m_command_pool = VK_NULL_HANDLE;
        m_color_format = VK_FORMAT_UNDEFINED;
        m_depth_format = VK_FORMAT_UNDEFINED;
        m_active_scene_id = 0;
        m_capture_generation = 0;
        m_pinned_capture_count = 0;
        m_last_captured_entity_id = -1;
        m_initialized = false;
    }

    bool VulkanReflectionProbeManager::requestCapture(
        const ReflectionProbeCaptureRequest& request) {
        if (!m_initialized || request.entity_id < 0) {
            return false;
        }

        ReflectionProbeCaptureRequest sanitized_request = request;
        sanitized_request.resolution = sanitizeCaptureResolution(request.resolution);
        sanitized_request.priority = std::min(request.priority, 100u);
        sanitized_request.near_clip = sanitizeMin(request.near_clip, 0.1f, 0.001f);
        sanitized_request.far_clip = std::max(
            sanitizeMin(request.far_clip, 40.0f, 0.01f),
            sanitized_request.near_clip + 0.001f);

        RuntimeCapture& capture = m_captures[sanitized_request.entity_id];
        capture.state = buildCaptureState(
            sanitized_request,
            ReflectionProbeCaptureStatus::Pending,
            false,
            "Queued reflection probe capture.");
        enqueueCapture(sanitized_request);
        return true;
    }

    bool VulkanReflectionProbeManager::clearCapture(int entity_id) {
        if (entity_id < 0) {
            return false;
        }

        const bool removed_pending = erasePendingCapture(entity_id);
        const auto capture_it = m_captures.find(entity_id);
        const bool removed_capture = capture_it != m_captures.end();
        if (removed_capture) {
            retireEnvironment(capture_it->second);
            m_captures.erase(capture_it);
        }
        return removed_pending || removed_capture;
    }

    ReflectionProbeCaptureState VulkanReflectionProbeManager::getCaptureState(
        int entity_id) const {
        if (entity_id < 0) {
            return {};
        }

        const auto capture_it = m_captures.find(entity_id);
        return capture_it != m_captures.end() ?
            capture_it->second.state :
            ReflectionProbeCaptureState{};
    }

    ReflectionProbeCaptureQueueState VulkanReflectionProbeManager::getQueueState() const {
        ReflectionProbeCaptureQueueState state;
        state.pending_count = static_cast<uint32_t>(m_pending_captures.size());
        state.capture_budget_per_frame = kCaptureBudgetPerFrame;
        state.resident_capture_count = countResidentCaptures();
        state.pinned_capture_count = m_pinned_capture_count;
        state.resident_capture_limit = kResidentCaptureLimit;
        state.last_captured_entity_id = m_last_captured_entity_id;
        state.last_captured_generation = m_capture_generation;
        state.message = state.pending_count > 0 ?
            "Reflection probe captures queued." :
            "Reflection probe capture queue idle.";
        return state;
    }

    void VulkanReflectionProbeManager::processFrame(
        VulkanPreparedFrame& prepared_frame,
        VulkanRenderResourceCache& resource_cache,
        AssetManager& asset_manager,
        const VulkanReflectionProbeCaptureCallbacks& callbacks) {
        if (!m_initialized) {
            return;
        }

        syncScene(prepared_frame.scene.scene_id);
        pruneCaptures(prepared_frame.scene);
        processPendingCaptures(
            prepared_frame,
            resource_cache,
            asset_manager,
            callbacks);
        bindRuntimeCapture(prepared_frame);
        enforceResidentBudget(
            prepared_frame.scene,
            prepared_frame.draw_list.active_reflection_probe.entity_id);
        m_pinned_capture_count = countPinnedCaptures(prepared_frame.scene);
    }

    ReflectionProbeCaptureState VulkanReflectionProbeManager::buildCaptureState(
        const ReflectionProbeCaptureRequest& request,
        ReflectionProbeCaptureStatus status,
        bool runtime_resource_ready,
        std::string message) const {
        ReflectionProbeCaptureState state;
        state.status = status;
        state.resolution = request.resolution;
        state.runtime_resource_ready = runtime_resource_ready;
        state.include_skybox = request.include_skybox;
        state.last_kind = request.kind;
        state.input_hash = request.input_hash;
        state.message = std::move(message);
        return state;
    }

    void VulkanReflectionProbeManager::setCaptureFailure(
        RuntimeCapture& capture,
        const ReflectionProbeCaptureRequest& request,
        std::string message) const {
        const bool previous_resource_ready =
            capture.environment && capture.environment->isReady();
        capture.state = buildCaptureState(
            request,
            ReflectionProbeCaptureStatus::Failed,
            previous_resource_ready,
            std::move(message));
        capture.state.generation = capture.generation;
        capture.state.baked_asset = capture.baked_asset;
        if (previous_resource_ready) {
            capture.state.resolution = capture.environment->getEnvironmentSize();
        }
    }

    void VulkanReflectionProbeManager::syncScene(uint64_t scene_id) {
        if (m_active_scene_id == scene_id) {
            return;
        }

        m_pending_captures.clear();
        clearCaptures();
        m_pinned_capture_count = 0;
        m_last_captured_entity_id = -1;
        m_active_scene_id = scene_id;
    }

    void VulkanReflectionProbeManager::pruneCaptures(
        const RenderSceneFrame& scene_frame) {
        for (auto capture_it = m_captures.begin(); capture_it != m_captures.end();) {
            const int entity_id = capture_it->first;
            if (!hasPendingCapture(entity_id) &&
                findProbeReference(scene_frame, entity_id) == nullptr) {
                retireEnvironment(capture_it->second);
                capture_it = m_captures.erase(capture_it);
                continue;
            }

            ++capture_it;
        }
    }

    void VulkanReflectionProbeManager::clearCaptures() {
        for (auto& [entity_id, capture] : m_captures) {
            (void)entity_id;
            retireEnvironment(capture);
        }
        m_captures.clear();
    }

    void VulkanReflectionProbeManager::retireEnvironment(RuntimeCapture& capture) {
        if (!capture.environment) {
            return;
        }
        if (m_retirement_queue != nullptr) {
            m_retirement_queue->retire(std::move(capture.environment));
        } else {
            capture.environment.reset();
        }
    }

    void VulkanReflectionProbeManager::enqueueCapture(
        const ReflectionProbeCaptureRequest& request) {
        auto existing_request = std::find_if(
            m_pending_captures.begin(),
            m_pending_captures.end(),
            [&](const ReflectionProbeCaptureRequest& queued_request) {
                return queued_request.entity_id == request.entity_id;
            });
        if (existing_request != m_pending_captures.end()) {
            *existing_request = request;
        } else {
            m_pending_captures.push_back(request);
        }

        std::stable_sort(
            m_pending_captures.begin(),
            m_pending_captures.end(),
            [](const ReflectionProbeCaptureRequest& lhs,
               const ReflectionProbeCaptureRequest& rhs) {
                return lhs.priority > rhs.priority;
            });
    }

    bool VulkanReflectionProbeManager::erasePendingCapture(int entity_id) {
        const auto old_size = m_pending_captures.size();
        m_pending_captures.erase(
            std::remove_if(
                m_pending_captures.begin(),
                m_pending_captures.end(),
                [entity_id](const ReflectionProbeCaptureRequest& request) {
                    return request.entity_id == entity_id;
                }),
            m_pending_captures.end());
        return m_pending_captures.size() != old_size;
    }

    bool VulkanReflectionProbeManager::hasPendingCapture(int entity_id) const {
        return std::any_of(
            m_pending_captures.begin(),
            m_pending_captures.end(),
            [entity_id](const ReflectionProbeCaptureRequest& request) {
                return request.entity_id == entity_id;
            });
    }

    uint32_t VulkanReflectionProbeManager::countResidentCaptures() const {
        uint32_t count = 0;
        for (const auto& [entity_id, capture] : m_captures) {
            (void)entity_id;
            if (capture.environment && capture.environment->isReady()) {
                ++count;
            }
        }
        return count;
    }

    bool VulkanReflectionProbeManager::isCapturePinned(
        const RenderSceneFrame& scene_frame,
        int entity_id,
        const RuntimeCapture& capture) const {
        if (!capture.baked_asset) {
            return false;
        }

        const RenderFrameReflectionProbeReference* probe =
            findProbeReference(scene_frame, entity_id);
        const bool referenced_by_scene =
            probe && probe->baked_environment_asset == capture.baked_asset;
        const bool bake_grace_period =
            capture.bake_pin_until_frame > 0 &&
            scene_frame.frame_serial <= capture.bake_pin_until_frame;
        return referenced_by_scene || bake_grace_period;
    }

    uint32_t VulkanReflectionProbeManager::countPinnedCaptures(
        const RenderSceneFrame& scene_frame) const {
        uint32_t count = 0;
        for (const auto& [entity_id, capture] : m_captures) {
            if (capture.environment &&
                capture.environment->isReady() &&
                isCapturePinned(scene_frame, entity_id, capture)) {
                ++count;
            }
        }
        return count;
    }

    bool VulkanReflectionProbeManager::evictCapture(
        const RenderSceneFrame& scene_frame,
        int protected_entity_id) {
        std::vector<ReflectionProbeResidencyCandidate> candidates;
        candidates.reserve(m_captures.size());
        for (const auto& [entity_id, capture] : m_captures) {
            ReflectionProbeResidencyCandidate candidate;
            candidate.entity_id = entity_id;
            candidate.last_used_frame = capture.last_used_frame;
            candidate.generation = capture.generation;
            candidate.resident = capture.environment && capture.environment->isReady();
            candidate.pinned = isCapturePinned(scene_frame, entity_id, capture);
            candidate.pending = hasPendingCapture(entity_id);
            candidates.push_back(candidate);
        }

        const int entity_id =
            selectReflectionProbeEvictionCandidate(candidates, protected_entity_id);
        const auto capture_it = m_captures.find(entity_id);
        if (entity_id < 0 || capture_it == m_captures.end()) {
            return false;
        }

        RuntimeCapture& capture = capture_it->second;
        retireEnvironment(capture);
        capture.baked_asset = AssetHandle{};
        capture.bake_pin_until_frame = 0;
        capture.state.status = ReflectionProbeCaptureStatus::Failed;
        capture.state.runtime_resource_ready = false;
        capture.state.baked_asset = AssetHandle{};
        capture.state.message =
            "Runtime reflection probe evicted to satisfy the resident budget.";
        return true;
    }

    bool VulkanReflectionProbeManager::canAcquireResidentSlot(
        const RenderSceneFrame& scene_frame,
        int requested_entity_id,
        int protected_entity_id) const {
        const auto requested_capture = m_captures.find(requested_entity_id);
        if ((requested_capture != m_captures.end() &&
             requested_capture->second.environment &&
             requested_capture->second.environment->isReady()) ||
            countResidentCaptures() < kResidentCaptureLimit) {
            return true;
        }

        std::vector<ReflectionProbeResidencyCandidate> candidates;
        candidates.reserve(m_captures.size());
        for (const auto& [entity_id, capture] : m_captures) {
            candidates.push_back({
                entity_id,
                capture.last_used_frame,
                capture.generation,
                capture.environment && capture.environment->isReady(),
                isCapturePinned(scene_frame, entity_id, capture),
                hasPendingCapture(entity_id)
            });
        }
        return selectReflectionProbeEvictionCandidate(candidates, protected_entity_id) >= 0;
    }

    bool VulkanReflectionProbeManager::ensureResidentSlot(
        const RenderSceneFrame& scene_frame,
        int requested_entity_id,
        int protected_entity_id) {
        const auto requested_capture = m_captures.find(requested_entity_id);
        if (requested_capture != m_captures.end() &&
            requested_capture->second.environment &&
            requested_capture->second.environment->isReady()) {
            return true;
        }

        while (countResidentCaptures() >= kResidentCaptureLimit) {
            if (!evictCapture(scene_frame, protected_entity_id)) {
                return false;
            }
        }
        return true;
    }

    void VulkanReflectionProbeManager::enforceResidentBudget(
        const RenderSceneFrame& scene_frame,
        int protected_entity_id) {
        while (countResidentCaptures() > kResidentCaptureLimit) {
            if (!evictCapture(scene_frame, protected_entity_id)) {
                return;
            }
        }
    }

    void VulkanReflectionProbeManager::processPendingCaptures(
        const VulkanPreparedFrame& prepared_frame,
        VulkanRenderResourceCache& resource_cache,
        AssetManager& asset_manager,
        const VulkanReflectionProbeCaptureCallbacks& callbacks) {
        const RenderSceneFrame& scene_frame = prepared_frame.scene;
        const VulkanDrawList& draw_list = prepared_frame.draw_list;
        uint32_t processed_count = 0;
        while (processed_count < kCaptureBudgetPerFrame && !m_pending_captures.empty()) {
            ReflectionProbeCaptureRequest request = m_pending_captures.front();
            m_pending_captures.erase(m_pending_captures.begin());

            RuntimeCapture& capture = m_captures[request.entity_id];
            capture.state = buildCaptureState(
                request,
                ReflectionProbeCaptureStatus::Capturing,
                false,
                "Capturing runtime reflection probe.");

            const RenderFrameReflectionProbe* probe = findProbe(scene_frame, request.entity_id);
            if (!probe) {
                setCaptureFailure(
                    capture,
                    request,
                    "Probe was not present in the current render frame.");
                ++processed_count;
                continue;
            }

            if (!callbacks.valid()) {
                setCaptureFailure(
                    capture,
                    request,
                    "Renderer capture callbacks were not available.");
                ++processed_count;
                continue;
            }

            if (!canAcquireResidentSlot(
                    scene_frame,
                    request.entity_id,
                    draw_list.active_reflection_probe.entity_id)) {
                setCaptureFailure(
                    capture,
                    request,
                    "Reflection probe capture failed: resident budget is full and all resources are pinned, active, or pending.");
                ++processed_count;
                continue;
            }

            std::string capture_error;
            std::unique_ptr<VulkanEnvironmentResource> runtime_environment = captureScene(
                draw_list,
                *probe,
                request,
                scene_frame.render_settings,
                resource_cache,
                callbacks,
                capture_error);
            if (!runtime_environment || !runtime_environment->isReady()) {
                setCaptureFailure(
                    capture,
                    request,
                    capture_error.empty() ?
                        "Failed to capture runtime reflection probe from scene." :
                        capture_error);
                ++processed_count;
                continue;
            }

            const uint32_t actual_resolution = runtime_environment->getEnvironmentSize();
            if (!ensureResidentSlot(
                    scene_frame,
                    request.entity_id,
                    draw_list.active_reflection_probe.entity_id)) {
                setCaptureFailure(
                    capture,
                    request,
                    "Reflection probe capture failed while reserving the resident resource slot.");
                ++processed_count;
                continue;
            }

            retireEnvironment(capture);
            capture.environment = std::move(runtime_environment);
            capture.generation = ++m_capture_generation;
            capture.last_used_frame = scene_frame.frame_serial;
            if (request.kind == ReflectionProbeCaptureKind::Bake) {
                if (!capture.baked_asset) {
                    capture.baked_asset = asset_manager.registerRuntimeAsset(
                        AssetType::EnvironmentMap,
                        "BakedReflectionProbe." + std::to_string(request.entity_id));
                }
                capture.bake_pin_until_frame = scene_frame.frame_serial + 1u;
            } else if (!capture.baked_asset) {
                capture.bake_pin_until_frame = 0;
            }

            capture.state = buildCaptureState(
                request,
                ReflectionProbeCaptureStatus::Ready,
                true,
                request.kind == ReflectionProbeCaptureKind::Bake ?
                    "Scene capture bake ready." :
                    "Scene capture ready.");
            capture.state.resolution = actual_resolution;
            capture.state.generation = capture.generation;
            capture.state.baked_asset = capture.baked_asset;
            m_last_captured_entity_id = request.entity_id;
            ++processed_count;
        }
    }

    std::unique_ptr<VulkanEnvironmentResource>
    VulkanReflectionProbeManager::captureScene(
        const VulkanDrawList& source_draw_list,
        const RenderFrameReflectionProbe& probe,
        const ReflectionProbeCaptureRequest& request,
        const RenderSettings& render_settings,
        VulkanRenderResourceCache& resource_cache,
        const VulkanReflectionProbeCaptureCallbacks& callbacks,
        std::string& error_message) {
        const uint32_t resolution = sanitizeCaptureResolution(request.resolution);
        if (!ensureCaptureTarget(resolution, error_message)) {
            return nullptr;
        }

        RenderSettings capture_settings = render_settings;
        capture_settings.ibl_debug.mode = RenderIblDebugMode::FinalLit;
        capture_settings.effects_debug.view = RenderEffectDebugView::FinalLit;
        capture_settings.shadow.cascade_debug_overlay = false;
        if (!callbacks.prepare(capture_settings, error_message)) {
            if (error_message.empty()) {
                error_message =
                    "Failed to prepare shadow targets for reflection probe capture.";
            }
            return nullptr;
        }

        VulkanRenderDataTranslator translator;
        for (uint32_t face = 0;
             face < VulkanReflectionProbeCaptureTarget::kFaceCount;
             ++face) {
            VulkanDrawList capture_draw_list = source_draw_list;
            const RenderView capture_view = buildCaptureView(
                probe,
                face,
                resolution,
                request.near_clip,
                request.far_clip);
            capture_draw_list.view = translator.buildRenderView(capture_view);
            capture_draw_list.active_reflection_probe = {};
            capture_draw_list.debug_draw = {};

            const VulkanRenderTarget face_target =
                m_capture_target.getFaceRenderTarget(face);
            const std::string operation =
                "vkQueueSubmit(reflection probe capture face " +
                std::to_string(face) + ")";
            if (!submitImmediateCommands(
                    operation.c_str(),
                    [&](VkCommandBuffer command_buffer) {
                        if (!callbacks.record_shadows(
                                command_buffer,
                                capture_view,
                                capture_draw_list,
                                capture_settings,
                                error_message)) {
                            return false;
                        }
                        transitionCaptureImagesToAttachment(command_buffer);
                        return callbacks.record_face(
                            command_buffer,
                            capture_draw_list,
                            face_target,
                            request.include_skybox,
                            error_message);
                    })) {
                if (error_message.empty()) {
                    error_message =
                        "Failed to render reflection probe cubemap face " +
                        std::to_string(face) + ".";
                }
                return nullptr;
            }
        }

        if (!copyCaptureToReadback()) {
            error_message = "Failed to copy reflection probe cubemap to readback buffer.";
            return nullptr;
        }

        std::vector<float> captured_pixels;
        if (!m_capture_target.readColorPixels(captured_pixels)) {
            error_message = "Failed to read reflection probe cubemap pixels.";
            return nullptr;
        }

        std::unique_ptr<VulkanEnvironmentResource> runtime_environment =
            resource_cache.createRuntimeEnvironmentFromCubePixels(
                resolution,
                captured_pixels,
                buildEnvironmentSettings(request));
        if (!runtime_environment || !runtime_environment->isReady()) {
            error_message =
                "Failed to create runtime reflection probe resource from scene capture.";
            return nullptr;
        }

        return runtime_environment;
    }

    void VulkanReflectionProbeManager::bindRuntimeCapture(
        VulkanPreparedFrame& prepared_frame) {
        VulkanActiveReflectionProbe& active_probe =
            prepared_frame.draw_list.active_reflection_probe;
        if (!active_probe.enabled || active_probe.entity_id < 0) {
            return;
        }

        const auto capture_it = m_captures.find(active_probe.entity_id);
        if (capture_it == m_captures.end()) {
            return;
        }

        const VulkanEnvironmentResource* runtime_environment =
            capture_it->second.environment.get();
        if (!runtime_environment || !runtime_environment->isReady()) {
            return;
        }

        active_probe.environment = runtime_environment;
        active_probe.using_runtime_capture = true;
        active_probe.prefilter_mip_count = runtime_environment->getPrefilterMipCount();
        capture_it->second.last_used_frame = prepared_frame.scene.frame_serial;
    }

    bool VulkanReflectionProbeManager::ensureCaptureTarget(
        uint32_t resolution,
        std::string& error_message) {
        resolution = sanitizeCaptureResolution(resolution);
        if (m_capture_target.isReady() &&
            m_capture_target.getResolution() == resolution) {
            return true;
        }

        if (!m_initialized ||
            m_resource_context.device == VK_NULL_HANDLE ||
            m_color_format == VK_FORMAT_UNDEFINED ||
            m_depth_format == VK_FORMAT_UNDEFINED) {
            error_message =
                "Renderer was not ready to create reflection probe capture target.";
            return false;
        }

        if (m_capture_target.isReady()) {
            vkDeviceWaitIdle(m_resource_context.device);
            if (!m_capture_target.resize(resolution)) {
                error_message = "Failed to resize reflection probe capture target.";
                return false;
            }
            return true;
        }

        if (!m_capture_target.init(
                m_resource_context,
                m_color_format,
                m_depth_format,
                resolution)) {
            error_message = "Failed to create reflection probe capture target.";
            return false;
        }
        return true;
    }

    bool VulkanReflectionProbeManager::createCommandPool(
        uint32_t queue_family_index) {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags =
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = queue_family_index;
        return VulkanDiagnosticsCollector::checkVk(
            vkCreateCommandPool(
                m_resource_context.device,
                &pool_info,
                nullptr,
                &m_command_pool),
            "vkCreateCommandPool(reflection probe)");
    }

    bool VulkanReflectionProbeManager::submitImmediateCommands(
        const char* operation,
        const std::function<bool(VkCommandBuffer)>& record_commands) const {
        if (!m_initialized ||
            m_resource_context.device == VK_NULL_HANDLE ||
            m_resource_context.graphics_queue == VK_NULL_HANDLE ||
            m_command_pool == VK_NULL_HANDLE ||
            !record_commands) {
            return false;
        }

        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        auto cleanup = [&]() {
            if (fence != VK_NULL_HANDLE) {
                vkDestroyFence(m_resource_context.device, fence, nullptr);
            }
            if (command_buffer != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(
                    m_resource_context.device,
                    m_command_pool,
                    1,
                    &command_buffer);
            }
        };

        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = m_command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;
        if (!VulkanDiagnosticsCollector::checkVk(
                vkAllocateCommandBuffers(
                    m_resource_context.device,
                    &allocate_info,
                    &command_buffer),
                "vkAllocateCommandBuffers(reflection probe capture)")) {
            cleanup();
            return false;
        }

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (!VulkanDiagnosticsCollector::checkVk(
                vkBeginCommandBuffer(command_buffer, &begin_info),
                "vkBeginCommandBuffer(reflection probe capture)")) {
            cleanup();
            return false;
        }

        if (!record_commands(command_buffer) ||
            !VulkanDiagnosticsCollector::checkVk(
                vkEndCommandBuffer(command_buffer),
                "vkEndCommandBuffer(reflection probe capture)")) {
            cleanup();
            return false;
        }

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (!VulkanDiagnosticsCollector::checkVk(
                vkCreateFence(
                    m_resource_context.device,
                    &fence_info,
                    nullptr,
                    &fence),
                "vkCreateFence(reflection probe capture)")) {
            cleanup();
            return false;
        }

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;
        if (!VulkanDiagnosticsCollector::checkVk(
                vkQueueSubmit(
                    m_resource_context.graphics_queue,
                    1,
                    &submit_info,
                    fence),
                operation) ||
            !VulkanDiagnosticsCollector::checkVk(
                vkWaitForFences(
                    m_resource_context.device,
                    1,
                    &fence,
                    VK_TRUE,
                    UINT64_MAX),
                "vkWaitForFences(reflection probe capture)")) {
            cleanup();
            return false;
        }

        cleanup();
        return true;
    }

    void VulkanReflectionProbeManager::transitionCaptureImagesToAttachment(
        VkCommandBuffer command_buffer) {
        const VkImageLayout old_color_layout = m_capture_target.getColorLayout();
        VkAccessFlags color_src_access = 0;
        VkPipelineStageFlags color_src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if (old_color_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            color_src_access = VK_ACCESS_TRANSFER_READ_BIT;
            color_src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (old_color_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            color_src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            color_src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        transitionImageLayout(
            command_buffer,
            m_capture_target.getColorImage(),
            old_color_layout,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            color_src_access,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            color_src_stage,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VulkanReflectionProbeCaptureTarget::kFaceCount);
        m_capture_target.setColorLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        const VkImageLayout old_depth_layout = m_capture_target.getDepthLayout();
        VkAccessFlags depth_src_access = 0;
        VkPipelineStageFlags depth_src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if (old_depth_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            depth_src_access = VK_ACCESS_SHADER_READ_BIT;
            depth_src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        transitionImageLayout(
            command_buffer,
            m_capture_target.getDepthImage(),
            old_depth_layout,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            depth_src_access,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            depth_src_stage,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            1);
        m_capture_target.setDepthLayout(
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    bool VulkanReflectionProbeManager::copyCaptureToReadback() {
        return submitImmediateCommands(
            "vkQueueSubmit(reflection probe capture readback)",
            [&](VkCommandBuffer command_buffer) {
                transitionImageLayout(
                    command_buffer,
                    m_capture_target.getColorImage(),
                    m_capture_target.getColorLayout(),
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VulkanReflectionProbeCaptureTarget::kFaceCount);
                m_capture_target.setColorLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                return m_capture_target.recordCopyToReadback(command_buffer);
            });
    }
} // namespace NexAur

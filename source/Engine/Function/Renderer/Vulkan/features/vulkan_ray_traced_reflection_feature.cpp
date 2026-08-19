#include "pch.h"
#include "vulkan_ray_traced_reflection_feature.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>

namespace NexAur {
    namespace {
        constexpr uint32_t kTraceFlagSsrEnabled = 1u << 0u;
        constexpr uint32_t kTraceDebugModeShift = 8u;

        VkDescriptorImageInfo sampledImageInfo(VkImageView view) {
            VkDescriptorImageInfo info{};
            info.imageView = view;
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            return info;
        }

        VkDescriptorImageInfo storageImageInfo(VkImageView view) {
            VkDescriptorImageInfo info{};
            info.imageView = view;
            info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            return info;
        }

        float sanitizeRange(
            float value,
            float fallback,
            float minimum,
            float maximum) {
            return std::isfinite(value) ?
                std::clamp(value, minimum, maximum) : fallback;
        }

        bool finiteMatrix(const glm::mat4& matrix) {
            for (uint32_t column = 0; column < 4; ++column) {
                for (uint32_t row = 0; row < 4; ++row) {
                    if (!std::isfinite(matrix[column][row])) {
                        return false;
                    }
                }
            }
            return true;
        }

        glm::vec2 signNotZero(const glm::vec2& value) {
            return {
                value.x >= 0.0f ? 1.0f : -1.0f,
                value.y >= 0.0f ? 1.0f : -1.0f
            };
        }
    } // namespace

    VkExtent2D getVulkanRayTracedReflectionExtent(
        uint32_t width,
        uint32_t height,
        bool half_resolution) {
        width = std::max(1u, width);
        height = std::max(1u, height);
        return {
            half_resolution ? std::max(1u, width / 2u) : width,
            half_resolution ? std::max(1u, height / 2u) : height
        };
    }

    glm::vec2 encodeVulkanReflectionSurfaceNormal(const glm::vec3& normal) {
        const float length_squared = glm::dot(normal, normal);
        if (!std::isfinite(length_squared) || length_squared <= 0.0000001f) {
            return glm::vec2{ 0.5f };
        }

        const glm::vec3 safe_normal = normal / std::sqrt(length_squared);
        const float denominator =
            std::abs(safe_normal.x) +
            std::abs(safe_normal.y) +
            std::abs(safe_normal.z);
        glm::vec2 encoded = glm::vec2{ safe_normal } / denominator;
        if (safe_normal.z < 0.0f) {
            encoded = glm::vec2{
                1.0f - std::abs(encoded.y),
                1.0f - std::abs(encoded.x)
            } * signNotZero(encoded);
        }
        return encoded * 0.5f + 0.5f;
    }

    glm::vec3 decodeVulkanReflectionSurfaceNormal(const glm::vec2& encoded) {
        const glm::vec2 octahedral = encoded * 2.0f - 1.0f;
        glm::vec3 normal{
            octahedral,
            1.0f - std::abs(octahedral.x) - std::abs(octahedral.y)
        };
        if (normal.z < 0.0f) {
            normal = glm::vec3{
                glm::vec2{
                    1.0f - std::abs(normal.y),
                    1.0f - std::abs(normal.x)
                } * signNotZero(glm::vec2{ normal }),
                normal.z
            };
        }

        const float length_squared = glm::dot(normal, normal);
        if (!std::isfinite(length_squared) || length_squared <= 0.0000001f) {
            return glm::vec3{ 0.0f, 0.0f, 1.0f };
        }
        return normal / std::sqrt(length_squared);
    }

    glm::uvec2 mapVulkanRayTracedReflectionSourcePixel(
        glm::uvec2 output_pixel,
        VkExtent2D source_extent,
        VkExtent2D output_extent) {
        if (source_extent.width == 0 ||
            source_extent.height == 0 ||
            output_extent.width == 0 ||
            output_extent.height == 0) {
            return glm::uvec2{ 0u };
        }

        output_pixel.x = std::min(output_pixel.x, output_extent.width - 1u);
        output_pixel.y = std::min(output_pixel.y, output_extent.height - 1u);
        const uint64_t source_x =
            (static_cast<uint64_t>(output_pixel.x) * 2u + 1u) * source_extent.width /
            (static_cast<uint64_t>(output_extent.width) * 2u);
        const uint64_t source_y =
            (static_cast<uint64_t>(output_pixel.y) * 2u + 1u) * source_extent.height /
            (static_cast<uint64_t>(output_extent.height) * 2u);
        return {
            std::min(static_cast<uint32_t>(source_x), source_extent.width - 1u),
            std::min(static_cast<uint32_t>(source_y), source_extent.height - 1u)
        };
    }

    glm::vec3 interpolateVulkanRayTracingTriangleAttribute(
        const glm::vec3& vertex0,
        const glm::vec3& vertex1,
        const glm::vec3& vertex2,
        const glm::vec2& barycentrics) {
        const glm::vec3 weights{
            1.0f - barycentrics.x - barycentrics.y,
            barycentrics.x,
            barycentrics.y
        };
        return vertex0 * weights.x + vertex1 * weights.y + vertex2 * weights.z;
    }

    glm::vec3 transformVulkanRayTracingHitNormal(
        const glm::mat4& object_to_world,
        const glm::vec3& object_normal) {
        const glm::mat3 linear_transform{ object_to_world };
        const float determinant = glm::determinant(linear_transform);
        if (!std::isfinite(determinant) || std::abs(determinant) <= 0.000001f) {
            return glm::vec3{ 0.0f };
        }

        const glm::vec3 world_normal =
            glm::transpose(glm::inverse(linear_transform)) * object_normal;
        const float length_squared = glm::dot(world_normal, world_normal);
        if (!std::isfinite(length_squared) || length_squared <= 0.0000001f) {
            return glm::vec3{ 0.0f };
        }
        return world_normal / std::sqrt(length_squared);
    }

    bool isVulkanRayTracedReflectionTraceEligible(
        float depth,
        const glm::vec4& reflection_surface,
        float ssr_confidence,
        const RenderRayTracedReflectionSettings& settings,
        bool ssr_enabled) {
        if (!std::isfinite(depth) ||
            !std::isfinite(reflection_surface.x) ||
            !std::isfinite(reflection_surface.y) ||
            !std::isfinite(reflection_surface.z) ||
            !std::isfinite(reflection_surface.w) ||
            depth >= 0.99999f ||
            reflection_surface.w <= 0.0001f) {
            return false;
        }

        const float max_roughness = sanitizeRange(
            settings.max_roughness,
            0.85f,
            0.04f,
            1.0f);
        if (reflection_surface.z > max_roughness) {
            return false;
        }
        return !ssr_enabled ||
               !std::isfinite(ssr_confidence) ||
               ssr_confidence <= 0.001f;
    }

    VulkanRayTracedReflectionFeature::~VulkanRayTracedReflectionFeature() {
        shutdown();
    }

    bool VulkanRayTracedReflectionFeature::init(
        const VulkanRenderFeatureContext& context,
        uint32_t texture_capacity,
        uint32_t geometry_descriptor_capacity) {
        shutdown();
        if (!context.valid() ||
            !context.ray_query_enabled ||
            texture_capacity == 0 ||
            geometry_descriptor_capacity == 0) {
            return false;
        }

        m_context = context;
        if (!createPipeline(texture_capacity, geometry_descriptor_capacity) ||
            !allocateDescriptors()) {
            shutdown();
            return false;
        }
        return true;
    }

    void VulkanRayTracedReflectionFeature::shutdown() {
        freeDescriptors();
        m_pipeline = {};
        m_trace_descriptor_set_layout = VK_NULL_HANDLE;
        m_scene_table_descriptor_set_layout = VK_NULL_HANDLE;
        m_environment_descriptor_set_layout = VK_NULL_HANDLE;
        m_dispatch_count = 0;
        m_last_dispatch_extent = {};
        m_context = {};
    }

    bool VulkanRayTracedReflectionFeature::isReady() const {
        if (!m_context.valid() ||
            !m_context.ray_query_enabled ||
            !m_pipeline.valid()) {
            return false;
        }
        return std::all_of(
            m_trace_descriptor_allocations.begin(),
            m_trace_descriptor_allocations.end(),
            [](const VulkanDescriptorSetAllocation& allocation) {
                return allocation.valid();
            });
    }

    bool VulkanRayTracedReflectionFeature::addPass(
        VulkanPassGraph& graph,
        VulkanGraphImageHandle reflection_surface,
        VulkanGraphImageHandle scene_depth,
        VulkanGraphImageHandle ssr_hit_mask,
        VulkanGraphImageHandle raw_reflection,
        VulkanGraphImageHandle hit_distance,
        VulkanGraphAccelerationStructureHandle ray_query_scene,
        const VulkanRayTracedReflectionInput& input,
        const VulkanRenderView& view,
        const RenderRayTracedReflectionSettings& settings,
        bool ssr_enabled,
        VulkanRayTracedReflectionDebugMode debug_mode,
        uint32_t frame_index,
        VulkanRayTracedReflectionTimingCallbacks timing_callbacks) {
        if (!isReady() ||
            !input.valid() ||
            !reflection_surface.valid() ||
            !scene_depth.valid() ||
            !ssr_hit_mask.valid() ||
            !raw_reflection.valid() ||
            !hit_distance.valid() ||
            !ray_query_scene.valid() ||
            frame_index >= m_trace_descriptor_allocations.size()) {
            return false;
        }

        graph.addPass("RayTracedReflectionTrace")
            .readImage(reflection_surface, VulkanGraphImageUsage::ComputeShaderRead)
            .readImage(scene_depth, VulkanGraphImageUsage::ComputeShaderRead)
            .readImage(ssr_hit_mask, VulkanGraphImageUsage::ComputeShaderRead)
            .readAccelerationStructure(
                ray_query_scene,
                VulkanGraphAccelerationStructureUsage::ComputeRayQueryShaderRead)
            .writeImage(raw_reflection, VulkanGraphImageUsage::ComputeStorageWrite)
            .writeImage(hit_distance, VulkanGraphImageUsage::ComputeStorageWrite)
            .execute([
                this,
                input,
                view,
                settings,
                ssr_enabled,
                debug_mode,
                frame_index,
                timing_callbacks = std::move(timing_callbacks)](VkCommandBuffer command_buffer) {
                const bool timing_started =
                    timing_callbacks.begin && timing_callbacks.begin(command_buffer);
                const bool recorded = recordTrace(
                    command_buffer,
                    input,
                    view,
                    settings,
                    ssr_enabled,
                    debug_mode,
                    frame_index);
                if (timing_started &&
                    (!recorded ||
                     !timing_callbacks.end ||
                     !timing_callbacks.end(command_buffer))) {
                    if (timing_callbacks.discard) {
                        timing_callbacks.discard();
                    }
                }
                return recorded;
            });
        return true;
    }

    bool VulkanRayTracedReflectionFeature::createPipeline(
        uint32_t texture_capacity,
        uint32_t geometry_descriptor_capacity) {
        m_trace_descriptor_set_layout =
            m_context.descriptor_layout_cache->getBuiltinLayout(
                VulkanDescriptorSetLayoutId::RayTracedReflectionTrace);
        m_scene_table_descriptor_set_layout =
            m_context.descriptor_layout_cache->getRayTracingShadingSceneLayout(
                texture_capacity,
                geometry_descriptor_capacity);
        m_environment_descriptor_set_layout =
            m_context.descriptor_layout_cache->getBuiltinLayout(
                VulkanDescriptorSetLayoutId::Environment);
        if (m_trace_descriptor_set_layout == VK_NULL_HANDLE ||
            m_scene_table_descriptor_set_layout == VK_NULL_HANDLE ||
            m_environment_descriptor_set_layout == VK_NULL_HANDLE) {
            return false;
        }

        VulkanComputePipelineDesc desc;
        desc.shader_program = VulkanShaderProgramId::RayTracedReflectionTrace;
        desc.descriptor_set_layouts = {
            m_trace_descriptor_set_layout,
            m_scene_table_descriptor_set_layout,
            m_environment_descriptor_set_layout
        };
        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant_range.size = sizeof(TracePushConstants);
        desc.push_constant_ranges = { push_constant_range };
        desc.debug_name = "RayTracedReflectionTrace";
        m_pipeline = m_context.pipeline_cache->getOrCreateComputePipeline(desc);
        return m_pipeline.valid();
    }

    bool VulkanRayTracedReflectionFeature::allocateDescriptors() {
        for (VulkanDescriptorSetAllocation& allocation : m_trace_descriptor_allocations) {
            allocation = m_context.descriptor_allocator->allocate(
                m_trace_descriptor_set_layout);
            if (!allocation.valid()) {
                freeDescriptors();
                return false;
            }
        }
        return true;
    }

    void VulkanRayTracedReflectionFeature::freeDescriptors() {
        if (m_context.descriptor_allocator != nullptr) {
            for (const VulkanDescriptorSetAllocation& allocation :
                 m_trace_descriptor_allocations) {
                if (allocation.valid()) {
                    m_context.descriptor_allocator->free(allocation);
                }
            }
        }
        m_trace_descriptor_allocations = {};
    }

    bool VulkanRayTracedReflectionFeature::updateDescriptor(
        uint32_t frame_index,
        const VulkanRayTracedReflectionInput& input) const {
        if (frame_index >= m_trace_descriptor_allocations.size() ||
            !m_trace_descriptor_allocations[frame_index].valid() ||
            !input.valid()) {
            return false;
        }

        VulkanDescriptorWriter()
            .writeImage(
                0,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                sampledImageInfo(input.reflection_surface_view))
            .writeImage(
                1,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                sampledImageInfo(input.scene_depth_view))
            .writeImage(
                2,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                sampledImageInfo(input.ssr_hit_mask_view))
            .writeImage(
                3,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                storageImageInfo(input.raw_reflection_view))
            .writeImage(
                4,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                storageImageInfo(input.hit_distance_view))
            .update(
                m_context.resources.device,
                m_trace_descriptor_allocations[frame_index].set);
        return true;
    }

    bool VulkanRayTracedReflectionFeature::recordTrace(
        VkCommandBuffer command_buffer,
        const VulkanRayTracedReflectionInput& input,
        const VulkanRenderView& view,
        const RenderRayTracedReflectionSettings& settings,
        bool ssr_enabled,
        VulkanRayTracedReflectionDebugMode debug_mode,
        uint32_t frame_index) {
        if (command_buffer == VK_NULL_HANDLE ||
            !isReady() ||
            !updateDescriptor(frame_index, input)) {
            return false;
        }

        TracePushConstants constants;
        constants.inverse_view_projection = glm::inverse(view.view_projection_matrix);
        if (!finiteMatrix(constants.inverse_view_projection)) {
            return false;
        }
        constants.camera_position_max_distance = glm::vec4{
            view.camera_position,
            sanitizeRange(settings.max_distance, 30.0f, 0.1f, 1000.0f)
        };
        constants.extents = glm::uvec4{
            input.source_extent.width,
            input.source_extent.height,
            input.output_extent.width,
            input.output_extent.height
        };
        constants.trace_params = glm::vec4{
            sanitizeRange(settings.max_roughness, 0.85f, 0.04f, 1.0f),
            sanitizeRange(settings.normal_bias, 0.02f, 0.0f, 1.0f),
            0.001f,
            sanitizeRange(input.environment_intensity, 1.0f, 0.0f, 64.0f)
        };
        const uint32_t flags =
            (ssr_enabled ? kTraceFlagSsrEnabled : 0u) |
            (static_cast<uint32_t>(debug_mode) << kTraceDebugModeShift);
        constants.table_counts_flags = glm::uvec4{
            input.instance_count,
            input.geometry_count,
            input.material_count,
            flags
        };

        const std::array<VkDescriptorSet, 3> descriptor_sets{
            m_trace_descriptor_allocations[frame_index].set,
            input.scene_table_descriptor_set,
            input.environment_descriptor_set
        };
        vkCmdBindPipeline(
            command_buffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_pipeline.pipeline);
        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_pipeline.layout,
            0,
            static_cast<uint32_t>(descriptor_sets.size()),
            descriptor_sets.data(),
            0,
            nullptr);
        vkCmdPushConstants(
            command_buffer,
            m_pipeline.layout,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            sizeof(constants),
            &constants);
        vkCmdDispatch(
            command_buffer,
            (input.output_extent.width + 7u) / 8u,
            (input.output_extent.height + 7u) / 8u,
            1u);
        ++m_dispatch_count;
        m_last_dispatch_extent = input.output_extent;
        return true;
    }
} // namespace NexAur

#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/features/vulkan_reflection_history.h"
#include "Function/Renderer/Vulkan/features/vulkan_render_feature_context.h"
#include "Function/Renderer/Vulkan/frame/vulkan_frame_constants.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_types.h"
#include "Function/Renderer/Vulkan/targets/vulkan_reflection_history_target.h"
#include "Function/Renderer/Vulkan/targets/vulkan_reflection_surface_target.h"

namespace NexAur {
    class VulkanDescriptorAllocator;
    class VulkanPassGraph;

    struct VulkanReflectionSurfaceFeatureGraphResources {
        VulkanGraphImageHandle reflection_surface;
        VulkanGraphImageHandle fallback_specular;
        VulkanGraphImageHandle motion_vector;
        VulkanGraphImageHandle raw_reflection;
        VulkanGraphImageHandle hit_distance;
        VulkanGraphImageHandle filtered_radiance_read;
        VulkanGraphImageHandle filtered_radiance_write;
        VulkanGraphImageHandle moments_read;
        VulkanGraphImageHandle moments_write;
        VulkanGraphImageHandle history_length_read;
        VulkanGraphImageHandle history_length_write;
        VulkanGraphImageHandle depth_read;
        VulkanGraphImageHandle depth_write;
        VulkanGraphImageHandle surface_read;
        VulkanGraphImageHandle surface_write;

        bool valid() const {
            return reflection_surface.valid() &&
                   fallback_specular.valid() &&
                   motion_vector.valid() &&
                   raw_reflection.valid() &&
                   hit_distance.valid() &&
                   filtered_radiance_read.valid() &&
                   filtered_radiance_write.valid() &&
                   moments_read.valid() &&
                   moments_write.valid() &&
                   history_length_read.valid() &&
                   history_length_write.valid() &&
                   depth_read.valid() &&
                   depth_write.valid() &&
                   surface_read.valid() &&
                   surface_write.valid();
        }

        bool anyValid() const {
            return reflection_surface.valid() ||
                   fallback_specular.valid() ||
                   motion_vector.valid() ||
                   raw_reflection.valid() ||
                   hit_distance.valid() ||
                   filtered_radiance_read.valid() ||
                   filtered_radiance_write.valid() ||
                   moments_read.valid() ||
                   moments_write.valid() ||
                   history_length_read.valid() ||
                   history_length_write.valid() ||
                   depth_read.valid() ||
                   depth_write.valid() ||
                   surface_read.valid() ||
                   surface_write.valid();
        }
    };

    struct VulkanReflectionHistoryDebugStats {
        bool ready = false;
        bool valid = false;
        bool pending_reset = true;
        uint32_t read_index = 0;
        uint32_t write_index = 1;
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t surface_generation = 0;
        std::string reset_reason = "First frame";
    };

    class VulkanReflectionSurfaceFeature final {
    public:
        VulkanReflectionSurfaceFeature() = default;
        ~VulkanReflectionSurfaceFeature();

        VulkanReflectionSurfaceFeature(const VulkanReflectionSurfaceFeature&) = delete;
        VulkanReflectionSurfaceFeature& operator=(const VulkanReflectionSurfaceFeature&) = delete;

        bool init(
            const VulkanRenderFeatureContext& context,
            VkFormat reflection_surface_format,
            VkFormat fallback_specular_format,
            VkFormat motion_vector_format,
            VkFormat history_format,
            uint32_t width,
            uint32_t height);
        bool resize(uint32_t width, uint32_t height);
        bool prepareHistoryTarget(
            uint32_t width,
            uint32_t height,
            bool half_resolution);
        void shutdown();

        bool isReady() const;
        uint64_t getSurfaceGeneration() const { return m_surface_generation; }
        bool isHistoryHalfResolution() const { return m_history_half_resolution; }
        VulkanReflectionSurfaceTarget& getSurfaceTarget() { return m_surface_target; }
        const VulkanReflectionSurfaceTarget& getSurfaceTarget() const { return m_surface_target; }
        VulkanReflectionHistoryTarget& getHistoryTarget() { return m_history_target; }
        const VulkanReflectionHistoryTarget& getHistoryTarget() const { return m_history_target; }

        void prepareHistory(
            VulkanDrawList& draw_list,
            const VulkanReflectionHistoryKey& key);
        void onFrameSubmitted();

        VulkanReflectionSurfaceFeatureGraphResources addGraphResources(VulkanPassGraph& graph);
        bool addPreparationPass(
            VulkanPassGraph& graph,
            const VulkanReflectionSurfaceFeatureGraphResources& resources,
            uint32_t frame_index);

        VulkanReflectionHistoryDebugStats buildDebugStats() const;

    private:
        struct ReflectionHistoryClearPushConstants {
            glm::uvec2 extent{ 1u, 1u };
            glm::vec2 padding{ 0.0f };
            glm::vec4 clear_value{ 0.0f };
        };
        static_assert(sizeof(ReflectionHistoryClearPushConstants) == 32);

        static constexpr uint32_t kClearDescriptorCount =
            static_cast<uint32_t>(VulkanReflectionHistoryImage::Count);

        bool createClearPipeline();
        bool allocateClearDescriptors();
        void freeClearDescriptors();
        VulkanGraphImageHandle addHistoryImage(
            VulkanPassGraph& graph,
            VulkanReflectionHistoryImage image,
            uint32_t ping_pong_index,
            const char* name);
        bool recordHistoryReset(VkCommandBuffer command_buffer, uint32_t frame_index) const;
        bool updateClearDescriptor(
            uint32_t frame_index,
            uint32_t descriptor_index,
            const VulkanImageViewState& image) const;

    private:
        VulkanRenderFeatureContext m_context;
        VulkanReflectionSurfaceTarget m_surface_target;
        VulkanReflectionHistoryTarget m_history_target;
        VulkanReflectionHistoryState m_history_state;
        VkDescriptorSetLayout m_clear_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanComputePipelineState m_clear_pipeline;
        std::array<
            std::array<VulkanDescriptorSetAllocation, kClearDescriptorCount>,
            kVulkanFramesInFlight> m_clear_descriptor_allocations;
        uint64_t m_surface_generation = 0;
        bool m_history_half_resolution = false;
        bool m_frame_active = false;
        bool m_history_write_scheduled = false;
    };
} // namespace NexAur

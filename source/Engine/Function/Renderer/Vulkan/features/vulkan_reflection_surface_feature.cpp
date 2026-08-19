#include "pch.h"
#include "vulkan_reflection_surface_feature.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"
#include "Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h"

#include <algorithm>

namespace NexAur {
    namespace {
        VkExtent2D reflectionHistoryExtent(
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
    } // namespace

    VulkanReflectionSurfaceFeature::~VulkanReflectionSurfaceFeature() {
        shutdown();
    }

    bool VulkanReflectionSurfaceFeature::init(
        const VulkanRenderFeatureContext& context,
        VkFormat reflection_surface_format,
        VkFormat fallback_specular_format,
        VkFormat motion_vector_format,
        VkFormat history_format,
        uint32_t width,
        uint32_t height) {
        shutdown();
        if (!context.valid() ||
            reflection_surface_format == VK_FORMAT_UNDEFINED ||
            fallback_specular_format == VK_FORMAT_UNDEFINED ||
            motion_vector_format == VK_FORMAT_UNDEFINED ||
            history_format == VK_FORMAT_UNDEFINED) {
            return false;
        }

        m_context = context;
        if (!m_surface_target.init(
                context.resources,
                reflection_surface_format,
                fallback_specular_format,
                motion_vector_format,
                width,
                height) ||
            !m_history_target.init(context.resources, history_format, width, height) ||
            !createClearPipeline() ||
            !allocateClearDescriptors()) {
            shutdown();
            return false;
        }

        m_surface_generation = 1;
        return true;
    }

    bool VulkanReflectionSurfaceFeature::resize(uint32_t width, uint32_t height) {
        const VkExtent2D history_extent = reflectionHistoryExtent(
            width,
            height,
            m_history_half_resolution);
        if (!m_context.valid() ||
            !m_surface_target.resize(width, height) ||
            !m_history_target.resize(history_extent.width, history_extent.height)) {
            return false;
        }

        ++m_surface_generation;
        m_history_state.reset();
        m_frame_active = false;
        m_history_write_scheduled = false;
        return true;
    }

    bool VulkanReflectionSurfaceFeature::prepareHistoryTarget(
        uint32_t width,
        uint32_t height,
        bool half_resolution) {
        if (!m_context.valid() || !m_history_target.isReady()) {
            return false;
        }

        const VkExtent2D expected_extent = reflectionHistoryExtent(
            width,
            height,
            half_resolution);
        const VkExtent2D current_extent = m_history_target.getExtent();
        if (current_extent.width == expected_extent.width &&
            current_extent.height == expected_extent.height &&
            m_history_half_resolution == half_resolution) {
            return true;
        }

        if (vkDeviceWaitIdle(m_context.resources.device) != VK_SUCCESS ||
            !m_history_target.resize(expected_extent.width, expected_extent.height)) {
            return false;
        }

        m_history_half_resolution = half_resolution;
        ++m_surface_generation;
        m_history_state.reset();
        m_frame_active = false;
        m_history_write_scheduled = false;
        return true;
    }

    void VulkanReflectionSurfaceFeature::shutdown() {
        freeClearDescriptors();
        m_clear_pipeline = {};
        m_clear_descriptor_set_layout = VK_NULL_HANDLE;
        m_history_state.reset();
        m_frame_active = false;
        m_history_write_scheduled = false;
        m_history_target.shutdown();
        m_surface_target.shutdown();
        m_surface_generation = 0;
        m_history_half_resolution = false;
        m_context = {};
    }

    bool VulkanReflectionSurfaceFeature::isReady() const {
        if (!m_context.valid() ||
            !m_surface_target.isReady() ||
            !m_history_target.isReady() ||
            !m_clear_pipeline.valid()) {
            return false;
        }

        for (const auto& frame_descriptors : m_clear_descriptor_allocations) {
            for (const VulkanDescriptorSetAllocation& allocation : frame_descriptors) {
                if (!allocation.valid()) {
                    return false;
                }
            }
        }
        return true;
    }

    void VulkanReflectionSurfaceFeature::prepareHistory(
        VulkanDrawList& draw_list,
        const VulkanReflectionHistoryKey& key) {
        m_history_state.prepareFrame(draw_list, key);
    }

    void VulkanReflectionSurfaceFeature::onFrameSubmitted() {
        if (!isReady()) {
            return;
        }
        if (m_frame_active) {
            m_history_state.onFrameSubmitted();
            if (m_history_write_scheduled) {
                m_history_target.swapPingPong();
                m_history_write_scheduled = false;
            }
        }
        m_frame_active = false;
    }

    VulkanReflectionSurfaceFeatureGraphResources
    VulkanReflectionSurfaceFeature::addGraphResources(VulkanPassGraph& graph) {
        VulkanReflectionSurfaceFeatureGraphResources resources;
        if (!isReady()) {
            return resources;
        }

        const VulkanImageViewState& reflection_surface =
            m_surface_target.getReflectionSurfaceImage();
        VulkanGraphImageDesc reflection_surface_desc;
        reflection_surface_desc.name = "ReflectionSurface";
        reflection_surface_desc.image = reflection_surface.image;
        reflection_surface_desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        reflection_surface_desc.initial_layout = reflection_surface.layout;
        reflection_surface_desc.commit_layout = [this](VkImageLayout layout) {
            m_surface_target.setReflectionSurfaceLayout(layout);
        };
        resources.reflection_surface = graph.addImage(std::move(reflection_surface_desc));

        const VulkanImageViewState& fallback_specular =
            m_surface_target.getFallbackSpecularImage();
        VulkanGraphImageDesc fallback_specular_desc;
        fallback_specular_desc.name = "FallbackSpecular";
        fallback_specular_desc.image = fallback_specular.image;
        fallback_specular_desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        fallback_specular_desc.initial_layout = fallback_specular.layout;
        fallback_specular_desc.commit_layout = [this](VkImageLayout layout) {
            m_surface_target.setFallbackSpecularLayout(layout);
        };
        resources.fallback_specular = graph.addImage(std::move(fallback_specular_desc));

        const VulkanImageViewState& motion_vector =
            m_surface_target.getMotionVectorImage();
        VulkanGraphImageDesc motion_vector_desc;
        motion_vector_desc.name = "MotionVector";
        motion_vector_desc.image = motion_vector.image;
        motion_vector_desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        motion_vector_desc.initial_layout = motion_vector.layout;
        motion_vector_desc.commit_layout = [this](VkImageLayout layout) {
            m_surface_target.setMotionVectorLayout(layout);
        };
        resources.motion_vector = graph.addImage(std::move(motion_vector_desc));

        resources.raw_reflection = addHistoryImage(
            graph,
            VulkanReflectionHistoryImage::RawReflection,
            0,
            "ReflectionRaw");
        resources.hit_distance = addHistoryImage(
            graph,
            VulkanReflectionHistoryImage::HitDistance,
            0,
            "ReflectionHitDistance");
        const uint32_t read_index = m_history_target.getReadIndex();
        const uint32_t write_index = m_history_target.getWriteIndex();
        resources.filtered_radiance_read = addHistoryImage(
            graph,
            VulkanReflectionHistoryImage::FilteredRadiance,
            read_index,
            "ReflectionFilteredRadianceRead");
        resources.filtered_radiance_write = addHistoryImage(
            graph,
            VulkanReflectionHistoryImage::FilteredRadiance,
            write_index,
            "ReflectionFilteredRadianceWrite");
        resources.moments_read = addHistoryImage(
            graph,
            VulkanReflectionHistoryImage::Moments,
            read_index,
            "ReflectionMomentsRead");
        resources.moments_write = addHistoryImage(
            graph,
            VulkanReflectionHistoryImage::Moments,
            write_index,
            "ReflectionMomentsWrite");
        resources.history_length_read = addHistoryImage(
            graph,
            VulkanReflectionHistoryImage::HistoryLength,
            read_index,
            "ReflectionHistoryLengthRead");
        resources.history_length_write = addHistoryImage(
            graph,
            VulkanReflectionHistoryImage::HistoryLength,
            write_index,
            "ReflectionHistoryLengthWrite");
        resources.depth_read = addHistoryImage(
            graph,
            VulkanReflectionHistoryImage::Depth,
            read_index,
            "ReflectionDepthRead");
        resources.depth_write = addHistoryImage(
            graph,
            VulkanReflectionHistoryImage::Depth,
            write_index,
            "ReflectionDepthWrite");
        resources.surface_read = addHistoryImage(
            graph,
            VulkanReflectionHistoryImage::Surface,
            read_index,
            "ReflectionSurfaceHistoryRead");
        resources.surface_write = addHistoryImage(
            graph,
            VulkanReflectionHistoryImage::Surface,
            write_index,
            "ReflectionSurfaceHistoryWrite");
        return resources;
    }

    bool VulkanReflectionSurfaceFeature::addPreparationPass(
        VulkanPassGraph& graph,
        const VulkanReflectionSurfaceFeatureGraphResources& resources,
        uint32_t frame_index) {
        if (!isReady()) {
            return false;
        }
        if (!resources.valid()) {
            return false;
        }
        m_frame_active = true;
        if (!m_history_state.isPendingReset()) {
            return true;
        }

        graph.addPass("ReflectionHistoryReset")
            .writeImage(resources.raw_reflection, VulkanGraphImageUsage::ComputeStorageWrite)
            .writeImage(resources.hit_distance, VulkanGraphImageUsage::ComputeStorageWrite)
            .writeImage(resources.filtered_radiance_write, VulkanGraphImageUsage::ComputeStorageWrite)
            .writeImage(resources.moments_write, VulkanGraphImageUsage::ComputeStorageWrite)
            .writeImage(resources.history_length_write, VulkanGraphImageUsage::ComputeStorageWrite)
            .writeImage(resources.depth_write, VulkanGraphImageUsage::ComputeStorageWrite)
            .writeImage(resources.surface_write, VulkanGraphImageUsage::ComputeStorageWrite)
            .execute([this, frame_index](VkCommandBuffer command_buffer) {
                return recordHistoryReset(command_buffer, frame_index);
            });
        m_history_write_scheduled = true;
        return true;
    }

    VulkanReflectionHistoryDebugStats
    VulkanReflectionSurfaceFeature::buildDebugStats() const {
        VulkanReflectionHistoryDebugStats stats;
        stats.ready = isReady();
        stats.valid = m_history_state.isValid();
        stats.pending_reset = m_history_state.isPendingReset();
        stats.read_index = m_history_target.getReadIndex();
        stats.write_index = m_history_target.getWriteIndex();
        stats.surface_generation = m_surface_generation;
        stats.reset_reason = m_history_state.getLastResetReason();
        if (m_history_target.isReady()) {
            const VkExtent2D extent = m_history_target.getExtent();
            stats.width = extent.width;
            stats.height = extent.height;
        }
        return stats;
    }

    bool VulkanReflectionSurfaceFeature::createClearPipeline() {
        m_clear_descriptor_set_layout =
            m_context.descriptor_layout_cache->getBuiltinLayout(
                VulkanDescriptorSetLayoutId::ReflectionStorageImage);
        if (m_clear_descriptor_set_layout == VK_NULL_HANDLE) {
            return false;
        }

        VulkanComputePipelineDesc desc;
        desc.debug_name = "ReflectionHistoryClear";
        desc.shader_program = VulkanShaderProgramId::ReflectionHistoryClear;
        desc.descriptor_set_layouts = { m_clear_descriptor_set_layout };
        VkPushConstantRange push_constant_range{};
        push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant_range.size = sizeof(ReflectionHistoryClearPushConstants);
        desc.push_constant_ranges = { push_constant_range };
        m_clear_pipeline = m_context.pipeline_cache->getOrCreateComputePipeline(desc);
        return m_clear_pipeline.valid();
    }

    bool VulkanReflectionSurfaceFeature::allocateClearDescriptors() {
        for (auto& frame_descriptors : m_clear_descriptor_allocations) {
            for (VulkanDescriptorSetAllocation& allocation : frame_descriptors) {
                allocation = m_context.descriptor_allocator->allocate(
                    m_clear_descriptor_set_layout);
                if (!allocation.valid()) {
                    freeClearDescriptors();
                    return false;
                }
            }
        }
        return true;
    }

    void VulkanReflectionSurfaceFeature::freeClearDescriptors() {
        if (m_context.descriptor_allocator == nullptr) {
            return;
        }
        for (auto& frame_descriptors : m_clear_descriptor_allocations) {
            for (VulkanDescriptorSetAllocation& allocation : frame_descriptors) {
                if (allocation.valid()) {
                    m_context.descriptor_allocator->free(allocation);
                }
                allocation = {};
            }
        }
    }

    VulkanGraphImageHandle VulkanReflectionSurfaceFeature::addHistoryImage(
        VulkanPassGraph& graph,
        VulkanReflectionHistoryImage image,
        uint32_t ping_pong_index,
        const char* name) {
        VulkanGraphImageHandle handle;
        const VulkanImageViewState* view = m_history_target.getImage(image, ping_pong_index);
        if (view == nullptr) {
            return handle;
        }

        VulkanGraphImageDesc desc;
        desc.name = name != nullptr ? name : "ReflectionHistoryImage";
        desc.image = view->image;
        desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.initial_layout = view->layout;
        desc.commit_layout = [this, image, ping_pong_index](VkImageLayout layout) {
            m_history_target.setImageLayout(image, ping_pong_index, layout);
        };
        return graph.addImage(std::move(desc));
    }

    bool VulkanReflectionSurfaceFeature::recordHistoryReset(
        VkCommandBuffer command_buffer,
        uint32_t frame_index) const {
        if (command_buffer == VK_NULL_HANDLE ||
            frame_index >= m_clear_descriptor_allocations.size()) {
            return false;
        }

        const VkExtent2D extent = m_history_target.getExtent();
        const uint32_t write_index = m_history_target.getWriteIndex();
        const VulkanImageViewState* raw =
            m_history_target.getImage(VulkanReflectionHistoryImage::RawReflection);
        const VulkanImageViewState* hit_distance =
            m_history_target.getImage(VulkanReflectionHistoryImage::HitDistance);
        const VulkanImageViewState* filtered =
            m_history_target.getImage(VulkanReflectionHistoryImage::FilteredRadiance, write_index);
        const VulkanImageViewState* moments =
            m_history_target.getImage(VulkanReflectionHistoryImage::Moments, write_index);
        const VulkanImageViewState* history_length =
            m_history_target.getImage(VulkanReflectionHistoryImage::HistoryLength, write_index);
        const VulkanImageViewState* depth =
            m_history_target.getImage(VulkanReflectionHistoryImage::Depth, write_index);
        const VulkanImageViewState* surface =
            m_history_target.getImage(VulkanReflectionHistoryImage::Surface, write_index);
        if (!raw || !hit_distance || !filtered || !moments || !history_length || !depth || !surface) {
            return false;
        }

        const std::array<const VulkanImageViewState*, kClearDescriptorCount> shader_targets{
            raw,
            hit_distance,
            filtered,
            moments,
            history_length,
            depth,
            surface
        };
        for (uint32_t index = 0; index < kClearDescriptorCount; ++index) {
            if (!updateClearDescriptor(frame_index, index, *shader_targets[index])) {
                return false;
            }
        }

        vkCmdBindPipeline(
            command_buffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_clear_pipeline.pipeline);
        const uint32_t group_count_x = (extent.width + 7u) / 8u;
        const uint32_t group_count_y = (extent.height + 7u) / 8u;
        ReflectionHistoryClearPushConstants constants;
        constants.extent = { extent.width, extent.height };

        for (uint32_t index = 0; index < kClearDescriptorCount; ++index) {
            const VkDescriptorSet descriptor_set =
                m_clear_descriptor_allocations[frame_index][index].set;
            vkCmdBindDescriptorSets(
                command_buffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                m_clear_pipeline.layout,
                0,
                1,
                &descriptor_set,
                0,
                nullptr);
            vkCmdPushConstants(
                command_buffer,
                m_clear_pipeline.layout,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                sizeof(constants),
                &constants);
            vkCmdDispatch(command_buffer, group_count_x, group_count_y, 1);
        }
        return true;
    }

    bool VulkanReflectionSurfaceFeature::updateClearDescriptor(
        uint32_t frame_index,
        uint32_t descriptor_index,
        const VulkanImageViewState& image) const {
        if (frame_index >= m_clear_descriptor_allocations.size() ||
            descriptor_index >= kClearDescriptorCount ||
            image.view == VK_NULL_HANDLE) {
            return false;
        }

        VkDescriptorImageInfo image_info{};
        image_info.imageView = image.view;
        image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VulkanDescriptorWriter()
            .writeImage(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, image_info)
            .update(
                m_context.resources.device,
                m_clear_descriptor_allocations[frame_index][descriptor_index].set);
        return true;
    }
} // namespace NexAur

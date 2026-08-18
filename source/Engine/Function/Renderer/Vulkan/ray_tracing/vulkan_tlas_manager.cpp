#include "pch.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_tlas_manager.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_state_planner.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace NexAur {
    namespace {
        struct TlasInstanceBuild {
            VkAccelerationStructureInstanceKHR instance{};
            const VulkanAccelerationStructure* acceleration_structure = nullptr;
        };

        constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ull;
        constexpr uint64_t kFnvPrime = 1099511628211ull;

        void hashBytes(
            uint64_t& hash,
            const void* data,
            size_t size) {
            const auto* bytes = static_cast<const uint8_t*>(data);
            for (size_t index = 0; index < size; ++index) {
                hash ^= bytes[index];
                hash *= kFnvPrime;
            }
        }

        std::pair<uint64_t, uint64_t> hashTlasInstances(
            std::span<const VkAccelerationStructureInstanceKHR> instances) {
            uint64_t topology_hash = kFnvOffsetBasis;
            uint64_t content_hash = kFnvOffsetBasis;
            for (const VkAccelerationStructureInstanceKHR& instance : instances) {
                hashBytes(
                    topology_hash,
                    &instance.accelerationStructureReference,
                    sizeof(instance.accelerationStructureReference));
                const uint32_t topology_values[] = {
                    instance.instanceCustomIndex,
                    instance.mask,
                    instance.instanceShaderBindingTableRecordOffset,
                    instance.flags
                };
                hashBytes(
                    topology_hash,
                    topology_values,
                    sizeof(topology_values));
                hashBytes(content_hash, &instance, sizeof(instance));
            }
            return { topology_hash, content_hash };
        }

        void recordTlasDependency(
            VkCommandBuffer command_buffer,
            VulkanGraphAccelerationStructureUsage destination_usage) {
            const VulkanGraphAccelerationStructureTransitionPlan transition =
                VulkanGraphStatePlanner::planAccelerationStructureTransition(
                    VulkanGraphStatePlanner::stateForAccelerationStructureUsage(
                        VulkanGraphAccelerationStructureUsage::BuildWrite,
                        VulkanGraphAccessType::Write),
                    VulkanGraphStatePlanner::stateForAccelerationStructureUsage(
                        destination_usage,
                        VulkanGraphAccessType::Read));
            if (!transition.requires_barrier) {
                return;
            }

            VkMemoryBarrier2 barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            barrier.srcStageMask = transition.source.stage;
            barrier.srcAccessMask = transition.source.access;
            barrier.dstStageMask = transition.destination.stage;
            barrier.dstAccessMask = transition.destination.access;

            VkDependencyInfo dependency_info{};
            dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependency_info.memoryBarrierCount = 1;
            dependency_info.pMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        }

        bool submitTlasBuild(
            const VulkanResourceContext& context,
            VkCommandPool command_pool,
            VkBuffer instance_buffer,
            VkDeviceSize instance_buffer_size,
            VulkanAccelerationStructure& acceleration_structure,
            VkAccelerationStructureBuildGeometryInfoKHR build_info,
            const VkAccelerationStructureBuildRangeInfoKHR& build_range,
            VulkanGpuTimestampQuery& gpu_timestamp_query) {
            VkCommandBuffer command_buffer = VK_NULL_HANDLE;
            VkFence fence = VK_NULL_HANDLE;
            auto cleanup = [&]() {
                if (fence != VK_NULL_HANDLE) {
                    vkDestroyFence(context.device, fence, nullptr);
                }
                if (command_buffer != VK_NULL_HANDLE) {
                    vkFreeCommandBuffers(
                        context.device,
                        command_pool,
                        1,
                        &command_buffer);
                }
            };

            VkCommandBufferAllocateInfo allocate_info{};
            allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocate_info.commandPool = command_pool;
            allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocate_info.commandBufferCount = 1;
            if (!VulkanDiagnosticsCollector::checkVk(
                    vkAllocateCommandBuffers(
                        context.device,
                        &allocate_info,
                        &command_buffer),
                    "vkAllocateCommandBuffers(TLAS)")) {
                cleanup();
                return false;
            }
            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (!VulkanDiagnosticsCollector::checkVk(
                    vkBeginCommandBuffer(command_buffer, &begin_info),
                    "vkBeginCommandBuffer(TLAS)")) {
                cleanup();
                return false;
            }
            const bool timing_recorded =
                gpu_timestamp_query.begin(command_buffer);

            const VulkanGraphBufferTransitionPlan instance_transition =
                VulkanGraphStatePlanner::planBufferTransition(
                    VulkanGraphStatePlanner::stateForBufferImport(
                        VK_PIPELINE_STAGE_2_HOST_BIT,
                        VK_ACCESS_2_HOST_WRITE_BIT,
                        VulkanGraphAccessType::Write),
                    VulkanGraphStatePlanner::stateForBufferUsage(
                        VulkanGraphBufferUsage::AccelerationStructureBuildInput,
                        VulkanGraphAccessType::Read));
            VkBufferMemoryBarrier2 instance_barrier{};
            instance_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            instance_barrier.srcStageMask = instance_transition.source.stage;
            instance_barrier.srcAccessMask = instance_transition.source.access;
            instance_barrier.dstStageMask = instance_transition.destination.stage;
            instance_barrier.dstAccessMask = instance_transition.destination.access;
            instance_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            instance_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            instance_barrier.buffer = instance_buffer;
            instance_barrier.offset = 0;
            instance_barrier.size = instance_buffer_size;

            VkDependencyInfo instance_dependency{};
            instance_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            instance_dependency.bufferMemoryBarrierCount = 1;
            instance_dependency.pBufferMemoryBarriers = &instance_barrier;
            vkCmdPipelineBarrier2(command_buffer, &instance_dependency);

            recordTlasDependency(
                command_buffer,
                VulkanGraphAccelerationStructureUsage::BuildInput);

            if (!acceleration_structure.recordBuild(
                    command_buffer,
                    build_info,
                    std::span<const VkAccelerationStructureBuildRangeInfoKHR>(&build_range, 1),
                    build_info.scratchData.deviceAddress)) {
                if (timing_recorded) {
                    gpu_timestamp_query.discard();
                }
                cleanup();
                return false;
            }
            recordTlasDependency(
                command_buffer,
                VulkanGraphAccelerationStructureUsage::RayQueryShaderRead);
            if (timing_recorded &&
                !gpu_timestamp_query.end(command_buffer)) {
                gpu_timestamp_query.discard();
            }

            if (!VulkanDiagnosticsCollector::checkVk(
                    vkEndCommandBuffer(command_buffer),
                    "vkEndCommandBuffer(TLAS)")) {
                if (timing_recorded) {
                    gpu_timestamp_query.discard();
                }
                cleanup();
                return false;
            }
            VkFenceCreateInfo fence_info{};
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            if (!VulkanDiagnosticsCollector::checkVk(
                    vkCreateFence(
                        context.device,
                        &fence_info,
                        nullptr,
                        &fence),
                    "vkCreateFence(TLAS)")) {
                if (timing_recorded) {
                    gpu_timestamp_query.discard();
                }
                cleanup();
                return false;
            }

            VkSubmitInfo submit_info{};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &command_buffer;
            const bool submitted = VulkanDiagnosticsCollector::checkVk(
                vkQueueSubmit(
                    context.graphics_queue,
                    1,
                    &submit_info,
                    fence),
                "vkQueueSubmit(TLAS)");
            const bool completed = submitted && VulkanDiagnosticsCollector::checkVk(
                vkWaitForFences(
                    context.device,
                    1,
                    &fence,
                    VK_TRUE,
                    UINT64_MAX),
                "vkWaitForFences(TLAS)");
            if (completed && timing_recorded) {
                gpu_timestamp_query.resolve();
            } else if (!submitted && timing_recorded) {
                gpu_timestamp_query.discard();
            }
            cleanup();
            return completed;
        }
    } // namespace

    uint32_t growVulkanTlasInstanceCapacity(
        uint32_t required_count,
        uint32_t current_capacity) {
        if (required_count == 0) {
            return 0;
        }

        uint32_t capacity = std::max(1u, current_capacity);
        while (capacity < required_count) {
            if (capacity > std::numeric_limits<uint32_t>::max() / 2u) {
                return required_count;
            }
            capacity *= 2u;
        }
        return capacity;
    }

    VulkanTlasBuildMode chooseVulkanTlasBuildMode(
        const VulkanTlasBuildState& previous,
        uint32_t instance_count,
        uint64_t topology_hash,
        uint64_t content_hash) {
        if (previous.ready &&
            previous.instance_count == instance_count &&
            previous.content_hash == content_hash) {
            return VulkanTlasBuildMode::Reuse;
        }
        if (previous.ready &&
            previous.update_capable &&
            previous.instance_count == instance_count &&
            previous.instance_capacity >= instance_count &&
            previous.topology_hash == topology_hash) {
            return VulkanTlasBuildMode::Update;
        }
        return VulkanTlasBuildMode::Build;
    }

    const char* vulkanTlasBuildModeName(VulkanTlasBuildMode mode) {
        switch (mode) {
        case VulkanTlasBuildMode::Build:
            return "Build";
        case VulkanTlasBuildMode::Update:
            return "Update";
        case VulkanTlasBuildMode::Reuse:
            return "Reuse";
        }
        return "Unknown";
    }

    VkTransformMatrixKHR toVulkanTransformMatrix(const glm::mat4& transform) {
        VkTransformMatrixKHR result{};
        for (uint32_t row = 0; row < 3; ++row) {
            for (uint32_t column = 0; column < 4; ++column) {
                result.matrix[row][column] = transform[column][row];
            }
        }
        return result;
    }

    VulkanTlasManager::~VulkanTlasManager() {
        shutdown();
    }

    bool VulkanTlasManager::init(
        const VulkanResourceContext& context,
        const VulkanRayTracingDeviceFunctions& functions,
        VkDeviceSize scratch_alignment) {
        shutdown();
        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() ||
            !context.buffer_device_address_enabled ||
            !functions.valid() ||
            scratch_alignment == 0 ||
            (scratch_alignment & (scratch_alignment - 1)) != 0) {
            m_stats.last_failure_reason =
                "TLAS manager received an invalid Ray Query context.";
            NX_CORE_ERROR("TLAS manager requires a valid Ray Query context.");
            return false;
        }

        m_context = context;
        m_functions = functions;
        m_scratch_alignment = scratch_alignment;
        if (!createCommandPool()) {
            shutdown();
            m_stats.last_failure_reason = "TLAS command pool creation failed.";
            return false;
        }
        if (!m_gpu_timestamp_query.init(
                context.physical_device,
                context.device,
                context.graphics_queue_family)) {
            NX_CORE_WARN("TLAS GPU timing is unavailable.");
        }
        m_stats = {};
        m_stats.initialized = true;
        m_stats.gpu_timing_supported =
            m_gpu_timestamp_query.isSupported();
        m_stats.last_failure_reason = "None";
        m_initialized = true;
        return true;
    }

    void VulkanTlasManager::clear() {
        for (FrameSlot& slot : m_frame_slots) {
            resetSlot(slot);
        }
        m_scratch_buffer.reset();
        m_stats = {};
        m_stats.last_failure_reason = "None";
        m_stats.initialized = m_initialized;
    }

    void VulkanTlasManager::shutdown() {
        clear();
        m_gpu_timestamp_query.shutdown();
        if (m_context.device != VK_NULL_HANDLE &&
            m_command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_context.device, m_command_pool, nullptr);
        }
        m_context = {};
        m_functions.reset();
        m_command_pool = VK_NULL_HANDLE;
        m_scratch_alignment = 1;
        m_stats = {};
        m_stats.last_failure_reason = "None";
        m_initialized = false;
    }

    bool VulkanTlasManager::buildFrame(
        uint32_t frame_index,
        std::span<const VulkanMeshDrawItem> opaque_items,
        const VulkanStaticMeshBlasCache& blas_cache) {
        if (!m_initialized || frame_index >= m_frame_slots.size()) {
            setFailure("TLAS build requested for an invalid frame slot.");
            return false;
        }

        FrameSlot& slot = m_frame_slots[frame_index];
        m_stats.ready = false;
        m_stats.frame_index = frame_index;
        VulkanRayTracingInstanceBuildStats instance_build_stats;
        slot.accepted_instances = buildVulkanRayTracingInstanceRecords(
            opaque_items,
            blas_cache,
            &instance_build_stats);
        if (slot.accepted_instances.size() > kVulkanRtMaxInstanceCustomIndex) {
            const size_t overflow_count =
                slot.accepted_instances.size() - kVulkanRtMaxInstanceCustomIndex;
            slot.accepted_instances.resize(kVulkanRtMaxInstanceCustomIndex);
            instance_build_stats.skipped_transform_count += static_cast<uint32_t>(
                std::min<size_t>(
                    overflow_count,
                    std::numeric_limits<uint32_t>::max() -
                        instance_build_stats.skipped_transform_count));
        }
        m_stats.source_instance_count = instance_build_stats.source_instance_count;
        m_stats.built_instance_count = 0;
        m_stats.skipped_blas_count = instance_build_stats.skipped_blas_count;
        m_stats.skipped_transform_count = instance_build_stats.skipped_transform_count;
        m_stats.skipped_material_count = instance_build_stats.skipped_material_count;
        m_stats.instance_buffer_bytes = 0;
        m_stats.instance_buffer_capacity_bytes = 0;
        m_stats.acceleration_structure_bytes = 0;
        m_stats.scratch_capacity_bytes = m_scratch_buffer.getCapacity();
        m_stats.instance_capacity = 0;
        m_stats.last_build_mode = "None";
        m_stats.last_failure_reason = "None";

        std::vector<TlasInstanceBuild> instances;
        instances.reserve(slot.accepted_instances.size());
        for (size_t record_index = 0;
             record_index < slot.accepted_instances.size();
             ++record_index) {
            const VulkanRayTracingInstanceRecord& record =
                slot.accepted_instances[record_index];
            TlasInstanceBuild instance_build;
            instance_build.acceleration_structure = record.blas;
            instance_build.instance.transform = toVulkanTransformMatrix(record.transform);
            instance_build.instance.instanceCustomIndex =
                getVulkanRtInstanceTableIndex(static_cast<uint32_t>(record_index));
            instance_build.instance.mask = 0xffu;
            instance_build.instance.instanceShaderBindingTableRecordOffset = 0;
            instance_build.instance.flags =
                VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance_build.instance.accelerationStructureReference =
                record.blas->getDeviceAddress();
            instances.push_back(instance_build);
        }

        if (instances.empty()) {
            resetSlot(slot);
            return true;
        }

        std::vector<VkAccelerationStructureInstanceKHR> instance_data;
        instance_data.reserve(instances.size());
        for (const TlasInstanceBuild& instance : instances) {
            instance_data.push_back(instance.instance);
        }
        const auto [topology_hash, content_hash] =
            hashTlasInstances(instance_data);
        VulkanTlasBuildMode build_mode = chooseVulkanTlasBuildMode(
            slot.buildState(),
            static_cast<uint32_t>(instance_data.size()),
            topology_hash,
            content_hash);

        const VkDeviceSize instance_buffer_size =
            static_cast<VkDeviceSize>(instance_data.size() *
                                      sizeof(VkAccelerationStructureInstanceKHR));
        if (build_mode == VulkanTlasBuildMode::Reuse) {
            slot.ready = true;
            m_stats.ready = true;
            m_stats.built_instance_count = slot.instance_count;
            m_stats.instance_buffer_bytes = instance_buffer_size;
            m_stats.instance_buffer_capacity_bytes =
                slot.instance_buffer.getSize();
            m_stats.acceleration_structure_bytes =
                slot.acceleration_structure.getSize();
            m_stats.scratch_capacity_bytes = m_scratch_buffer.getCapacity();
            m_stats.instance_capacity = slot.instance_capacity;
            m_stats.last_build_mode = vulkanTlasBuildModeName(build_mode);
            ++m_stats.reuse_count;
            return true;
        }

        const uint32_t instance_capacity = growVulkanTlasInstanceCapacity(
            static_cast<uint32_t>(instance_data.size()),
            slot.instance_capacity);
        if (instance_capacity == 0) {
            setFailure("TLAS instance capacity growth failed.");
            resetSlot(slot);
            return false;
        }
        const VkDeviceSize instance_buffer_capacity_size =
            static_cast<VkDeviceSize>(instance_capacity) *
            sizeof(VkAccelerationStructureInstanceKHR);
        slot.ready = false;
        if (!ensureInstanceBuffer(
                slot,
                instance_buffer_capacity_size,
                "TLAS instance buffer")) {
            setFailure("TLAS instance buffer allocation failed.");
            resetSlot(slot);
            return false;
        }
        void* mapped_data = nullptr;
        if (!slot.instance_buffer.map(mapped_data) || mapped_data == nullptr) {
            setFailure("TLAS instance buffer mapping failed.");
            resetSlot(slot);
            return false;
        }
        std::memcpy(
            mapped_data,
            instance_data.data(),
            static_cast<size_t>(instance_buffer_size));
        const bool flushed = slot.instance_buffer.flush(0, instance_buffer_size);
        slot.instance_buffer.unmap();
        if (!flushed) {
            setFailure("TLAS instance buffer flush failed.");
            resetSlot(slot);
            return false;
        }

        VkAccelerationStructureBuildGeometryInfoKHR build_info{};
        build_info.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        build_info.flags =
            VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
            VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build_info.geometryCount = 1;

        VkAccelerationStructureGeometryInstancesDataKHR instances_data{};
        instances_data.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        instances_data.arrayOfPointers = VK_FALSE;
        instances_data.data.deviceAddress =
            slot.instance_buffer.getDeviceAddress();

        VkAccelerationStructureGeometryKHR geometry{};
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances = instances_data;
        build_info.pGeometries = &geometry;

        const std::array<uint32_t, 1> primitive_counts{
            instance_capacity
        };
        VulkanAccelerationStructureBuildSizes build_sizes;
        if (!queryVulkanAccelerationStructureBuildSizes(
                m_context.device,
                m_functions,
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                build_info,
                primitive_counts,
                build_sizes)) {
            setFailure("TLAS build-size query failed.");
            resetSlot(slot);
            return false;
        }
        VulkanAccelerationStructure replacement;
        VulkanAccelerationStructure* target = &slot.acceleration_structure;
        bool allocated_replacement = false;
        if (!slot.acceleration_structure.isReady() ||
            slot.acceleration_structure.getSize() <
                build_sizes.acceleration_structure_size) {
            build_mode = VulkanTlasBuildMode::Build;
            VulkanAccelerationStructureCreateInfo create_info;
            create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            create_info.size = build_sizes.acceleration_structure_size;
            create_info.debug_name =
                "TLAS frame " + std::to_string(frame_index);
            if (!replacement.create(
                    *m_context.gpu_allocator,
                    m_functions,
                    create_info)) {
                setFailure("TLAS acceleration structure creation failed.");
                resetSlot(slot);
                return false;
            }
            target = &replacement;
            allocated_replacement = true;
        }
        if (build_mode == VulkanTlasBuildMode::Update &&
            build_sizes.update_scratch_size > 0) {
            build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
            build_info.srcAccelerationStructure = target->get();
        } else {
            build_mode = VulkanTlasBuildMode::Build;
            build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            build_info.srcAccelerationStructure = VK_NULL_HANDLE;
        }

        const VkDeviceSize required_scratch_size =
            build_mode == VulkanTlasBuildMode::Update ?
                build_sizes.update_scratch_size :
                build_sizes.build_scratch_size;
        if (!m_scratch_buffer.ensureCapacity(
                *m_context.gpu_allocator,
                required_scratch_size,
                m_scratch_alignment,
                "TLAS build scratch")) {
            setFailure("TLAS scratch buffer allocation failed.");
            resetSlot(slot);
            return false;
        }
        build_info.scratchData.deviceAddress =
            m_scratch_buffer.getDeviceAddress();
        build_info.dstAccelerationStructure = target->get();

        VkAccelerationStructureBuildRangeInfoKHR build_range{};
        build_range.primitiveCount = static_cast<uint32_t>(instance_data.size());
        const uint64_t timing_sample_count =
            m_gpu_timestamp_query.getStats().sample_count;
        if (!submitTlasBuild(
                m_context,
                m_command_pool,
                slot.instance_buffer.get(),
                instance_buffer_size,
                *target,
                build_info,
                build_range,
                m_gpu_timestamp_query)) {
            setFailure("TLAS build submission failed.");
            resetSlot(slot);
            return false;
        }

        if (target == &replacement) {
            slot.acceleration_structure = std::move(replacement);
        }
        slot.instance_count = static_cast<uint32_t>(instance_data.size());
        slot.instance_capacity = instance_capacity;
        slot.topology_hash = topology_hash;
        slot.content_hash = content_hash;
        slot.update_capable = true;
        slot.ready = true;
        m_stats.ready = true;
        m_stats.built_instance_count = slot.instance_count;
        m_stats.instance_buffer_bytes = instance_buffer_size;
        m_stats.instance_buffer_capacity_bytes =
            slot.instance_buffer.getSize();
        m_stats.acceleration_structure_bytes = slot.acceleration_structure.getSize();
        m_stats.scratch_capacity_bytes = m_scratch_buffer.getCapacity();
        m_stats.instance_capacity = slot.instance_capacity;
        m_stats.last_build_mode = vulkanTlasBuildModeName(build_mode);
        const VulkanGpuTimestampQueryStats timing_stats =
            m_gpu_timestamp_query.getStats();
        m_stats.gpu_timing_supported = timing_stats.supported;
        if (timing_stats.sample_count > timing_sample_count) {
            m_stats.last_build_gpu_ms = timing_stats.last_duration_ms;
        }
        ++m_stats.build_count;
        if (build_mode == VulkanTlasBuildMode::Update) {
            ++m_stats.update_count;
        } else {
            ++m_stats.rebuild_count;
        }
        if (allocated_replacement) {
            ++m_stats.allocation_count;
        }
        return true;
    }

    const VulkanAccelerationStructure* VulkanTlasManager::get(
        uint32_t frame_index) const {
        if (frame_index >= m_frame_slots.size()) {
            return nullptr;
        }
        const FrameSlot& slot = m_frame_slots[frame_index];
        return slot.ready && slot.acceleration_structure.isReady() ?
            &slot.acceleration_structure : nullptr;
    }

    std::span<const VulkanRayTracingInstanceRecord>
    VulkanTlasManager::getAcceptedInstanceRecords(uint32_t frame_index) const {
        if (frame_index >= m_frame_slots.size()) {
            return {};
        }
        const FrameSlot& slot = m_frame_slots[frame_index];
        return slot.ready ?
            std::span<const VulkanRayTracingInstanceRecord>(slot.accepted_instances) :
            std::span<const VulkanRayTracingInstanceRecord>{};
    }

    VulkanTlasBuildStats VulkanTlasManager::getStats() const {
        VulkanTlasBuildStats stats = m_stats;
        stats.initialized = m_initialized;
        stats.gpu_timing_supported =
            m_gpu_timestamp_query.isSupported();
        stats.gpu_timing_sample_count =
            m_gpu_timestamp_query.getStats().sample_count;
        stats.scratch_capacity_bytes = m_scratch_buffer.getCapacity();
        return stats;
    }

    bool VulkanTlasManager::createCommandPool() {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool_info.queueFamilyIndex = m_context.graphics_queue_family;
        return VulkanDiagnosticsCollector::checkVk(
            vkCreateCommandPool(
                m_context.device,
                &pool_info,
                nullptr,
                &m_command_pool),
            "vkCreateCommandPool(TLAS)");
    }

    bool VulkanTlasManager::ensureInstanceBuffer(
        FrameSlot& slot,
        VkDeviceSize required_size,
        const char* debug_name) {
        if (slot.instance_buffer.isReady() &&
            slot.instance_buffer.getSize() >= required_size &&
            slot.instance_buffer.getDeviceAddress() % 16u == 0) {
            return true;
        }

        VulkanOwnedBufferCreateInfo create_info;
        create_info.size = required_size;
        create_info.usage =
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        create_info.memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        create_info.allocation_flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        create_info.minimum_alignment = 16;
        create_info.debug_name = debug_name != nullptr && debug_name[0] != '\0' ?
            debug_name : "TLAS instance buffer";

        VulkanOwnedBuffer replacement;
        if (!replacement.create(*m_context.gpu_allocator, create_info)) {
            return false;
        }
        slot.instance_buffer = std::move(replacement);
        return true;
    }

    void VulkanTlasManager::resetSlot(FrameSlot& slot) {
        slot.ready = false;
        slot.instance_count = 0;
        slot.instance_capacity = 0;
        slot.topology_hash = 0;
        slot.content_hash = 0;
        slot.update_capable = false;
        slot.accepted_instances.clear();
        slot.acceleration_structure.reset();
        slot.instance_buffer.reset();
    }

    void VulkanTlasManager::setFailure(std::string failure_reason) {
        m_stats.ready = false;
        m_stats.last_failure_reason = std::move(failure_reason);
        NX_CORE_WARN("{}", m_stats.last_failure_reason);
    }
} // namespace NexAur

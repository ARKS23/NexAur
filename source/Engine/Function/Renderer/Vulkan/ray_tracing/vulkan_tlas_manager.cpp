#include "pch.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_tlas_manager.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

namespace NexAur {
    namespace {
        constexpr uint32_t kInstanceCustomIndexMask = 0x00ffffffu;

        struct TlasInstanceBuild {
            VkAccelerationStructureInstanceKHR instance{};
            const VulkanAccelerationStructure* acceleration_structure = nullptr;
        };

        bool isFiniteAffineTransform(const glm::mat4& transform) {
            for (int column = 0; column < 4; ++column) {
                for (int row = 0; row < 4; ++row) {
                    if (!std::isfinite(transform[column][row])) {
                        return false;
                    }
                }
            }

            constexpr float epsilon = 0.0001f;
            return std::abs(transform[0][3]) <= epsilon &&
                   std::abs(transform[1][3]) <= epsilon &&
                   std::abs(transform[2][3]) <= epsilon &&
                   std::abs(transform[3][3] - 1.0f) <= epsilon;
        }

        void recordTlasDependency(
            VkCommandBuffer command_buffer,
            VkPipelineStageFlags2 destination_stage,
            VkAccessFlags2 destination_access) {
            VkMemoryBarrier2 barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            barrier.srcStageMask =
                VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
            barrier.srcAccessMask =
                VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            barrier.dstStageMask = destination_stage;
            barrier.dstAccessMask = destination_access;

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
            const VkAccelerationStructureBuildRangeInfoKHR& build_range) {
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

            VkBufferMemoryBarrier2 instance_barrier{};
            instance_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            instance_barrier.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
            instance_barrier.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
            instance_barrier.dstStageMask =
                VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
            instance_barrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
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
                VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR);

            if (!acceleration_structure.recordBuild(
                    command_buffer,
                    build_info,
                    std::span<const VkAccelerationStructureBuildRangeInfoKHR>(&build_range, 1),
                    build_info.scratchData.deviceAddress)) {
                cleanup();
                return false;
            }

            recordTlasDependency(
                command_buffer,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR);

            if (!VulkanDiagnosticsCollector::checkVk(
                    vkEndCommandBuffer(command_buffer),
                    "vkEndCommandBuffer(TLAS)")) {
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
            cleanup();
            return completed;
        }
    } // namespace

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
        m_stats = {};
        m_stats.initialized = true;
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
        slot.ready = false;
        slot.instance_count = 0;
        m_stats.ready = false;
        m_stats.frame_index = frame_index;
        m_stats.source_instance_count = static_cast<uint32_t>(
            std::min<size_t>(opaque_items.size(), std::numeric_limits<uint32_t>::max()));
        m_stats.built_instance_count = 0;
        m_stats.skipped_blas_count = 0;
        m_stats.skipped_transform_count = 0;
        m_stats.instance_buffer_bytes = 0;
        m_stats.acceleration_structure_bytes = 0;

        std::vector<TlasInstanceBuild> instances;
        instances.reserve(opaque_items.size());
        for (const VulkanMeshDrawItem& item : opaque_items) {
            if (item.mesh == nullptr) {
                ++m_stats.skipped_blas_count;
                continue;
            }

            const VulkanAccelerationStructure* blas = blas_cache.find(*item.mesh);
            if (blas == nullptr || !blas->isReady()) {
                ++m_stats.skipped_blas_count;
                continue;
            }
            if (!isFiniteAffineTransform(item.transform)) {
                ++m_stats.skipped_transform_count;
                continue;
            }
            if (instances.size() >= kInstanceCustomIndexMask) {
                ++m_stats.skipped_transform_count;
                continue;
            }

            TlasInstanceBuild instance_build;
            instance_build.acceleration_structure = blas;
            instance_build.instance.transform = toVulkanTransformMatrix(item.transform);
            instance_build.instance.instanceCustomIndex =
                static_cast<uint32_t>(instances.size());
            instance_build.instance.mask = 0xffu;
            instance_build.instance.instanceShaderBindingTableRecordOffset = 0;
            instance_build.instance.flags =
                VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance_build.instance.accelerationStructureReference =
                blas->getDeviceAddress();
            instances.push_back(instance_build);
        }

        if (instances.empty()) {
            return true;
        }

        const VkDeviceSize instance_buffer_size =
            static_cast<VkDeviceSize>(instances.size() *
                                      sizeof(VkAccelerationStructureInstanceKHR));
        if (!ensureInstanceBuffer(
                slot,
                instance_buffer_size,
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
        std::vector<VkAccelerationStructureInstanceKHR> instance_data;
        instance_data.reserve(instances.size());
        for (const TlasInstanceBuild& instance : instances) {
            instance_data.push_back(instance.instance);
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
        build_info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
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
            static_cast<uint32_t>(instances.size())
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
        if (!slot.acceleration_structure.isReady() ||
            slot.acceleration_structure.getSize() <
                build_sizes.acceleration_structure_size) {
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
        }

        if (!m_scratch_buffer.ensureCapacity(
                *m_context.gpu_allocator,
                build_sizes.build_scratch_size,
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
        build_range.primitiveCount = static_cast<uint32_t>(instances.size());
        if (!submitTlasBuild(
                m_context,
                m_command_pool,
                slot.instance_buffer.get(),
                instance_buffer_size,
                *target,
                build_info,
                build_range)) {
            setFailure("TLAS build submission failed.");
            resetSlot(slot);
            return false;
        }

        if (target == &replacement) {
            slot.acceleration_structure = std::move(replacement);
        }
        slot.instance_count = static_cast<uint32_t>(instances.size());
        slot.ready = true;
        m_stats.ready = true;
        m_stats.built_instance_count = slot.instance_count;
        m_stats.instance_buffer_bytes = instance_buffer_size;
        m_stats.acceleration_structure_bytes = slot.acceleration_structure.getSize();
        ++m_stats.build_count;
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

    VulkanTlasBuildStats VulkanTlasManager::getStats() const {
        VulkanTlasBuildStats stats = m_stats;
        stats.initialized = m_initialized;
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
        slot.acceleration_structure.reset();
        slot.instance_buffer.reset();
    }

    void VulkanTlasManager::setFailure(std::string failure_reason) {
        m_stats.ready = false;
        m_stats.last_failure_reason = std::move(failure_reason);
        NX_CORE_WARN("{}", m_stats.last_failure_reason);
    }
} // namespace NexAur

#include "pch.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_static_mesh_blas_cache.h"

#include "Function/Resource/mesh.h"
#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_state_planner.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace NexAur {
    namespace {
        struct StaticMeshBlasBuildInput {
            VkAccelerationStructureGeometryKHR geometry{};
            uint32_t primitive_count = 0;
            VkBuildAccelerationStructureFlagsKHR flags =
                VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

            VkAccelerationStructureBuildGeometryInfoKHR buildInfo() const {
                VkAccelerationStructureBuildGeometryInfoKHR build_info{};
                build_info.sType =
                    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
                build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                build_info.flags = flags;
                build_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
                build_info.geometryCount = 1;
                build_info.pGeometries = &geometry;
                return build_info;
            }
        };

        struct PendingStaticMeshBlasBuild {
            VulkanMeshResourceKey key;
            StaticMeshBlasBuildInput input;
            VulkanAccelerationStructureBuildSizes sizes;
            VulkanAccelerationStructure acceleration_structure;
            VulkanAccelerationStructure compacted_acceleration_structure;
            VkDeviceSize compacted_size = 0;
        };

        std::string meshLabel(const VulkanMeshResourceKey& key) {
            return std::to_string(static_cast<uint64_t>(key.identity.model_asset.id)) +
                   ":" + std::to_string(key.identity.mesh_index) +
                   "@" + std::to_string(key.generation);
        }

        bool buildGeometryInput(
            const VulkanMeshResource& mesh,
            StaticMeshBlasBuildInput& input,
            std::string& failure_reason) {
            input = {};
            failure_reason.clear();
            if (!mesh.getKey().valid()) {
                failure_reason = "Mesh resource key is invalid.";
                return false;
            }
            if (!mesh.isReady()) {
                failure_reason = "Mesh upload is not ready.";
                return false;
            }
            if (!mesh.hasDeviceAddressBuffers()) {
                failure_reason = "Mesh buffers do not expose device addresses.";
                return false;
            }
            if (mesh.getVertexCount() == 0 || mesh.getIndexCount() == 0) {
                failure_reason = "Mesh geometry is empty.";
                return false;
            }
            if (mesh.getIndexCount() % 3u != 0) {
                failure_reason = "Mesh index count is not divisible by three.";
                return false;
            }

            static_assert(
                offsetof(Vertex, position) == 0,
                "Static mesh BLAS expects Vertex::position at byte offset zero.");

            VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
            triangles.sType =
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            triangles.vertexData.deviceAddress = mesh.getVertexBufferDeviceAddress();
            triangles.vertexStride = sizeof(Vertex);
            triangles.maxVertex = mesh.getVertexCount() - 1u;
            triangles.indexType = VK_INDEX_TYPE_UINT32;
            triangles.indexData.deviceAddress = mesh.getIndexBufferDeviceAddress();

            input.geometry.sType =
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            input.geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            input.geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
            input.geometry.geometry.triangles = triangles;
            input.primitive_count = mesh.getIndexCount() / 3u;
            return true;
        }

        void recordBuildDependency(VkCommandBuffer command_buffer) {
            const VulkanGraphAccelerationStructureTransitionPlan transition =
                VulkanGraphStatePlanner::planAccelerationStructureTransition(
                    VulkanGraphStatePlanner::stateForAccelerationStructureUsage(
                        VulkanGraphAccelerationStructureUsage::BuildWrite,
                        VulkanGraphAccessType::Write),
                    VulkanGraphStatePlanner::stateForAccelerationStructureUsage(
                        VulkanGraphAccelerationStructureUsage::BuildWrite,
                        VulkanGraphAccessType::ReadWrite));
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

        bool submitBuildBatch(
            const VulkanResourceContext& context,
            const VulkanRayTracingDeviceFunctions& functions,
            VkCommandPool command_pool,
            VkDeviceAddress scratch_address,
            VkQueryPool compaction_query_pool,
            VulkanGpuTimestampQuery& gpu_timestamp_query,
            std::vector<PendingStaticMeshBlasBuild>& builds) {
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
                    "vkAllocateCommandBuffers(static mesh BLAS)")) {
                cleanup();
                return false;
            }

            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (!VulkanDiagnosticsCollector::checkVk(
                    vkBeginCommandBuffer(command_buffer, &begin_info),
                    "vkBeginCommandBuffer(static mesh BLAS)")) {
                cleanup();
                return false;
            }

            const bool timing_recorded =
                gpu_timestamp_query.begin(command_buffer);
            if (compaction_query_pool != VK_NULL_HANDLE) {
                vkCmdResetQueryPool(
                    command_buffer,
                    compaction_query_pool,
                    0,
                    static_cast<uint32_t>(builds.size()));
            }

            for (PendingStaticMeshBlasBuild& build : builds) {
                VkAccelerationStructureBuildGeometryInfoKHR build_info =
                    build.input.buildInfo();
                VkAccelerationStructureBuildRangeInfoKHR range_info{};
                range_info.primitiveCount = build.input.primitive_count;
                const std::array<VkAccelerationStructureBuildRangeInfoKHR, 1>
                    build_ranges{ range_info };
                if (!build.acceleration_structure.recordBuild(
                        command_buffer,
                        build_info,
                        build_ranges,
                        scratch_address)) {
                    if (timing_recorded) {
                        gpu_timestamp_query.discard();
                    }
                    cleanup();
                    return false;
                }
                recordBuildDependency(command_buffer);
            }

            if (compaction_query_pool != VK_NULL_HANDLE) {
                std::vector<VkAccelerationStructureKHR> handles;
                handles.reserve(builds.size());
                for (const PendingStaticMeshBlasBuild& build : builds) {
                    handles.push_back(build.acceleration_structure.get());
                }
                functions.cmd_write_acceleration_structures_properties(
                    command_buffer,
                    static_cast<uint32_t>(handles.size()),
                    handles.data(),
                    VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                    compaction_query_pool,
                    0);
            }
            if (timing_recorded &&
                !gpu_timestamp_query.end(command_buffer)) {
                gpu_timestamp_query.discard();
            }

            if (!VulkanDiagnosticsCollector::checkVk(
                    vkEndCommandBuffer(command_buffer),
                    "vkEndCommandBuffer(static mesh BLAS)")) {
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
                    "vkCreateFence(static mesh BLAS)")) {
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
                "vkQueueSubmit(static mesh BLAS)");
            const bool completed = submitted && VulkanDiagnosticsCollector::checkVk(
                vkWaitForFences(
                    context.device,
                    1,
                    &fence,
                    VK_TRUE,
                    UINT64_MAX),
                "vkWaitForFences(static mesh BLAS)");
            if (completed && timing_recorded) {
                gpu_timestamp_query.resolve();
            } else if (!submitted && timing_recorded) {
                gpu_timestamp_query.discard();
            }
            cleanup();
            return completed;
        }

        bool submitCompactionBatch(
            const VulkanResourceContext& context,
            const VulkanRayTracingDeviceFunctions& functions,
            VkCommandPool command_pool,
            VulkanGpuTimestampQuery& gpu_timestamp_query,
            std::vector<PendingStaticMeshBlasBuild>& builds) {
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
                    "vkAllocateCommandBuffers(static mesh BLAS compaction)")) {
                cleanup();
                return false;
            }

            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (!VulkanDiagnosticsCollector::checkVk(
                    vkBeginCommandBuffer(command_buffer, &begin_info),
                    "vkBeginCommandBuffer(static mesh BLAS compaction)")) {
                cleanup();
                return false;
            }

            const bool timing_recorded =
                gpu_timestamp_query.begin(command_buffer);
            recordBuildDependency(command_buffer);
            for (const PendingStaticMeshBlasBuild& build : builds) {
                if (!build.compacted_acceleration_structure.isReady()) {
                    continue;
                }

                VkCopyAccelerationStructureInfoKHR copy_info{};
                copy_info.sType =
                    VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR;
                copy_info.src = build.acceleration_structure.get();
                copy_info.dst = build.compacted_acceleration_structure.get();
                copy_info.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
                functions.cmd_copy_acceleration_structure(
                    command_buffer,
                    &copy_info);
            }
            if (timing_recorded &&
                !gpu_timestamp_query.end(command_buffer)) {
                gpu_timestamp_query.discard();
            }

            if (!VulkanDiagnosticsCollector::checkVk(
                    vkEndCommandBuffer(command_buffer),
                    "vkEndCommandBuffer(static mesh BLAS compaction)")) {
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
                    "vkCreateFence(static mesh BLAS compaction)")) {
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
                "vkQueueSubmit(static mesh BLAS compaction)");
            const bool completed = submitted && VulkanDiagnosticsCollector::checkVk(
                vkWaitForFences(
                    context.device,
                    1,
                    &fence,
                    VK_TRUE,
                    UINT64_MAX),
                "vkWaitForFences(static mesh BLAS compaction)");
            if (completed && timing_recorded) {
                gpu_timestamp_query.resolve();
            } else if (!submitted && timing_recorded) {
                gpu_timestamp_query.discard();
            }
            cleanup();
            return completed;
        }

        VkQueryPool createCompactionQueryPool(
            VkDevice device,
            uint32_t query_count) {
            if (device == VK_NULL_HANDLE || query_count == 0) {
                return VK_NULL_HANDLE;
            }

            VkQueryPoolCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            create_info.queryType =
                VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
            create_info.queryCount = query_count;

            VkQueryPool query_pool = VK_NULL_HANDLE;
            if (!VulkanDiagnosticsCollector::checkVk(
                    vkCreateQueryPool(
                        device,
                        &create_info,
                        nullptr,
                        &query_pool),
                    "vkCreateQueryPool(static mesh BLAS compaction)")) {
                return VK_NULL_HANDLE;
            }
            return query_pool;
        }
    } // namespace

    bool shouldCompactVulkanAccelerationStructure(
        VkDeviceSize original_size,
        VkDeviceSize compacted_size,
        VkDeviceSize minimum_savings) {
        return original_size > compacted_size &&
               compacted_size > 0 &&
               original_size - compacted_size >= minimum_savings;
    }

    VulkanStaticMeshBlasCache::~VulkanStaticMeshBlasCache() {
        shutdown();
    }

    bool VulkanStaticMeshBlasCache::init(
        const VulkanResourceContext& context,
        const VulkanRayTracingDeviceFunctions& functions,
        VkDeviceSize scratch_alignment,
        VulkanStaticMeshBlasCacheConfig config) {
        shutdown();
        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() ||
            !context.buffer_device_address_enabled ||
            !functions.valid() ||
            !config.valid() ||
            scratch_alignment == 0 ||
            (scratch_alignment & (scratch_alignment - 1)) != 0) {
            m_last_failure_reason =
                "Static mesh BLAS cache received an invalid Ray Query context.";
            NX_CORE_ERROR("Static mesh BLAS cache requires a valid Ray Query context.");
            return false;
        }

        m_context = context;
        m_functions = functions;
        m_config = config;
        m_scratch_alignment = scratch_alignment;
        if (!createCommandPool()) {
            shutdown();
            m_last_failure_reason =
                "Static mesh BLAS command pool creation failed.";
            return false;
        }
        if (!m_gpu_timestamp_query.init(
                context.physical_device,
                context.device,
                context.graphics_queue_family)) {
            NX_CORE_WARN("Static mesh BLAS GPU timing is unavailable.");
        }
        m_initialized = true;
        return true;
    }

    void VulkanStaticMeshBlasCache::clear() {
        m_entries.clear();
        m_scratch_buffer.reset();
        m_build_count = 0;
        m_cache_hit_count = 0;
        m_failed_build_count = 0;
        m_acceleration_structure_bytes = 0;
        m_prepare_epoch = 0;
        m_compaction_count = 0;
        m_failed_compaction_count = 0;
        m_compaction_saved_bytes = 0;
        m_eviction_count = 0;
        m_retired_entry_count = 0;
        m_retired_bytes = 0;
        m_last_build_gpu_ms = 0.0;
        m_last_compaction_gpu_ms = 0.0;
        m_last_failure_reason = "None";
    }

    void VulkanStaticMeshBlasCache::shutdown() {
        clear();
        m_gpu_timestamp_query.shutdown();
        if (m_context.device != VK_NULL_HANDLE &&
            m_command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_context.device, m_command_pool, nullptr);
        }
        m_context = {};
        m_functions.reset();
        m_config = {};
        m_command_pool = VK_NULL_HANDLE;
        m_scratch_alignment = 1;
        m_initialized = false;
    }

    bool VulkanStaticMeshBlasCache::prepare(
        std::span<const VulkanMeshResource* const> meshes) {
        if (!m_initialized) {
            return false;
        }
        ++m_prepare_epoch;

        std::unordered_map<
            VulkanMeshResourceIdentity,
            const VulkanMeshResource*,
            VulkanMeshResourceIdentityHash> unique_meshes;
        unique_meshes.reserve(meshes.size());
        for (const VulkanMeshResource* mesh : meshes) {
            if (mesh == nullptr || !mesh->getKey().valid()) {
                continue;
            }

            auto [mesh_it, inserted] = unique_meshes.emplace(
                mesh->getKey().identity,
                mesh);
            if (!inserted &&
                mesh->getKey().generation > mesh_it->second->getKey().generation) {
                mesh_it->second = mesh;
            }
        }

        std::vector<PendingStaticMeshBlasBuild> pending_builds;
        pending_builds.reserve(unique_meshes.size());
        VkDeviceSize maximum_scratch_size = 0;
        for (const auto& [identity, mesh] : unique_meshes) {
            (void)identity;
            const VulkanMeshResourceKey& key = mesh->getKey();
            const auto entry_it = m_entries.find(key.identity);
            if (entry_it != m_entries.end() &&
                entry_it->second.generation == key.generation) {
                entry_it->second.last_used_epoch = m_prepare_epoch;
                ++m_cache_hit_count;
                continue;
            }

            PendingStaticMeshBlasBuild build;
            build.key = key;
            std::string failure_reason;
            if (!buildGeometryInput(*mesh, build.input, failure_reason)) {
                storeFailure(key, std::move(failure_reason));
                continue;
            }
            if (m_config.enable_compaction &&
                m_functions.supportsCompaction()) {
                build.input.flags |=
                    VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
            }

            const VkAccelerationStructureBuildGeometryInfoKHR build_info =
                build.input.buildInfo();
            const std::array<uint32_t, 1> primitive_counts{
                build.input.primitive_count
            };
            if (!queryVulkanAccelerationStructureBuildSizes(
                    m_context.device,
                    m_functions,
                    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                    build_info,
                    primitive_counts,
                    build.sizes)) {
                storeFailure(key, "Acceleration structure size query failed.");
                continue;
            }

            VulkanAccelerationStructureCreateInfo create_info;
            create_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            create_info.size = build.sizes.acceleration_structure_size;
            create_info.debug_name = "Static mesh BLAS " + meshLabel(key);
            if (!build.acceleration_structure.create(
                    *m_context.gpu_allocator,
                    m_functions,
                    create_info)) {
                storeFailure(key, "Acceleration structure creation failed.");
                continue;
            }

            maximum_scratch_size = std::max(
                maximum_scratch_size,
                build.sizes.build_scratch_size);
            pending_builds.push_back(std::move(build));
        }

        if (pending_builds.empty()) {
            evictUnusedEntries(unique_meshes);
            return true;
        }

        if (!m_scratch_buffer.ensureCapacity(
                *m_context.gpu_allocator,
                maximum_scratch_size,
                m_scratch_alignment,
                "Static mesh BLAS build scratch")) {
            for (const PendingStaticMeshBlasBuild& build : pending_builds) {
                storeFailure(build.key, "Scratch buffer allocation failed.");
            }
            return false;
        }

        VkQueryPool compaction_query_pool = VK_NULL_HANDLE;
        if (m_config.enable_compaction &&
            m_functions.supportsCompaction()) {
            compaction_query_pool = createCompactionQueryPool(
                m_context.device,
                static_cast<uint32_t>(pending_builds.size()));
        }

        const uint64_t build_timing_sample_count =
            m_gpu_timestamp_query.getStats().sample_count;
        if (!submitBuildBatch(
                m_context,
                m_functions,
                m_command_pool,
                m_scratch_buffer.getDeviceAddress(),
                compaction_query_pool,
                m_gpu_timestamp_query,
                pending_builds)) {
            if (compaction_query_pool != VK_NULL_HANDLE) {
                vkDestroyQueryPool(
                    m_context.device,
                    compaction_query_pool,
                    nullptr);
            }
            for (const PendingStaticMeshBlasBuild& build : pending_builds) {
                storeFailure(build.key, "Acceleration structure build submission failed.");
            }
            return false;
        }
        const VulkanGpuTimestampQueryStats build_timing_stats =
            m_gpu_timestamp_query.getStats();
        if (build_timing_stats.sample_count > build_timing_sample_count) {
            m_last_build_gpu_ms = build_timing_stats.last_duration_ms;
        }

        if (compaction_query_pool != VK_NULL_HANDLE) {
            std::vector<VkDeviceSize> compacted_sizes(pending_builds.size());
            const VkResult compacted_size_result = vkGetQueryPoolResults(
                m_context.device,
                compaction_query_pool,
                0,
                static_cast<uint32_t>(compacted_sizes.size()),
                sizeof(VkDeviceSize) * compacted_sizes.size(),
                compacted_sizes.data(),
                sizeof(VkDeviceSize),
                VK_QUERY_RESULT_64_BIT);
            vkDestroyQueryPool(
                m_context.device,
                compaction_query_pool,
                nullptr);
            compaction_query_pool = VK_NULL_HANDLE;

            if (VulkanDiagnosticsCollector::checkVk(
                    compacted_size_result,
                    "vkGetQueryPoolResults(static mesh BLAS compaction)")) {
                uint64_t pending_compaction_count = 0;
                for (size_t index = 0; index < pending_builds.size(); ++index) {
                    PendingStaticMeshBlasBuild& build = pending_builds[index];
                    const VkDeviceSize compacted_size = compacted_sizes[index];
                    if (!shouldCompactVulkanAccelerationStructure(
                            build.sizes.acceleration_structure_size,
                            compacted_size,
                            m_config.minimum_compaction_savings)) {
                        continue;
                    }

                    VulkanAccelerationStructureCreateInfo create_info;
                    create_info.type =
                        VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                    create_info.size = compacted_size;
                    create_info.debug_name =
                        "Compacted static mesh BLAS " + meshLabel(build.key);
                    if (!build.compacted_acceleration_structure.create(
                            *m_context.gpu_allocator,
                            m_functions,
                            create_info)) {
                        ++m_failed_compaction_count;
                        continue;
                    }
                    build.compacted_size = compacted_size;
                    ++pending_compaction_count;
                }

                if (pending_compaction_count > 0) {
                    const uint64_t compaction_timing_sample_count =
                        m_gpu_timestamp_query.getStats().sample_count;
                    if (submitCompactionBatch(
                            m_context,
                            m_functions,
                            m_command_pool,
                            m_gpu_timestamp_query,
                            pending_builds)) {
                        const VulkanGpuTimestampQueryStats compaction_timing_stats =
                            m_gpu_timestamp_query.getStats();
                        if (compaction_timing_stats.sample_count >
                            compaction_timing_sample_count) {
                            m_last_compaction_gpu_ms =
                                compaction_timing_stats.last_duration_ms;
                        }
                        for (PendingStaticMeshBlasBuild& build : pending_builds) {
                            if (!build.compacted_acceleration_structure.isReady()) {
                                continue;
                            }
                            m_compaction_saved_bytes +=
                                build.sizes.acceleration_structure_size -
                                build.compacted_size;
                            ++m_compaction_count;
                            build.sizes.acceleration_structure_size =
                                build.compacted_size;
                            build.acceleration_structure =
                                std::move(build.compacted_acceleration_structure);
                        }
                    } else {
                        m_failed_compaction_count += pending_compaction_count;
                    }
                }
            } else {
                m_failed_compaction_count += pending_builds.size();
            }
        }

        for (PendingStaticMeshBlasBuild& build : pending_builds) {
            Entry entry;
            entry.generation = build.key.generation;
            entry.state = EntryState::Ready;
            entry.bytes = build.acceleration_structure.getSize();
            entry.last_used_epoch = m_prepare_epoch;
            entry.acceleration_structure =
                std::move(build.acceleration_structure);
            replaceEntry(build.key, std::move(entry));
            ++m_build_count;
        }
        evictUnusedEntries(unique_meshes);
        return true;
    }

    const VulkanAccelerationStructure* VulkanStaticMeshBlasCache::find(
        const VulkanMeshResource& mesh) const {
        const VulkanMeshResourceKey& key = mesh.getKey();
        if (!key.valid()) {
            return nullptr;
        }

        const auto entry_it = m_entries.find(key.identity);
        if (entry_it == m_entries.end() ||
            entry_it->second.generation != key.generation ||
            entry_it->second.state != EntryState::Ready ||
            !entry_it->second.acceleration_structure.isReady()) {
            return nullptr;
        }
        return &entry_it->second.acceleration_structure;
    }

    VulkanStaticMeshBlasCacheStats VulkanStaticMeshBlasCache::getStats() const {
        VulkanStaticMeshBlasCacheStats stats;
        stats.initialized = m_initialized;
        stats.entry_count = m_entries.size();
        for (const auto& [identity, entry] : m_entries) {
            (void)identity;
            if (entry.state == EntryState::Ready &&
                entry.acceleration_structure.isReady()) {
                ++stats.ready_entry_count;
            } else {
                ++stats.failed_entry_count;
            }
        }
        stats.build_count = m_build_count;
        stats.cache_hit_count = m_cache_hit_count;
        stats.failed_build_count = m_failed_build_count;
        stats.acceleration_structure_bytes = m_acceleration_structure_bytes;
        stats.memory_budget_bytes =
            m_initialized ? m_config.memory_budget_bytes : 0;
        stats.compaction_count = m_compaction_count;
        stats.failed_compaction_count = m_failed_compaction_count;
        stats.compaction_saved_bytes = m_compaction_saved_bytes;
        stats.eviction_count = m_eviction_count;
        stats.retired_entry_count = m_retired_entry_count;
        stats.retired_bytes = m_retired_bytes;
        stats.scratch_capacity_bytes = m_scratch_buffer.getCapacity();
        stats.compaction_enabled =
            m_initialized &&
            m_config.enable_compaction &&
            m_functions.supportsCompaction();
        const VulkanGpuTimestampQueryStats timing_stats =
            m_gpu_timestamp_query.getStats();
        stats.gpu_timing_supported = timing_stats.supported;
        stats.gpu_timing_sample_count = timing_stats.sample_count;
        stats.last_build_gpu_ms = m_last_build_gpu_ms;
        stats.last_compaction_gpu_ms = m_last_compaction_gpu_ms;
        stats.last_failure_reason = m_last_failure_reason;
        return stats;
    }

    bool VulkanStaticMeshBlasCache::createCommandPool() {
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
            "vkCreateCommandPool(static mesh BLAS)");
    }

    void VulkanStaticMeshBlasCache::storeFailure(
        const VulkanMeshResourceKey& key,
        std::string failure_reason) {
        if (failure_reason.empty()) {
            failure_reason = "Unknown static mesh BLAS failure.";
        }

        Entry entry;
        entry.generation = key.generation;
        entry.state = EntryState::Failed;
        entry.last_used_epoch = m_prepare_epoch;
        entry.failure_reason = failure_reason;
        replaceEntry(key, std::move(entry));
        ++m_failed_build_count;
        m_last_failure_reason = failure_reason;
        NX_CORE_WARN(
            "Static mesh BLAS {} unavailable: {}",
            meshLabel(key),
            failure_reason);
    }

    void VulkanStaticMeshBlasCache::replaceEntry(
        const VulkanMeshResourceKey& key,
        Entry&& replacement) {
        auto entry_it = m_entries.find(key.identity);
        if (entry_it == m_entries.end()) {
            if (replacement.state == EntryState::Ready) {
                m_acceleration_structure_bytes += replacement.bytes;
            }
            m_entries.emplace(key.identity, std::move(replacement));
            return;
        }

        if (entry_it->second.state == EntryState::Ready) {
            ++m_retired_entry_count;
            m_retired_bytes += entry_it->second.bytes;
            m_acceleration_structure_bytes -= std::min<uint64_t>(
                m_acceleration_structure_bytes,
                entry_it->second.bytes);
        }
        if (replacement.state == EntryState::Ready) {
            m_acceleration_structure_bytes += replacement.bytes;
        }
        entry_it->second = std::move(replacement);
    }

    void VulkanStaticMeshBlasCache::evictUnusedEntries(
        const std::unordered_map<
            VulkanMeshResourceIdentity,
            const VulkanMeshResource*,
            VulkanMeshResourceIdentityHash>& active_meshes) {
        auto entry = m_entries.begin();
        while (entry != m_entries.end()) {
            const bool active = active_meshes.contains(entry->first);
            const uint64_t inactive_age =
                m_prepare_epoch >= entry->second.last_used_epoch ?
                    m_prepare_epoch - entry->second.last_used_epoch : 0;
            if (!active &&
                inactive_age >= m_config.inactive_frame_retention) {
                auto evicted = entry++;
                eraseEntry(evicted);
                continue;
            }
            ++entry;
        }

        while (m_acceleration_structure_bytes > m_config.memory_budget_bytes) {
            auto candidate = m_entries.end();
            for (auto current = m_entries.begin();
                 current != m_entries.end();
                 ++current) {
                if (active_meshes.contains(current->first) ||
                    current->second.state != EntryState::Ready) {
                    continue;
                }
                if (candidate == m_entries.end() ||
                    current->second.last_used_epoch <
                        candidate->second.last_used_epoch) {
                    candidate = current;
                }
            }
            if (candidate == m_entries.end()) {
                break;
            }
            eraseEntry(candidate);
        }
    }

    void VulkanStaticMeshBlasCache::eraseEntry(EntryMap::iterator entry) {
        if (entry == m_entries.end()) {
            return;
        }
        if (entry->second.state == EntryState::Ready) {
            m_acceleration_structure_bytes -= std::min<uint64_t>(
                m_acceleration_structure_bytes,
                entry->second.bytes);
            ++m_retired_entry_count;
            m_retired_bytes += entry->second.bytes;
        }
        ++m_eviction_count;
        m_entries.erase(entry);
    }
} // namespace NexAur

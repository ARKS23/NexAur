#include "pch.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_static_mesh_blas_cache.h"

#include "Function/Resource/mesh.h"
#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"

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

            VkAccelerationStructureBuildGeometryInfoKHR buildInfo() const {
                VkAccelerationStructureBuildGeometryInfoKHR build_info{};
                build_info.sType =
                    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
                build_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                build_info.flags =
                    VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
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
            VkMemoryBarrier2 barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            barrier.srcStageMask =
                VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
            barrier.srcAccessMask =
                VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            barrier.dstStageMask =
                VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
            barrier.dstAccessMask =
                VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR |
                VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

            VkDependencyInfo dependency_info{};
            dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependency_info.memoryBarrierCount = 1;
            dependency_info.pMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        }

        bool submitBuildBatch(
            const VulkanResourceContext& context,
            VkCommandPool command_pool,
            VkDeviceAddress scratch_address,
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
                    cleanup();
                    return false;
                }
                recordBuildDependency(command_buffer);
            }

            if (!VulkanDiagnosticsCollector::checkVk(
                    vkEndCommandBuffer(command_buffer),
                    "vkEndCommandBuffer(static mesh BLAS)")) {
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
            cleanup();
            return completed;
        }
    } // namespace

    VulkanStaticMeshBlasCache::~VulkanStaticMeshBlasCache() {
        shutdown();
    }

    bool VulkanStaticMeshBlasCache::init(
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
            m_last_failure_reason =
                "Static mesh BLAS cache received an invalid Ray Query context.";
            NX_CORE_ERROR("Static mesh BLAS cache requires a valid Ray Query context.");
            return false;
        }

        m_context = context;
        m_functions = functions;
        m_scratch_alignment = scratch_alignment;
        if (!createCommandPool()) {
            shutdown();
            m_last_failure_reason =
                "Static mesh BLAS command pool creation failed.";
            return false;
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
        m_last_failure_reason = "None";
    }

    void VulkanStaticMeshBlasCache::shutdown() {
        clear();
        if (m_context.device != VK_NULL_HANDLE &&
            m_command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_context.device, m_command_pool, nullptr);
        }
        m_context = {};
        m_functions.reset();
        m_command_pool = VK_NULL_HANDLE;
        m_scratch_alignment = 1;
        m_initialized = false;
    }

    bool VulkanStaticMeshBlasCache::prepare(
        std::span<const VulkanMeshResource* const> meshes) {
        if (!m_initialized) {
            return false;
        }
        if (meshes.empty()) {
            return true;
        }

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

        if (!submitBuildBatch(
                m_context,
                m_command_pool,
                m_scratch_buffer.getDeviceAddress(),
                pending_builds)) {
            for (const PendingStaticMeshBlasBuild& build : pending_builds) {
                storeFailure(build.key, "Acceleration structure build submission failed.");
            }
            return false;
        }

        for (PendingStaticMeshBlasBuild& build : pending_builds) {
            Entry entry;
            entry.generation = build.key.generation;
            entry.state = EntryState::Ready;
            entry.bytes = build.sizes.acceleration_structure_size;
            entry.acceleration_structure =
                std::move(build.acceleration_structure);
            replaceEntry(build.key, std::move(entry));
            ++m_build_count;
        }
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
            m_acceleration_structure_bytes -= std::min<uint64_t>(
                m_acceleration_structure_bytes,
                entry_it->second.bytes);
        }
        if (replacement.state == EntryState::Ready) {
            m_acceleration_structure_bytes += replacement.bytes;
        }
        entry_it->second = std::move(replacement);
    }
} // namespace NexAur

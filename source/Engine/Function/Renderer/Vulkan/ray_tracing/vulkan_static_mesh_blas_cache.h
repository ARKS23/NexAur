#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>

#include <vulkan/vulkan.h>

#include "Function/Renderer/Vulkan/diagnostics/vulkan_gpu_timestamp_query.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_acceleration_structure.h"
#include "Function/Renderer/Vulkan/resources/vulkan_mesh_resource.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    struct VulkanStaticMeshBlasCacheConfig {
        uint64_t memory_budget_bytes = 256ull * 1024ull * 1024ull;
        uint64_t inactive_frame_retention = 120;
        VkDeviceSize minimum_compaction_savings = 4096;
        bool enable_compaction = true;

        bool valid() const {
            return memory_budget_bytes > 0 && inactive_frame_retention > 0;
        }
    };

    bool shouldCompactVulkanAccelerationStructure(
        VkDeviceSize original_size,
        VkDeviceSize compacted_size,
        VkDeviceSize minimum_savings);

    struct VulkanStaticMeshBlasCacheStats {
        bool initialized = false;
        size_t entry_count = 0;
        size_t ready_entry_count = 0;
        size_t failed_entry_count = 0;
        uint64_t build_count = 0;
        uint64_t cache_hit_count = 0;
        uint64_t failed_build_count = 0;
        uint64_t acceleration_structure_bytes = 0;
        uint64_t memory_budget_bytes = 0;
        uint64_t compaction_count = 0;
        uint64_t failed_compaction_count = 0;
        uint64_t compaction_saved_bytes = 0;
        uint64_t eviction_count = 0;
        uint64_t retired_entry_count = 0;
        uint64_t retired_bytes = 0;
        uint64_t scratch_capacity_bytes = 0;
        bool compaction_enabled = false;
        bool gpu_timing_supported = false;
        uint64_t gpu_timing_sample_count = 0;
        double last_build_gpu_ms = 0.0;
        double last_compaction_gpu_ms = 0.0;
        std::string last_failure_reason = "None";
    };

    class VulkanStaticMeshBlasCache final {
    public:
        VulkanStaticMeshBlasCache() = default;
        ~VulkanStaticMeshBlasCache();

        VulkanStaticMeshBlasCache(const VulkanStaticMeshBlasCache&) = delete;
        VulkanStaticMeshBlasCache& operator=(const VulkanStaticMeshBlasCache&) = delete;

        bool init(
            const VulkanResourceContext& context,
            const VulkanRayTracingDeviceFunctions& functions,
            VkDeviceSize scratch_alignment,
            VulkanStaticMeshBlasCacheConfig config = {});
        void clear();
        void shutdown();

        bool prepare(std::span<const VulkanMeshResource* const> meshes);
        const VulkanAccelerationStructure* find(
            const VulkanMeshResource& mesh) const;
        VulkanStaticMeshBlasCacheStats getStats() const;

        bool isInitialized() const { return m_initialized; }

    private:
        enum class EntryState : uint8_t {
            Ready = 0,
            Failed
        };

        struct Entry {
            Entry() = default;
            Entry(const Entry&) = delete;
            Entry& operator=(const Entry&) = delete;
            Entry(Entry&&) noexcept = default;
            Entry& operator=(Entry&&) noexcept = default;

            uint64_t generation = 0;
            EntryState state = EntryState::Failed;
            VulkanAccelerationStructure acceleration_structure;
            VkDeviceSize bytes = 0;
            uint64_t last_used_epoch = 0;
            std::string failure_reason;
        };

        using EntryMap = std::unordered_map<
            VulkanMeshResourceIdentity,
            Entry,
            VulkanMeshResourceIdentityHash>;

        bool createCommandPool();
        void storeFailure(
            const VulkanMeshResourceKey& key,
            std::string failure_reason);
        void replaceEntry(
            const VulkanMeshResourceKey& key,
            Entry&& replacement);
        void evictUnusedEntries(
            const std::unordered_map<
                VulkanMeshResourceIdentity,
                const VulkanMeshResource*,
                VulkanMeshResourceIdentityHash>& active_meshes);
        void eraseEntry(EntryMap::iterator entry);

        VulkanResourceContext m_context;
        VulkanRayTracingDeviceFunctions m_functions;
        VulkanStaticMeshBlasCacheConfig m_config;
        VkCommandPool m_command_pool = VK_NULL_HANDLE;
        VkDeviceSize m_scratch_alignment = 1;
        VulkanAccelerationStructureScratchBuffer m_scratch_buffer;
        VulkanGpuTimestampQuery m_gpu_timestamp_query;
        EntryMap m_entries;
        uint64_t m_build_count = 0;
        uint64_t m_cache_hit_count = 0;
        uint64_t m_failed_build_count = 0;
        uint64_t m_acceleration_structure_bytes = 0;
        uint64_t m_prepare_epoch = 0;
        uint64_t m_compaction_count = 0;
        uint64_t m_failed_compaction_count = 0;
        uint64_t m_compaction_saved_bytes = 0;
        uint64_t m_eviction_count = 0;
        uint64_t m_retired_entry_count = 0;
        uint64_t m_retired_bytes = 0;
        double m_last_build_gpu_ms = 0.0;
        double m_last_compaction_gpu_ms = 0.0;
        std::string m_last_failure_reason = "None";
        bool m_initialized = false;
    };
} // namespace NexAur

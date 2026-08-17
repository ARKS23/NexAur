#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>

#include <vulkan/vulkan.h>

#include "Function/Renderer/Vulkan/ray_tracing/vulkan_acceleration_structure.h"
#include "Function/Renderer/Vulkan/resources/vulkan_mesh_resource.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    struct VulkanStaticMeshBlasCacheStats {
        bool initialized = false;
        size_t entry_count = 0;
        size_t ready_entry_count = 0;
        size_t failed_entry_count = 0;
        uint64_t build_count = 0;
        uint64_t cache_hit_count = 0;
        uint64_t failed_build_count = 0;
        uint64_t acceleration_structure_bytes = 0;
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
            VkDeviceSize scratch_alignment);
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
            std::string failure_reason;
        };

        bool createCommandPool();
        void storeFailure(
            const VulkanMeshResourceKey& key,
            std::string failure_reason);
        void replaceEntry(
            const VulkanMeshResourceKey& key,
            Entry&& replacement);

        VulkanResourceContext m_context;
        VulkanRayTracingDeviceFunctions m_functions;
        VkCommandPool m_command_pool = VK_NULL_HANDLE;
        VkDeviceSize m_scratch_alignment = 1;
        VulkanAccelerationStructureScratchBuffer m_scratch_buffer;
        std::unordered_map<
            VulkanMeshResourceIdentity,
            Entry,
            VulkanMeshResourceIdentityHash> m_entries;
        uint64_t m_build_count = 0;
        uint64_t m_cache_hit_count = 0;
        uint64_t m_failed_build_count = 0;
        uint64_t m_acceleration_structure_bytes = 0;
        std::string m_last_failure_reason = "None";
        bool m_initialized = false;
    };
} // namespace NexAur

#pragma once

#include <cstddef>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"
#include "Function/Renderer/Vulkan/resources/vulkan_mesh_resource.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanAccelerationStructure;
    class VulkanDescriptorAllocator;
    class VulkanMaterialResource;
    class VulkanStaticMeshBlasCache;
    class VulkanTextureResource;

    inline constexpr uint32_t kVulkanRtInvalidTableIndex = 0;
    inline constexpr uint32_t kVulkanRtMaxInstanceCustomIndex = 0x00ffffffu;
    inline constexpr uint32_t kVulkanRtFallbackWhiteTextureIndex = 0;
    inline constexpr uint32_t kVulkanRtFallbackBlackTextureIndex = 1;
    inline constexpr uint32_t kVulkanRtFallbackFlatNormalTextureIndex = 2;
    inline constexpr uint32_t kVulkanRtFallbackMetallicRoughnessTextureIndex = 3;
    inline constexpr uint32_t kVulkanRtFallbackTextureSlotCount = 4;

    inline constexpr uint32_t kVulkanRtMaterialBaseColorTextureBit = 1u << 0u;
    inline constexpr uint32_t kVulkanRtMaterialNormalTextureBit = 1u << 1u;
    inline constexpr uint32_t kVulkanRtMaterialMetallicTextureBit = 1u << 2u;
    inline constexpr uint32_t kVulkanRtMaterialRoughnessTextureBit = 1u << 3u;
    inline constexpr uint32_t kVulkanRtMaterialPackedMetallicRoughnessTextureBit = 1u << 4u;
    inline constexpr uint32_t kVulkanRtMaterialAoTextureBit = 1u << 5u;
    inline constexpr uint32_t kVulkanRtMaterialEmissiveTextureBit = 1u << 6u;

    // This is the single CPU-side list consumed by both TLAS and scene tables.
    struct VulkanRayTracingInstanceRecord {
        const VulkanMeshResource* mesh = nullptr;
        const VulkanMaterialResource* material = nullptr;
        const VulkanAccelerationStructure* blas = nullptr;
        glm::mat4 transform{ 1.0f };
        VulkanMeshResourceKey mesh_key;
        uint64_t material_generation = 0;
        int entity_id = -1;
    };

    struct VulkanRayTracingInstanceBuildStats {
        uint32_t source_instance_count = 0;
        uint32_t accepted_instance_count = 0;
        uint32_t skipped_blas_count = 0;
        uint32_t skipped_transform_count = 0;
        uint32_t skipped_material_count = 0;
    };

    std::vector<VulkanRayTracingInstanceRecord> buildVulkanRayTracingInstanceRecords(
        std::span<const VulkanMeshDrawItem> opaque_items,
        const VulkanStaticMeshBlasCache& blas_cache,
        VulkanRayTracingInstanceBuildStats* stats = nullptr);

    struct GpuRtInstanceRecord {
        uint32_t geometry_index = kVulkanRtInvalidTableIndex;
        uint32_t material_index = kVulkanRtInvalidTableIndex;
        uint32_t object_flags = 0;
        uint32_t entity_id = 0;
        bool operator==(const GpuRtInstanceRecord&) const = default;
    };

    struct GpuRtGeometryRecord {
        uint32_t vertex_buffer_index = kVulkanRtInvalidTableIndex;
        uint32_t index_buffer_index = kVulkanRtInvalidTableIndex;
        uint32_t vertex_stride = 0;
        uint32_t vertex_count = 0;
        uint32_t index_count = 0;
        uint32_t flags = 0;
        uint32_t reserved0 = 0;
        uint32_t reserved1 = 0;
        bool operator==(const GpuRtGeometryRecord&) const = default;
    };

    struct GpuRtMaterialRecord {
        glm::vec4 base_color_factor{ 1.0f };
        glm::vec4 emissive_factor_normal_scale{ 0.0f, 0.0f, 0.0f, 1.0f };
        glm::vec4 metallic_roughness_alpha_flags{ 0.0f, 1.0f, 0.5f, 0.0f };
        glm::uvec4 texture_indices0{ 0u };
        glm::uvec4 texture_indices1{ 0u };
        bool operator==(const GpuRtMaterialRecord&) const = default;
    };

    static_assert(std::is_standard_layout_v<GpuRtInstanceRecord>);
    static_assert(std::is_standard_layout_v<GpuRtGeometryRecord>);
    static_assert(std::is_standard_layout_v<GpuRtMaterialRecord>);
    static_assert(sizeof(GpuRtInstanceRecord) == 16);
    static_assert(sizeof(GpuRtGeometryRecord) == 32);
    static_assert(sizeof(GpuRtMaterialRecord) == 80);
    static_assert(offsetof(GpuRtGeometryRecord, vertex_buffer_index) == 0);
    static_assert(offsetof(GpuRtGeometryRecord, index_buffer_index) == 4);
    static_assert(offsetof(GpuRtGeometryRecord, vertex_stride) == 8);
    static_assert(offsetof(GpuRtMaterialRecord, texture_indices0) == 48);
    static_assert(offsetof(GpuRtMaterialRecord, texture_indices1) == 64);

    bool isVulkanRtTrianglePrimitiveInBounds(
        uint32_t primitive_index,
        uint32_t index_count);
    bool getVulkanRtTriangleIndexOffset(
        uint32_t primitive_index,
        uint32_t index_count,
        uint32_t& out_index_offset);
    uint32_t getVulkanRtInstanceTableIndex(uint32_t accepted_instance_index);

    struct VulkanRayTracingSceneTableStats {
        bool initialized = false;
        bool ready = false;
        bool descriptor_ready = false;
        bool bda_geometry_fetch_enabled = false;
        bool descriptor_indexed_geometry_fetch_enabled = false;
        uint32_t instance_count = 0;
        uint32_t geometry_count = 0;
        uint32_t material_count = 0;
        uint32_t texture_count = 0;
        uint32_t texture_capacity = 0;
        uint32_t texture_overflow_count = 0;
        uint32_t geometry_descriptor_capacity = 0;
        uint32_t geometry_overflow_count = 0;
        uint64_t instance_buffer_bytes = 0;
        uint64_t geometry_buffer_bytes = 0;
        uint64_t material_buffer_bytes = 0;
        uint64_t update_count = 0;
        uint64_t rebuild_count = 0;
        uint64_t reuse_count = 0;
        uint64_t buffer_upload_count = 0;
        uint64_t uploaded_bytes = 0;
        uint64_t last_uploaded_bytes = 0;
        uint64_t descriptor_write_count = 0;
        std::string last_failure_reason = "None";
    };

    struct VulkanRayTracingFallbackTextures {
        const VulkanTextureResource* white = nullptr;
        const VulkanTextureResource* black = nullptr;
        const VulkanTextureResource* flat_normal = nullptr;
        const VulkanTextureResource* metallic_roughness = nullptr;

        bool valid() const {
            return white != nullptr &&
                   black != nullptr &&
                   flat_normal != nullptr &&
                   metallic_roughness != nullptr;
        }
    };

    class VulkanRayTracingSceneShadingTable final {
    public:
        VulkanRayTracingSceneShadingTable() = default;
        ~VulkanRayTracingSceneShadingTable();

        VulkanRayTracingSceneShadingTable(const VulkanRayTracingSceneShadingTable&) = delete;
        VulkanRayTracingSceneShadingTable& operator=(const VulkanRayTracingSceneShadingTable&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VulkanDescriptorAllocator& descriptor_allocator,
            VkDescriptorSetLayout descriptor_set_layout,
            uint32_t texture_capacity,
            uint32_t geometry_descriptor_capacity);
        void shutdown();
        void clear();

        // Call only after this frame slot's fence has completed. Cached inputs
        // are values; only resources in accepted_instances/fallback_textures are read.
        bool update(
            const VulkanAccelerationStructure* acceleration_structure,
            std::span<const VulkanRayTracingInstanceRecord> accepted_instances,
            const VulkanRayTracingFallbackTextures& fallback_textures);

        // The graph's first consumer should call this after importing the table buffers.
        void recordShaderReadBarrier(VkCommandBuffer command_buffer) const;

        bool isInitialized() const { return m_initialized; }
        bool isReady() const { return m_ready; }
        VkDescriptorSet getDescriptorSet() const {
            return m_ready ? m_descriptor_allocation.set : VK_NULL_HANDLE;
        }
        const VulkanOwnedBuffer& getInstanceBuffer() const { return m_instance_buffer; }
        const VulkanOwnedBuffer& getGeometryBuffer() const { return m_geometry_buffer; }
        const VulkanOwnedBuffer& getMaterialBuffer() const { return m_material_buffer; }
        const VulkanRayTracingSceneTableStats& getStats() const { return m_stats; }

    private:
        struct TextureSnapshot {
            const VulkanTextureResource* resource = nullptr;
            VkImageView view = VK_NULL_HANDLE;
            VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
            uint64_t upload_id = 0;
            bool ready = false;
            bool operator==(const TextureSnapshot&) const = default;
        };

        struct InstanceSnapshot {
            const VulkanMeshResource* mesh = nullptr;
            VulkanMeshResourceKey mesh_key;
            VkBuffer vertex_buffer = VK_NULL_HANDLE;
            VkBuffer index_buffer = VK_NULL_HANDLE;
            uint32_t vertex_count = 0;
            uint32_t index_count = 0;
            int entity_id = -1;
            const VulkanMaterialResource* material = nullptr;
            uint64_t material_generation = 0;
            GpuRtMaterialRecord material_record;
            uint32_t texture_flags = 0;
            bool material_ready = false;
            bool double_sided = false;
            std::array<TextureSnapshot, 7> textures;
            bool operator==(const InstanceSnapshot&) const = default;
        };

        // Snapshots own values only; cached resource pointers are never dereferenced.
        // Transforms live in TLAS and deliberately do not invalidate shading data.
        struct InputSnapshot {
            std::vector<InstanceSnapshot> instances;
            std::array<TextureSnapshot, kVulkanRtFallbackTextureSlotCount> fallbacks;
            VkSampler sampler = VK_NULL_HANDLE;
            bool operator==(const InputSnapshot&) const = default;
        };

        struct TableData {
            std::vector<GpuRtInstanceRecord> instances;
            std::vector<GpuRtGeometryRecord> geometries;
            std::vector<GpuRtMaterialRecord> materials;
            std::vector<VkDescriptorImageInfo> textures;
            std::vector<uint64_t> texture_upload_ids;
            std::vector<VkDescriptorBufferInfo> vertices;
            std::vector<VkDescriptorBufferInfo> indices;
            std::vector<VulkanMeshResourceKey> geometry_keys;
            std::unordered_map<const VulkanMeshResource*, uint32_t> geometry_indices;
            std::unordered_map<const VulkanMaterialResource*, uint32_t> material_indices;
            std::unordered_map<const VulkanTextureResource*, uint32_t> texture_indices;
        };

        static TextureSnapshot snapshotTexture(const VulkanTextureResource* texture);
        bool captureInputs(
            std::span<const VulkanRayTracingInstanceRecord> accepted_instances,
            const VulkanRayTracingFallbackTextures& fallback_textures);
        bool ensureBuffer(
            VulkanOwnedBuffer& buffer,
            VkDeviceSize required_size,
            const char* debug_name);
        bool writeBuffer(
            const VulkanOwnedBuffer& buffer,
            const void* data,
            VkDeviceSize size) const;
        void setFailure(std::string reason);

    private:
        const VulkanGpuAllocator* m_gpu_allocator = nullptr;
        VulkanDescriptorAllocator* m_descriptor_allocator = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_descriptor_set_layout = VK_NULL_HANDLE;
        VulkanDescriptorSetAllocation m_descriptor_allocation;
        VulkanOwnedBuffer m_instance_buffer;
        VulkanOwnedBuffer m_geometry_buffer;
        VulkanOwnedBuffer m_material_buffer;
        VulkanOwnedBuffer m_fallback_vertex_buffer;
        VulkanOwnedBuffer m_fallback_index_buffer;
        uint32_t m_texture_capacity = 0;
        uint32_t m_geometry_descriptor_capacity = 0;
        VulkanRayTracingSceneTableStats m_stats;
        InputSnapshot m_inputs;
        InputSnapshot m_pending_inputs;
        TableData m_data;
        TableData m_pending_data;
        std::array<VkDescriptorBufferInfo, 3> m_bound_buffers{};
        VkAccelerationStructureKHR m_bound_acceleration_structure = VK_NULL_HANDLE;
        VkSampler m_bound_sampler = VK_NULL_HANDLE;
        bool m_cache_valid = false;
        bool m_initialized = false;
        bool m_ready = false;
    };
} // namespace NexAur

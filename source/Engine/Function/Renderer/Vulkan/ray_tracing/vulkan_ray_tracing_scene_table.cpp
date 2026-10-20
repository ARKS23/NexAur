#include "pch.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_ray_tracing_scene_table.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_acceleration_structure.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_static_mesh_blas_cache.h"
#include "Function/Renderer/Vulkan/resources/vulkan_material_resource.h"
#include "Function/Renderer/Vulkan/resources/vulkan_texture_resource.h"
#include "Function/Resource/mesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>

namespace NexAur {
    namespace {
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

        uint32_t entityIdForGpu(int entity_id) {
            return entity_id < 0 ? 0xffffffffu : static_cast<uint32_t>(entity_id);
        }

        bool isReadyTexture(const VulkanTextureResource* texture) {
            return texture != nullptr && texture->isReady();
        }

        VkDescriptorImageInfo textureImageInfo(const VulkanTextureResource& texture) {
            VkDescriptorImageInfo image_info{};
            image_info.imageView = texture.getImageView();
            image_info.imageLayout = texture.getImageLayout();
            return image_info;
        }

        bool sameDescriptor(const VkDescriptorBufferInfo& lhs, const VkDescriptorBufferInfo& rhs) {
            return lhs.buffer == rhs.buffer && lhs.offset == rhs.offset && lhs.range == rhs.range;
        }

        bool sameDescriptor(const VkDescriptorImageInfo& lhs, const VkDescriptorImageInfo& rhs) {
            return lhs.sampler == rhs.sampler && lhs.imageView == rhs.imageView &&
                   lhs.imageLayout == rhs.imageLayout;
        }

        template<typename T>
        bool sameDescriptors(const std::vector<T>& lhs, const std::vector<T>& rhs) {
            return lhs.size() == rhs.size() &&
                std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                    [](const T& a, const T& b) { return sameDescriptor(a, b); });
        }

    } // namespace

    std::vector<VulkanRayTracingInstanceRecord> buildVulkanRayTracingInstanceRecords(
        std::span<const VulkanMeshDrawItem> opaque_items,
        const VulkanStaticMeshBlasCache& blas_cache,
        VulkanRayTracingInstanceBuildStats* stats) {
        VulkanRayTracingInstanceBuildStats local_stats;
        local_stats.source_instance_count = static_cast<uint32_t>(
            std::min<size_t>(opaque_items.size(), std::numeric_limits<uint32_t>::max()));

        std::vector<VulkanRayTracingInstanceRecord> records;
        records.reserve(opaque_items.size());
        for (const VulkanMeshDrawItem& item : opaque_items) {
            if (item.mesh == nullptr) {
                ++local_stats.skipped_blas_count;
                continue;
            }

            const VulkanAccelerationStructure* blas = blas_cache.find(*item.mesh);
            if (blas == nullptr || !blas->isReady()) {
                ++local_stats.skipped_blas_count;
                continue;
            }
            if (!isFiniteAffineTransform(item.transform)) {
                ++local_stats.skipped_transform_count;
                continue;
            }
            if (item.material != nullptr &&
                (!item.material->isReady() ||
                 !item.material->isRayTracingOpaque())) {
                ++local_stats.skipped_material_count;
                continue;
            }

            VulkanRayTracingInstanceRecord record;
            record.mesh = item.mesh;
            record.material = item.material;
            record.blas = blas;
            record.transform = item.transform;
            record.mesh_key = item.mesh->getKey();
            record.material_generation = item.material != nullptr ?
                item.material->getGeneration() : 0;
            record.entity_id = item.entity_id;
            records.push_back(record);
        }

        local_stats.accepted_instance_count = static_cast<uint32_t>(
            std::min<size_t>(records.size(), std::numeric_limits<uint32_t>::max()));
        if (stats != nullptr) {
            *stats = local_stats;
        }
        return records;
    }

    bool isVulkanRtTrianglePrimitiveInBounds(
        uint32_t primitive_index,
        uint32_t index_count) {
        return index_count % 3u == 0u &&
               primitive_index < index_count / 3u;
    }

    bool getVulkanRtTriangleIndexOffset(
        uint32_t primitive_index,
        uint32_t index_count,
        uint32_t& out_index_offset) {
        out_index_offset = 0;
        if (!isVulkanRtTrianglePrimitiveInBounds(primitive_index, index_count)) {
            return false;
        }

        out_index_offset = primitive_index * 3u;
        return true;
    }

    uint32_t getVulkanRtInstanceTableIndex(uint32_t accepted_instance_index) {
        return accepted_instance_index < kVulkanRtMaxInstanceCustomIndex ?
            accepted_instance_index + 1u :
            kVulkanRtInvalidTableIndex;
    }

    VulkanRayTracingSceneShadingTable::~VulkanRayTracingSceneShadingTable() {
        shutdown();
    }

    bool VulkanRayTracingSceneShadingTable::init(
        const VulkanResourceContext& context,
        VulkanDescriptorAllocator& descriptor_allocator,
        VkDescriptorSetLayout descriptor_set_layout,
        uint32_t texture_capacity,
        uint32_t geometry_descriptor_capacity) {
        shutdown();
        if (!context.valid() ||
            context.gpu_allocator == nullptr ||
            !context.gpu_allocator->isInitialized() ||
            descriptor_set_layout == VK_NULL_HANDLE ||
            texture_capacity < kVulkanRtFallbackTextureSlotCount ||
            geometry_descriptor_capacity < 2) {
            NX_CORE_ERROR("VulkanRayTracingSceneShadingTable received an invalid context.");
            return false;
        }

        m_gpu_allocator = context.gpu_allocator;
        m_descriptor_allocator = &descriptor_allocator;
        m_device = context.device;
        m_descriptor_set_layout = descriptor_set_layout;
        m_texture_capacity = texture_capacity;
        m_geometry_descriptor_capacity = geometry_descriptor_capacity;
        m_descriptor_allocation = descriptor_allocator.allocate(descriptor_set_layout);
        if (!m_descriptor_allocation.valid()) {
            shutdown();
            return false;
        }

        m_stats = {};
        m_stats.initialized = true;
        m_stats.texture_capacity = texture_capacity;
        m_stats.geometry_descriptor_capacity = geometry_descriptor_capacity;
        m_stats.bda_geometry_fetch_enabled = false;
        m_stats.descriptor_indexed_geometry_fetch_enabled = true;
        m_stats.last_failure_reason = "None";
        const std::array<Vertex, 3> fallback_vertices{};
        const std::array<uint32_t, 3> fallback_indices{};
        if (!ensureBuffer(
                m_fallback_vertex_buffer,
                sizeof(fallback_vertices),
                "RT scene fallback vertex buffer") ||
            !ensureBuffer(
                m_fallback_index_buffer,
                sizeof(fallback_indices),
                "RT scene fallback index buffer") ||
            !writeBuffer(
                m_fallback_vertex_buffer,
                fallback_vertices.data(),
                sizeof(fallback_vertices)) ||
            !writeBuffer(
                m_fallback_index_buffer,
                fallback_indices.data(),
                sizeof(fallback_indices))) {
            shutdown();
            return false;
        }
        m_initialized = true;
        return true;
    }

    void VulkanRayTracingSceneShadingTable::shutdown() {
        if (m_descriptor_allocator != nullptr && m_descriptor_allocation.valid()) {
            m_descriptor_allocator->free(m_descriptor_allocation);
        }
        m_descriptor_allocation = {};
        m_instance_buffer.reset();
        m_geometry_buffer.reset();
        m_material_buffer.reset();
        m_fallback_vertex_buffer.reset();
        m_fallback_index_buffer.reset();
        m_gpu_allocator = nullptr;
        m_descriptor_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
        m_descriptor_set_layout = VK_NULL_HANDLE;
        m_texture_capacity = 0;
        m_geometry_descriptor_capacity = 0;
        m_stats = {};
        m_stats.last_failure_reason = "None";
        m_initialized = false;
        m_ready = false;
        m_cache_valid = false;
        m_inputs = {};
        m_pending_inputs = {};
        m_data = {};
        m_pending_data = {};
        m_bound_buffers = {};
        m_bound_acceleration_structure = VK_NULL_HANDLE;
        m_bound_sampler = VK_NULL_HANDLE;
    }

    void VulkanRayTracingSceneShadingTable::clear() {
        m_ready = false;
        m_cache_valid = false;
        m_stats.ready = false;
        m_stats.descriptor_ready = false;
        m_stats.instance_count = 0;
        m_stats.geometry_count = 0;
        m_stats.material_count = 0;
        m_stats.texture_count = 0;
        m_stats.texture_overflow_count = 0;
        m_stats.geometry_overflow_count = 0;
        m_stats.instance_buffer_bytes = 0;
        m_stats.geometry_buffer_bytes = 0;
        m_stats.material_buffer_bytes = 0;
        m_stats.last_uploaded_bytes = 0;
        m_stats.last_failure_reason = "None";
    }

    VulkanRayTracingSceneShadingTable::TextureSnapshot
    VulkanRayTracingSceneShadingTable::snapshotTexture(const VulkanTextureResource* texture) {
        TextureSnapshot snapshot;
        snapshot.resource = texture;
        snapshot.ready = isReadyTexture(texture);
        if (snapshot.ready) {
            snapshot.view = texture->getImageView();
            snapshot.layout = texture->getImageLayout();
            snapshot.upload_id = texture->getUploadTicket().getId();
        }
        return snapshot;
    }

    bool VulkanRayTracingSceneShadingTable::captureInputs(
        std::span<const VulkanRayTracingInstanceRecord> accepted_instances,
        const VulkanRayTracingFallbackTextures& fallback_textures) {
        m_pending_inputs.instances.clear();
        m_pending_inputs.instances.reserve(accepted_instances.size());
        m_pending_inputs.fallbacks = {
            snapshotTexture(fallback_textures.white),
            snapshotTexture(fallback_textures.black),
            snapshotTexture(fallback_textures.flat_normal),
            snapshotTexture(fallback_textures.metallic_roughness)
        };
        m_pending_inputs.sampler = fallback_textures.white->getSampler();
        for (const VulkanRayTracingInstanceRecord& instance : accepted_instances) {
            if (instance.mesh == nullptr || instance.blas == nullptr ||
                !instance.blas->isReady() || !instance.mesh->isReady() ||
                !instance.mesh->hasDeviceAddressBuffers()) {
                setFailure("Canonical instance list contained an invalid geometry record.");
                return false;
            }
            InstanceSnapshot snapshot;
            snapshot.mesh = instance.mesh;
            snapshot.mesh_key = instance.mesh->getKey();
            snapshot.vertex_buffer = instance.mesh->getVertexBuffer();
            snapshot.index_buffer = instance.mesh->getIndexBuffer();
            snapshot.vertex_count = instance.mesh->getVertexCount();
            snapshot.index_count = instance.mesh->getIndexCount();
            snapshot.entity_id = instance.entity_id;
            snapshot.material = instance.material;
            snapshot.material_ready = instance.material != nullptr && instance.material->isReady();
            if (snapshot.material_ready) {
                const VulkanMaterialShadingData data = instance.material->getRayTracingShadingData();
                snapshot.material_generation = data.generation;
                snapshot.material_record = data.record;
                snapshot.texture_flags = data.texture_flags;
                snapshot.double_sided = data.double_sided;
                snapshot.textures = {
                    snapshotTexture(data.base_color), snapshotTexture(data.normal),
                    snapshotTexture(data.metallic), snapshotTexture(data.roughness),
                    snapshotTexture(data.metallic_roughness), snapshotTexture(data.ao),
                    snapshotTexture(data.emissive)
                };
            }
            m_pending_inputs.instances.push_back(snapshot);
        }
        return true;
    }

    bool VulkanRayTracingSceneShadingTable::update(
        const VulkanAccelerationStructure* acceleration_structure,
        std::span<const VulkanRayTracingInstanceRecord> accepted_instances,
        const VulkanRayTracingFallbackTextures& fallback_textures) {
        ++m_stats.update_count;
        m_stats.last_uploaded_bytes = 0;

        if (!m_initialized ||
            acceleration_structure == nullptr ||
            !acceleration_structure->isReady() ||
            !fallback_textures.valid() ||
            !isReadyTexture(fallback_textures.white) ||
            !isReadyTexture(fallback_textures.black) ||
            !isReadyTexture(fallback_textures.flat_normal) ||
            !isReadyTexture(fallback_textures.metallic_roughness)) {
            setFailure("Scene shading table update requires a ready TLAS and fallback textures.");
            return false;
        }

        if (!captureInputs(accepted_instances, fallback_textures)) {
            return false;
        }
        if (m_cache_valid && m_inputs == m_pending_inputs) {
            // TLAS replacement changes binding 0, not the shading table contents.
            if (m_bound_acceleration_structure != acceleration_structure->get()) {
                VulkanDescriptorWriter()
                    .writeAccelerationStructure(0, acceleration_structure->get())
                    .update(m_device, m_descriptor_allocation.set);
                m_bound_acceleration_structure = acceleration_structure->get();
                ++m_stats.descriptor_write_count;
            }
            ++m_stats.reuse_count;
            return true;
        }

        m_ready = false;
        m_stats.ready = false;
        m_stats.descriptor_ready = false;
        m_stats.texture_overflow_count = 0;
        m_stats.geometry_overflow_count = 0;
        m_stats.last_failure_reason = "None";
        auto& instance_records = m_pending_data.instances;
        auto& geometry_records = m_pending_data.geometries;
        auto& material_records = m_pending_data.materials;
        auto& texture_infos = m_pending_data.textures;
        auto& texture_upload_ids = m_pending_data.texture_upload_ids;
        auto& vertex_buffer_infos = m_pending_data.vertices;
        auto& index_buffer_infos = m_pending_data.indices;
        auto& geometry_indices = m_pending_data.geometry_indices;
        auto& material_indices = m_pending_data.material_indices;
        auto& texture_indices = m_pending_data.texture_indices;
        instance_records.assign(1, GpuRtInstanceRecord{});
        geometry_records.assign(1, GpuRtGeometryRecord{});
        material_records.assign(1, GpuRtMaterialRecord{});
        m_pending_data.geometry_keys.assign(1, VulkanMeshResourceKey{});
        geometry_indices.clear();
        material_indices.clear();
        texture_indices.clear();
        texture_infos.assign(
            m_texture_capacity,
            textureImageInfo(*fallback_textures.white));
        texture_upload_ids.assign(m_texture_capacity, fallback_textures.white->getUploadTicket().getId());
        for (uint32_t index = 0; index < kVulkanRtFallbackTextureSlotCount; ++index) {
            texture_upload_ids[index] = m_pending_inputs.fallbacks[index].upload_id;
        }
        VkDescriptorBufferInfo fallback_vertex_info{};
        fallback_vertex_info.buffer = m_fallback_vertex_buffer.get();
        fallback_vertex_info.offset = 0;
        fallback_vertex_info.range = VK_WHOLE_SIZE;
        VkDescriptorBufferInfo fallback_index_info{};
        fallback_index_info.buffer = m_fallback_index_buffer.get();
        fallback_index_info.offset = 0;
        fallback_index_info.range = VK_WHOLE_SIZE;
        vertex_buffer_infos.assign(
            m_geometry_descriptor_capacity,
            fallback_vertex_info);
        index_buffer_infos.assign(
            m_geometry_descriptor_capacity,
            fallback_index_info);
        texture_infos[kVulkanRtFallbackWhiteTextureIndex] =
            textureImageInfo(*fallback_textures.white);
        texture_infos[kVulkanRtFallbackBlackTextureIndex] =
            textureImageInfo(*fallback_textures.black);
        texture_infos[kVulkanRtFallbackFlatNormalTextureIndex] =
            textureImageInfo(*fallback_textures.flat_normal);
        texture_infos[kVulkanRtFallbackMetallicRoughnessTextureIndex] =
            textureImageInfo(*fallback_textures.metallic_roughness);
        texture_indices.emplace(
            fallback_textures.white,
            kVulkanRtFallbackWhiteTextureIndex);
        texture_indices.emplace(
            fallback_textures.black,
            kVulkanRtFallbackBlackTextureIndex);
        texture_indices.emplace(
            fallback_textures.flat_normal,
            kVulkanRtFallbackFlatNormalTextureIndex);
        texture_indices.emplace(
            fallback_textures.metallic_roughness,
            kVulkanRtFallbackMetallicRoughnessTextureIndex);

        auto resolveTextureIndex = [&](const VulkanTextureResource* texture) {
            if (!isReadyTexture(texture)) {
                return kVulkanRtFallbackWhiteTextureIndex;
            }

            const auto existing = texture_indices.find(texture);
            if (existing != texture_indices.end()) {
                return existing->second;
            }
            const uint32_t assigned_texture_count =
                static_cast<uint32_t>(texture_indices.size());
            if (assigned_texture_count >= m_texture_capacity) {
                ++m_stats.texture_overflow_count;
                return kVulkanRtFallbackWhiteTextureIndex;
            }

            const uint32_t index = assigned_texture_count;
            texture_indices.emplace(texture, index);
            texture_infos[index] = textureImageInfo(*texture);
            texture_upload_ids[index] = texture->getUploadTicket().getId();
            return index;
        };

        auto resolveMaterialIndex = [&](const VulkanMaterialResource* material) {
            if (material == nullptr || !material->isReady()) {
                return kVulkanRtInvalidTableIndex;
            }

            const auto existing = material_indices.find(material);
            if (existing != material_indices.end()) {
                return existing->second;
            }

            const uint32_t index = static_cast<uint32_t>(material_records.size());
            VulkanMaterialShadingData shading_data =
                material->getRayTracingShadingData();
            GpuRtMaterialRecord record = shading_data.record;
            record.texture_indices0 = glm::uvec4{
                resolveTextureIndex(shading_data.base_color),
                resolveTextureIndex(shading_data.normal),
                resolveTextureIndex(shading_data.metallic),
                resolveTextureIndex(shading_data.roughness)
            };
            record.texture_indices1 = glm::uvec4{
                resolveTextureIndex(shading_data.metallic_roughness),
                resolveTextureIndex(shading_data.ao),
                resolveTextureIndex(shading_data.emissive),
                shading_data.texture_flags
            };
            material_records.push_back(record);
            material_indices.emplace(material, index);
            return index;
        };

        for (const VulkanRayTracingInstanceRecord& instance : accepted_instances) {
            uint32_t geometry_index = kVulkanRtInvalidTableIndex;
            const auto geometry_it = geometry_indices.find(instance.mesh);
            if (geometry_it != geometry_indices.end()) {
                geometry_index = geometry_it->second;
            } else {
                geometry_index = static_cast<uint32_t>(geometry_records.size());
                if (geometry_index >= m_geometry_descriptor_capacity) {
                    ++m_stats.geometry_overflow_count;
                    setFailure("RT scene geometry descriptor capacity was exceeded.");
                    return false;
                }
                GpuRtGeometryRecord geometry_record;
                geometry_record.vertex_buffer_index = geometry_index;
                geometry_record.index_buffer_index = geometry_index;
                geometry_record.vertex_stride = sizeof(Vertex);
                geometry_record.vertex_count = instance.mesh->getVertexCount();
                geometry_record.index_count = instance.mesh->getIndexCount();
                geometry_record.flags = 1u;
                geometry_records.push_back(geometry_record);
                m_pending_data.geometry_keys.push_back(instance.mesh->getKey());
                geometry_indices.emplace(instance.mesh, geometry_index);
                vertex_buffer_infos[geometry_index].buffer =
                    instance.mesh->getVertexBuffer();
                index_buffer_infos[geometry_index].buffer =
                    instance.mesh->getIndexBuffer();
            }

            GpuRtInstanceRecord instance_record;
            instance_record.geometry_index = geometry_index;
            instance_record.material_index = resolveMaterialIndex(instance.material);
            instance_record.object_flags = instance.material != nullptr &&
                instance.material->getRayTracingShadingData().double_sided ? 1u : 0u;
            instance_record.entity_id = entityIdForGpu(instance.entity_id);
            instance_records.push_back(instance_record);
        }

        const VkDeviceSize instance_bytes =
            static_cast<VkDeviceSize>(instance_records.size() * sizeof(GpuRtInstanceRecord));
        const VkDeviceSize geometry_bytes =
            static_cast<VkDeviceSize>(geometry_records.size() * sizeof(GpuRtGeometryRecord));
        const VkDeviceSize material_bytes =
            static_cast<VkDeviceSize>(material_records.size() * sizeof(GpuRtMaterialRecord));
        const auto upload_changed = [&]<typename T>(
            VulkanOwnedBuffer& buffer, const std::vector<T>& records,
            const std::vector<T>& previous, const char* name) {
            if (m_cache_valid && records == previous) {
                return true;
            }
            const VkDeviceSize bytes = records.size() * sizeof(T);
            if (!ensureBuffer(buffer, bytes, name) ||
                !writeBuffer(buffer, records.data(), bytes)) {
                return false;
            }
            ++m_stats.buffer_upload_count;
            m_stats.uploaded_bytes += bytes;
            m_stats.last_uploaded_bytes += bytes;
            return true;
        };
        if (!upload_changed(m_instance_buffer, instance_records, m_data.instances, "RT scene instance table") ||
            !upload_changed(m_geometry_buffer, geometry_records, m_data.geometries, "RT scene geometry table") ||
            !upload_changed(m_material_buffer, material_records, m_data.materials, "RT scene material table")) {
            setFailure("Scene shading table buffer allocation or upload failed.");
            return false;
        }

        const std::array<VkDescriptorBufferInfo, 3> buffer_infos{{
            { m_instance_buffer.get(), 0, instance_bytes },
            { m_geometry_buffer.get(), 0, geometry_bytes },
            { m_material_buffer.get(), 0, material_bytes }
        }};
        VulkanDescriptorWriter writer;
        uint64_t descriptor_writes = 0;
        if (!m_cache_valid || m_bound_acceleration_structure != acceleration_structure->get()) {
            writer.writeAccelerationStructure(0, acceleration_structure->get());
            ++descriptor_writes;
        }
        for (uint32_t index = 0; index < buffer_infos.size(); ++index) {
            if (!m_cache_valid || !sameDescriptor(m_bound_buffers[index], buffer_infos[index])) {
                writer.writeBuffer(index + 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffer_infos[index]);
                ++descriptor_writes;
            }
        }
        // Recreated resources can reuse Vulkan handle values. Upload identities
        // and mesh generations still require the corresponding descriptors to refresh.
        if (!m_cache_valid || !sameDescriptors(texture_infos, m_data.textures) ||
            texture_upload_ids != m_data.texture_upload_ids) {
            writer.writeImageArray(4, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, texture_infos);
            ++descriptor_writes;
        }
        if (!m_cache_valid || m_bound_sampler != m_pending_inputs.sampler ||
            m_inputs.fallbacks[0].upload_id != m_pending_inputs.fallbacks[0].upload_id) {
            VkDescriptorImageInfo sampler_info{};
            sampler_info.sampler = m_pending_inputs.sampler;
            writer.writeImage(5, VK_DESCRIPTOR_TYPE_SAMPLER, sampler_info);
            ++descriptor_writes;
        }
        const bool geometry_resources_changed = m_pending_data.geometry_keys != m_data.geometry_keys;
        if (!m_cache_valid || geometry_resources_changed ||
            !sameDescriptors(vertex_buffer_infos, m_data.vertices)) {
            writer.writeBufferArray(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertex_buffer_infos);
            ++descriptor_writes;
        }
        if (!m_cache_valid || geometry_resources_changed ||
            !sameDescriptors(index_buffer_infos, m_data.indices)) {
            writer.writeBufferArray(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, index_buffer_infos);
            ++descriptor_writes;
        }
        if (descriptor_writes > 0) {
            writer.update(m_device, m_descriptor_allocation.set);
        }
        m_stats.descriptor_write_count += descriptor_writes;
        m_bound_buffers = buffer_infos;
        m_bound_acceleration_structure = acceleration_structure->get();
        m_bound_sampler = m_pending_inputs.sampler;

        m_stats.ready = true;
        m_stats.descriptor_ready = true;
        m_stats.instance_count = static_cast<uint32_t>(instance_records.size());
        m_stats.geometry_count = static_cast<uint32_t>(geometry_records.size());
        m_stats.material_count = static_cast<uint32_t>(material_records.size());
        m_stats.texture_count = static_cast<uint32_t>(texture_indices.size());
        m_stats.instance_buffer_bytes = instance_bytes;
        m_stats.geometry_buffer_bytes = geometry_bytes;
        m_stats.material_buffer_bytes = material_bytes;
        ++m_stats.rebuild_count;
        std::swap(m_inputs, m_pending_inputs);
        std::swap(m_data, m_pending_data);
        m_cache_valid = true;
        m_ready = true;
        return true;
    }

    void VulkanRayTracingSceneShadingTable::recordShaderReadBarrier(
        VkCommandBuffer command_buffer) const {
        if (!m_ready || command_buffer == VK_NULL_HANDLE) {
            return;
        }

        VkBufferMemoryBarrier2 barriers[3]{};
        const VulkanOwnedBuffer* buffers[] = {
            &m_instance_buffer,
            &m_geometry_buffer,
            &m_material_buffer
        };
        for (uint32_t index = 0; index < 3; ++index) {
            barriers[index].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            barriers[index].srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
            barriers[index].srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
            barriers[index].dstStageMask =
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            barriers[index].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            barriers[index].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[index].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[index].buffer = buffers[index]->get();
            barriers[index].size = buffers[index]->getSize();
        }

        VkDependencyInfo dependency_info{};
        dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency_info.bufferMemoryBarrierCount = 3;
        dependency_info.pBufferMemoryBarriers = barriers;
        vkCmdPipelineBarrier2(command_buffer, &dependency_info);
    }

    bool VulkanRayTracingSceneShadingTable::ensureBuffer(
        VulkanOwnedBuffer& buffer,
        VkDeviceSize required_size,
        const char* debug_name) {
        if (buffer.isReady() && buffer.getSize() >= required_size) {
            return true;
        }

        VulkanOwnedBufferCreateInfo create_info;
        create_info.size = required_size;
        create_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        create_info.memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        create_info.allocation_flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        create_info.debug_name = debug_name != nullptr ? debug_name : "RT scene table";

        VulkanOwnedBuffer replacement;
        if (!replacement.create(*m_gpu_allocator, create_info)) {
            return false;
        }
        buffer = std::move(replacement);
        return true;
    }

    bool VulkanRayTracingSceneShadingTable::writeBuffer(
        const VulkanOwnedBuffer& buffer,
        const void* data,
        VkDeviceSize size) const {
        if (!buffer.isReady() || data == nullptr || size > buffer.getSize()) {
            return false;
        }

        void* mapped_data = nullptr;
        if (!buffer.map(mapped_data) || mapped_data == nullptr) {
            return false;
        }
        std::memcpy(mapped_data, data, static_cast<size_t>(size));
        const bool flushed = buffer.isHostCoherent() || buffer.flush(0, size);
        buffer.unmap();
        return flushed;
    }

    void VulkanRayTracingSceneShadingTable::setFailure(std::string reason) {
        m_ready = false;
        m_cache_valid = false;
        m_stats.ready = false;
        m_stats.descriptor_ready = false;
        m_stats.instance_count = 0;
        m_stats.geometry_count = 0;
        m_stats.material_count = 0;
        m_stats.texture_count = 0;
        m_stats.instance_buffer_bytes = 0;
        m_stats.geometry_buffer_bytes = 0;
        m_stats.material_buffer_bytes = 0;
        m_stats.last_failure_reason = std::move(reason);
        NX_CORE_WARN("RT scene shading table unavailable: {}", m_stats.last_failure_reason);
    }
} // namespace NexAur

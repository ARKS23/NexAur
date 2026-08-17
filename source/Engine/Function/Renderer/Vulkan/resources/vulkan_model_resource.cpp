#include "pch.h"
#include "vulkan_model_resource.h"

#include "Function/Resource/asset_manager.h"
#include "Function/Resource/material_asset.h"
#include "Function/Resource/model.h"
#include "Function/Renderer/Vulkan/vulkan_render_resource_cache.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace NexAur {
    bool VulkanModelResource::create(
        const VulkanResourceUploadContext& context,
        const Model& model,
        AssetHandle model_asset,
        uint64_t generation,
        const std::string& debug_name,
        VulkanRenderResourceCache& resource_cache,
        AssetManager& asset_manager) {
        reset();

        if (!context.valid() || !model_asset || generation == 0) {
            NX_CORE_ERROR(
                "VulkanModelResource requires a valid upload context and model identity.");
            return false;
        }
        if (!model.isLoaded()) {
            NX_CORE_ERROR("VulkanModelResource requires a loaded CPU model.");
            return false;
        }

        const std::vector<Mesh>& cpu_meshes = model.getMeshes();
        if (cpu_meshes.empty()) {
            NX_CORE_ERROR("VulkanModelResource requires at least one mesh.");
            return false;
        }
        if (cpu_meshes.size() > std::numeric_limits<uint32_t>::max()) {
            NX_CORE_ERROR("VulkanModelResource has too many meshes: {}", debug_name);
            return false;
        }

        m_debug_name = debug_name;
        m_model_asset = model_asset;
        m_generation = generation;
        m_meshes.reserve(cpu_meshes.size());
        m_materials.reserve(cpu_meshes.size());
        m_material_assets.reserve(cpu_meshes.size());

        for (size_t mesh_index = 0; mesh_index < cpu_meshes.size(); ++mesh_index) {
            const Mesh& cpu_mesh = cpu_meshes[mesh_index];
            VulkanMeshResource mesh_resource;
            VulkanMeshResourceKey mesh_key;
            mesh_key.identity.model_asset = model_asset;
            mesh_key.identity.mesh_index = static_cast<uint32_t>(mesh_index);
            mesh_key.generation = generation;
            if (!mesh_resource.create(context, cpu_mesh, mesh_key)) {
                NX_CORE_ERROR("Failed to create Vulkan mesh resource for model: {}", debug_name);
                reset();
                return false;
            }

            std::shared_ptr<MaterialAsset> material_asset = asset_manager.createMaterialFromImportData(cpu_mesh.getMaterialImportData());
            if (!material_asset) {
                NX_CORE_ERROR("Failed to create CPU material asset for model: {}", debug_name);
                reset();
                return false;
            }

            m_meshes.push_back(std::move(mesh_resource));
            m_materials.emplace_back();
            m_material_assets.push_back(std::move(material_asset));

            MaterialAsset& stored_material = *m_material_assets.back();
            if (resource_cache.areMaterialTexturesReady(
                    stored_material,
                    asset_manager) &&
                !resource_cache.createMaterialResource(
                    m_materials.back(),
                    stored_material,
                    asset_manager)) {
                NX_CORE_ERROR("Failed to create Vulkan material resource for model: {}", debug_name);
                reset();
                return false;
            }
        }

        return true;
    }

    void VulkanModelResource::reset() {
        m_material_assets.clear();
        m_materials.clear();
        m_meshes.clear();
        m_debug_name.clear();
        m_model_asset = {};
        m_generation = 0;
    }

    void VulkanModelResource::refreshMaterials(
        VulkanRenderResourceCache& resource_cache,
        AssetManager& asset_manager) {
        const size_t material_count = std::min(
            m_materials.size(),
            m_material_assets.size());
        for (size_t index = 0; index < material_count; ++index) {
            if (m_materials[index].isReady() || !m_material_assets[index] ||
                !resource_cache.areMaterialTexturesReady(
                    *m_material_assets[index],
                    asset_manager)) {
                continue;
            }

            VulkanMaterialResource material;
            if (resource_cache.createMaterialResource(
                    material,
                    *m_material_assets[index],
                    asset_manager)) {
                m_materials[index] = std::move(material);
            } else {
                NX_CORE_ERROR(
                    "Failed to finalize Vulkan material resource for model: {}",
                    m_debug_name);
            }
        }
    }

    bool VulkanModelResource::isReady() const {
        if (m_meshes.empty()) {
            return false;
        }

        return std::all_of(m_meshes.begin(), m_meshes.end(), [](const VulkanMeshResource& mesh) {
            return mesh.isReady();
        });
    }

    bool VulkanModelResource::hasUploadFailed() const {
        return std::any_of(
            m_meshes.begin(),
            m_meshes.end(),
            [](const VulkanMeshResource& mesh) {
                const VulkanUploadStatus status = mesh.getUploadStatus();
                return status == VulkanUploadStatus::Failed ||
                       status == VulkanUploadStatus::Cancelled;
            });
    }
} // namespace NexAur

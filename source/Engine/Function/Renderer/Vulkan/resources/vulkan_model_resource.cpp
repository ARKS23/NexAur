#include "pch.h"
#include "vulkan_model_resource.h"

#include "Function/Resource/asset_manager.h"
#include "Function/Resource/material_asset.h"
#include "Function/Resource/model.h"
#include "Function/Renderer/Vulkan/vulkan_render_resource_cache.h"

#include <algorithm>
#include <utility>

namespace NexAur {
    bool VulkanModelResource::create(
        const VulkanResourceUploadContext& context,
        const Model& model,
        const std::string& debug_name,
        VulkanRenderResourceCache& resource_cache,
        AssetManager& asset_manager) {
        reset();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanModelResource requires a valid upload context.");
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

        m_debug_name = debug_name;
        m_meshes.reserve(cpu_meshes.size());
        m_materials.reserve(cpu_meshes.size());

        for (const Mesh& cpu_mesh : cpu_meshes) {
            VulkanMeshResource mesh_resource;
            if (!mesh_resource.create(context, cpu_mesh)) {
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

            VulkanMaterialResource material_resource;
            if (!resource_cache.createMaterialResource(material_resource, *material_asset, asset_manager)) {
                NX_CORE_ERROR("Failed to create Vulkan material resource for model: {}", debug_name);
                reset();
                return false;
            }

            m_meshes.push_back(std::move(mesh_resource));
            m_materials.push_back(std::move(material_resource));
        }

        return true;
    }

    void VulkanModelResource::reset() {
        m_materials.clear();
        m_meshes.clear();
        m_debug_name.clear();
    }

    bool VulkanModelResource::isReady() const {
        if (m_meshes.empty()) {
            return false;
        }

        return std::all_of(m_meshes.begin(), m_meshes.end(), [](const VulkanMeshResource& mesh) {
            return mesh.isReady();
        });
    }
} // namespace NexAur

#pragma once

#include <string>
#include <vector>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/resources/vulkan_material_resource.h"
#include "Function/Renderer/Vulkan/resources/vulkan_mesh_resource.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class AssetManager;
    class Model;
    class VulkanRenderResourceCache;

    class NEXAUR_API VulkanModelResource {
    public:
        VulkanModelResource() = default;
        ~VulkanModelResource() = default;

        VulkanModelResource(const VulkanModelResource&) = delete;
        VulkanModelResource& operator=(const VulkanModelResource&) = delete;

        bool create(
            const VulkanResourceUploadContext& context,
            const Model& model,
            const std::string& debug_name,
            VulkanRenderResourceCache& resource_cache,
            AssetManager& asset_manager);
        void reset();

        bool isReady() const;
        const std::string& getDebugName() const { return m_debug_name; }
        const std::vector<VulkanMeshResource>& getMeshes() const { return m_meshes; }
        const std::vector<VulkanMaterialResource>& getMaterials() const { return m_materials; }

    private:
        std::string m_debug_name;
        std::vector<VulkanMeshResource> m_meshes;
        std::vector<VulkanMaterialResource> m_materials;
    };
} // namespace NexAur

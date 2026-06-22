#include "pch.h"
#include "render_resource_uploader.h"

#include "Function/Renderer/data/render_data.h"
#include "Function/Renderer/RHI/render_device.h"
#include "Function/Renderer/Resources/render_resource_cache.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Resource/model.h"

namespace NexAur {
    std::shared_ptr<RenderModelData> RenderResourceUploader::uploadModelData(
        const std::shared_ptr<Model>& cpu_model,
        AssetManager& asset_manager,
        RenderResourceCache& resource_cache) {
        if (!cpu_model || !cpu_model->isLoaded()) {
            NX_CORE_ERROR("Invalid model provided for upload.");
            return nullptr;
        }

        std::shared_ptr<RenderModelData> render_model = std::make_shared<RenderModelData>();
        for (const Mesh& mesh : cpu_model->getMeshes()) {
            render_model->meshes.push_back(uploadSingleMesh(mesh, asset_manager, resource_cache));
        }

        return render_model;
    }

    RenderMeshData RenderResourceUploader::uploadSingleMesh(const Mesh& cpu_mesh, AssetManager& asset_manager, RenderResourceCache& resource_cache) {
        RenderMeshData gpu_mesh;

        // 顶点上传仍然走当前 RHI 工厂接口，后续 Vulkan 可在这里切换为 staging buffer。
        gpu_mesh.vertex_array = RendererFactory::createVertexArray();
        const std::vector<Vertex>& vertices = cpu_mesh.GetVertices();
        auto vbo = RendererFactory::createVertexBuffer(reinterpret_cast<float*>(const_cast<Vertex*>(vertices.data())), vertices.size() * sizeof(Vertex));
        vbo->setLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoords" },
            { ShaderDataType::Float3, "a_Tangent" },
            { ShaderDataType::Float3, "a_Bitangent" }
        });

        const std::vector<unsigned int>& indices = cpu_mesh.GetIndices();
        auto ebo = RendererFactory::createIndexBuffer(reinterpret_cast<uint32_t*>(const_cast<unsigned int*>(indices.data())), indices.size());
        gpu_mesh.index_count = static_cast<uint32_t>(indices.size());

        gpu_mesh.vertex_array->addVertexBuffer(vbo);
        gpu_mesh.vertex_array->setIndexBuffer(ebo);

        const MaterialData& material_data = cpu_mesh.GetMaterialData();
        auto resolve_texture = [&](const std::string& texture_path) {
            AssetHandle texture_asset = asset_manager.importTextureAsset(texture_path);
            if (!texture_asset) {
                return std::shared_ptr<Texture2D>();
            }

            return resource_cache.getOrCreateTexture2D(texture_asset, asset_manager);
        };

        gpu_mesh.material.albedo_map = resolve_texture(material_data.albedo_path);
        gpu_mesh.material.normal_map = resolve_texture(material_data.normal_path);
        gpu_mesh.material.metallic_map = resolve_texture(material_data.metallic_path);
        gpu_mesh.material.roughness_map = resolve_texture(material_data.roughness_path);
        gpu_mesh.material.ao_map = resolve_texture(material_data.ao_path);

        return gpu_mesh;
    }
} // namespace NexAur

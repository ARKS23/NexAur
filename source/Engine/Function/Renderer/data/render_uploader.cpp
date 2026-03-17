#include "pch.h"
#include "render_uploader.h"
#include "Function/Renderer/data/render_data.h"
#include "Function/Renderer/RHI/vertex_array.h"
#include "Function/Renderer/RHI/buffer.h"
#include "Function/Resource/asset_manager.h"

namespace NexAur {
    std::shared_ptr<RenderModelData> RenderResourceUploader::uploadModelData(const std::shared_ptr<Model>& cpu_model) {
        if (!cpu_model || !cpu_model->isLoaded()) {
            NX_CORE_ERROR("Invalid model provided for upload.");
            return nullptr;
        }

        std::shared_ptr<RenderModelData> render_model = std::make_shared<RenderModelData>();
        for (const Mesh& mesh : cpu_model->getMeshes()) {
            render_model->meshes.push_back(uploadSingleMesh(mesh, cpu_model->getDirectory()));
        }

        return render_model;
    }

    RenderMeshData RenderResourceUploader::uploadSingleMesh(const Mesh& cpu_mesh, const std::string& parent_dir) {
        RenderMeshData gpu_mesh;

        // 顶点数据上传: VAO + VBO + EBO
        gpu_mesh.vertex_array = VertexArray::create(); // VAO
        const std::vector<Vertex>& vertices = cpu_mesh.GetVertices();
        auto vbo = VertexBuffer::create(reinterpret_cast<float*>(const_cast<Vertex*>(vertices.data())), vertices.size() * sizeof(Vertex)); // VBO
        vbo->setLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoords" },
            { ShaderDataType::Float3, "a_Tangent" },
            { ShaderDataType::Float3, "a_Bitangent" }
        });
        const std::vector<unsigned int>& indices = cpu_mesh.GetIndices();
        auto ebo = IndexBuffer::create(reinterpret_cast<uint32_t*>(const_cast<unsigned int*>(indices.data())), indices.size() * sizeof(unsigned int)); // EBO
        gpu_mesh.index_count = indices.size();

        gpu_mesh.vertex_array->addVertexBuffer(vbo);
        gpu_mesh.vertex_array->setIndexBuffer(ebo);

        // 处理材质
        const MaterialData& material_data = cpu_mesh.GetMaterialData();
        // AssetManager加载材质
        AssetManager& asset_manager = AssetManager::getInstance();
        UUID albedo_uuid = asset_manager.loadTexture(material_data.albedo_path);
        UUID normal_uuid = asset_manager.loadTexture(material_data.normal_path);
        UUID metallic_uuid = asset_manager.loadTexture(material_data.metallic_path);
        UUID roughness_uuid = asset_manager.loadTexture(material_data.roughness_path);
        UUID ao_uuid = asset_manager.loadTexture(material_data.ao_path);

        gpu_mesh.material.albedo_map = albedo_uuid == INVALID_UUID ? nullptr : asset_manager.getTexture(albedo_uuid);
        gpu_mesh.material.normal_map = normal_uuid == INVALID_UUID ? nullptr : asset_manager.getTexture(normal_uuid);
        gpu_mesh.material.metallic_map = metallic_uuid == INVALID_UUID ? nullptr : asset_manager.getTexture(metallic_uuid);
        gpu_mesh.material.roughness_map = roughness_uuid == INVALID_UUID ? nullptr : asset_manager.getTexture(roughness_uuid);
        gpu_mesh.material.ao_map = ao_uuid == INVALID_UUID ? nullptr : asset_manager.getTexture(ao_uuid);

        return gpu_mesh;
    }
} // namespace NexAur

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

        auto render_model = std::make_shared<RenderModelData>();
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
        // TODO: AssetManager加载贴图
        

        return gpu_mesh;
    }
} // namespace NexAur

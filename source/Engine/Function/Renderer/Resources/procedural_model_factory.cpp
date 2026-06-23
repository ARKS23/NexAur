#include "pch.h"
#include "procedural_model_factory.h"
#include "Function/Renderer/data/render_data.h"
#include "Function/Renderer/RHI/render_device.h"

namespace NexAur {
    static std::shared_ptr<VertexArray> genCubeMeshVAO() {
        auto vertex_array = RendererFactory::createVertexArray();

        float vertices[] = {
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,

		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 0.0f,

		-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

		 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

		-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f};

        unsigned int indexArray[36];
        for (int i = 0; i < 36; ++i) indexArray[i] = i;

        auto vbo = RendererFactory::createVertexBuffer(vertices, sizeof(vertices));
        vbo->setLayout({
            { ShaderDataType::Float3, "a_Pos" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoord" }
        });

        auto ebo = RendererFactory::createIndexBuffer(indexArray, 36);

        vertex_array->addVertexBuffer(vbo);
        vertex_array->setIndexBuffer(ebo);

        return vertex_array;
    }

    static std::shared_ptr<VertexArray> genSphereMeshVAO(unsigned int x_segments, unsigned int y_segments) {
        auto vertex_array = RendererFactory::createVertexArray();

        std::vector<float> data;
        std::vector<unsigned int> indices;

        const unsigned int X_SEGMENTS = x_segments;
        const unsigned int Y_SEGMENTS = y_segments;
        const float PI = 3.14159265359f;

        // 1. 生成顶点数据 (交错排列: Position, Normal, UV)
        for (unsigned int y = 0; y <= Y_SEGMENTS; ++y) {
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
                // 计算归一化的比例 (0.0 到 1.0)
                float xSegment = (float)x / (float)X_SEGMENTS;
                float ySegment = (float)y / (float)Y_SEGMENTS;

                // 球坐标系 -> 笛卡尔坐标系转换
                float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                float yPos = std::cos(ySegment * PI);
                float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                // Position (3 floats)
                data.push_back(xPos);
                data.push_back(yPos);
                data.push_back(zPos);

                // Normal (3 floats) - 对于原点在(0,0,0)的单位球，法线就是位置本身
                data.push_back(xPos);
                data.push_back(yPos);
                data.push_back(zPos);

                // TexCoord / UV (2 floats)
                data.push_back(xSegment);
                data.push_back(ySegment);
            }
        }

        // 2. 生成索引 (使用标准的 GL_TRIANGLES 生成面，兼容引擎架构)
        for (unsigned int y = 0; y < Y_SEGMENTS; ++y) {
            for (unsigned int x = 0; x < X_SEGMENTS; ++x) {
                unsigned int current = y * (X_SEGMENTS + 1) + x;
                unsigned int next    = current + 1;
                unsigned int below   = (y + 1) * (X_SEGMENTS + 1) + x;
                unsigned int belowNext = below + 1;

                // 第一个三角形
                indices.push_back(current);
                indices.push_back(below);
                indices.push_back(next);

                // 第二个三角形
                indices.push_back(next);
                indices.push_back(below);
                indices.push_back(belowNext);
            }
        }

        // 3. 上传数据到 GPU
        auto vbo = RendererFactory::createVertexBuffer(
            data.data(),
            static_cast<uint32_t>(data.size() * sizeof(float)));
        
        vbo->setLayout({
            { ShaderDataType::Float3, "a_Pos" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoord" } 
        });
        vertex_array->addVertexBuffer(vbo);

        auto ebo = RendererFactory::createIndexBuffer(indices.data(), static_cast<uint32_t>(indices.size()));
        vertex_array->setIndexBuffer(ebo);

        return vertex_array;
    }

    std::shared_ptr<RenderModelData> ProceduralModelFactory::createSphereModel(unsigned int x_segments, unsigned int y_segments) {
        std::shared_ptr<RenderModelData> model_data = std::make_shared<RenderModelData>();
        std::shared_ptr<VertexArray> sphere_vertex_array = genSphereMeshVAO(x_segments, y_segments);
        RenderMeshData mesh_data;
        mesh_data.vertex_array = sphere_vertex_array;
        mesh_data.index_count = x_segments * y_segments * 6;
        model_data->meshes.push_back(mesh_data);
        return model_data;
    }

    std::shared_ptr<RenderModelData> ProceduralModelFactory::createCubeModel() {
        std::shared_ptr<RenderModelData> model_data = std::make_shared<RenderModelData>();
        std::shared_ptr<VertexArray> cube_vertex_array = genCubeMeshVAO();
        RenderMeshData mesh_data;
        mesh_data.vertex_array = cube_vertex_array;
        mesh_data.index_count = 36;
        model_data->meshes.push_back(mesh_data);
        return model_data;
    }
} // namespace NexAur

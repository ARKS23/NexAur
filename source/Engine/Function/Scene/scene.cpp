#include "pch.h"
#include "scene.h"
#include "Function/Renderer/RHI/renderer_system.h"

namespace NexAur {
    Scene::Scene() {
        // 初始化场景
        RenderEntity cube_mesh;
        //RenderEntity sphere_mesh;

        std::shared_ptr<VertexArray> cube_vertex_array = MeshFactory::createCubeMesh();
        std::shared_ptr<Shader> shader = RendererFactory::createShaderByPaths("container shader", 
        "E:/ComputerGraphics/NexAur/assets/shaders/container.vs", "E:/ComputerGraphics/NexAur/assets/shaders/container.fs");
        std::shared_ptr<Texture2D> texture = RendererFactory::createTexture2D("E:/ComputerGraphics/NexAur/assets/textures/container/container.jpg");
        std::shared_ptr<Material> material = RendererFactory::createMaterial(shader);
        material->setTexture("u_DiffuseMap", texture);
        cube_mesh.mesh = cube_vertex_array;
        cube_mesh.material = material;
        cube_mesh.name = "cube_test";
        m_entities.push_back(cube_mesh);
    }

    void Scene::addEntity(const RenderEntity& entity) {
        m_entities.push_back(entity);
    }

    void Scene::onUpdate(float deltaTime) {
        // 更新场景逻辑，例如动画、物理等
    }

    std::shared_ptr<VertexArray> MeshFactory::createCubeMesh() {
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

    std::shared_ptr<VertexArray> MeshFactory::createSphereMesh() {
        auto vertex_array = RendererFactory::createVertexArray();

        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        const float radius = 1.0f;
        const int sectors = 36; // 经度切分 (竖着切)
        const int stacks = 18;  // 纬度切分 (横着切)
        const float M_PI = 3.14159265358979323846f;

        float sectorStep = 2 * M_PI / sectors;
        float stackStep = M_PI / stacks;

        for (int i = 0; i <= stacks; ++i) {
            float stackAngle = M_PI / 2 - i * stackStep;        // from pi/2 to -pi/2
            float xy = radius * cosf(stackAngle);             // r * cos(u)
            float z = radius * sinf(stackAngle);              // r * sin(u)

            // 每一层的点
            for (int j = 0; j <= sectors; ++j) {
                float sectorAngle = j * sectorStep;           // from 0 to 2pi

                // 顶点坐标 (Position)
                float x = xy * cosf(sectorAngle);
                float y = xy * sinf(sectorAngle);
                
                // 添加 Position (Layout: 0) -> Float3
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);

                // 添加 Normal (法线，球心的法线就是归一化的坐标) -> Float3
                // (对于球心在原点的球，法线就是 pos / radius)
                vertices.push_back(x / radius);
                vertices.push_back(y / radius);
                vertices.push_back(z / radius);

                // 可以通过 Normal 简单当做颜色来通过测试： Color = Normal * 0.5 + 0.5
            }
        }

        for (int i = 0; i < stacks; ++i) {
            int k1 = i * (sectors + 1);     // 当前层起始索引
            int k2 = k1 + sectors + 1;      // 下一层起始索引

            for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
                // 两个三角形构成一个四边形面
                
                // k1 != 0 顶端不需要画第一个三角形
                if (i != 0) {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }

                // k1 != stacks-1 底端不需要画第二个三角形
                if (i != (stacks - 1)) {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }

        // 3. 上传数据
        auto vbo = RendererFactory::createVertexBuffer(vertices.data(), vertices.size() * sizeof(float));
        // Layout: Pos(3) + Normal(3)
        vbo->setLayout({ 
            { ShaderDataType::Float3, "a_Pos" }, 
            { ShaderDataType::Float3, "a_Normal" } 
        });
        vertex_array->addVertexBuffer(vbo);

        auto ebo = RendererFactory::createIndexBuffer(indices.data(), indices.size());
        vertex_array->setIndexBuffer(ebo);

        return vertex_array;
    }
} // namespace NexAur


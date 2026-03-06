#include "pch.h"
#include <string>
#include "scene.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Global/global_context.h"
#include "Function/File/file_system.h"
#include "Function/Input/input_system.h"

namespace NexAur {
    static RenderEntity createLightCube(glm::vec3 position, glm::vec3 color) {
        RenderEntity light_cube;

        // 光源立方体
        std::shared_ptr<VertexArray> cube_vertex_array = MeshFactory::createCubeMesh();
        std::shared_ptr<Shader> light_shader = RendererFactory::createShaderByPaths("light cube shader", 
        NX_ASSET("assets/shaders/lightCube/light_cube.vs"), NX_ASSET("assets/shaders/lightCube/light_cube.fs"));
        
        std::shared_ptr<Material> light_material = RendererFactory::createMaterial(light_shader);
        light_material->setFloat3("u_Color", color);
        light_cube.mesh = cube_vertex_array;
        light_cube.material = light_material;
        light_cube.name = "light_cube";
        light_cube.transform = glm::scale(light_cube.transform, glm::vec3(0.2f));
        light_cube.transform = glm::translate(light_cube.transform, position);

        return light_cube;
    }

    static RenderEntity createPhongCubeEntity() {
        RenderEntity cube_mesh_phong;

        // blling-phong Cube
        std::shared_ptr<VertexArray> cube_vertex_array = MeshFactory::createCubeMesh();
        std::shared_ptr<Shader> phong_shader = RendererFactory::createShaderByPaths("container phong shader", 
        NX_ASSET("assets/shaders/phong/phong.vs"), NX_ASSET("assets/shaders/phong/phong.fs"));
        
        std::shared_ptr<Material> phong_material = RendererFactory::createMaterial(phong_shader);
        std::shared_ptr<Texture2D> diffuse_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/container/container2.png"));
        std::shared_ptr<Texture2D> specular_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/container/container2_specular.png"));
        phong_material->setTexture("u_Material.diffuse", diffuse_texture);
        phong_material->setTexture("u_Material.specular", specular_texture);
        phong_material->setFloat("u_Material.shininess", 32.0f);
        cube_mesh_phong.mesh = cube_vertex_array;
        cube_mesh_phong.material = phong_material;
        cube_mesh_phong.name = "cube_test_phong";
        cube_mesh_phong.transform = glm::translate(cube_mesh_phong.transform, glm::vec3(1.5f, 0.0f, 0.0f));

        return cube_mesh_phong;
    }

    static RenderEntity createFloorEntity() {
        RenderEntity floor_entity;

        // 地面
        std::shared_ptr<VertexArray> floor_vertex_array = MeshFactory::createCubeMesh();
        std::shared_ptr<Shader> floor_shader = RendererFactory::createShaderByPaths("floor shader", 
        NX_ASSET("assets/shaders/phong/phong.vs"), NX_ASSET("assets/shaders/phong/phong.fs"));
        std::shared_ptr<Material> floor_material = RendererFactory::createMaterial(floor_shader);
        std::shared_ptr<Texture2D> floor_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/wood/wood.png"));
        std::shared_ptr<Texture2D> specular_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/PBR/gold/roughness.png"));
        floor_material->setTexture("u_Material.diffuse", floor_texture);
        floor_material->setTexture("u_Material.specular", specular_texture);
        floor_entity.mesh = floor_vertex_array;
        floor_entity.material = floor_material;
        floor_entity.name = "floor";
        floor_entity.transform = glm::translate(floor_entity.transform, glm::vec3(0.0f, -5.0f, 0.0f));
        floor_entity.transform = glm::rotate(floor_entity.transform, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        floor_entity.transform = glm::scale(floor_entity.transform, glm::vec3(50.0f, 50.f, 1.0f));

        return floor_entity;
    }

    static RenderEntity createPhongSphereEntity() {
        RenderEntity sphere_mesh_phong;

        // blling-phong Sphere
        std::shared_ptr<VertexArray> sphere_vertex_array = MeshFactory::createSphereMesh();
        std::shared_ptr<Shader> phong_shader = RendererFactory::createShaderByPaths("sphere phong shader", 
        NX_ASSET("assets/shaders/phong/phong.vs"), NX_ASSET("assets/shaders/phong/phong.fs"));
        std::shared_ptr<Material> phong_material = RendererFactory::createMaterial(phong_shader);

        std::shared_ptr<Texture2D> diffuse_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/PBR/plastic/albedo.png"));
        std::shared_ptr<Texture2D> specular_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/PBR/plastic/roughness.png"));
        phong_material->setTexture("u_Material.diffuse", diffuse_texture);
        phong_material->setTexture("u_Material.specular", specular_texture);
        phong_material->setFloat("u_Material.shininess", 32.0f);

        sphere_mesh_phong.mesh = sphere_vertex_array;
        sphere_mesh_phong.material = phong_material;
        sphere_mesh_phong.name = "sphere_test_phong";
        sphere_mesh_phong.transform = glm::translate(sphere_mesh_phong.transform, glm::vec3(1.5f, -2.0f, 0.0f));
        sphere_mesh_phong.transform = glm::scale(sphere_mesh_phong.transform, glm::vec3(0.7f));

        return sphere_mesh_phong;
    }

    Scene::Scene() {
        // 初始化光源
        initLight();

        // 初始化场景
        RenderEntity cube_mesh_phong = createPhongCubeEntity();
        m_entities.push_back(cube_mesh_phong);
        RenderEntity sphere_mesh_phong = createPhongSphereEntity();
        m_entities.push_back(sphere_mesh_phong);
        RenderEntity floor_entity = createFloorEntity();
        m_entities.push_back(floor_entity);

        for (int i = 0; i < 5; ++i) {
            RenderEntity entity = cube_mesh_phong;
            entity.transform = glm::translate(entity.transform, glm::vec3(i * 2.0f, 0.0f, 0.0f));
            entity.name = "cube_" + std::to_string(i);
            m_entities.push_back(entity);
        }
    }

    void Scene::addEntity(const RenderEntity& entity) {
        m_entities.push_back(entity);
    }

    void Scene::onUpdate(float deltaTime) {
        // 天空盒查询更新(临时方案)
        std::shared_ptr<InputSystem> input_system = g_runtime_global_context.m_input_system;
        static bool z_key_pressed_last_frame = false;
        bool z_key_pressed = input_system->isKeyPressed(KeyCode::Z);
        if (z_key_pressed && !z_key_pressed_last_frame) {
            m_skybox_enabled = !m_skybox_enabled;
            m_directional_light.intensity = m_skybox_enabled ? 3.0f : 0.0f; // 启用天空盒时增加定向光强度，模拟白天效果
        }
        z_key_pressed_last_frame = z_key_pressed;

        // 定向光绕X轴旋转，模拟日出日落
        float rotationSpeed = glm::radians(30.0f * deltaTime);
        // glm::mat4 lightRotation = glm::rotate(glm::mat4(1.0f), rotationSpeed, glm::vec3(1.0f, 0.0f, 0.0f));
        // m_directional_light.direction = glm::vec3(lightRotation * glm::vec4(m_directional_light.direction, 0.0f));
        // m_directional_light.direction = glm::normalize(m_directional_light.direction);

        // 点光源绕Y轴旋转，模拟灯光旋转效果
        float deltaAngle = glm::radians(30.0f * deltaTime); 
        glm::vec3 orbitCenter = glm::vec3(5.0f, 1.0f, 0.0f);
        float targetRadius = 3.0f; 

        for (int i = 0; i < m_point_lights.size(); ++i) {
            PointLight& point_light = m_point_lights[i];

            // 1. 计算光源相对于“圆心”的相对坐标
            float relX = point_light.position.x - orbitCenter.x;
            float relZ = point_light.position.z - orbitCenter.z;

            // 2. 根据相对坐标反推当前角度
            float currentAngle = std::atan2(relZ, relX);
            float newAngle = currentAngle + deltaAngle;

            // 3. 计算新的相对位置
            point_light.position.x = orbitCenter.x + targetRadius * std::cos(newAngle);
            point_light.position.y = orbitCenter.y;
            point_light.position.z = orbitCenter.z + targetRadius * std::sin(newAngle);

            if (i < m_lights_entities.size()) {
                m_lights_entities[i].transform = glm::translate(glm::mat4(1.0f), point_light.position);
                m_lights_entities[i].transform = glm::scale(m_lights_entities[i].transform, glm::vec3(0.1f));
            }
        }
    }

    void Scene::initLight() {
        // 定向光
        m_directional_light = DirectionalLight();
        m_directional_light.direction = glm::normalize(glm::vec3(0.2f, -1.0f, -0.3f));
        m_directional_light.color = glm::vec3(1.0f, 1.0f, 1.0f);
        m_directional_light.intensity = 0.0f;

        // 点光源
        PointLight point_light1;
        point_light1.position = glm::vec3(-5.0f, 0.0f, 0.0f);
        point_light1.color = glm::vec3(0.3f, 0.2f, 0.66f);
        point_light1.intensity = 20.0f;
        m_point_lights.push_back(point_light1);
        RenderEntity light_cube1 = createLightCube(point_light1.position, point_light1.color);
        m_lights_entities.push_back(light_cube1);
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

        std::vector<float> data;
        std::vector<unsigned int> indices;

        const unsigned int X_SEGMENTS = 64;
        const unsigned int Y_SEGMENTS = 64;
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
        auto vbo = RendererFactory::createVertexBuffer(data.data(), data.size() * sizeof(float));
        
        vbo->setLayout({
            { ShaderDataType::Float3, "a_Pos" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoord" } 
        });
        vertex_array->addVertexBuffer(vbo);

        auto ebo = RendererFactory::createIndexBuffer(indices.data(), indices.size());
        vertex_array->setIndexBuffer(ebo);

        return vertex_array;
    }
} // namespace NexAur


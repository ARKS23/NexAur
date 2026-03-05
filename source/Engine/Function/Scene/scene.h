#pragma once
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/Renderer/RHI/material.h"
#include "Function/Renderer/RHI/vertex_array.h"

namespace NexAur {
    // 场景中的一个基础可视物体
    struct RenderEntity {
        std::shared_ptr<VertexArray> mesh;
        std::shared_ptr<Material> material;
        glm::mat4 transform{ 1.0f }; // 模型矩阵
        std::string name;
    };

    // 基本网格体生成工厂
    class MeshFactory {
    public:
        static std::shared_ptr<VertexArray> createCubeMesh();
        static std::shared_ptr<VertexArray> createSphereMesh();
        //static std::shared_ptr<VertexArray> createPlaneMesh();
    };

    // 定向光结构体
    struct DirectionalLight {
        glm::vec3 direction{ -0.2f, -1.0f, -0.3f };
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
    };

    // 点光源结构体
    struct PointLight {
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;

        // 衰减参数
        float constant = 1.0f;
        float linear = 0.09f;
        float quadratic = 0.032f;
    };

    class NEXAUR_API Scene {
    public:
        Scene();
        ~Scene() = default;

        // 添加场景物体
        void addEntity(const RenderEntity& entity);

        // 获取场景中所有物体，用于渲染器遍历
        const std::vector<RenderEntity>& getEntities() const { return m_entities; }

        // 每帧更新场景逻辑
        void onUpdate(float deltaTime);

        // 获取定向光
        const DirectionalLight& getDirectionalLight() const { return m_directional_light; }

        // 获取点光源列表
        const std::vector<PointLight>& getPointLights() const { return m_point_lights; }
        const std::vector<RenderEntity>& getLightsEntities() const { return m_lights_entities; }
        int getPointLightMax() const { return point_light_max; }

        // 是否启用天空盒
        bool isSkyboxEnabled() const { return m_skybox_enabled; }
    
    private:
        void initLight();

    private:
        std::vector<RenderEntity> m_entities;   // 场景中的所有物体
        std::vector<RenderEntity> m_lights_entities;     // 场景中的光源物体
        DirectionalLight m_directional_light;   // 场景定向光
        std::vector<PointLight> m_point_lights; // 场景点光源列表
        int point_light_max = 4; // 点光源最大数量
        bool m_skybox_enabled = true; // 是否启用天空盒
    };
    
} // namespace NexAur

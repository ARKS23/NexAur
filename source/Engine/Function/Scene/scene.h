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

    class MeshFactory {
    public:
        static std::shared_ptr<VertexArray> createCubeMesh();
        static std::shared_ptr<VertexArray> createSphereMesh();
        //static std::shared_ptr<VertexArray> createPlaneMesh();
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

    private:
        std::vector<RenderEntity> m_entities;
    };
    
} // namespace NexAur

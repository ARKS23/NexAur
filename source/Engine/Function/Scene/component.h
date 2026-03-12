#pragma once
#include <string>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/Base.h"
#include "Function/Renderer/RHI/material.h"
#include "Function/Renderer/RHI/vertex_array.h"

namespace NexAur {
    // 标签组件: 识别实体名称
    struct TagComponent {
        std::string name;
        TagComponent() = default;
        TagComponent(const TagComponent&) = default;
        TagComponent(const std::string& name) : name(name) {}
    };

    // 变换组件
    struct TransformComponent {
        glm::vec3 translation{ 0.0f };
        glm::vec3 rotation{ 0.0f };
        glm::vec3 scale{ 1.0f };

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
            : translation(translation), rotation(rotation), scale(scale) {}

        // 辅助生成模型矩阵
        glm::mat4 getTransform() const {
            glm::mat4 rotationMat = glm::rotate(glm::mat4(1.0f), rotation.x, { 1, 0, 0 })
                              * glm::rotate(glm::mat4(1.0f), rotation.y, { 0, 1, 0 })
                              * glm::rotate(glm::mat4(1.0f), rotation.z, { 0, 0, 1 });
            return glm::translate(glm::mat4(1.0f), translation) * rotationMat * glm::scale(glm::mat4(1.0f), scale);
        }
    };

    // 点光源组件
    struct PointLightComponent {
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float constant = 1.0f;
        float linear = 0.09f;
        float quadratic = 0.032f;
    };

    // 定向光组件
    struct DirectionalLightComponent {
        glm::vec3 direction{ -0.2f, -1.0f, -0.3f };
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
    };

    // TODO: 网格渲染组件,替代原来的 mesh 和 material 组合, 后期优化
    struct MeshRendererComponent {
        std::shared_ptr<VertexArray> Mesh;
        std::shared_ptr<Material> Material;

        MeshRendererComponent() = default;
        MeshRendererComponent(const MeshRendererComponent&) = default;
    };

    // TODO: 环境/天空盒组件,后期优化
    struct EnvironmentComponent {
        bool Enabled = false;
        std::shared_ptr<TextureCubeMap> SkyboxTexture;
        std::shared_ptr<TextureCubeMap> IrradianceMap;
        std::shared_ptr<TextureCubeMap> PrefilterMap;
        std::shared_ptr<Texture2D> BRDFLUTMap;
    };
} // namespace NexAur
#pragma once
#include <string>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"

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

    // 摄像机组件
    struct CameraComponent {
        glm::mat4 viewMatrix{ 1.0f };
        glm::mat4 projectionMatrix{ 1.0f };
        glm::mat4 viewProjectionMatrix{ 1.0f }; // 缓存VP, shader中无需再次计算
        glm::vec3 position{ 0.0f };             // 摄像机世界坐标，用于高光/PBR计算
        float nearClip = 0.1f;
        float farClip = 1000.0f;
    };

    // 运行时主相机标记。EditorCamera 不写入场景，GameView 从这个标记选择 CameraComponent。
    struct ActiveCameraComponent {
        bool enabled = true;
    };

    // 点光源组件
    struct PointLightComponent {
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float constant = 1.0f;
        float linear = 0.09f;
        float quadratic = 0.032f;
        bool cast_shadow = false;
        float shadow_range = 8.0f;
        float shadow_strength = 0.85f;
    };

    // 矩形面光源组件。本地 X/Z 表示灯面宽/高方向，本地 -Y 表示默认发光法线。
    struct RectLightComponent {
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 8.0f;
        glm::vec2 size{ 1.0f, 1.0f };
        float range = 8.0f;
        bool two_sided = false;
    };

    // 定向光组件
    struct DirectionalLightComponent {
        glm::vec3 direction{ -0.2f, -1.0f, -0.3f };
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        bool cast_shadow = true;
        float shadow_strength = 0.65f;
        float shadow_bias = 0.002f;
        float shadow_distance = 30.0f;
    };

    // 网格体组件
    struct MeshRendererComponent {
        AssetHandle model_asset;
        bool is_transparent = false; // 是否为透明物体

        MeshRendererComponent() = default;
        MeshRendererComponent(const MeshRendererComponent&) = default;
        MeshRendererComponent(AssetHandle model_handle) { setModel(model_handle); }

        void setModel(AssetHandle model_handle) {
            model_asset = model_handle;
        }

        AssetHandle getModelHandle() const {
            return model_asset;
        }

        UUID getModelUUID() const {
            return model_asset.id;
        }
    };

    struct EnvironmentComponent {
        AssetHandle environment_asset;
        glm::vec3 background_color{ 0.08f, 0.10f, 0.14f };
        float skybox_intensity = 0.75f;
        float ibl_intensity = 0.65f;
        float intensity = 1.0f; // 环境光亮度控制

        EnvironmentComponent() = default;
        EnvironmentComponent(const EnvironmentComponent&) = default;
        EnvironmentComponent(AssetHandle env_handle) { setEnvironmentMap(env_handle); }

        void setEnvironmentMap(AssetHandle env_handle) {
            environment_asset = env_handle;
        }

        AssetHandle getEnvironmentHandle() const {
            return environment_asset;
        }

        UUID getEnvironmentUUID() const {
            return environment_asset.id;
        }
    };
} // namespace NexAur

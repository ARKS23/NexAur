#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include "Core/Base.h"
#include "Function/Renderer/RHI/texture.h"
#include "Function/Renderer/RHI/vertex_array.h"

namespace NexAur {
    struct RendererCameraData {
        glm::mat4 view_matrix{ 1.0f };
        glm::mat4 projection_matrix{ 1.0f };
        glm::mat4 view_projection_matrix{ 1.0f }; // 缓存VP, shader中无需再次计算
        glm::vec3 position{ 0.0f };             // 摄像机世界坐标，用于高光/PBR计算
    };

    struct RendererDirectionalLightData {
        glm::vec3 direction{ -0.2f, -1.0f, -0.3f };
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
    };

    struct RendererPointLightData{
        glm::vec3 position{ 0.0f };
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float constant = 1.0f;
        float linear = 0.09f;
        float quadratic = 0.032f;
    };

    struct RendererEnvironmentData {
        std::shared_ptr<TextureCubeMap> skybox_texture = nullptr;
        std::shared_ptr<TextureCubeMap> irradiance_map = nullptr;
        std::shared_ptr<TextureCubeMap> prefilter_map = nullptr;
        std::shared_ptr<Texture2D> brdf_lut_map = nullptr;
        float intensity = 1.0f;
    };

    struct RendererMaterialData {
        glm::vec3 albedo{ 1.0f, 1.0f, 1.0f };
        float metallic = 0.0f;
        float roughness = 1.0f;
        float ao = 1.0f;

        std::shared_ptr<Texture2D> albedo_map = nullptr;
        std::shared_ptr<Texture2D> normal_map = nullptr;
        std::shared_ptr<Texture2D> metallic_map = nullptr;
        std::shared_ptr<Texture2D> roughness_map = nullptr;
        std::shared_ptr<Texture2D> ao_map = nullptr;
    };

    // 网格体数据
    struct RenderMeshData {
        std::shared_ptr<VertexArray> vertex_array = nullptr;
        RendererMaterialData material;
        uint32_t index_count = 0;   // 需要绘制的顶点数量
    };

    // 模型数据
    struct RenderModelData {
        std::vector<RenderMeshData> meshes; // 模型由多个网格体组成
    };
    
    // 物体数据: 包含模型数据和变换矩阵
    struct RenderObjectData {
        std::shared_ptr<RenderModelData> model_data = nullptr; // 模型数据
        glm::mat4 transform{ 1.0f };
    };
    
    // 渲染数据包: 每帧从场景收集的数据,供渲染器使用
    struct RenderDataPacket {
        RendererCameraData camera_data;

        RendererDirectionalLightData directional_light_data;
        std::vector<RendererPointLightData> point_lights_data;

        RendererEnvironmentData environment_data;

        std::vector<RenderObjectData> opaque_objects;   // 不透明物体
        std::vector<RenderObjectData> transparent_objects; // 透明物体

        void clear() {
            point_lights_data.clear();
            opaque_objects.clear();
            transparent_objects.clear();
        }
    };
} // namespace NexAur

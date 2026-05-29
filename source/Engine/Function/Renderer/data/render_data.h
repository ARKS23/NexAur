#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
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
        AssetHandle environment_asset;
        float intensity = 1.0f;
    };

    struct ResolvedRendererEnvironmentData {
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
    
    // 场景提取阶段的物体引用：只保存可序列化资产身份，不直接保存 GPU 指针。
    struct RenderObjectData {
        AssetHandle model_asset;
        glm::mat4 transform{ 1.0f };
        int entity_id = -1; // 用于标记实体ID，编辑器选中时需要
    };

    // RendererSystem 解析后的绘制数据：Pass 只消费已经准备好的 GPU 资源。
    struct ResolvedRenderObjectData {
        std::shared_ptr<RenderModelData> model_data = nullptr;
        glm::mat4 transform{ 1.0f };
        int entity_id = -1;
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
            environment_data = RendererEnvironmentData();
            point_lights_data.clear();
            opaque_objects.clear();
            transparent_objects.clear();
        }
    };

    struct ResolvedRenderDataPacket {
        RendererCameraData camera_data;

        RendererDirectionalLightData directional_light_data;
        std::vector<RendererPointLightData> point_lights_data;

        ResolvedRendererEnvironmentData environment_data;

        std::vector<ResolvedRenderObjectData> opaque_objects;   // 已解析的不透明物体
        std::vector<ResolvedRenderObjectData> transparent_objects; // 已解析的透明物体

        void clear() {
            environment_data = ResolvedRendererEnvironmentData();
            point_lights_data.clear();
            opaque_objects.clear();
            transparent_objects.clear();
        }
    };
} // namespace NexAur

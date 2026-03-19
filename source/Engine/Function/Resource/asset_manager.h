#pragma once
#include <string>
#include <unordered_map>
#include <memory>

#include "Core/Base.h"
#include "Core/UUID.h"
#include "Function/Renderer/data/render_data.h"
#include "Function/Renderer/RHI/texture.h"
#include "Function/Renderer/RHI/shader.h"

namespace NexAur {
    class Model;
    struct EnvironmentMap;

    class NEXAUR_API AssetManager {
    public:
        // 单例获取实例
        static AssetManager& getInstance() {
            static AssetManager instance;
            return instance;
        }

        void init();
        void shutdown();

        // 模型
        UUID loadModel(const std::string& path); // 加载模型并返回UUID
        std::shared_ptr<Model> getModel(const UUID& handle); // 通过UUID获取CPU模型数据
        std::shared_ptr<RenderModelData> getRenderModel(const UUID& handle); // 通过UUID获取GPU模型数据

        // 贴图
        UUID loadTexture(const std::string& path); // 加载贴图并返回UUID
        std::shared_ptr<Texture2D> getTexture(const UUID& handle); // 通过UUID获取贴图
        UUID loadTextureCube(const std::string& path); 
        std::shared_ptr<TextureCubeMap> getTextureCube(const UUID& handle);

        // 着色器
        UUID loadShader(const std::string name, const std::string& vertex_path, const std::string& fragment_path); // 加载着色器并返回UUID
        std::shared_ptr<Shader> getShader(const UUID& handle); // 通过UUID获取着色器

        // 环境光照
        UUID loadEnvironmentMap(const std::string& hdr_path); // 加载环境光照并返回UUID
        std::shared_ptr<EnvironmentMap> getEnvironmentMap(const UUID& handle); // 通过UUID获取环境光照

        // TODO：暂时不使用该函数
        void clearUnusedAssets();

    private:
        AssetManager() = default;
        ~AssetManager() = default;
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

    private:
        std::unordered_map<std::string, UUID> m_path_to_uuid; // 防止重复加载: 路径到UUID的映射, 用于资源管理和引用
        std::unordered_map<UUID, std::string> m_uuid_to_path; // UUID到路径的反向映射: 用于资源卸载和调试

        // ================================== 缓存记录 ==================================
        // 模型管理
        std::unordered_map<UUID, std::shared_ptr<Model>> m_uuid_cpu_model_cache;
        std::unordered_map<UUID, std::shared_ptr<RenderModelData>> m_uuid_gpu_model_cache;

        // 材质管理
        std::unordered_map<UUID, std::shared_ptr<Texture2D>> m_uuid_texture_cache;
        std::unordered_map<UUID, std::shared_ptr<TextureCubeMap>> m_uuid_texture_cube_cache;

        // 着色器管理
        std::unordered_map<UUID, std::shared_ptr<Shader>> m_uuid_shader_cache;

        // 环境光照缓存
        std::unordered_map<UUID, std::shared_ptr<EnvironmentMap>> m_uuid_environment_map_cache;
    };
} // namespace NexAur

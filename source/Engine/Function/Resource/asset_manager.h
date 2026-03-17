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

    class AssetManager {
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

        // 着色器
        UUID loadShader(const std::string name, const std::string& vertex_path, const std::string& fragment_path); // 加载着色器并返回UUID
        std::shared_ptr<Shader> getShader(const UUID& handle); // 通过UUID获取着色器

        // TODO：清理不再使用的资源，这个函数待完善
        void clearUnusedAssets();

    private:
        AssetManager() = default;
        ~AssetManager() = default;
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

    private:
        std::unordered_map<std::string, UUID> m_path_registry; // 防止重复加载: 路径到UUID的映射,方便资源管理和引用

        // ================================== 缓存记录 ==================================
        // 模型管理
        std::unordered_map<UUID, std::shared_ptr<Model>> m_uuid_cpu_model_cache;
        std::unordered_map<UUID, std::shared_ptr<RenderModelData>> m_uuid_gpu_model_cache;

        // 材质管理
        std::unordered_map<UUID, std::shared_ptr<Texture2D>> m_uuid_texture_cache;

        // 着色器管理
        std::unordered_map<UUID, std::shared_ptr<Shader>> m_uuid_shader_cache;
    };
} // namespace NexAur

#pragma once
#include <string>
#include <unordered_map>
#include <memory>

#include "Core/Base.h"
#include "Core/UUID.h"

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
        std::shared_ptr<Model> getModel(const UUID& handle); // 通过UUID获取模型

        // 贴图

        // 着色器

        // 清理不再使用的资源
        void clearUnusedAssets();

    private:
        AssetManager() = default;
        ~AssetManager() = default;
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

    private:
        std::unordered_map<std::string, UUID> m_path_registry; // 防止重复加载: 路径到UUID的映射,方便资源管理和引用

        // 缓存记录
        std::unordered_map<UUID, std::shared_ptr<Model>> m_uuid_model_cache;

    };
} // namespace NexAur

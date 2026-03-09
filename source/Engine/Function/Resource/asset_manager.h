#pragma once
#include <string>
#include <unordered_map>
#include <memory>

#include "Function/Resource/model.h"
#include "Core/Base.h"

namespace NexAur {
    class AssetManager {
    public:
        // 单例获取实例
        static AssetManager& getInstance() {
            static AssetManager instance;
            return instance;
        }

        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        void init();
        void shutdown();

        // 获取模型，进行缓存管理
        std::shared_ptr<Model> getModel(const std::string& path);

        // 清理不再使用的资源
        void clearUnusedAssets();

    private:
        AssetManager() = default;
        ~AssetManager() = default;

    private:
        std::unordered_map<std::string, std::shared_ptr<Model>> m_model_cache;
    };
} // namespace NexAur

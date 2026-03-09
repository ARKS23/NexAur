#include "pch.h"
#include "asset_manager.h"

namespace NexAur {
    void AssetManager::init() {
        // TODO
        NX_CORE_INFO("AssetManager Initialized.");
    }

    void AssetManager::shutdown() {
        // TODO
        NX_CORE_INFO("AssetManager Shutdown.");
    }

    std::shared_ptr<Model> AssetManager::getModel(const std::string& path) {
        // 检查缓存
        auto it = m_model_cache.find(path);
        if (it != m_model_cache.end()) {
            return it->second;
        }

        // 不在缓存中，加载模型
        std::shared_ptr<Model> model = std::make_shared<Model>(path);
        if (model->isLoaded()) {
            m_model_cache[path] = model; // 加入缓存
            return model;
        }
        else {
            NX_CORE_ERROR("Failed to load model: {}", path);
            return nullptr;
        }
    }

    void AssetManager::clearUnusedAssets() {
        // 遍历缓存，如果某个资源的引用计数为1，说明只有AssetManager持有它，可以安全删除
        for (auto it = m_model_cache.begin(); it != m_model_cache.end(); ) {
            if (it->second.use_count() == 1) 
                it = m_model_cache.erase(it); // 删除并获取下一个迭代器
            else 
                ++it; // 继续检查下一个
        }
    }
} // namespace NexAur

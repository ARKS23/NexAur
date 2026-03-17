#include "pch.h"
#include "asset_manager.h"
#include "Function/Resource/model.h"
#include "Function/Renderer/data/render_uploader.h"


namespace NexAur {
    void AssetManager::init() {
        // TODO
        NX_CORE_INFO("AssetManager Initialized.");
    }

    void AssetManager::shutdown() {
        // TODO
        NX_CORE_INFO("AssetManager Shutdown.");
    }

    UUID AssetManager::loadModel(const std::string& path) {
        // 已经加载过，返回对应UUID
        if (m_path_registry.find(path) != m_path_registry.end()) {
            return m_path_registry[path];
        }

        // 没加载过，硬盘读取
        std::shared_ptr<Model> model = std::make_shared<Model>(path);
        if (model->isLoaded()) {
            UUID new_uuid; // 随机生成一个新的UUID

            // CPU数据缓存
            m_path_registry[path] = new_uuid; // 路径到UUID的映射
            m_uuid_cpu_model_cache[new_uuid] = model; // UUID到模型的映射

            // GPU数据生成
            std::shared_ptr<RenderModelData> gpu_model = RenderResourceUploader::uploadModelData(model);
            m_uuid_gpu_model_cache[new_uuid] = gpu_model; // GPU数据缓存

            return new_uuid;
        }
        else {
            NX_CORE_ERROR("Failed to load model: {}", path);
            return INVALID_UUID;
        }
    }

    std::shared_ptr<Model> AssetManager::getModel(const UUID& handle) {
        if (handle == INVALID_UUID) {
            NX_CORE_WARN("Attempted to get model with invalid UUID.");
            return nullptr;
        }

        auto it = m_uuid_cpu_model_cache.find(handle);
        if (it != m_uuid_cpu_model_cache.end()) {
            return it->second;
        }

        return nullptr;
    }

    std::shared_ptr<RenderModelData> AssetManager::getRenderModel(const UUID& handle) {
        if (handle == INVALID_UUID) {
            NX_CORE_WARN("Attempted to get render model with invalid UUID.");
            return nullptr;
        }

        auto it = m_uuid_gpu_model_cache.find(handle);
        if (it != m_uuid_gpu_model_cache.end()) {
            return it->second;
        }

        return nullptr;
    }

    void AssetManager::clearUnusedAssets() {
        // 这里需要双重清理：从资源缓存清理，并对应从 path 缓存清理
        for (auto it = m_uuid_cpu_model_cache.begin(); it != m_uuid_cpu_model_cache.end(); ) {
            // 引用计数为 1 表示只有 AssetManager 拿着指针了
            if (it->second.use_count() == 1) {
                // TODO: 未作uuid到路径的反向映射，后续优化
                it = m_uuid_cpu_model_cache.erase(it);
            } 
            else {
                ++it;
            }
        }
    }
} // namespace NexAur

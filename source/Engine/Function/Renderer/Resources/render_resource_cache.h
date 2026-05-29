#pragma once

#include <memory>
#include <string>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
#include "Function/Renderer/data/render_data.h"

namespace NexAur {
    class AssetManager;
    class Texture2D;
    struct RenderEnvironmentMap;

    // Renderer 侧资源缓存：负责把资产句柄解析为 GPU 可用的数据。
    class NEXAUR_API RenderResourceCache {
    public:
        static RenderResourceCache& getInstance();

        void init();
        void shutdown();

        std::shared_ptr<RenderModelData> getOrCreateModel(AssetHandle model_asset, AssetManager& asset_manager);
        std::shared_ptr<RenderModelData> getModel(AssetHandle model_asset) const;
        std::shared_ptr<Texture2D> getOrCreateTexture2D(AssetHandle texture_asset, AssetManager& asset_manager);
        std::shared_ptr<Texture2D> getTexture2D(AssetHandle texture_asset) const;
        std::shared_ptr<RenderEnvironmentMap> getOrCreateEnvironmentMap(AssetHandle environment_asset, AssetManager& asset_manager);
        std::shared_ptr<RenderEnvironmentMap> getEnvironmentMap(AssetHandle environment_asset) const;

        AssetHandle registerProceduralModel(
            const std::shared_ptr<RenderModelData>& gpu_model,
            AssetManager& asset_manager,
            const std::string& debug_name = "ProceduralRenderModel");

    private:
        RenderResourceCache() = default;
        ~RenderResourceCache() = default;
        RenderResourceCache(const RenderResourceCache&) = delete;
        RenderResourceCache& operator=(const RenderResourceCache&) = delete;
    };
} // namespace NexAur

#pragma once

#include <memory>
#include <string>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
#include "Function/Renderer/data/render_data.h"

namespace NexAur {
    class AssetManager;

    // PR10 过渡缓存：先由 Renderer 接管模型 GPU 数据，PR11 再扩展为统一 RenderResourceCache。
    class NEXAUR_API RenderModelCache {
    public:
        static RenderModelCache& getInstance();

        void init();
        void shutdown();

        std::shared_ptr<RenderModelData> getOrCreateModel(AssetHandle model_asset, AssetManager& asset_manager);
        std::shared_ptr<RenderModelData> getModel(AssetHandle model_asset) const;

        AssetHandle registerProceduralModel(
            const std::shared_ptr<RenderModelData>& gpu_model,
            AssetManager& asset_manager,
            const std::string& debug_name = "ProceduralRenderModel");

    private:
        RenderModelCache() = default;
        ~RenderModelCache() = default;
        RenderModelCache(const RenderModelCache&) = delete;
        RenderModelCache& operator=(const RenderModelCache&) = delete;

    };
} // namespace NexAur

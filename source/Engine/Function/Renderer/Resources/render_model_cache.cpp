#include "pch.h"
#include "render_model_cache.h"

#include <unordered_map>

#include "Function/Resource/asset_manager.h"
#include "Function/Resource/model.h"
#include "Function/Renderer/data/render_uploader.h"

namespace NexAur {
    namespace {
        using RenderModelMap = std::unordered_map<AssetHandle, std::shared_ptr<RenderModelData>>;

        RenderModelMap& getRenderModelMap() {
            static RenderModelMap cache;
            return cache;
        }
    } // namespace

    RenderModelCache& RenderModelCache::getInstance() {
        static RenderModelCache instance;
        return instance;
    }

    void RenderModelCache::init() {
        NX_CORE_INFO("RenderModelCache Initialized.");
    }

    void RenderModelCache::shutdown() {
        getRenderModelMap().clear();
        NX_CORE_INFO("RenderModelCache Shutdown.");
    }

    std::shared_ptr<RenderModelData> RenderModelCache::getOrCreateModel(AssetHandle model_asset, AssetManager& asset_manager) {
        if (!model_asset) {
            NX_CORE_WARN("Attempted to resolve render model with invalid AssetHandle.");
            return nullptr;
        }

        RenderModelMap& model_cache = getRenderModelMap();
        auto cached_it = model_cache.find(model_asset);
        if (cached_it != model_cache.end()) {
            return cached_it->second;
        }

        if (asset_manager.getAssetType(model_asset) == AssetType::ProceduralModel) {
            NX_CORE_WARN("Procedural render model is missing from RenderModelCache: {}", static_cast<uint64_t>(model_asset.id));
            return nullptr;
        }

        std::shared_ptr<Model> cpu_model = asset_manager.loadModelCPU(model_asset);
        if (!cpu_model) {
            return nullptr;
        }

        // CPU Model -> GPU RenderModelData 的转换属于 Renderer 侧职责，不再放在 AssetManager 中。
        std::shared_ptr<RenderModelData> gpu_model = RenderResourceUploader::uploadModelData(cpu_model);
        if (!gpu_model) {
            NX_CORE_ERROR("Failed to upload render model: {}", static_cast<uint64_t>(model_asset.id));
            return nullptr;
        }

        model_cache[model_asset] = gpu_model;
        return gpu_model;
    }

    std::shared_ptr<RenderModelData> RenderModelCache::getModel(AssetHandle model_asset) const {
        if (!model_asset) {
            return nullptr;
        }

        const RenderModelMap& model_cache = getRenderModelMap();
        auto cached_it = model_cache.find(model_asset);
        return cached_it != model_cache.end() ? cached_it->second : nullptr;
    }

    AssetHandle RenderModelCache::registerProceduralModel(
        const std::shared_ptr<RenderModelData>& gpu_model,
        AssetManager& asset_manager,
        const std::string& debug_name) {
        if (!gpu_model) {
            NX_CORE_ERROR("Attempted to register null procedural render model.");
            return AssetHandle();
        }

        AssetHandle model_asset = asset_manager.registerRuntimeAsset(AssetType::ProceduralModel, debug_name);
        getRenderModelMap()[model_asset] = gpu_model;
        return model_asset;
    }
} // namespace NexAur

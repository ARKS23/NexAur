#include "pch.h"
#include "render_resource_cache.h"

#include <unordered_map>

#include "Function/Resource/asset_manager.h"
#include "Function/Resource/model.h"
#include "Function/Renderer/RHI/texture.h"
#include "Function/Renderer/Resources/render_ibl_builder.h"
#include "Function/Renderer/Resources/render_resource_uploader.h"

namespace NexAur {
    namespace {
        using RenderModelMap = std::unordered_map<AssetHandle, std::shared_ptr<RenderModelData>>;
        using RenderTexture2DMap = std::unordered_map<AssetHandle, std::shared_ptr<Texture2D>>;
        using RenderEnvironmentMapCache = std::unordered_map<AssetHandle, std::shared_ptr<RenderEnvironmentMap>>;

        RenderModelMap& getRenderModelMap() {
            static RenderModelMap cache;
            return cache;
        }

        RenderTexture2DMap& getRenderTexture2DMap() {
            static RenderTexture2DMap cache;
            return cache;
        }

        RenderEnvironmentMapCache& getRenderEnvironmentMapCache() {
            static RenderEnvironmentMapCache cache;
            return cache;
        }
    } // namespace

    RenderResourceCache& RenderResourceCache::getInstance() {
        static RenderResourceCache instance;
        return instance;
    }

    void RenderResourceCache::init() {
        NX_CORE_INFO("RenderResourceCache Initialized.");
    }

    void RenderResourceCache::shutdown() {
        getRenderEnvironmentMapCache().clear();
        getRenderTexture2DMap().clear();
        getRenderModelMap().clear();
        RenderIBLBuilder::shutdown();
        NX_CORE_INFO("RenderResourceCache Shutdown.");
    }

    std::shared_ptr<RenderModelData> RenderResourceCache::getOrCreateModel(AssetHandle model_asset, AssetManager& asset_manager) {
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
            NX_CORE_WARN("Procedural render model is missing from RenderResourceCache: {}", static_cast<uint64_t>(model_asset.id));
            return nullptr;
        }

        std::shared_ptr<Model> cpu_model = asset_manager.loadModelCPU(model_asset);
        if (!cpu_model) {
            return nullptr;
        }

        // CPU 模型上传到 GPU 的路径集中在 Renderer 侧，便于后续替换为 Vulkan 上传队列。
        std::shared_ptr<RenderModelData> gpu_model = RenderResourceUploader::uploadModelData(cpu_model, asset_manager, *this);
        if (!gpu_model) {
            NX_CORE_ERROR("Failed to upload render model: {}", static_cast<uint64_t>(model_asset.id));
            return nullptr;
        }

        model_cache[model_asset] = gpu_model;
        return gpu_model;
    }

    std::shared_ptr<RenderModelData> RenderResourceCache::getModel(AssetHandle model_asset) const {
        if (!model_asset) {
            return nullptr;
        }

        const RenderModelMap& model_cache = getRenderModelMap();
        auto cached_it = model_cache.find(model_asset);
        return cached_it != model_cache.end() ? cached_it->second : nullptr;
    }

    std::shared_ptr<Texture2D> RenderResourceCache::getOrCreateTexture2D(AssetHandle texture_asset, AssetManager& asset_manager) {
        if (!texture_asset) {
            return nullptr;
        }

        RenderTexture2DMap& texture_cache = getRenderTexture2DMap();
        auto cached_it = texture_cache.find(texture_asset);
        if (cached_it != texture_cache.end()) {
            return cached_it->second;
        }

        if (asset_manager.getAssetType(texture_asset) != AssetType::Texture2D) {
            NX_CORE_WARN("AssetHandle is not a Texture2D asset: {}", static_cast<uint64_t>(texture_asset.id));
            return nullptr;
        }

        const std::string texture_path = asset_manager.getPath(texture_asset);
        if (texture_path.empty()) {
            NX_CORE_WARN("Texture2D asset has empty path: {}", static_cast<uint64_t>(texture_asset.id));
            return nullptr;
        }

        // 贴图 GPU 对象由 Renderer 侧缓存创建，避免 Resource 层绑定具体图形 API。
        std::shared_ptr<Texture2D> texture = Texture2D::create(texture_path);
        if (!texture || !texture->isLoaded()) {
            NX_CORE_ERROR("Failed to upload texture: {}", texture_path);
            return nullptr;
        }

        texture_cache[texture_asset] = texture;
        return texture;
    }

    std::shared_ptr<Texture2D> RenderResourceCache::getTexture2D(AssetHandle texture_asset) const {
        if (!texture_asset) {
            return nullptr;
        }

        const RenderTexture2DMap& texture_cache = getRenderTexture2DMap();
        auto cached_it = texture_cache.find(texture_asset);
        return cached_it != texture_cache.end() ? cached_it->second : nullptr;
    }

    std::shared_ptr<RenderEnvironmentMap> RenderResourceCache::getOrCreateEnvironmentMap(AssetHandle environment_asset, AssetManager& asset_manager) {
        if (!environment_asset) {
            return nullptr;
        }

        RenderEnvironmentMapCache& environment_cache = getRenderEnvironmentMapCache();
        auto cached_it = environment_cache.find(environment_asset);
        if (cached_it != environment_cache.end()) {
            return cached_it->second;
        }

        if (asset_manager.getAssetType(environment_asset) != AssetType::EnvironmentMap) {
            NX_CORE_WARN("AssetHandle is not an EnvironmentMap asset: {}", static_cast<uint64_t>(environment_asset.id));
            return nullptr;
        }

        const std::string hdr_path = asset_manager.getPath(environment_asset);
        if (hdr_path.empty()) {
            NX_CORE_WARN("EnvironmentMap asset has empty path: {}", static_cast<uint64_t>(environment_asset.id));
            return nullptr;
        }

        // HDR -> skybox/irradiance/prefilter/BRDF LUT 的 GPU 烘焙属于 Renderer 侧职责。
        std::shared_ptr<RenderEnvironmentMap> environment_map = RenderIBLBuilder::bakeFromHDR(hdr_path);
        if (!environment_map) {
            NX_CORE_ERROR("Failed to bake environment map: {}", hdr_path);
            return nullptr;
        }

        environment_cache[environment_asset] = environment_map;
        return environment_map;
    }

    std::shared_ptr<RenderEnvironmentMap> RenderResourceCache::getEnvironmentMap(AssetHandle environment_asset) const {
        if (!environment_asset) {
            return nullptr;
        }

        const RenderEnvironmentMapCache& environment_cache = getRenderEnvironmentMapCache();
        auto cached_it = environment_cache.find(environment_asset);
        return cached_it != environment_cache.end() ? cached_it->second : nullptr;
    }

    AssetHandle RenderResourceCache::registerProceduralModel(
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

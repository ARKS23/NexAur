#include "pch.h"
#include "asset_manager.h"
#include "Function/Resource/model.h"
#include "Function/Renderer/data/render_uploader.h"
#include "Function/Resource/ibl_builder.h"


namespace NexAur {
    void AssetManager::init() {
        // TODO
        NX_CORE_INFO("AssetManager Initialized.");
    }

    void AssetManager::shutdown() {
        m_uuid_environment_map_cache.clear();
        IBLBuilder::shutdown();

        m_uuid_gpu_model_cache.clear();
        m_uuid_texture_cube_cache.clear();
        m_uuid_texture_cache.clear();
        m_uuid_shader_cache.clear();

        m_uuid_cpu_model_cache.clear();
        m_uuid_metadata.clear();
        m_uuid_to_path.clear();
        m_path_to_uuid.clear();

        NX_CORE_INFO("AssetManager Shutdown.");
    }

    UUID AssetManager::loadModel(const std::string& path) {
        // 已经加载过，返回对应UUID
        if (m_path_to_uuid.find(path) != m_path_to_uuid.end()) {
            return m_path_to_uuid[path];
        }

        // 没加载过，硬盘读取
        std::shared_ptr<Model> model = std::make_shared<Model>(path);
        if (model->isLoaded()) {
            UUID new_uuid; // 随机生成一个新的UUID
            m_path_to_uuid[path] = new_uuid; // 路径到UUID的映射
            m_uuid_to_path[new_uuid] = path; // UUID到路径的反向映射

            // CPU数据缓存
            m_uuid_cpu_model_cache[new_uuid] = model; // UUID到模型的映射
            recordAssetMetadata(new_uuid, AssetType::Model, path);

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

    UUID AssetManager::loadTexture(const std::string& path) {
        if (m_path_to_uuid.find(path) != m_path_to_uuid.end()) {
            return m_path_to_uuid[path];
        }

        std::shared_ptr<Texture2D> texture = Texture2D::create(path);
        if (texture && texture->isLoaded()) {
            UUID new_uuid; 
            m_path_to_uuid[path] = new_uuid;
            m_uuid_to_path[new_uuid] = path;
            
            m_uuid_texture_cache[new_uuid] = texture;
            recordAssetMetadata(new_uuid, AssetType::Texture2D, path);

            return new_uuid;
        }
        else {
            NX_CORE_ERROR("Failed to load texture: {}", path);
            return INVALID_UUID;
        }
    }

    UUID AssetManager::loadTextureCube(const std::string& path) {
        if (m_path_to_uuid.find(path) != m_path_to_uuid.end()) {
            return m_path_to_uuid[path];
        }

        std::shared_ptr<TextureCubeMap> texture_cube = TextureCubeMap::create(path);
        if (texture_cube && texture_cube->isLoaded()) {
            UUID new_uuid; 
            m_path_to_uuid[path] = new_uuid;
            m_uuid_to_path[new_uuid] = path;
            
            m_uuid_texture_cube_cache[new_uuid] = texture_cube;
            recordAssetMetadata(new_uuid, AssetType::TextureCube, path);

            return new_uuid;
        }
        else {
            NX_CORE_ERROR("Failed to load cube texture: {}", path);
            return INVALID_UUID;
        }
    }


    UUID AssetManager::loadShader(const std::string name, const std::string& vertex_path, const std::string& fragment_path) {
        std::string combined_path = name + ": " + vertex_path + "|" + fragment_path; // 组合路径作为唯一标识
        if (m_path_to_uuid.find(combined_path) != m_path_to_uuid.end()) {
            return m_path_to_uuid[combined_path];
        }

        UUID new_uuid;
        m_path_to_uuid[combined_path] = new_uuid;
        m_uuid_to_path[new_uuid] = combined_path;
        recordAssetMetadata(new_uuid, AssetType::Shader, combined_path, false, name);

        // 未进行合法性检查，后续优化
        std::shared_ptr<Shader> shader = Shader::create(name, vertex_path, fragment_path);

        m_uuid_shader_cache[new_uuid] = shader;
        return new_uuid;
    }

    UUID AssetManager::registerRenderModel(const std::shared_ptr<RenderModelData>& gpu_model) {
        if (!gpu_model) {
            NX_CORE_ERROR("Attempted to register null GPU model.");
            return INVALID_UUID;
        }
        
        UUID new_uuid;
        m_uuid_gpu_model_cache[new_uuid] = gpu_model; // 直接放入GPU模型缓存
        recordAssetMetadata(new_uuid, AssetType::ProceduralModel, "", true, "ProceduralRenderModel");
        return new_uuid;
    }

    UUID AssetManager::loadEnvironmentMap(const std::string& hdr_path) {
        if (m_path_to_uuid.find(hdr_path) != m_path_to_uuid.end()) 
            return m_path_to_uuid[hdr_path];

        std::shared_ptr<EnvironmentMap> env_map = IBLBuilder::bakeIBLFromHDR(hdr_path);
        if (!env_map) {
            NX_CORE_ERROR("Failed to load environment map: {}", hdr_path);
            return INVALID_UUID;
        }

        UUID new_uuid;
        m_path_to_uuid[hdr_path] = new_uuid;
        m_uuid_to_path[new_uuid] = hdr_path;
        m_uuid_environment_map_cache[new_uuid] = env_map;
        recordAssetMetadata(new_uuid, AssetType::EnvironmentMap, hdr_path);

        return new_uuid;
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

    std::shared_ptr<Texture2D> AssetManager::getTexture(const UUID& handle) {
        if (handle == INVALID_UUID) {
            NX_CORE_WARN("Attempted to get texture with invalid UUID.");
            return nullptr;
        }

        auto it = m_uuid_texture_cache.find(handle);
        if (it != m_uuid_texture_cache.end()) {
            return it->second;
        }

        return nullptr;
    }

    std::shared_ptr<TextureCubeMap> AssetManager::getTextureCube(const UUID& handle) {
        if (handle == INVALID_UUID) {
            NX_CORE_WARN("Attempted to get cube texture with invalid UUID.");
            return nullptr;
        }

        auto it = m_uuid_texture_cube_cache.find(handle);
        if (it != m_uuid_texture_cube_cache.end()) {
            return it->second;
        }

        return nullptr;
    }

    std::shared_ptr<EnvironmentMap> AssetManager::getEnvironmentMap(const UUID& handle) {
        if (handle == INVALID_UUID) {
            NX_CORE_WARN("Attempted to get EnvironmentMap with invalid UUID.");
            return nullptr;
        }

        if (m_uuid_environment_map_cache.find(handle) != m_uuid_environment_map_cache.end())
            return m_uuid_environment_map_cache[handle];

        return nullptr;
    }

    std::shared_ptr<Shader> AssetManager::getShader(const UUID& handle) {
        if (handle == INVALID_UUID) {
            NX_CORE_WARN("Attempted to get shader with invalid UUID.");
            return nullptr;
        }

        auto it = m_uuid_shader_cache.find(handle);
        if (it != m_uuid_shader_cache.end()) {
            return it->second;
        }

        return nullptr;
    }

    const AssetMetadata* AssetManager::getMetadata(const UUID& handle) const {
        if (handle == INVALID_UUID) {
            return nullptr;
        }

        auto it = m_uuid_metadata.find(handle);
        if (it != m_uuid_metadata.end()) {
            return &it->second;
        }

        return nullptr;
    }

    std::string AssetManager::getPath(const UUID& handle) const {
        auto it = m_uuid_to_path.find(handle);
        if (it != m_uuid_to_path.end()) {
            return it->second;
        }

        const AssetMetadata* metadata = getMetadata(handle);
        return metadata ? metadata->path : "";
    }

    AssetType AssetManager::getAssetType(const UUID& handle) const {
        const AssetMetadata* metadata = getMetadata(handle);
        return metadata ? metadata->type : AssetType::Unknown;
    }

    void AssetManager::recordAssetMetadata(const UUID& handle, AssetType type, const std::string& path, bool runtime_generated, const std::string& debug_name) {
        AssetMetadata metadata;
        metadata.handle = AssetHandle(handle);
        metadata.type = type;
        metadata.path = path;
        metadata.debug_name = debug_name.empty() ? assetTypeToString(type) : debug_name;
        metadata.runtime_generated = runtime_generated;
        m_uuid_metadata[handle] = metadata;
    }

    void AssetManager::clearUnusedAssets() {
        
    }
} // namespace NexAur

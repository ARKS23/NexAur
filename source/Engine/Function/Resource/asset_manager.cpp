#include "pch.h"
#include "asset_manager.h"
#include "Function/Resource/model.h"
#include "Function/Resource/ibl_builder.h"


namespace NexAur {
    void AssetManager::init() {
        // TODO
        NX_CORE_INFO("AssetManager Initialized.");
    }

    void AssetManager::shutdown() {
        m_uuid_environment_map_cache.clear();
        IBLBuilder::shutdown();

        m_uuid_texture_cube_cache.clear();
        m_uuid_texture_cache.clear();
        m_uuid_shader_cache.clear();

        m_uuid_cpu_model_cache.clear();
        m_uuid_metadata.clear();
        m_uuid_to_path.clear();
        m_path_to_uuid.clear();

        NX_CORE_INFO("AssetManager Shutdown.");
    }

    AssetHandle AssetManager::importModelAsset(const std::string& path) {
        // 已经导入过的模型直接复用资产身份，避免重复解析磁盘文件。
        auto loaded_it = m_path_to_uuid.find(path);
        if (loaded_it != m_path_to_uuid.end()) {
            return AssetHandle(loaded_it->second);
        }

        // 这里只导入 CPU 模型数据；GPU buffer/VAO 由 Renderer 侧缓存按需创建。
        std::shared_ptr<Model> model = std::make_shared<Model>(path);
        if (model->isLoaded()) {
            UUID new_uuid;
            m_path_to_uuid[path] = new_uuid;
            m_uuid_to_path[new_uuid] = path;

            m_uuid_cpu_model_cache[new_uuid] = model;
            recordAssetMetadata(new_uuid, AssetType::Model, path);

            return AssetHandle(new_uuid);
        }
        else {
            NX_CORE_ERROR("Failed to load model: {}", path);
            return AssetHandle();
        }
    }

    UUID AssetManager::loadModel(const std::string& path) {
        // 过渡 API：旧代码仍可拿 UUID，但不会再触发 GPU 上传。
        return importModelAsset(path).id;
    }

    AssetHandle AssetManager::importTextureAsset(const std::string& path) {
        if (path.empty()) {
            return AssetHandle();
        }

        auto loaded_it = m_path_to_uuid.find(path);
        if (loaded_it != m_path_to_uuid.end()) {
            return AssetHandle(loaded_it->second);
        }

        UUID new_uuid;
        m_path_to_uuid[path] = new_uuid;
        m_uuid_to_path[new_uuid] = path;
        recordAssetMetadata(new_uuid, AssetType::Texture2D, path);

        return AssetHandle(new_uuid);
    }

    UUID AssetManager::loadTexture(const std::string& path) {
        if (path.empty()) {
            return INVALID_UUID;
        }

        AssetHandle texture_asset = importTextureAsset(path);
        if (!texture_asset) {
            return INVALID_UUID;
        }

        auto cached_it = m_uuid_texture_cache.find(texture_asset.id);
        if (cached_it != m_uuid_texture_cache.end()) {
            return texture_asset.id;
        }

        const AssetMetadata* metadata = getMetadata(texture_asset);
        if (!metadata || metadata->type != AssetType::Texture2D) {
            NX_CORE_WARN("Path is already registered as non-texture asset: {}", path);
            return INVALID_UUID;
        }

        std::shared_ptr<Texture2D> texture = Texture2D::create(path);
        if (texture && texture->isLoaded()) {
            m_uuid_texture_cache[texture_asset.id] = texture;
            return texture_asset.id;
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
        return loadModelCPU(AssetHandle(handle));
    }

    std::shared_ptr<Model> AssetManager::loadModelCPU(AssetHandle handle) {
        if (!handle) {
            NX_CORE_WARN("Attempted to load CPU model with invalid AssetHandle.");
            return nullptr;
        }

        auto loaded_it = m_uuid_cpu_model_cache.find(handle.id);
        if (loaded_it != m_uuid_cpu_model_cache.end()) {
            return loaded_it->second;
        }

        const AssetMetadata* metadata = getMetadata(handle);
        if (!metadata || metadata->type != AssetType::Model || metadata->path.empty()) {
            NX_CORE_WARN("AssetHandle is not a loadable CPU model: {}", static_cast<uint64_t>(handle.id));
            return nullptr;
        }

        std::shared_ptr<Model> model = std::make_shared<Model>(metadata->path);
        if (!model->isLoaded()) {
            NX_CORE_ERROR("Failed to reload CPU model: {}", metadata->path);
            return nullptr;
        }

        m_uuid_cpu_model_cache[handle.id] = model;
        return model;
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

    AssetHandle AssetManager::registerRuntimeAsset(AssetType type, const std::string& debug_name) {
        UUID new_uuid;
        recordAssetMetadata(new_uuid, type, "", true, debug_name);
        return AssetHandle(new_uuid);
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

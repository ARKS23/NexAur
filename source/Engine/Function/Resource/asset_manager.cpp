#include "pch.h"
#include "asset_manager.h"
#include "Function/Resource/environment_map_asset.h"
#include "Function/Resource/material_asset.h"
#include "Function/Resource/model.h"
#include "Function/Resource/Import/gltf/tiny_gltf_importer.h"
#include "Function/Resource/texture_loader.h"


namespace NexAur {
    void AssetManager::init() {
        m_model_importers.clear();
        m_model_importers.registerImporter(std::make_unique<TinyGltfImporter>());
        NX_CORE_INFO("AssetManager Initialized.");
    }

    void AssetManager::shutdown() {
        m_uuid_cpu_environment_cache.clear();
        m_uuid_cpu_material_cache.clear();
        m_uuid_cpu_texture_cache.clear();
        m_uuid_cpu_model_cache.clear();
        m_uuid_metadata.clear();
        m_uuid_to_path.clear();
        m_path_to_uuid.clear();
        m_model_importers.clear();

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

    ModelImportResult AssetManager::inspectModelImportMetadata(const std::string& path) const {
        ModelImportRequest request;
        request.path = path;
        request.mode = ModelImportMode::MetadataOnly;
        return m_model_importers.importModel(request);
    }

    AssetHandle AssetManager::importTextureAsset(const std::string& path, TextureColorSpace color_space) {
        if (path.empty()) {
            return AssetHandle();
        }

        auto loaded_it = m_path_to_uuid.find(path);
        if (loaded_it != m_path_to_uuid.end()) {
            const AssetMetadata* metadata = getMetadata(loaded_it->second);
            if (!metadata || metadata->type != AssetType::Texture2D) {
                NX_CORE_WARN("Path is already registered as non-texture asset: {}", path);
                return AssetHandle();
            }
            if (metadata->texture_color_space != color_space) {
                NX_CORE_WARN("Texture asset already registered with a different color space: {}", path);
            }
            return AssetHandle(loaded_it->second);
        }

        UUID new_uuid;
        m_path_to_uuid[path] = new_uuid;
        m_uuid_to_path[new_uuid] = path;
        recordAssetMetadata(new_uuid, AssetType::Texture2D, path, false, "", color_space);

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

        const AssetMetadata* metadata = getMetadata(texture_asset);
        if (!metadata || metadata->type != AssetType::Texture2D) {
            NX_CORE_WARN("Path is already registered as non-texture asset: {}", path);
            return INVALID_UUID;
        }

        // Legacy UUID API：只返回资产身份。GPU Texture2D 由 RenderResourceCache 创建。
        return texture_asset.id;
    }

    std::shared_ptr<TextureAsset> AssetManager::loadTextureCPU(AssetHandle handle) {
        if (!handle) {
            NX_CORE_WARN("Attempted to load CPU texture with invalid AssetHandle.");
            return nullptr;
        }

        auto loaded_it = m_uuid_cpu_texture_cache.find(handle.id);
        if (loaded_it != m_uuid_cpu_texture_cache.end()) {
            return loaded_it->second;
        }

        const AssetMetadata* metadata = getMetadata(handle);
        if (!metadata || metadata->type != AssetType::Texture2D || metadata->path.empty()) {
            NX_CORE_WARN("AssetHandle is not a loadable CPU texture: {}", static_cast<uint64_t>(handle.id));
            return nullptr;
        }

        std::shared_ptr<TextureAsset> texture = TextureLoader::load2D(metadata->path, metadata->texture_color_space);
        if (!texture || !texture->isLoaded()) {
            NX_CORE_ERROR("Failed to load CPU texture: {}", metadata->path);
            return nullptr;
        }

        m_uuid_cpu_texture_cache[handle.id] = texture;
        return texture;
    }

    AssetHandle AssetManager::importTextureCubeAsset(const std::string& path) {
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
        recordAssetMetadata(new_uuid, AssetType::TextureCube, path);

        return AssetHandle(new_uuid);
    }

    UUID AssetManager::loadTextureCube(const std::string& path) {
        AssetHandle texture_asset = importTextureCubeAsset(path);
        if (!texture_asset) {
            return INVALID_UUID;
        }

        const AssetMetadata* metadata = getMetadata(texture_asset);
        if (!metadata || metadata->type != AssetType::TextureCube) {
            NX_CORE_WARN("Path is already registered as non-cube-texture asset: {}", path);
            return INVALID_UUID;
        }

        // Legacy UUID API：只返回资产身份。GPU cubemap 由 Renderer 侧资源流程创建。
        return texture_asset.id;
    }


    std::shared_ptr<MaterialAsset> AssetManager::createMaterialFromImportData(const MaterialImportData& import_data) {
        auto import_texture = [this](const std::string& path, TextureColorSpace color_space) {
            return path.empty() ? AssetHandle() : importTextureAsset(path, color_space);
        };

        const AssetHandle base_color_texture =
            import_texture(import_data.base_color_texture_path, TextureColorSpace::SRGB);
        const AssetHandle normal_texture =
            import_texture(import_data.normal_texture_path, TextureColorSpace::Linear);
        const AssetHandle metallic_texture =
            import_texture(import_data.metallic_texture_path, TextureColorSpace::Linear);
        const AssetHandle roughness_texture =
            import_texture(import_data.roughness_texture_path, TextureColorSpace::Linear);
        const AssetHandle metallic_roughness_texture =
            import_texture(import_data.metallic_roughness_texture_path, TextureColorSpace::Linear);
        const AssetHandle ao_texture =
            import_texture(import_data.ao_texture_path, TextureColorSpace::Linear);
        const AssetHandle emissive_texture =
            import_texture(import_data.emissive_texture_path, TextureColorSpace::SRGB);

        return std::make_shared<MaterialAsset>(
            import_data,
            base_color_texture,
            normal_texture,
            metallic_texture,
            roughness_texture,
            metallic_roughness_texture,
            ao_texture,
            emissive_texture);
    }

    AssetHandle AssetManager::registerRuntimeMaterial(const std::shared_ptr<MaterialAsset>& material, const std::string& debug_name) {
        if (!material) {
            NX_CORE_ERROR("Attempted to register invalid runtime material.");
            return AssetHandle();
        }

        UUID new_uuid;
        m_uuid_cpu_material_cache[new_uuid] = material;
        recordAssetMetadata(new_uuid, AssetType::Material, "", true, debug_name.empty() ? material->getDebugName() : debug_name);
        return AssetHandle(new_uuid);
    }

    std::shared_ptr<MaterialAsset> AssetManager::loadMaterialCPU(AssetHandle handle) {
        if (!handle) {
            NX_CORE_WARN("Attempted to load CPU material with invalid AssetHandle.");
            return nullptr;
        }

        auto loaded_it = m_uuid_cpu_material_cache.find(handle.id);
        if (loaded_it != m_uuid_cpu_material_cache.end()) {
            return loaded_it->second;
        }

        const AssetMetadata* metadata = getMetadata(handle);
        if (!metadata || metadata->type != AssetType::Material) {
            NX_CORE_WARN("AssetHandle is not a CPU material: {}", static_cast<uint64_t>(handle.id));
            return nullptr;
        }

        NX_CORE_WARN("Material asset is registered but has no CPU data loaded: {}", static_cast<uint64_t>(handle.id));
        return nullptr;
    }

    AssetHandle AssetManager::importShaderAsset(const std::string& name, const std::string& vertex_path, const std::string& fragment_path) {
        std::string combined_path = name + ": " + vertex_path + "|" + fragment_path; // 组合路径作为唯一标识
        if (m_path_to_uuid.find(combined_path) != m_path_to_uuid.end()) {
            return AssetHandle(m_path_to_uuid[combined_path]);
        }

        UUID new_uuid;
        m_path_to_uuid[combined_path] = new_uuid;
        m_uuid_to_path[new_uuid] = combined_path;
        recordAssetMetadata(new_uuid, AssetType::Shader, combined_path, false, name);

        return AssetHandle(new_uuid);
    }

    UUID AssetManager::loadShader(const std::string name, const std::string& vertex_path, const std::string& fragment_path) {
        // Legacy UUID API：只返回资产身份。GPU Shader 应由 Renderer 侧 RenderDevice 创建。
        return importShaderAsset(name, vertex_path, fragment_path).id;
    }

    AssetHandle AssetManager::importEnvironmentMapAsset(const std::string& hdr_path) {
        if (hdr_path.empty()) {
            return AssetHandle();
        }

        auto loaded_it = m_path_to_uuid.find(hdr_path);
        if (loaded_it != m_path_to_uuid.end()) {
            return AssetHandle(loaded_it->second);
        }

        UUID new_uuid;
        m_path_to_uuid[hdr_path] = new_uuid;
        m_uuid_to_path[new_uuid] = hdr_path;
        recordAssetMetadata(new_uuid, AssetType::EnvironmentMap, hdr_path);

        return AssetHandle(new_uuid);
    }

    std::shared_ptr<EnvironmentMapAsset> AssetManager::loadEnvironmentMapCPU(AssetHandle handle) {
        if (!handle) {
            NX_CORE_WARN("Attempted to load CPU environment map with invalid AssetHandle.");
            return nullptr;
        }

        auto loaded_it = m_uuid_cpu_environment_cache.find(handle.id);
        if (loaded_it != m_uuid_cpu_environment_cache.end()) {
            return loaded_it->second;
        }

        const AssetMetadata* metadata = getMetadata(handle);
        if (!metadata || metadata->type != AssetType::EnvironmentMap || metadata->path.empty()) {
            NX_CORE_WARN("AssetHandle is not a loadable environment map: {}", static_cast<uint64_t>(handle.id));
            return nullptr;
        }

        std::shared_ptr<EnvironmentMapAsset> environment =
            TextureLoader::loadHDREnvironment(metadata->path);
        if (!environment || !environment->isLoaded()) {
            NX_CORE_ERROR("Failed to load CPU environment map: {}", metadata->path);
            return nullptr;
        }

        m_uuid_cpu_environment_cache[handle.id] = environment;
        return environment;
    }

    UUID AssetManager::loadEnvironmentMap(const std::string& hdr_path) {
        // 过渡 API：旧代码仍可拿 UUID，但不会再触发 IBL GPU 烘焙。
        return importEnvironmentMapAsset(hdr_path).id;
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

    AssetHandle AssetManager::registerRuntimeModel(const std::shared_ptr<Model>& model, const std::string& debug_name) {
        if (!model || !model->isLoaded()) {
            NX_CORE_ERROR("Attempted to register invalid runtime CPU model.");
            return AssetHandle();
        }

        UUID new_uuid;
        m_uuid_cpu_model_cache[new_uuid] = model;
        recordAssetMetadata(new_uuid, AssetType::Model, "", true, debug_name);
        return AssetHandle(new_uuid);
    }

    void AssetManager::recordAssetMetadata(
        const UUID& handle,
        AssetType type,
        const std::string& path,
        bool runtime_generated,
        const std::string& debug_name,
        TextureColorSpace texture_color_space) {
        AssetMetadata metadata;
        metadata.handle = AssetHandle(handle);
        metadata.type = type;
        metadata.path = path;
        metadata.debug_name = debug_name.empty() ? assetTypeToString(type) : debug_name;
        metadata.texture_color_space = texture_color_space;
        metadata.runtime_generated = runtime_generated;
        m_uuid_metadata[handle] = metadata;
    }

    void AssetManager::clearUnusedAssets() {
        
    }
} // namespace NexAur

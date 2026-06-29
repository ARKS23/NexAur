#pragma once
#include <string>
#include <unordered_map>
#include <memory>

#include "Core/Base.h"
#include "Core/UUID.h"
#include "Function/Resource/asset_metadata.h"
#include "Function/Resource/Import/model_importer_registry.h"

namespace NexAur {
    class EnvironmentMapAsset;
    class Model;
    class MaterialAsset;
    class TextureAsset;
    struct MaterialImportData;

    class NEXAUR_API AssetManager {
    public:
        // 单例获取实例
        static AssetManager& getInstance() {
            static AssetManager instance;
            return instance;
        }

        void init();
        void shutdown();

        // 模型资产：AssetManager 只负责 CPU 数据和资产身份，GPU 上传交给 Renderer 侧缓存。
        AssetHandle importModelAsset(const std::string& path);
        std::shared_ptr<Model> loadModelCPU(AssetHandle handle);
        UUID loadModel(const std::string& path); // 过渡 API：兼容旧调用，内部等价于 importModelAsset(path).id
        AssetHandle loadModelAsset(const std::string& path) { return importModelAsset(path); }
        std::shared_ptr<Model> getModel(const UUID& handle); // 通过UUID获取CPU模型数据
        ModelImportResult inspectModelImportMetadata(const std::string& path) const;
        ModelImporterRegistry& getModelImporterRegistry() { return m_model_importers; }
        const ModelImporterRegistry& getModelImporterRegistry() const { return m_model_importers; }

        // 贴图资产：AssetManager 只登记身份，GPU texture 由 Renderer 后端创建。
        AssetHandle importTextureAsset(const std::string& path, TextureColorSpace color_space = TextureColorSpace::SRGB);
        std::shared_ptr<TextureAsset> loadTextureCPU(AssetHandle handle);
        AssetHandle importTextureCubeAsset(const std::string& path);
        UUID loadTexture(const std::string& path); // 加载贴图并返回UUID
        AssetHandle loadTextureAsset(const std::string& path) { return importTextureAsset(path); }
        UUID loadTextureCube(const std::string& path);
        AssetHandle loadTextureCubeAsset(const std::string& path) { return importTextureCubeAsset(path); }

        std::shared_ptr<MaterialAsset> createMaterialFromImportData(const MaterialImportData& import_data);
        AssetHandle registerRuntimeMaterial(const std::shared_ptr<MaterialAsset>& material, const std::string& debug_name = "RuntimeMaterial");
        std::shared_ptr<MaterialAsset> loadMaterialCPU(AssetHandle handle);

        // 着色器
        AssetHandle importShaderAsset(const std::string& name, const std::string& vertex_path, const std::string& fragment_path);
        UUID loadShader(const std::string name, const std::string& vertex_path, const std::string& fragment_path); // 过渡 API：只登记着色器身份并返回 UUID
        AssetHandle loadShaderAsset(const std::string name, const std::string& vertex_path, const std::string& fragment_path) { return importShaderAsset(name, vertex_path, fragment_path); }

        // 环境 HDR 资产：Resource 只登记 HDR 身份，环境贴图资源由 Renderer 后端负责。
        AssetHandle importEnvironmentMapAsset(const std::string& hdr_path);
        std::shared_ptr<EnvironmentMapAsset> loadEnvironmentMapCPU(AssetHandle handle);
        UUID loadEnvironmentMap(const std::string& hdr_path); // 过渡 API：兼容旧调用，内部等价于 importEnvironmentMapAsset(hdr_path).id
        AssetHandle loadEnvironmentMapAsset(const std::string& hdr_path) { return importEnvironmentMapAsset(hdr_path); }

        // 资产元数据查询，后续场景序列化和 Resource/Renderer 解耦会依赖这里。
        const AssetMetadata* getMetadata(const UUID& handle) const;
        const AssetMetadata* getMetadata(AssetHandle handle) const { return getMetadata(handle.id); }
        std::string getPath(const UUID& handle) const;
        std::string getPath(AssetHandle handle) const { return getPath(handle.id); }
        AssetType getAssetType(const UUID& handle) const;
        AssetType getAssetType(AssetHandle handle) const { return getAssetType(handle.id); }
        AssetHandle registerRuntimeModel(const std::shared_ptr<Model>& model, const std::string& debug_name = "RuntimeModel");

        // 运行期资产只登记身份和元数据，底层 GPU 对象由对应 Renderer 缓存保存。
        AssetHandle registerRuntimeAsset(AssetType type, const std::string& debug_name = "");

        // TODO：暂时不使用该函数
        void clearUnusedAssets();

    private:
        AssetManager() = default;
        ~AssetManager() = default;
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        void recordAssetMetadata(
            const UUID& handle,
            AssetType type,
            const std::string& path,
            bool runtime_generated = false,
            const std::string& debug_name = "",
            TextureColorSpace texture_color_space = TextureColorSpace::SRGB);

    private:
        std::unordered_map<std::string, UUID> m_path_to_uuid; // 防止重复加载: 路径到UUID的映射, 用于资源管理和引用
        std::unordered_map<UUID, std::string> m_uuid_to_path; // UUID到路径的反向映射: 用于资源卸载和调试
        std::unordered_map<UUID, AssetMetadata> m_uuid_metadata; // 资产元数据，后续用于序列化和资源边界拆分

        // ================================== 缓存记录 ==================================
        // 模型管理
        std::unordered_map<UUID, std::shared_ptr<Model>> m_uuid_cpu_model_cache;
        std::unordered_map<UUID, std::shared_ptr<TextureAsset>> m_uuid_cpu_texture_cache;
        std::unordered_map<UUID, std::shared_ptr<MaterialAsset>> m_uuid_cpu_material_cache;
        std::unordered_map<UUID, std::shared_ptr<EnvironmentMapAsset>> m_uuid_cpu_environment_cache;
        ModelImporterRegistry m_model_importers;

    };
} // namespace NexAur

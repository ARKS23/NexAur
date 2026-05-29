#pragma once

#include <memory>

#include "Core/Base.h"
#include "Function/Resource/mesh.h"

namespace NexAur {
    class AssetManager;
    class Model;
    class RenderResourceCache;
    struct RenderMeshData;
    struct RenderModelData;

    class NEXAUR_API RenderResourceUploader {
    public:
        // 将 CPU 资产上传为当前 RHI 可绘制的 GPU 数据。
        static std::shared_ptr<RenderModelData> uploadModelData(
            const std::shared_ptr<Model>& cpu_model,
            AssetManager& asset_manager,
            RenderResourceCache& resource_cache);

    private:
        static RenderMeshData uploadSingleMesh(const Mesh& cpu_mesh, AssetManager& asset_manager, RenderResourceCache& resource_cache);
    };
} // namespace NexAur

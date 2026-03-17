#pragma once
#include "Core/Base.h"
#include <memory>
#include "Function/Resource/mesh.h"

namespace NexAur {
    class Model;
    struct RenderMeshData;
    struct RenderModelData;

    class NEXAUR_API RenderResourceUploader {
    public:
        // cpu数据转gpu数据
        static std::shared_ptr<RenderModelData> uploadModelData(const std::shared_ptr<Model>& cpu_model);

    private:
        static RenderMeshData uploadSingleMesh(const Mesh& cpu_mesh, const std::string& parent_dir);
    };
} // namespace NexAur

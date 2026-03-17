#pragma once
#include "Core/Base.h"
#include <memory>
#include "Function/Resource/model.h"

namespace NexAur {
    class RenderMeshData;
    class RenderModelData;

    class RenderResourceUploader {
    public:
        // cpu数据转gpu数据
        static std::shared_ptr<RenderModelData> uploadModelData(const std::shared_ptr<Model>& cpu_model);

    private:
        static RenderMeshData uploadSingleMesh(const Mesh& cpu_mesh, const std::string& parent_dir);
    };
} // namespace NexAur

#pragma once

#include <memory>

#include "Core/Base.h"
#include "Function/Resource/mesh.h"

namespace NexAur {
    class Model;

    class NEXAUR_API ProceduralModelBuilder {
    public:
        static std::shared_ptr<Model> createCubeModel(const MaterialImportData& material);
        static std::shared_ptr<Model> createSphereModel(unsigned int x_segments, unsigned int y_segments, const MaterialImportData& material);
    };
} // namespace NexAur

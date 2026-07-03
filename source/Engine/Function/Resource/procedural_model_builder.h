#pragma once

#include <memory>

#include "Core/Base.h"
#include "Function/Resource/mesh.h"
#include "Function/Resource/procedural_primitive.h"

namespace NexAur {
    class Model;

    class NEXAUR_API ProceduralModelBuilder {
    public:
        static std::shared_ptr<Model> createCubeModel(const MaterialImportData& material);
        static std::shared_ptr<Model> createSphereModel(unsigned int x_segments, unsigned int y_segments, const MaterialImportData& material);
        static std::shared_ptr<Model> createPlaneModel(const MaterialImportData& material);
        static std::shared_ptr<Model> createCylinderModel(unsigned int radial_segments, const MaterialImportData& material);
        static std::shared_ptr<Model> createConeModel(unsigned int radial_segments, const MaterialImportData& material);
        static std::shared_ptr<Model> createPrimitiveModel(
            ProceduralPrimitiveType type,
            unsigned int segments,
            unsigned int rings,
            const MaterialImportData& material);
    };
} // namespace NexAur

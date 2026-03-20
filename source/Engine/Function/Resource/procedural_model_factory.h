#pragma once
#include "Core/Base.h" 

namespace NexAur {
    struct RenderModelData;

    class NEXAUR_API ProceduralModelFactory {
    public:
        static std::shared_ptr<RenderModelData> createSphereModel(unsigned int x_segments = 64, unsigned int y_segments = 64);
        static std::shared_ptr<RenderModelData> createCubeModel();
    };
} // namespace NexAur

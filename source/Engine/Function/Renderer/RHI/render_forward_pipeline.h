#pragma once

#include "Core/Base.h"
#include "render_pipeline.h"

namespace NexAur {
    class TrianglePass;
    class SpherePass;

    class NEXAUR_API RenderForwardPipeline : public RenderPipeline {
    public:
        virtual void init() override;

        virtual void render(std::shared_ptr<Camera> camera) override;

        //virtual void onWindowResize(uint32_t width, uint32_t height) override;

    private:
        std::shared_ptr<SpherePass> m_sphere_pass;
    };
} // namespace NexAur
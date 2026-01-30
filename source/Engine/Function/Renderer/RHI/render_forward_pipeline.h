#pragma once

#include "Core/Base.h"
#include "render_pipeline.h"

namespace NexAur {
    class TrianglePass;

    class NEXAUR_API RenderForwardPipeline : public RenderPipeline {
    public:
        virtual void init() override;

        virtual void render(std::shared_ptr<Camera> camera) override;

        //virtual void onWindowResize(uint32_t width, uint32_t height) override;

    private:
        std::shared_ptr<TrianglePass> m_triangle_pass;
    };
} // namespace NexAur
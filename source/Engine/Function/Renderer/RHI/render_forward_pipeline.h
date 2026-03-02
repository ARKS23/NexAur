#pragma once

#include "Core/Base.h"
#include "render_pipeline.h"

namespace NexAur {
    class ContainerPass;
    class SpherePass;
    class SkyboxPass;
    class FloorPass;

    class NEXAUR_API RenderForwardPipeline : public RenderPipeline {
    public:
        virtual void init() override;

        virtual void render(std::shared_ptr<Camera> camera) override;

        virtual void renderScene(std::shared_ptr<Scene> scene, std::shared_ptr<Camera> camera) override;
        //virtual void onWindowResize(uint32_t width, uint32_t height) override;

    private:
        std::shared_ptr<SpherePass> m_sphere_pass;
        std::shared_ptr<ContainerPass> m_container_pass;
        std::shared_ptr<FloorPass> m_floor_pass;
        std::shared_ptr<SkyboxPass> m_skybox_pass;
    };
} // namespace NexAur
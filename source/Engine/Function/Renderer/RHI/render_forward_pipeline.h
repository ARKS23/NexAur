#pragma once

#include "Core/Base.h"
#include "render_pipeline.h"

namespace NexAur {
    class SkyboxPassV2;
    class ShadowPassV2;
    class Shader;

    class NEXAUR_API RenderForwardPipeline : public RenderPipeline {
    public:
        virtual void init() override;

        virtual void render(const RenderDataPacket& render_data) override;
        //virtual void onWindowResize(uint32_t width, uint32_t height) override;

    private:
        std::shared_ptr<Shader> m_pbr_shader;
        std::shared_ptr<SkyboxPassV2> m_skybox_pass_v2;
        std::shared_ptr<ShadowPassV2> m_shadow_pass_v2;
    };
} // namespace NexAur

#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/render_pipeline.h"

namespace NexAur {
    class Shader;
    class SkyboxPassV2;
    class ShadowPassV2;

    class NEXAUR_API RenderForwardPipeline : public RenderPipeline {
    public:
        void init() override;
        void render(const RenderPassContext& pass_context, const ResolvedRenderDataPacket& render_data) override;

    private:
        std::shared_ptr<Shader> m_pbr_shader;
        std::shared_ptr<SkyboxPassV2> m_skybox_pass_v2;
        std::shared_ptr<ShadowPassV2> m_shadow_pass_v2;
    };
} // namespace NexAur

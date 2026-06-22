#pragma once

#include "Core/Base.h"
#include "Function/Renderer/Passes/interface_render_pass.h"
#include "Function/Renderer/RHI/buffer.h"
#include "Function/Renderer/RHI/material.h"
#include "Function/Renderer/RHI/shader.h"
#include "Function/Renderer/RHI/texture.h"
#include "Function/Renderer/RHI/vertex_array.h"

namespace NexAur {
    class SkyboxPassV2 : public IRenderPass {
    public:
        explicit SkyboxPassV2(const RenderPassSpecificationV2& spec) : IRenderPass(spec) {
            initResources();
        }

        ~SkyboxPassV2() override = default;

    private:
        void initResources();
        void execute(const RenderPassContext& pass_context, const ResolvedRenderDataPacket& render_data) override;
        void setSkyboxTexture(const std::shared_ptr<TextureCubeMap>& texture) { m_skybox_texture = texture; }

    private:
        std::shared_ptr<VertexArray> m_vertex_array;
        std::shared_ptr<Shader> m_shader;
        std::shared_ptr<TextureCubeMap> m_skybox_texture;
    };
} // namespace NexAur

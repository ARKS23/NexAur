#pragma once
#include "Core/Base.h"
#include "Function/Renderer/passes/interface_render_pass.h"
#include "Function/Renderer/RHI/vertex_array.h"
#include "Function/Renderer/RHI/shader.h"
#include "Function/Renderer/RHI/buffer.h"
#include "Function/Renderer/RHI/texture.h"
#include "Function/Renderer/RHI/material.h"
#include "Core/Time/Clock.h"

namespace NexAur {
    class SkyboxPassV2 : public IRenderPass {
    public:
        SkyboxPassV2(const RenderPassSpecificationV2& spec) : IRenderPass(spec) {
            initResources();
        }

        virtual ~SkyboxPassV2() = default;

    private:
        void initResources();
        void execute(const RenderDataPacket& render_data) override;
        void setSkyboxTexture(const std::shared_ptr<TextureCubeMap>& texture) { m_skybox_texture = texture; }

    private:
        std::shared_ptr<VertexArray> m_vertex_array;
        std::shared_ptr<Shader> m_shader;
        std::shared_ptr<TextureCubeMap> m_skybox_texture;
    };
} // namespace NexAur

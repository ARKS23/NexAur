#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/render_pass.h"
#include "Function/Renderer/RHI/vertex_array.h"
#include "Function/Renderer/RHI/shader.h"
#include "Function/Renderer/RHI/buffer.h"
#include "Core/Time/Clock.h"

namespace NexAur {
    class SpherePass : public RenderPass {
    public:
        SpherePass(const RenderPassSpecification& spec) : RenderPass(spec) {
            initResources();
        }

        virtual ~SpherePass() = default;
        
    protected:
        virtual void execute() override;

    private:
        void initResources();

    private:
        std::shared_ptr<VertexArray> m_vertex_array;
        std::shared_ptr<Shader> m_shader;
    };
} // namespace NexAur

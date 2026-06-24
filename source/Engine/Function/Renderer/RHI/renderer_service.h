#pragma once

#include <cstdint>
#include <utility>

#include "Core/Base.h"
#include "Core/Time/TimeStep.h"
#include "Function/Renderer/RHI/renderer_service_types.h"

namespace NexAur {
    struct RenderDataPacket;

    // RendererModule 的对外门面。Editor/Runtime 只依赖这个接口，
    // 不应该知道当前后端是 OpenGL、Vulkan 还是其他实现。
    class NEXAUR_API RendererService {
    public:
        virtual ~RendererService() = default;

        virtual RendererBackendType getBackendType() const = 0;
        virtual void render(TimeStep ts, const RenderDataPacket& render_data) = 0;
        virtual void setViewportSize(uint32_t width, uint32_t height) = 0;
        virtual std::pair<uint32_t, uint32_t> getViewportSize() const = 0;
        virtual ViewportOutput getViewportOutput() const = 0;
        virtual ViewportPickResult pickViewport(const ViewportPickRequest& request) = 0;

        // 迁移期兼容接口。旧 Editor viewport 仍可工作，新代码应使用 getViewportOutput()。
        virtual uint32_t getViewportColorAttachment() const {
            const ViewportOutput output = getViewportOutput();
            if (output.kind != ViewportOutputKind::OpenGLTexture || !output.valid()) {
                return 0;
            }
            return static_cast<uint32_t>(output.numeric_handle);
        }

        // 迁移期兼容接口。旧 picking 调用仍可工作，新代码应使用 pickViewport()。
        virtual int readViewportEntityID(int x, int y) {
            const ViewportPickResult result = pickViewport(ViewportPickRequest{ x, y });
            if (!result.supported || !result.ready) {
                return -1;
            }
            return result.entity_id;
        }
    };
} // namespace NexAur

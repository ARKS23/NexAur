#pragma once

#include <cstdint>
#include <utility>

#include "Core/Base.h"
#include "Core/Time/TimeStep.h"

namespace NexAur {
    struct RenderDataPacket;

    // RendererModule 的对外门面。Editor/Runtime 只依赖这个接口，
    // 不应该知道当前后端是 OpenGL、Vulkan 还是其他实现。
    class NEXAUR_API RendererService {
    public:
        virtual ~RendererService() = default;

        virtual void render(TimeStep ts, const RenderDataPacket& render_data) = 0;
        virtual void setViewportSize(uint32_t width, uint32_t height) = 0;
        virtual std::pair<uint32_t, uint32_t> getViewportSize() const = 0;
        virtual uint32_t getViewportColorAttachment() const = 0;
        virtual int readViewportEntityID(int x, int y) = 0;
    };
} // namespace NexAur

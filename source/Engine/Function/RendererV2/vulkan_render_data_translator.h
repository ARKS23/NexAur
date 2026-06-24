#pragma once

#include "Core/Base.h"

namespace NexAur {
    struct RenderDataPacket;

    // RendererV2 的数据转换入口。D5 先保留骨架，D6/D8 再补充
    // RenderDataPacket -> RenderView / RenderScene 的实际转换逻辑。
    class NEXAUR_API VulkanRenderDataTranslator {
    public:
        void resetFrame();
    };
} // namespace NexAur

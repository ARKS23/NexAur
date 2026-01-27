#pragma once

#include "Core/Base.h"

namespace NexAur {
    class NEXAUR_API RenderPipeline {
    public:
        virtual ~RenderPipeline() = default;

        virtual void init() = 0;    // 初始化pass资源

        virtual void render() = 0;

        //virtual void onWindowResize(uint32_t width, uint32_t height) = 0;
    };
} // namespace NexAur

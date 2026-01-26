#pragma once

#include "Core/Base.h"

namespace NexAur {
    class NEXAUR_API GraphicsContext {
    public:
        virtual void init() = 0;
        virtual void swapBuffers() = 0;

        virtual ~GraphicsContext() = default;
    };
} // namespace NexAur

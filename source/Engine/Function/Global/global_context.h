#pragma once

#include <memory>

#include "Core/Base.h"

namespace NexAur {
    // 前置声明

    class NEXAUR_API RunTimeGlobalContext {
    public:
        void startSystems();
        void shutdownSystems();

    private:
        // TODO: 各系统模块
    };

    extern RunTimeGlobalContext g_runtime_global_context;
} // namespace NexAur
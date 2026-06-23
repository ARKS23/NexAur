#pragma once

#include <memory>

namespace NexAur {
    class EngineModule;

    std::unique_ptr<EngineModule> createRenderContextModule();
    std::unique_ptr<EngineModule> createRendererModule();
} // namespace NexAur

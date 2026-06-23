#pragma once

#include <memory>

namespace NexAur {
    class EngineModule;

    std::unique_ptr<EngineModule> createCoreEngineModule();
} // namespace NexAur

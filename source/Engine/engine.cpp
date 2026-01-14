#include "pch.h"
#include "engine.h"

#include "Function/Global/global_context.h"

namespace NexAur {
    void Engine::startEngine() {
        g_runtime_global_context.startSystems();

        NX_CORE_INFO("NexAur Engine started.");
    }

    void Engine::shutdownEngine() {
        g_runtime_global_context.shutdownSystems();

        NX_CORE_INFO("NexAur Engine shut down.");
    }

    void Engine::run() {

    }

    bool Engine::tickOneFrame(float delta_time) {
        return false;
    }

    void Engine::logicalTick(float delta_time) {

    }

    void Engine::rendererTick(float delta_time) {

    }

    void Engine::calculateFPS(float delta_time) {

    }

    float Engine::calculateDeltaTime() {
        return 0.0f;
    }
} // namespace NexAur
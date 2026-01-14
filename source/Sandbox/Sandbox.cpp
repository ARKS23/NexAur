#include "iostream"
#include "NexAur.h"
#include "TempTest/TestWindow.h"

int main() {
    NexAur::Engine *engine = new NexAur::Engine();
    engine->startEngine();
    
    auto window = std::make_unique<NexAur::Window>(1280, 720, "NexAur Engine Test");

    // 3. 主循环 (Game Loop)
    while (!window->ShouldClose())
    {
        window->OnUpdate();
    }

    engine->shutdownEngine();
    NX_CORE_INFO("Sandbox Stopped!");
    return 0;
}
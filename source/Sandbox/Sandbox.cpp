#include "iostream"
#include "NexAur.h"
#include "TempTest/TestWindow.h"

int main() {
    NexAur::Log::Init();
    NA_CORE_INFO("Hello from NexAur Engine!");
    
    auto window = std::make_unique<NexAur::Window>(1280, 720, "NexAur Engine Test");

    // 3. 主循环 (Game Loop)
    while (!window->ShouldClose())
    {
        window->OnUpdate();
    }

    NA_INFO("Sandbox Stopped!");
    return 0;
}
#include <iostream>
#include "NexAur.h"

#include "scene_test.h"

int main() {
    NexAur::Engine *engine = new NexAur::Engine();
    engine->startEngine();

    /* 场景测试*/
    NexAur::SceneTestClass scene_test;
    NexAur::Entity test_entity = scene_test.addSphereEntity();

    engine->run();
    engine->shutdownEngine();
    NX_CORE_INFO("Sandbox Stopped!");
    return 0;
}
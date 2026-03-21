#include <iostream>
#include "NexAur.h"

#include "scene_test.h"

void setupScene() {
    NexAur::SceneTestClass scene_test;
    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("Sphere" + std::to_string(i), "gold", glm::vec3(i * 1.5f, 0.0f, -2.0f));
        scene_test.addCubeEntity("Cube" + std::to_string(i), "gold", glm::vec3(i * 1.5f, 0.0f, 0.0f));
    }

    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("Sphere" + std::to_string(i), "rusted_iron", glm::vec3(i * 1.5f, 1.0f, -2.0f));
        scene_test.addCubeEntity("Cube" + std::to_string(i), "rusted_iron", glm::vec3(i * 1.5f, 1.0f, -0.0f));
    }

    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("Sphere" + std::to_string(i), "grass", glm::vec3(i * 1.5f, 2.0f, -2.0f));
        scene_test.addCubeEntity("Cube" + std::to_string(i), "grass", glm::vec3(i * 1.5f, 2.0f, -0.0f));
    }

    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("Sphere" + std::to_string(i), "plastic", glm::vec3(i * 1.5f, 3.0f, -2.0f));
        scene_test.addCubeEntity("Cube" + std::to_string(i), "plastic", glm::vec3(i * 1.5f, 3.0f, -0.0f));
    }

    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("Sphere" + std::to_string(i), "wall", glm::vec3(i * 1.5f, 4.0f, -2.0f));
        scene_test.addCubeEntity("Cube" + std::to_string(i), "wall", glm::vec3(i * 1.5f, 4.0f, -0.0f));
    }
}

int main() {
    NexAur::Engine *engine = new NexAur::Engine();
    engine->startEngine();

    // 场景测试
    setupScene();

    engine->run();
    engine->shutdownEngine();
    NX_CORE_INFO("Sandbox Stopped!");
    return 0;
}
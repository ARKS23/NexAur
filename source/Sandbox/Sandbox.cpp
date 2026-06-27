#include <iostream>
#include <filesystem>
#include "NexAur.h"
#include "Core/Module/engine_module.h"
#include "Function/File/file_system.h"
#include "Function/Global/global_context.h"
#include "Function/Input/input_action_system.h"
#include "Function/Platform/platform_services.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Scene/component.h"
#include "Function/Scene/scene_serializer.h"
#include "Function/Scene/scene_service.h"
#include "Function/Scene/scene_v2.h"
#include "scene_test.h"

#include <imgui.h>

namespace {
    class FakeInputService final : public NexAur::InputService {
    public:
        void update() override {}
        const NexAur::InputState& getState() const override { return m_state; }

        void setKey(NexAur::KeyCode key_code, bool pressed) {
            m_state.setKeyPressed(key_code, pressed);
        }

        void setMouse(NexAur::MouseCode mouse_code, bool pressed) {
            m_state.setMouseButtonPressed(mouse_code, pressed);
        }

    private:
        NexAur::InputState m_state;
    };

    bool expectInputAction(bool condition, const std::string& message, std::string& failure) {
        if (condition) {
            return true;
        }

        failure = message;
        return false;
    }
} // namespace

void setupScene() {
    NexAur::SceneTestClass scene_test;
    // 材质测试
    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("gold Sphere" + std::to_string(i), "gold", glm::vec3(i * 1.5f, 0.0f, -2.0f), glm::vec3(0.0f), glm::vec3(0.5f));
        scene_test.addCubeEntity("gold Cube" + std::to_string(i), "gold", glm::vec3(i * 1.5f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.5f));
    }

    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("rusted_iron Sphere" + std::to_string(i), "rusted_iron", glm::vec3(i * 1.5f, 1.0f, -2.0f), glm::vec3(0.0f), glm::vec3(0.5f));
        scene_test.addCubeEntity("rusted_iron Cube" + std::to_string(i), "rusted_iron", glm::vec3(i * 1.5f, 1.0f, -0.0f), glm::vec3(0.0f), glm::vec3(0.5f));
    }

    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("grass Sphere" + std::to_string(i), "grass", glm::vec3(i * 1.5f, 2.0f, -2.0f), glm::vec3(0.0f), glm::vec3(0.5f));
        scene_test.addCubeEntity("grass Cube" + std::to_string(i), "grass", glm::vec3(i * 1.5f, 2.0f, -0.0f), glm::vec3(0.0f), glm::vec3(0.5f));
    }

    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("plastic Sphere" + std::to_string(i), "plastic", glm::vec3(i * 1.5f, 3.0f, -2.0f), glm::vec3(0.0f), glm::vec3(0.5f));
        scene_test.addCubeEntity("plastic Cube" + std::to_string(i), "plastic", glm::vec3(i * 1.5f, 3.0f, -0.0f), glm::vec3(0.0f), glm::vec3(0.5f));
    }

    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("wall Sphere" + std::to_string(i), "wall", glm::vec3(i * 1.5f, 4.0f, -2.0f), glm::vec3(0.0f), glm::vec3(0.5f));
        scene_test.addCubeEntity("wall Cube" + std::to_string(i), "wall", glm::vec3(i * 1.5f, 4.0f, -0.0f), glm::vec3(0.0f), glm::vec3(0.5f));
    }

    scene_test.addCubeEntity("Floor", "plastic", glm::vec3(5.0f, -3.5f, -5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(50.0f, 0.1f, 50.0f));

    // DamagedHelmet 是本地可选样例资产，缺失时跳过，避免新克隆仓库启动 Sandbox 时报错。
    const std::string damaged_helmet_path = NX_ASSET("assets/models/DamagedHelmet/DamagedHelmet.gltf");
    if (std::filesystem::exists(damaged_helmet_path)) {
        scene_test.addModelEntity("DamagedHelmet", damaged_helmet_path, glm::vec3(5.0f, 0.0f, 4.0f));
    } else {
        NX_CORE_WARN("Optional sample model missing: {0}. See assets/asset_manifest.md.", damaged_helmet_path);
    }
}

bool hasEntityNamed(const std::shared_ptr<NexAur::SceneV2>& scene, const std::string& name) {
    if (!scene) {
        return false;
    }

    const entt::registry& registry = scene->getRegistry();
    auto view = registry.view<NexAur::TagComponent>();
    for (entt::entity entity : view) {
        if (view.get<NexAur::TagComponent>(entity).name == name) {
            return true;
        }
    }

    return false;
}

int runSceneSerializerSmoke() {
    NexAur::Engine engine;
    engine.startEngine();

    bool success = true;
    std::string failure;

    NexAur::ModuleRegistry* registry = NexAur::g_runtime_global_context.getModuleRegistry();
    std::shared_ptr<NexAur::SceneService> scene_service =
        registry ? registry->getService<NexAur::SceneService>() : nullptr;
    std::shared_ptr<NexAur::AssetManager> asset_manager =
        registry ? registry->getService<NexAur::AssetManager>() : nullptr;

    const std::filesystem::path smoke_path =
        std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "scene_serializer_smoke.nxscene";

    if (!scene_service || !scene_service->getActiveScene()) {
        success = false;
        failure = "SceneSerializer smoke failed: no active scene.";
    } else if (!asset_manager) {
        success = false;
        failure = "SceneSerializer smoke failed: no AssetManager service.";
    } else {
        NexAur::SceneSerializer serializer(*asset_manager);
        const NexAur::SceneSerializationResult save_result =
            serializer.save(*scene_service->getActiveScene(), smoke_path);
        if (!save_result) {
            success = false;
            failure = save_result.message;
        } else {
            const NexAur::SceneLoadResult load_result = serializer.load(smoke_path);
            if (!load_result) {
                success = false;
                failure = load_result.message;
            } else if (
                !hasEntityNamed(load_result.scene, "MainCamera") ||
                !hasEntityNamed(load_result.scene, "Environment") ||
                !hasEntityNamed(load_result.scene, "DirectionalLight") ||
                !hasEntityNamed(load_result.scene, "PointLight")) {
                success = false;
                failure = "SceneSerializer smoke failed: loaded scene is missing default entities.";
            } else {
                NX_CORE_INFO(
                    "SceneSerializer smoke passed. Saved {} entities and loaded {} entities.",
                    save_result.entity_count,
                    load_result.entity_count);
            }
        }
    }

    std::error_code remove_error;
    std::filesystem::remove(smoke_path, remove_error);

    if (!success) {
        NX_CORE_ERROR("{}", failure);
    }

    engine.shutdownEngine();
    return success ? 0 : 1;
}

int runInputActionSmoke() {
    auto fake_input = std::make_shared<FakeInputService>();
    NexAur::InputActionSystem input_actions(fake_input);
    input_actions.configureDefaultBindings();

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectInputAction(condition, message, failure);
    };

    input_actions.update();
    expect(input_actions.getAxis2D(NexAur::DefaultInputActions::Move).y == 0.0f, "Move.y should be 0 on frame 0.");
    expect(!input_actions.wasPressed(NexAur::DefaultInputActions::Jump), "Jump should not be pressed on frame 0.");

    fake_input->setKey(NexAur::KeyCode::W, true);
    fake_input->setKey(NexAur::KeyCode::Space, true);
    input_actions.update();
    expect(input_actions.getAxis2D(NexAur::DefaultInputActions::Move).y == 1.0f, "Move.y should be 1 on frame 1.");
    expect(input_actions.isHeld(NexAur::DefaultInputActions::Jump), "Jump should be held on frame 1.");
    expect(input_actions.wasPressed(NexAur::DefaultInputActions::Jump), "Jump should be pressed on frame 1.");
    expect(!input_actions.wasReleased(NexAur::DefaultInputActions::Jump), "Jump should not be released on frame 1.");

    input_actions.update();
    expect(input_actions.getAxis2D(NexAur::DefaultInputActions::Move).y == 1.0f, "Move.y should remain 1 on frame 2.");
    expect(input_actions.isHeld(NexAur::DefaultInputActions::Jump), "Jump should remain held on frame 2.");
    expect(!input_actions.wasPressed(NexAur::DefaultInputActions::Jump), "Jump should not be pressed again on frame 2.");
    expect(!input_actions.wasReleased(NexAur::DefaultInputActions::Jump), "Jump should not be released on frame 2.");

    fake_input->setKey(NexAur::KeyCode::W, false);
    fake_input->setKey(NexAur::KeyCode::Space, false);
    input_actions.update();
    expect(input_actions.getAxis2D(NexAur::DefaultInputActions::Move).y == 0.0f, "Move.y should return to 0 on frame 3.");
    expect(!input_actions.isHeld(NexAur::DefaultInputActions::Jump), "Jump should not be held on frame 3.");
    expect(!input_actions.wasPressed(NexAur::DefaultInputActions::Jump), "Jump should not be pressed on frame 3.");
    expect(input_actions.wasReleased(NexAur::DefaultInputActions::Jump), "Jump should be released on frame 3.");

    fake_input->setKey(NexAur::KeyCode::A, true);
    fake_input->setKey(NexAur::KeyCode::D, true);
    input_actions.update();
    expect(input_actions.getAxis2D(NexAur::DefaultInputActions::Move).x == 0.0f, "Move.x should cancel opposite keys.");

    fake_input->setKey(NexAur::KeyCode::A, false);
    fake_input->setKey(NexAur::KeyCode::D, false);
    fake_input->setMouse(NexAur::MouseCode::ButtonLeft, true);
    input_actions.update();
    expect(input_actions.isHeld(NexAur::DefaultInputActions::Fire), "Fire should be held when left mouse is down.");
    expect(input_actions.wasPressed(NexAur::DefaultInputActions::Fire), "Fire should be pressed when left mouse goes down.");

    if (!success) {
        std::cerr << "InputAction smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "InputAction smoke passed." << std::endl;
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--scene-serializer-smoke") {
        return runSceneSerializerSmoke();
    }
    if (argc > 1 && std::string(argv[1]) == "--input-action-smoke") {
        return runInputActionSmoke();
    }

    NexAur::Engine *engine = new NexAur::Engine();
    engine->startEngine();

    // 场景测试
    setupScene();

    // 编辑器测试
    engine->enableEditorMode(true);

    engine->run();
    engine->shutdownEngine();
    NX_CORE_INFO("Sandbox Stopped!");
    return 0;
}

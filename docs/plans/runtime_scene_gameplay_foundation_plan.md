# NexAur Runtime / Scene / Game Demo Foundation Plan

日期：2026-06-26

## 1. 文档目标

Renderer Vulkan 主线已经完成到 PR-R25，当前渲染能力足够支撑第一版小游戏 demo。
本文件记录 Renderer 之后的下一阶段专项计划：把 NexAur 从“可以展示默认场景”推进到“可以保存场景、加载场景、运行玩法逻辑”的状态。

本计划是 `docs/plans/engine_architecture_vulkan_demo_roadmap.md` 的后续执行拆分，重点覆盖：

- Scene serialization。
- RuntimeModule 职责下沉。
- GameModule / GameplaySystem 骨架。
- Runtime input、camera、HUD、简单碰撞和 demo flow。

## 2. 当前状态判断

已经完成或基本可用：

- `SceneV2` 使用 EnTT 保存实体和组件。
- `SceneManager` / `SceneService` 已经由 `RuntimeModule` 注册。
- `RenderDataPacket` 已经是 Scene 到 Renderer 的后端无关帧数据契约。
- `AssetHandle` / `AssetMetadata` 已经存在，`AssetManager` 可以查询 path、type、debug name 等序列化所需信息。
- Renderer Vulkan 后端已经支持 model、material、texture、lighting、skybox、shadow、debug draw、viewport、picking 和 renderer debug panel。
- Editor 已经有 DockSpace、Viewport、Scene Hierarchy、Properties、Selection、Gizmo 和 Renderer Debug 面板。

尚未完成的关键能力：

- 场景无法保存和加载，默认场景仍由 C++ factory 构造。
- Runtime scene tick 仍在 `Engine::logicalTick()` 顶层调度。
- `GameModule` / `GameplaySystem` 尚未建立，Sandbox 仍承担样例和实验入口。
- Runtime input 还没有 action mapping，玩法代码如果现在写会直接依赖 key code。
- 没有 runtime camera controller、HUD、碰撞/触发器、Audio、Prefab / spawn 模板。

因此下一阶段不应继续扩大 Renderer，而应先补 Runtime / Scene / Gameplay 基础。

## 3. 设计原则

- Scene 只保存后端无关数据，不保存 Vulkan / GPU handle。
- Resource 负责资产身份、元数据和 CPU asset import，不创建 GPU 资源。
- Renderer 只消费 `RenderDataPacket` 和 `AssetHandle`，不反向依赖 Scene serializer。
- Editor 可以调用 Scene / Resource 的窄服务，但不直接访问 Renderer backend。
- Runtime / Gameplay 不依赖 ImGui，不依赖 Editor panel。
- Sandbox 可以继续作为样例入口，但不要长期承载核心 gameplay 逻辑。
- 每个 PR 都要保持 Sandbox 可运行。

## 4. 推荐执行顺序

```text
PR-G1 SceneSerializer v1
PR-G2 Editor Save / Load scene
PR-G3 Runtime scene tick 下沉
PR-G4 GameModule / ApplicationModule 骨架
PR-G5 InputActionSystem v1
PR-G6 Gameplay components + basic systems
PR-G7 Simple Collider / Trigger
PR-G8 RuntimeCameraController + HUD / GameState
PR-G9 PrefabFactory / Spawn templates
PR-G10 Audio v1
```

第一阶段最重要的是 PR-G1 到 PR-G4。完成后，NexAur 就可以开始承载一个真正的小游戏 demo，而不是只靠 Sandbox 手写场景。

## 5. PR-G1：SceneSerializer v1

执行状态：已完成。

目标：

- 保存和加载基础场景。
- 支持默认 scene round-trip。
- 为后续 Editor Save / Load 和 GameModule 加载关卡打基础。

完成记录：

- `vcpkg.json` 加入 `nlohmann-json`，`source/Engine/CMakeLists.txt` 接入 `nlohmann_json::nlohmann_json`。
- `SceneV2` 增加 const registry accessor，保存路径可以接受只读 scene。
- 新增 `scene_serializer.h/.cpp`，提供 `SceneSerializer`、`SceneSerializationResult` 和 `SceneLoadResult`。
- Scene 文件使用 `.nxscene` JSON 格式，顶层包含 `format = "NexAurScene"` 和 `version = 1`。
- 支持保存 / 加载 Tag、Transform、Camera、ActiveCamera、MeshRenderer、DirectionalLight、PointLight 和 Environment。
- Asset 引用保存 uuid 字符串、asset type、path、debug name 和 runtime generated 标记；加载时以 path 为主重新 import。
- Sandbox 增加 `--scene-serializer-smoke` 模式，执行默认场景保存 / 加载 round-trip 并检查核心实体。

建议新增文件：

```text
source/Engine/Function/Scene/
  scene_serializer.h
  scene_serializer.cpp
```

建议第一版 API：

```cpp
struct SceneSerializationResult {
    bool success = false;
    std::string message;
    size_t entity_count = 0;
};

struct SceneLoadResult {
    bool success = false;
    std::string message;
    size_t entity_count = 0;
    std::shared_ptr<SceneV2> scene;
};

class SceneSerializer {
public:
    explicit SceneSerializer(AssetManager& asset_manager);

    SceneSerializationResult save(
        const SceneV2& scene,
        const std::filesystem::path& path) const;

    SceneLoadResult load(const std::filesystem::path& path) const;
};
```

结果类型第一版只保留 success、message、entity count 和 loaded scene。后续可以继续扩展缺失资产列表、未知组件列表和统计信息。

### 5.1 数据格式

建议使用 JSON，而不是手写临时文本格式。

PR-G1 已选择 `nlohmann-json`。Scene serialization 是结构化数据，不应长期手写解析。

建议文件扩展名：

```text
.nxscene
```

文件结构建议：

```json
{
  "format": "NexAurScene",
  "version": 1,
  "entities": [
    {
      "name": "MainCamera",
      "components": {
        "Transform": {},
        "Camera": {},
        "ActiveCamera": {}
      }
    }
  ]
}
```

### 5.2 第一版组件范围

必须支持：

- `TagComponent`
- `TransformComponent`
- `CameraComponent`
- `ActiveCameraComponent`
- `MeshRendererComponent`
- `DirectionalLightComponent`
- `PointLightComponent`
- `EnvironmentComponent`

暂时不支持：

- Editor-only selection / gizmo state。
- Renderer debug visualization options。
- Runtime-generated material 的完整落盘。
- Prefab 继承关系。
- 脚本 / gameplay components。
- Physics / Audio components。

### 5.3 Asset 引用策略

第一版建议同时保存：

- `asset_type`
- `asset_uuid`
- `path`
- `debug_name`
- `runtime_generated`

加载时以 `path` 为主：

- 如果 path 非空，调用 `AssetManager` import 对应资产，得到当前运行期 `AssetHandle`。
- 如果 path 为空但 `runtime_generated = true`，第一版可以跳过或使用 fallback，并打 log。
- 保存 UUID 主要用于调试和未来 AssetRegistry 稳定化，不把它作为第一版加载的唯一依据。

原因：

- 当前还没有完整 `AssetRegistry`。
- UUID 目前更像运行期身份，不能假定跨机器、跨工程稳定。
- path fallback 更适合第一版 scene 文件。

### 5.4 加载流程

建议流程：

```text
read file
parse scene version
create empty SceneV2
for each entity:
  create entity with Tag
  apply Transform
  apply Camera / ActiveCamera
  import model/environment assets by path
  apply MeshRenderer / Environment
  apply lights
return scene
```

加载不要直接修改当前 active scene。`SceneSerializer::load()` 只返回一个新的 `SceneV2`，由 `SceneService::setActiveScene()` 或后续 Editor command 决定是否切换。

### 5.5 验收

- 默认场景保存为 `.nxscene` 后可以重新加载。
- 加载后的实体数量、Tag、Transform、Camera、Light、MeshRenderer、Environment 与保存前一致。
- 模型和环境贴图能通过 path 重新 import，并在 Renderer 中正常显示。
- `SceneSerializer` 不 include `Renderer/Vulkan/*`。
- Scene serializer 不创建 GPU 资源。
- `cmake --build --preset msvc-vcpkg-debug` 通过。
- Sandbox smoke 通过。

## 6. PR-G2：Editor Save / Load scene

执行状态：待开始。

目标：

- 让 Editor 可以调用 PR-G1 的 serializer。
- 提供最小 Save / Load 入口。
- 加载后通过 `SceneService::setActiveScene()` 切换当前场景。

建议范围：

- 在 Editor DockSpace menu bar 增加 `File` 菜单。
- `Save Scene`：保存 active scene 到固定路径或最近路径。
- `Load Scene`：第一版可以先加载固定路径，后续再接文件浏览器。
- 加载后清空 selection，避免持有旧 scene entity。

暂时不做：

- 文件浏览器。
- dirty state。
- undo / redo。
- 最近文件列表。
- 自动保存。

## 7. PR-G3：Runtime scene tick 下沉

执行状态：待开始。

目标：

- 让 RuntimeModule 拥有 scene tick 的调度责任。
- `Engine` 保持顶层 frame loop，不继续承担具体 runtime 逻辑。

当前状态：

```text
Engine::logicalTick()
  active_scene->tick(delta_time)
  active_scene->extractSceneData(write_packet)
```

建议迁移方向：

```text
RuntimeModule::tick()
  SceneManager::tick()
  active_scene->tick()

Engine / RenderContext boundary
  still controls when render packet is swapped
```

注意：

- EditorCamera 覆盖 SceneView camera 当前发生在 `EditorModule::postUpdate()`。
- 迁移时必须保持 `Scene extraction -> Editor postUpdate camera override -> UI -> swap render buffers -> rendererTick` 这个行为不破。
- 不建议在同一个 PR 里同时引入 GameModule。

## 8. PR-G4：GameModule / ApplicationModule 骨架

执行状态：待开始。

目标：

- 新增独立于 Editor 的 runtime/game entry。
- 后续 gameplay system、HUD、game state 都挂在这里。

建议职责：

```text
GameModule
  owns gameplay system list
  reads InputActionSystem
  updates game state
  can request scene load / transition
  can draw runtime HUD
```

不应该：

- 直接访问 Vulkan。
- 直接访问 Editor panel。
- 直接操作 Renderer backend。
- 把大量 demo 逻辑继续写进 Sandbox。

## 9. PR-G5：InputActionSystem v1

执行状态：待开始。

目标：

- Gameplay 不直接依赖 GLFW key code。
- 建立 action / axis 查询接口。

第一版可以支持：

- digital action：pressed / released / held。
- axis1D：例如 move forward/back。
- axis2D：例如 WASD move。

建议示例：

```text
Move
Look
Jump
Fire
Pause
Interact
```

## 10. PR-G6：Gameplay components + basic systems

执行状态：待开始。

目标：

- 让 demo 有最小玩法循环。

建议组件：

```cpp
struct PlayerComponent {};
struct EnemyComponent {};
struct VelocityComponent { glm::vec3 velocity; };
struct HealthComponent { int current = 1; int max = 1; };
struct CollectibleComponent { int score = 1; };
struct LifetimeComponent { float seconds = 1.0f; };
```

建议系统：

```text
PlayerControlSystem
EnemySystem
ProjectileSystem
CollectibleSystem
LifetimeSystem
HealthSystem
GameRuleSystem
```

第一版先保持简单，不急着建立完整 scheduler。可以先由 `GameModule` 按固定顺序调用。

## 11. 后续 PR 简述

### PR-G7：Simple Collider / Trigger

- `SphereColliderComponent`
- `AABBColliderComponent`
- Trigger overlap。
- 简单 collision layer。
- 简单 raycast / ground check。

第一版不接完整物理引擎，避免把 demo 阻塞在 Physics backend 上。

### PR-G8：RuntimeCameraController + HUD / GameState

- 第一人称或第三人称 runtime camera。
- `GameState`：MainMenu / Playing / Paused / GameOver / Victory。
- HUD：score / health / pause / win / lose。
- 第一版 HUD 可以先用 ImGui，后续再拆 runtime UI。

### PR-G9：PrefabFactory / Spawn templates

- `createPlayer()`
- `createEnemy()`
- `createCollectible()`
- `createProjectile()`

第一版先做代码模板，后续再序列化 prefab。

### PR-G10：Audio v1

- background music。
- one-shot sound。
- volume。
- pause / resume。

可选后端：miniaudio / SoLoud / OpenAL。第一版建议优先评估 miniaudio。

## 12. 不建议现在做的事

- 完整 prefab / blueprint / script system。
- 完整物理引擎接入。
- 完整 runtime UI 系统。
- 完整 Asset Browser。
- 完整 RenderGraph 重写。
- Post process pipeline。
- 完整 PBR / IBL production pipeline。
- GPU profiler。

这些都重要，但不是从“场景展示”进入“可玩 demo”的第一阻塞点。

## 13. 下一步建议

下一步建议先执行 PR-G1：SceneSerializer v1。

开始 PR-G1 前需要确认两个小决策：

1. 是否接受在 `vcpkg.json` 中加入 `nlohmann-json`。
2. 第一版 `.nxscene` 文件是否保存到固定测试路径，例如：

```text
assets/scenes/default.nxscene
```

确认后可以直接实现 serializer、写入 CMake、跑默认场景 round-trip 和 Sandbox smoke。

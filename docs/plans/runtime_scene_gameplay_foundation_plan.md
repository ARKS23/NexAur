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

已完成或基础可用：

- SceneSerializer v1 和 Editor Save / Load scene 已完成，Editor 可以保存/加载 `.nxscene`。
- Runtime scene tick 已下沉到 `RuntimeModule::tick()`。

尚未完成的关键能力：

- `GameModule` 骨架已建立；基础 `GameplaySystem` 和轻量 Physics trigger 已建立，后续仍需 HUD / prefab 等玩法配套。
- `InputActionSystem v1` 已建立，玩法代码可以读取语义输入；Play/Edit 输入隔离、可配置 binding、gamepad 等仍未完成。
- 没有 runtime camera controller、HUD、Audio、Prefab / spawn 模板。

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

执行状态：已完成。

目标：

- 让 Editor 可以调用 PR-G1 的 serializer。
- 提供最小 Save / Load 入口。
- 加载后通过 `SceneService::setActiveScene()` 切换当前场景。
- 保持 Editor / Scene / Resource 边界清晰，不让 Editor 访问全局单例或 Renderer backend。

完成记录：

- `EditorContext` 已注入 `AssetManager`，`EditorModule` 通过 `ModuleRegistry` 显式获取资源服务。
- `EditorLayer` 已在 DockSpace menu bar 中增加 `File -> Save Scene` 和 `File -> Load Scene`。
- 第一版 scene IO 使用固定路径 `assets/scenes/editor_scene.nxscene`，由 `SceneSerializer` 负责读写 `.nxscene`。
- Load 成功后通过 `SceneService::setActiveScene()` 切换 active scene，并清空 selection，避免面板继续持有旧 scene entity。
- `SceneManager::setActiveScene()` 已改为立即处理切换，保证 Editor load 后当前帧即可看到新 active scene。
- Editor 仍只依赖 Scene / Resource 的窄服务接口，不访问 Renderer backend。

建议范围：

1. `EditorContext` 注入 `AssetManager`。
   - 新增 `std::shared_ptr<AssetManager> asset_manager`。
   - `EditorModule` 从 `ModuleRegistry` 获取 `AssetManager` 并写入 `EditorContext`。
   - `EditorLayer` 创建 `SceneSerializer` 时使用显式依赖，不调用 `AssetManager::getInstance()`。

2. Editor DockSpace 增加 File 菜单。
   - `EditorLayer::beginDockSpace()` 当前已经使用 `ImGuiWindowFlags_MenuBar`。
   - 建议新增：
     ```text
     File
       Save Scene
       Load Scene
     ```
   - 如果 active scene、scene service 或 asset manager 不可用，菜单项应禁用或安全失败。

3. 第一版使用固定 scene 路径。
   - 建议路径：
     ```text
     assets/scenes/editor_scene.nxscene
     ```
   - 暂时不做文件浏览器。
   - `SceneSerializer::save()` 已负责创建父目录。

4. `EditorLayer` 增加最小 scene IO helper。
   - 建议私有方法：
     ```cpp
     void drawMainMenuBar();
     void drawFileMenu();
     void saveActiveScene();
     void loadScene();
     std::filesystem::path getDefaultScenePath() const;
     ```
   - Save 成功 / 失败先写 core log。
   - Load 成功后调用 `SceneService::setActiveScene()`，再清空 selection。

5. 加载后清空 selection。
   - 调用 `clearSelection()`。
   - 避免 Properties / Viewport / Hierarchy 持有旧 scene 的 `Entity`。

6. 修正 SceneManager 切换语义。
   - 当前 `SceneManager::setActiveScene()` 只是设置 `m_next_scene`。
   - 但 `RuntimeModule` 目前没有每帧调用 `SceneManager::tick()`，所以 pending scene change 不会自动处理。
   - PR-G2 需要保证 Load 后 active scene 真正切换。
   - 第一版建议让 `SceneManager::setActiveScene()` 在设置 `m_next_scene` 后立即调用 `processSceneChange()`。
   - PR-G3 再进一步整理 Runtime scene tick 下沉，不在 PR-G2 扩大重构范围。

建议数据流：

```text
Editor File Menu
  -> EditorLayer::saveActiveScene()
    -> SceneSerializer::save(active_scene, fixed_path)

Editor File Menu
  -> EditorLayer::loadScene()
    -> SceneSerializer::load(fixed_path)
    -> SceneService::setActiveScene(loaded_scene)
    -> clearSelection()
```

注意：

- 当前 Sandbox demo 中大量网格来自 `ProceduralModelBuilder`，属于 runtime-generated asset。
- PR-G1 第一版不支持 runtime-generated mesh 完整 round-trip。
- 因此 PR-G2 的 Save / Load 基础流程可用，但保存当前 Sandbox demo 后重新加载，部分 runtime-generated MeshRenderer 可能没有可恢复的 model asset。
- 这不是 PR-G2 要解决的问题，后续由 Prefab / Procedural asset serialization 处理。

验收：

- Editor 顶部菜单出现 `File -> Save Scene` 和 `File -> Load Scene`。
- `Save Scene` 会写出 `assets/scenes/editor_scene.nxscene`。
- `Load Scene` 能读取该文件并切换 active scene。
- Load 后 selection 被清空，Properties 不继续引用旧 entity。
- 缺少 active scene / scene service / asset manager 时不会崩溃。
- `EditorLayer` / `EditorContext` 不 include 或访问 `Renderer/Vulkan/*`。
- `SceneSerializer` 仍不创建 GPU 资源。
- `cmake --build --preset msvc-vcpkg-debug` 通过。
- `Sandbox.exe --scene-serializer-smoke` 通过。
- 常规 Sandbox smoke 通过。

暂时不做：

- 文件浏览器。
- dirty state。
- undo / redo。
- 最近文件列表。
- 自动保存。
- runtime-generated model / material 的完整序列化。

## 7. PR-G3：Runtime scene tick 下沉

执行状态：已完成。

目标：

- 让 RuntimeModule 拥有 scene tick 的调度责任。
- `Engine` 保持顶层 frame loop，不继续承担具体 runtime 逻辑。
- 为后续 GameModule / GameplaySystem 提供干净的 runtime 更新入口。
- 保持现有 SceneView / GameView 渲染顺序不变。

完成记录：

- `RuntimeModule` 已缓存 `RenderContext` 窄依赖，不再依赖 Renderer backend module。
- `RuntimeModule::tick()` 已负责调用 `SceneManager::tick(delta_time)`。
- Runtime tick 后立即提取 active scene 到 `RenderContext::getWriteData()`。
- `debug_visualization_options` 仍从 `RenderContext` 同步到 write packet。
- `Engine::logicalTick()` 已删除，`Engine` 不再直接访问 `SceneService` / `SceneV2`。
- `Engine` 继续保留 frame loop、UI、buffer swap、renderer tick 和 window present/poll 调度。

迁移前状态：

```text
Engine::tickOneFrame()
  ModuleManager::tickModules()
    PlatformModule::tick()

  Engine::logicalTick()
    SceneService::getActiveScene()
    active_scene->tick(delta_time)
    RenderContext::getWriteData()
    write_packet.debug_visualization_options = ...
    active_scene->extractSceneData(write_packet)

  ModuleManager::postUpdateModules()
    EditorModule::postUpdate()
      SceneView 模式下覆盖 viewport camera

  UI begin / render / finalize
  RenderContext::swapBuffers()
  RendererService::render(read_packet)
```

问题：

- `Engine` 当前直接 include / 查询 `SceneService` 和 `SceneV2`，承担了 runtime scene 的具体调度。
- 后续如果把 gameplay system、runtime camera、关卡切换继续挂在 `Engine::logicalTick()`，顶层 frame loop 会逐渐变成业务层。
- `RuntimeModule` 目前只创建和注册 `SceneManager`，没有真正拥有 scene tick。

目标调用链：

```text
Engine::tickOneFrame()
  calculateFPS()

  ModuleManager::tickModules()
    PlatformModule::tick()
      update input/window platform state

    RuntimeModule::tick()
      SceneManager::tick(delta_time)
        active_scene->tick(delta_time)

      RuntimeModule::extractActiveScene()
        RenderContext::getWriteData()
        write_packet.debug_visualization_options = ...
        active_scene->extractSceneData(write_packet)

  ModuleManager::postUpdateModules()
    EditorModule::postUpdate()
      SceneView 模式下用 EditorCamera 覆盖 write_packet.camera_data

  UI begin / render / finalize

  RenderContext::swapBuffers()

  Engine::rendererTick()
    RendererService::render(delta_time, read_packet)

  WindowService::present()
  WindowService::pollEvents()
```

建议范围：

1. `RuntimeModule` 缓存 runtime scene 所需窄依赖。
   - 保留 `std::shared_ptr<SceneManager> m_scene_manager`。
   - 新增 `std::shared_ptr<RenderContext> m_render_context`。
   - `initialize()` 中从 `ModuleRegistry` 获取 `RenderContext`，不要访问全局上下文。

2. `RuntimeModule` 实现 `tick()`。
   - 调用 `m_scene_manager->tick(delta_time)`。
   - 读取 active scene。
   - 写入 `RenderContext::getWriteData()`。
   - 同步 `debug_visualization_options`。
   - 调用 `active_scene->extractSceneData(write_packet)`。

3. `Engine::logicalTick()` 下线。
   - 第一版可以直接删除函数和调用点。
   - 如果为了过渡保留函数，也应变为空壳，不再直接访问 `SceneService` / `SceneV2`。
   - `Engine` 仍保留 `rendererTick()`，因为 Renderer consume read packet 是顶层 frame boundary 的一部分。

4. 保持 frame ordering。
   - Scene extraction 必须仍然发生在 `EditorModule::postUpdate()` 之前。
   - `RenderContext::swapBuffers()` 必须仍然发生在 UI finalize 之后、renderer tick 之前。
   - `EditorModule::postUpdate()` 仍负责 SceneView camera override，不移动到 RuntimeModule。

5. 清理 include 和依赖边界。
   - `engine.cpp` 不再需要 include `Function/Scene/scene_service.h` 和 `Function/Scene/scene_v2.h`。
   - `RuntimeModule` 可以依赖 `SceneManager`、`SceneService` 和 `RenderContext`。
   - Runtime / Scene 仍不依赖 Editor 或 Renderer backend。

建议 RuntimeModule 内部形态：

```cpp
class RuntimeModule final : public EngineModule {
public:
    void initialize(ModuleContext& context) override;
    void tick(const TickContext& tick_context) override;
    void shutdown(ModuleContext& context) override;

private:
    void extractActiveScene();

private:
    std::shared_ptr<SceneManager> m_scene_manager;
    std::shared_ptr<RenderContext> m_render_context;
};
```

`tick()` 语义建议：

```text
RuntimeModule::tick(delta_time)
  if scene_manager:
    scene_manager->tick(delta_time)

  if scene_manager && render_context && active_scene:
    write_packet = render_context->getWriteData()
    write_packet.debug_visualization_options = render_context->getDebugVisualizationOptions()
    active_scene->extractSceneData(&write_packet)
```

注意事项：

- EditorCamera 覆盖 SceneView camera 当前发生在 `EditorModule::postUpdate()`。
- 迁移时必须保持 `Scene extraction -> Editor postUpdate camera override -> UI -> swap render buffers -> rendererTick` 这个行为不破。
- `SceneManager::setActiveScene()` 在 PR-G2 后已经立即切换 active scene，PR-G3 不需要重新设计 scene transition。
- 不建议在同一个 PR 里同时引入 GameModule。
- 不建议在这一 PR 引入 play mode、runtime camera controller 或 gameplay scheduler。

验收：

- `Engine` 不再直接调用 `SceneV2::tick()`。
- `Engine` 不再直接调用 `SceneV2::extractSceneData()`。
- `RuntimeModule::tick()` 负责 active scene tick 和 render data extraction。
- `Scene extraction -> Editor postUpdate camera override -> UI -> swap buffers -> rendererTick` 顺序保持不变。
- SceneView / GameView 行为不回退。
- Save / Load 后 active scene 能在下一帧正常 tick 和 extract。
- `engine.cpp` 不再 include `Function/Scene/scene_service.h` / `scene_v2.h`。
- `RuntimeModule` 不 include 或访问 Editor panel、Renderer backend。
- `cmake --build --preset msvc-vcpkg-debug` 通过。
- `Sandbox.exe --scene-serializer-smoke` 通过。
- 常规 Sandbox smoke 通过。

暂时不做：

- GameModule / ApplicationModule。
- GameplaySystem scheduler。
- Play mode 状态机。
- Runtime camera controller。
- 异步 scene transition。
- scene event bus。

## 8. PR-G4：GameModule / ApplicationModule 骨架

执行状态：已完成。

目标：

- 新增独立于 Editor 的 runtime/game entry。
- 后续 gameplay system、HUD、game state 都挂在这里。
- 为 PR-G5 InputActionSystem 和 PR-G6 gameplay systems 提供稳定挂载点。
- 不让 `Engine`、`Sandbox` 或 `RuntimeModule` 继续承担玩法层职责。

完成记录：

- 新增 `source/Engine/Function/Game/game_module.h/.cpp`。
- `BuiltinModuleNames` 已加入 `Game`。
- `RunTimeGlobalContext` 已在 `RuntimeModule` 后、`UI` / `Editor` 前注册 `GameModule`。
- `GameModule` 第一版缓存 `SceneService`，提供初始化、tick、renderUI 和 shutdown 生命周期入口。
- `GameModule::tick()` 第一版保持 no-op，作为 PR-G6 gameplay systems 的固定挂载点。
- `RuntimeModule` 已将 scene extraction 从 `tick()` 移到 `postUpdate()`，确保未来 gameplay 修改 scene 后能在同一帧进入 `RenderDataPacket`。

定位：

- 第一版实现 `GameModule`，暂时不单独实现 `ApplicationModule`。
- `ApplicationModule` 更适合后续引擎/游戏分发边界更清晰时再抽象。
- 当前阶段 `GameModule` 作为 NexAur demo/gameplay 的 runtime application entry。

建议新增文件：

```text
source/Engine/Function/Game/
  game_module.h
  game_module.cpp
```

建议新增模块名：

```cpp
namespace NexAur::BuiltinModuleNames {
    inline constexpr const char* Game = "Game";
}
```

注册位置建议：

```text
RunTimeGlobalContext::startSystems()
  register RuntimeModule
  register GameModule
  register UIModule
  register EditorModule
```

`GameModule` 放在 `RuntimeModule` 后、`UI` / `Editor` 前：

- 可以读取 Runtime 暴露的 `SceneService`。
- 后续可以在 UI 阶段之前提交 runtime HUD。
- 不需要 `Engine` 直接知道 GameModule。

关键调用顺序：

PR-G3 后 `RuntimeModule::tick()` 已经同时做了 scene tick 和 scene extraction。
PR-G4 引入 GameModule 后，需要给 gameplay 一个在 extraction 前修改 scene 的机会，因此建议顺手把 scene extraction 从 `RuntimeModule::tick()` 移到 `RuntimeModule::postUpdate()`：

```text
Engine::tickOneFrame()

  ModuleManager::tickModules()
    PlatformModule::tick()
      refresh input

    RuntimeModule::tick()
      SceneManager::tick(delta_time)

    GameModule::tick()
      update game state / future gameplay systems

  ModuleManager::postUpdateModules()
    RuntimeModule::postUpdate()
      active_scene->extractSceneData(write_packet)

    EditorModule::postUpdate()
      SceneView 模式覆盖 viewport camera

  UI begin / render / finalize
  RenderContext::swapBuffers()
  RendererService::render(read_packet)
```

这样后续 gameplay 修改 Transform、Camera、Light、MeshRenderer 等 scene data 后，可以在同一帧进入 `RenderDataPacket`。

建议职责：

```text
GameModule
  owns future gameplay system list
  caches SceneService
  updates game state
  can request scene load / transition later
  can draw runtime HUD later
```

第一版内部形态：

```cpp
class GameModule final : public EngineModule {
public:
    ModuleInfo getInfo() const override;
    void initialize(ModuleContext& context) override;
    void tick(const TickContext& tick_context) override;
    void renderUI(const TickContext& tick_context) override;
    void shutdown(ModuleContext& context) override;

private:
    bool m_is_enabled = true;
    std::shared_ptr<SceneService> m_scene_service;
};
```

第一版 `tick()` 可以保持 no-op，只保留清晰的 future hook：

```text
GameModule::tick()
  if disabled:
    return
  if no active scene:
    return
  // PR-G6 starts fixed-order gameplay systems here.
```

建议依赖：

```text
GameModule depends on Runtime
```

第一版不建议依赖 `Renderer`、`Editor`、`UI` 或具体 input backend。
PR-G5 接入 InputActionSystem 后，再让 GameModule 读取 input action 服务。

不应该：

- 直接访问 Vulkan。
- 直接访问 Editor panel。
- 直接操作 Renderer backend。
- 把大量 demo 逻辑继续写进 Sandbox。

暂时不做：

- InputActionSystem。
- Player / Enemy / Health 等 gameplay components。
- GameplaySystem scheduler。
- Play mode。
- Runtime camera controller。
- HUD 实际 UI。
- Scene transition event bus。
- Sandbox demo 大迁移。

验收：

- `GameModule` 能随 engine 初始化、tick、shutdown。
- `GameModule` 已注册进 `ModuleManager`。
- `Engine` 不 include 或直接访问 GameModule。
- GameModule 不 include Editor / Vulkan backend。
- Runtime scene extraction 发生在 GameModule tick 之后、Editor camera override 之前。
- Sandbox 仍能正常启动。
- `cmake --build --preset msvc-vcpkg-debug` 通过。
- `Sandbox.exe --scene-serializer-smoke` 通过。
- 常规 Sandbox smoke 通过。

## 9. PR-G5：InputActionSystem v1

执行状态：已完成。

目标：

- Gameplay 不直接依赖 GLFW key code。
- 建立 action / axis 查询接口。
- 让 `GameModule` 后续只读取语义输入，例如 `Move`、`Jump`、`Fire`。
- 为 PR-G6 gameplay systems 提供稳定输入边界。

完成记录：

- 新增 `InputActionSystem` / `InputActionService`，提供 digital action 和 axis 查询接口。
- 新增 `InputActionModule`，在 `PlatformModule` 后更新语义输入状态。
- `BuiltinModuleNames` 已加入 `InputAction`。
- 默认 action map 已支持 `Move`、`Jump`、`Fire`、`Pause` 和 `Interact`。
- `GameModule` 已缓存 `InputActionService`，后续 gameplay systems 可以直接读取语义输入。
- 新增 `Sandbox.exe --input-action-smoke`，使用 fake input 覆盖 `held` / `pressed` / `released` 和 axis2D 行为。

当前输入链路：

```text
PlatformModule::tick()
  InputSystemGLFW::update()
    poll GLFW key/mouse
    write InputState

EditorCamera / Viewport / Editor
  read InputService / InputState
  directly query KeyCode / MouseCode
```

这对 Editor 工具可以接受，但 gameplay 不应该继续写成：

```cpp
if (input->isKeyPressed(KeyCode::W)) {
    // move forward
}
```

目标调用链：

```text
PlatformModule::tick()
  update raw InputState

InputActionModule::tick()
  read InputService
  update action states

RuntimeModule::tick()
  SceneManager::tick()

GameModule::tick()
  read InputActionService
  update future gameplay systems
```

建议新增文件：

```text
source/Engine/Function/Input/
  input_action_system.h
  input_action_system.cpp
  input_action_module.h
  input_action_module.cpp
```

建议新增模块名：

```cpp
namespace NexAur::BuiltinModuleNames {
    inline constexpr const char* InputAction = "InputAction";
}
```

建议注册位置：

```text
RunTimeGlobalContext::startSystems()
  register PlatformModule
  register InputActionModule
  register ResourceModule
  register RenderContextModule
  register RendererModule
  register RuntimeModule
  register GameModule
```

关键顺序：

```text
Platform raw input
  -> InputAction semantic input
  -> GameModule gameplay
```

建议服务接口：

```cpp
class InputActionService {
public:
    virtual ~InputActionService() = default;

    virtual bool isHeld(std::string_view action) const = 0;
    virtual bool wasPressed(std::string_view action) const = 0;
    virtual bool wasReleased(std::string_view action) const = 0;

    virtual float getAxis1D(std::string_view axis) const = 0;
    virtual glm::vec2 getAxis2D(std::string_view axis) const = 0;
};
```

第一版可以用 `std::string` / `std::string_view` 作为 action id。后续如果需要更高频或可序列化 ID，再引入 hash / enum / asset 化 action map。

建议默认 action 常量：

```cpp
namespace NexAur::DefaultInputActions {
    inline constexpr std::string_view Move = "Move";
    inline constexpr std::string_view Jump = "Jump";
    inline constexpr std::string_view Fire = "Fire";
    inline constexpr std::string_view Pause = "Pause";
    inline constexpr std::string_view Interact = "Interact";
}
```

建议第一版绑定数据：

```cpp
enum class InputBindingDevice {
    Keyboard,
    MouseButton
};

struct InputButtonBinding {
    InputBindingDevice device = InputBindingDevice::Keyboard;
    KeyCode key = KeyCode::Unknown;
    MouseCode mouse = MouseCode::Button0;
};

struct DigitalActionBinding {
    std::string action;
    std::vector<InputButtonBinding> buttons;
};

struct Axis1DBinding {
    std::string axis;
    InputButtonBinding negative;
    InputButtonBinding positive;
};

struct Axis2DBinding {
    std::string axis;
    InputButtonBinding negative_x;
    InputButtonBinding positive_x;
    InputButtonBinding negative_y;
    InputButtonBinding positive_y;
};
```

如果当前 `KeyCode` 没有 `Unknown`，PR-G5 可以新增一个安全默认值，或给 `InputButtonBinding` 增加静态工厂方法来避免无效字段被读取：

```cpp
static InputButtonBinding key(KeyCode key);
static InputButtonBinding mouse(MouseCode mouse);
```

关键数据语义：

`pressed` / `released` 不能只靠当前 `InputState` 判断，必须保存上一帧 action 状态：

```text
held     = current == true
pressed  = current == true  && previous == false
released = current == false && previous == true
```

因此 `InputActionSystem::update()` 建议流程：

```text
previous_states = current_states
read raw InputService
evaluate digital actions
evaluate axis1D
evaluate axis2D
```

第一版可以支持：

- digital action：pressed / released / held。
- axis1D：例如 move forward/back。
- axis2D：例如 WASD move。

默认 ActionMap 建议：

```text
Move axis2D
  W / S -> Y +1 / -1
  D / A -> X +1 / -1

Jump digital
  Space

Fire digital
  Mouse Left

Pause digital
  Escape

Interact digital
  E
```

建议默认绑定初始化位置：

```text
InputActionModule::initialize()
  create InputActionSystem
  input_actions->bindAxis2D(DefaultInputActions::Move, A, D, S, W)
  input_actions->bindDigital(DefaultInputActions::Jump, Space)
  input_actions->bindDigital(DefaultInputActions::Fire, MouseLeft)
  input_actions->bindDigital(DefaultInputActions::Pause, Escape)
  input_actions->bindDigital(DefaultInputActions::Interact, E)
  register InputActionService
```

`Look` 暂时只预留名字或暂不实现。当前 `InputState` 有 mouse position，但没有 mouse delta；mouse-look 更适合 PR-G8 RuntimeCameraController 再接。

GameModule 接入建议：

```text
GameModule depends on Runtime + InputAction
```

`GameModule` 可以新增：

```cpp
std::shared_ptr<InputActionService> m_input_actions;
```

PR-G5 仍不需要写实际 gameplay，只要保证 GameModule 后续可以读取 action service。

建议实现清单：

1. `builtin_module_names.h` 增加 `InputAction`。
2. `CMakeLists.txt` 加入 `input_action_system.*` 和 `input_action_module.*`。
3. `RunTimeGlobalContext::startSystems()` 在 `PlatformModule` 后注册 `InputActionModule`。
4. `InputActionModule` 依赖 `Platform`，初始化时获取 `InputService`。
5. `InputActionModule::tick()` 调用 `InputActionSystem::update()`。
6. `GameModule` 增加对 `InputActionService` 的缓存，依赖调整为 `Runtime + InputAction`。

建议 smoke 测试：

新增 `Sandbox.exe --input-action-smoke`，不依赖真实窗口输入，使用 fake `InputService` 验证：

```text
frame 0:
  W up
  update
  Move.y == 0
  Jump pressed == false

frame 1:
  W down
  Space down
  update
  Move.y == 1
  Jump held == true
  Jump pressed == true
  Jump released == false

frame 2:
  W down
  Space down
  update
  Move.y == 1
  Jump held == true
  Jump pressed == false
  Jump released == false

frame 3:
  W up
  Space up
  update
  Move.y == 0
  Jump held == false
  Jump pressed == false
  Jump released == true
```

这个 smoke 可以直接覆盖 PR-G5 最容易出错的地方：`pressed/released` 的上一帧状态、axis2D 聚合和默认映射。

不应该：

- `InputActionSystem` include GLFW。
- Gameplay 直接读取 `KeyCode` / `MouseCode`。
- 在 PR-G5 做按键配置 UI。
- 在 PR-G5 做手柄、raw mouse、input context stack。
- 在 PR-G5 写 PlayerControlSystem。

验收：

- 新增 `InputActionSystem` / `InputActionService`。
- 新增 `InputActionModule` 并注册到 `ModuleManager`。
- `Platform -> InputAction -> Game` tick 顺序正确。
- 支持 digital `held` / `pressed` / `released`。
- 支持 `axis1D` / `axis2D`。
- 默认 action map 可查询。
- GameModule 可以缓存 `InputActionService`。
- InputActionSystem 不 include GLFW。
- `cmake --build --preset msvc-vcpkg-debug` 通过。
- `Sandbox.exe --scene-serializer-smoke` 通过。
- 常规 Sandbox smoke 通过。
- 建议新增 `Sandbox.exe --input-action-smoke`，用 fake input 验证 pressed / released / axis 行为。

暂时不做：

- 可编辑 input binding UI。
- action map 保存 / 加载。
- 多 input context。
- gamepad。
- mouse delta / raw mouse。
- play mode 输入屏蔽策略。
- replay / network input buffer。

## 10. PR-G6：Gameplay components + basic systems

执行状态：已完成。

目标：

- 让 Runtime 从静态场景展示进入最小 gameplay 更新链路。
- 复用 PR-G5 的 `InputActionService`，让 gameplay 读取语义输入而不是直接读按键。
- 让 `GameModule::tick()` 成为第一批 gameplay systems 的固定挂载点。
- 保持 Scene / Renderer / Editor 边界干净：Scene 只保存数据，Game 负责行为，Renderer 只消费提取后的 `RenderDataPacket`。

当前基础：

- `GameModule` 已依赖 `Runtime` 和 `InputAction`，并缓存 `SceneService` / `InputActionService`。
- `RuntimeModule::postUpdate()` 已在所有 module tick 之后提取 active scene，所以 gameplay systems 在 `GameModule::tick()` 修改 `TransformComponent` 后，渲染数据会在同一帧后半段被提取。
- `SceneSerializer` 已追加 gameplay components 可选读写，旧 `.nxscene` 缺字段时仍按默认值加载。

完成记录：

- 新增 `Function/Scene/gameplay_component.h`，包含 `PlayerComponent`、`EnemyComponent`、`VelocityComponent`、`HealthComponent`、`CollectibleComponent` 和 `LifetimeComponent`。
- 新增 `Function/Game/gameplay_systems.h/.cpp`，实现 `PlayerControlSystem`、`EnemySystem`、`MovementSystem`、`LifetimeSystem` 和 `HealthSystem`。
- `GameModule::tick()` 按固定顺序调用第一批 gameplay systems。
- `SceneSerializer` 支持 gameplay components round-trip。
- Sandbox 新增 `--gameplay-systems-smoke`，覆盖 input-driven player movement、enemy velocity、movement integration、lifetime destroy、health destroy 和 serializer round-trip。

建议组件：

```cpp
struct PlayerComponent { float move_speed = 5.0f; };
struct EnemyComponent { float move_speed = 2.0f; };
struct VelocityComponent { glm::vec3 velocity{ 0.0f }; };
struct HealthComponent { int current = 1; int max = 1; };
struct CollectibleComponent { int score = 1; };
struct LifetimeComponent { float seconds = 1.0f; };
```

组件放置建议：

- 优先新增 `Function/Scene/gameplay_component.h`，存放 data-only gameplay components。
- `Function/Game` 只 include 这些组件并实现行为，不反向让 Scene 依赖 Game。
- 暂时不引入通用 ComponentRegistry / 反射 / 脚本绑定，避免把 PR-G6 做大。

建议系统：

```text
PlayerControlSystem
EnemySystem
ProjectileSystem
CollectibleSystem
MovementSystem
LifetimeSystem
HealthSystem
GameRuleSystem
```

第一版先保持简单，不急着建立完整 scheduler。可以先由 `GameModule` 按固定顺序调用：

```text
GameModule::tick()
  active_scene = SceneService::getActiveScene()
  PlayerControlSystem::update(scene, input_actions, delta_time)
  EnemySystem::update(scene, delta_time)
  MovementSystem::update(scene, delta_time)
  LifetimeSystem::update(scene, delta_time)
  HealthSystem::update(scene)
```

建议第一版优先落地这些系统：

1. `PlayerControlSystem`
   - 读取 `DefaultInputActions::Move`。
   - 写入玩家实体的 `VelocityComponent`。
   - 第一版按世界 XZ 平面移动，`Move.x -> X`，`Move.y -> -Z`。
   - 不做相机相对移动；相机控制留到 PR-G8。
2. `MovementSystem`
   - 对所有 `TransformComponent + VelocityComponent` 执行积分。
   - `translation += velocity * delta_time`。
   - 第一版不处理碰撞、不处理地面吸附。
3. `EnemySystem`
   - 可以先做非常轻的追踪玩家逻辑：找到第一个 `PlayerComponent`，让 `EnemyComponent + VelocityComponent` 朝玩家移动。
   - 如果不想影响默认编辑器场景，也可以先只在 smoke test 中验证。
4. `LifetimeSystem`
   - 对 `LifetimeComponent` 递减秒数。
   - 到期后延迟收集实体，再统一 destroy，避免遍历 view 时直接销毁导致迭代风险。
5. `HealthSystem`
   - `current <= 0` 时销毁实体。
   - 只处理生命周期，不处理伤害来源；伤害/触发留给 PR-G7。

`ProjectileSystem` / `CollectibleSystem` / `GameRuleSystem` 第一版可以先不做真实交互：

- Projectile 没有 prefab / spawn template 前，不建议在系统里硬编码模型资源。
- Collectible 需要 trigger overlap，应该等 PR-G7 collider / trigger。
- GameRule / score / victory / game over 更适合和 PR-G8 GameState / HUD 一起做。

推荐调用链：

```text
PlatformModule::tick()
  -> raw input snapshot

InputActionModule::tick()
  -> semantic input actions

RuntimeModule::tick()
  -> SceneManager::tick()
  -> SceneV2::tick()

GameModule::tick()
  -> gameplay systems mutate Scene components

RuntimeModule::postUpdate()
  -> active_scene->extractSceneData()

EditorModule::postUpdate()
  -> SceneView can override preview camera
```

这里的关键是：具体玩法不要塞回 `SceneV2::tick()`。`SceneV2` 继续作为场景数据容器和提取入口，`GameModule` 承担 runtime application/gameplay 行为。

序列化建议：

- `SceneSerializer` 增加 gameplay component 的可选读写。
- 旧 `.nxscene` 没有这些字段时按默认值加载，不需要升级 scene version。
- 新字段建议保持简单：
  - `"Player": { "move_speed": 5.0 }`
  - `"Enemy": { "move_speed": 2.0 }`
  - `"Velocity": { "velocity": [0, 0, 0] }`
  - `"Health": { "current": 1, "max": 1 }`
  - `"Collectible": { "score": 1 }`
  - `"Lifetime": { "seconds": 1.0 }`

测试建议：

- 新增 `Sandbox.exe --gameplay-systems-smoke`。
- smoke 中直接构建内存场景和 fake input，不依赖 Editor UI。
- 验证：
  - 按下 `Move` 后，player 的 `TransformComponent.translation` 按预期变化。
  - `VelocityComponent` 能被 `MovementSystem` 积分。
  - `LifetimeComponent` 到期实体被销毁。
  - `HealthComponent.current <= 0` 实体被销毁。
  - 如果本 PR 扩展 serializer，则 save/load 后 gameplay components 不丢失。

不应该：

- 不引入完整 scheduler / system graph。
- 不让 `GameModule` 依赖 Editor / ImGui / Vulkan。
- 不在 `SceneV2::tick()` 中写 Player / Enemy 行为。
- 不在 PR-G6 做物理、碰撞、trigger、raycast。
- 不在 PR-G6 做 RuntimeCameraController / HUD / GameState。
- 不在 PR-G6 做 prefab / spawn template。
- 不在默认材质展示场景里强行启用 WASD Player，避免和 EditorCamera 输入抢控制。

验收：

- 新增 gameplay component 数据定义。
- 新增基础 gameplay systems，并由 `GameModule::tick()` 固定顺序调用。
- `PlayerControlSystem` 使用 `InputActionService`，不 include GLFW / raw key code。
- `MovementSystem` / `LifetimeSystem` / `HealthSystem` 行为可被 smoke 覆盖。
- Scene / Renderer / Editor 不依赖 Game systems。
- `cmake --build --preset msvc-vcpkg-debug` 通过。
- `Sandbox.exe --input-action-smoke` 通过。
- `Sandbox.exe --gameplay-systems-smoke` 通过。
- 常规 Sandbox smoke 通过。

暂时不做：

- Play Mode / Edit Mode 输入隔离。
- 可视化 component 添加菜单。
- 组件反射 / 脚本绑定。
- gameplay component inspector。
- 游戏 HUD / score UI。
- 敌人生成、子弹生成、收集物触发。

## 11. 后续 PR 简述

### PR-G7：Simple Collider / Trigger

执行状态：已完成。

目标：

- 为 G6 的 gameplay components 补上最小实体关系能力：谁和谁发生 overlap。
- 支撑 `CollectibleComponent`、后续 Projectile / damage / simple enemy contact。
- 建立 `Function/Physics` 目录，但第一版仍保持轻量碰撞查询和 trigger overlap，不接完整物理引擎。

完成记录：

- 新增 `Function/Scene/collision_component.h`，包含 Sphere / AABB collider、Trigger 和 CollisionFilter 数据组件。
- 新增 `Function/Physics/physics_types.h`、`collision_query.*` 和 `trigger_overlap_system.*`。
- `TriggerOverlapSystem` 支持 sphere/sphere、aabb/aabb、sphere/aabb 和 layer/mask 过滤。
- `GameModule::tick()` 在 `MovementSystem` 之后运行 `TriggerOverlapSystem`。
- `CollectibleSystem` 消费 overlap 结果，支持 player overlap collectible 后销毁 collectible。
- `SceneSerializer` 支持 collider / trigger / filter round-trip。
- Sandbox 新增 `--physics-trigger-smoke`，覆盖 overlap、filter、collectible pickup 和 serializer round-trip。

目录边界建议：

```text
source/Engine/Function/Scene/
  collision_component.h        // 可序列化、后端无关的 collider 数据组件

source/Engine/Function/Physics/
  physics_types.h              // Physics layer / overlap result / world collider 描述
  collision_query.h
  collision_query.cpp          // sphere/aabb overlap 等纯查询函数
  trigger_overlap_system.h
  trigger_overlap_system.cpp   // 从 Scene 收集 collider，输出当前帧 overlap
```

这个拆法比把所有东西都塞进 `Function/Game` 更清爽：

- Scene 保存数据：Collider / Trigger / Filter 是可保存的场景组件。
- Physics 负责算法：shape world-space 转换、overlap 检测、layer/mask 过滤。
- Game 消费结果：Collectible / damage / game rule 只关心哪些实体发生了 trigger。

第一版不建议立刻注册独立 `PhysicsModule` tick。

原因是当前 `MovementSystem` 仍由 `GameModule::tick()` 调用，Trigger overlap 必须在 movement 之后执行。如果把 Physics 做成独立 module tick，但没有更细的 tick phase，容易出现：

```text
Physics tick before Game movement -> overlap 使用上一帧位置
Physics tick after Game movement  -> Game 本帧不好消费结果
```

因此 PR-G7 第一版建议：代码放在 `Function/Physics`，但由 `GameModule` 在 `MovementSystem` 之后显式调用。后续如果引入 scheduler / fixed update phase，再把它升级成真正的 `PhysicsModule`。

建议组件：

```cpp
struct SphereColliderComponent {
    glm::vec3 offset{ 0.0f };
    float radius = 0.5f;
    bool enabled = true;
};

struct AABBColliderComponent {
    glm::vec3 offset{ 0.0f };
    glm::vec3 half_extents{ 0.5f };
    bool enabled = true;
};

struct TriggerComponent {
    bool enabled = true;
};

struct CollisionFilterComponent {
    uint32_t layer = 1u;
    uint32_t mask = 0xffffffffu;
};
```

第一版约束：

- 一个实体优先只挂一种 collider shape。
- `TriggerComponent` 表示这个实体参与 trigger overlap。
- `CollisionFilterComponent` 可选；没有时使用默认 layer/mask。
- 只做 overlap，不做阻挡、不做位移修正、不做刚体响应。

世界空间规则：

- Sphere center = `Transform.translation + offset`。
- Sphere radius = `radius * max(abs(scale.x), abs(scale.y), abs(scale.z))`。
- AABB center = `Transform.translation + offset`。
- AABB half extents = `half_extents * abs(scale)`。
- 第一版忽略 rotation，因为 AABB 是 axis-aligned。

建议 Physics API：

```cpp
struct TriggerOverlap {
    entt::entity first{ entt::null };
    entt::entity second{ entt::null };
};

struct TriggerOverlapFrame {
    std::vector<TriggerOverlap> overlaps;
};

class TriggerOverlapSystem {
public:
    void update(SceneV2& scene);
    const TriggerOverlapFrame& getFrame() const;
};
```

`CollisionQuery` 第一版支持：

```text
sphere vs sphere
aabb vs aabb
sphere vs aabb
layer / mask filter
```

GameModule 调用顺序建议：

```text
GameModule::tick()
  PlayerControlSystem
  EnemySystem
  MovementSystem

  TriggerOverlapSystem
  CollectibleSystem

  LifetimeSystem
  HealthSystem
```

`CollectibleSystem` 第一版可以只做最小效果：

- 找到 `PlayerComponent` 与 `CollectibleComponent` 的 trigger overlap。
- 销毁 collectible entity。
- 不做 score UI，score / HUD 留到 PR-G8。

序列化建议：

- `SceneSerializer` 增加 collider / trigger / filter 的可选读写。
- 旧场景缺字段时按默认组件值加载，不升级 scene version。
- 建议字段：
  - `"SphereCollider": { "offset": [0,0,0], "radius": 0.5, "enabled": true }`
  - `"AABBCollider": { "offset": [0,0,0], "half_extents": [0.5,0.5,0.5], "enabled": true }`
  - `"Trigger": { "enabled": true }`
  - `"CollisionFilter": { "layer": 1, "mask": 4294967295 }`

测试建议：

- 新增 `Sandbox.exe --physics-trigger-smoke`。
- smoke 不依赖窗口和 Editor UI，直接构建内存 scene。
- 验证：
  - sphere/sphere overlap 命中。
  - aabb/aabb overlap 命中。
  - sphere/aabb overlap 命中。
  - 非 overlap 不产生结果。
  - layer/mask 过滤正确。
  - player overlap collectible 后 collectible 被销毁。
  - serializer round-trip 后 collider / trigger / filter 不丢。

暂时不做：

- 不接 Bullet / Jolt / PhysX。
- 不做 rigidbody。
- 不做 collision response / blocking movement / slide。
- 不做 sweep / continuous collision。
- 不做完整 raycast / ground check；如确实需要，可后续单独开 `Physics query v1`。
- 不做 trigger enter / stay / exit 历史事件；第一版只输出当前帧 overlap。
- 不做 editor collider inspector。
- 不做 renderer 效果；collider debug visualization 可作为后续调试小 PR。

验收：

- `Function/Physics` 目录建立，包含轻量 collision query 和 trigger overlap system。
- Collider / Trigger / Filter 组件可保存和加载。
- `GameModule` 在 movement 后运行 trigger overlap。
- 至少一个 gameplay 消费方使用 overlap 结果，例如 collectible pickup。
- `cmake --build --preset msvc-vcpkg-debug` 通过。
- `Sandbox.exe --gameplay-systems-smoke` 通过。
- `Sandbox.exe --physics-trigger-smoke` 通过。
- 常规 Sandbox smoke 通过。

### PR-G8：RuntimeCameraController + HUD / GameState

执行状态：已完成。

目标：

- 让 GameView 真正使用 runtime camera，而不是只能依赖默认静态 CameraComponent。
- 引入最小 `GameState`，让 pause / game over / victory 有明确状态边界。
- 把 G7 的 collectible trigger 结果转成 score，并在 HUD 中展示 score / health / pause / win / lose。
- 继续保持 Scene / Game / UI 边界清晰：Scene 保存数据，Game 运行玩法状态，HUD 只是状态的可视化。

实施前基础：

- `SceneV2::extractSceneData()` 已优先读取 `ActiveCameraComponent`，GameView 会使用场景里的 runtime camera。
- `ViewportPanel` 已有 Scene / Game 模式；SceneView 用 EditorCamera 覆盖相机，GameView 保留 active CameraComponent。
- `GameModule::tick()` 已按固定顺序运行 gameplay、movement、trigger、collectible、lifetime、health。
- `GameModule::renderUI()` 已预留 runtime HUD 提交点。
- `InputActionSystem` 已有 `DefaultInputActions::Pause`，默认绑定 Escape。
- G7 已有 `CollectibleSystem` 和 trigger overlap，但还没有 score / victory 状态承接。

完成记录：

- 新增 `RuntimeCameraControllerComponent` 和 `RuntimeCameraControllerSystem`，第一版支持 third-person follow，并写回 active `CameraComponent`。
- 新增 `GameStateModel`，支持 `Playing` / `Paused` / `GameOver` / `Victory` 和 score / health / collectible snapshot。
- `CollectibleSystem` 改为返回 `CollectibleFrameResult`，由 GameState 承接分数，而不是直接写 UI。
- 新增 `RuntimeHud`，第一版使用 ImGui overlay 绘制 score / health / pause / win / lose。
- `GameModule` 接入 pause gating、game state rule update、runtime camera update 和 HUD render。
- `SceneSerializer` 支持 `RuntimeCameraControllerComponent` 可选读写。
- Sandbox 新增 `--runtime-camera-smoke` 和 `--runtime-game-flow-smoke`。

建议第一版范围：

1. Runtime camera controller。
   - 优先做第三人称跟随相机，默认跟随第一个 `PlayerComponent`。
   - 不复用 `EditorCamera`；编辑器观察相机和运行时相机继续分离。
   - 新增 data-only 的 `RuntimeCameraControllerComponent`，挂在 active camera entity 上。
   - 新增 `RuntimeCameraControllerSystem`，在 `GameModule::tick()` 里更新 `CameraComponent` 的 view / projection / viewProjection。
   - camera transform 可以同步写回 `TransformComponent`，方便 Scene Hierarchy / Properties 后续观察。
2. GameState。
   - 新增轻量 `GameStateModel` 或 `GameStateService`，由 `GameModule` 拥有。
   - 第一版状态建议：
     ```cpp
     enum class GameState {
         MainMenu,
         Playing,
         Paused,
         GameOver,
         Victory
     };
     ```
   - 第一版可以默认从 `Playing` 开始，暂不做真正 main menu flow；保留 `MainMenu` 枚举供后续扩展。
   - `Pause` action 在 `Playing <-> Paused` 之间切换；GameOver / Victory 后可以忽略 gameplay tick。
3. Score / health / rule。
   - 建议让 `CollectibleSystem` 返回本帧收集结果，而不是直接把分数写进 HUD。
   - 例如返回 `CollectibleFrameResult { collected_count, collected_score }`，再由 `GameStateSystem` 累加 score。
   - `GameStateSystem` 从 scene 查询玩家 `HealthComponent`，同步 HUD 所需 health。
   - 胜利条件第一版可以是：本关 collectibles 总数大于 0，且当前 remaining collectibles 为 0。
   - 失败条件第一版可以是：找不到有效玩家，或玩家 `HealthComponent.current <= 0`。
4. HUD。
   - 第一版使用 ImGui 绘制，后续再替换成独立 runtime UI。
   - HUD 只读取 `GameStateModel` 的快照，不直接遍历 scene、不直接修改 gameplay。
   - ImGui 依赖集中在很小的 adapter 文件里，例如 `runtime_hud.cpp`，不要扩散到 camera / game state / gameplay systems。
   - HUD 不保存进 `.nxscene`。

建议新增或调整文件：

```text
source/Engine/Function/Scene/
  gameplay_component.h                  // 增加 RuntimeCameraControllerComponent，保持 data-only

source/Engine/Function/Game/
  runtime_camera_controller_system.h
  runtime_camera_controller_system.cpp
  game_state.h
  game_state.cpp
  runtime_hud.h
  runtime_hud.cpp
```

如果 `gameplay_component.h` 后续继续膨胀，可以再把 camera controller 数据拆到独立 `runtime_camera_component.h`。PR-G8 第一版为了少打散组件位置，可以先保持在现有 gameplay component 文件里。

建议组件：

```cpp
enum class RuntimeCameraMode {
    ThirdPersonFollow,
    FirstPerson
};

struct RuntimeCameraControllerComponent {
    RuntimeCameraMode mode = RuntimeCameraMode::ThirdPersonFollow;
    glm::vec3 follow_offset{ 0.0f, 3.0f, 6.0f };
    glm::vec3 target_offset{ 0.0f, 1.0f, 0.0f };
    float follow_speed = 10.0f;
    float fov_degrees = 45.0f;
    float aspect_ratio = 16.0f / 9.0f;
    bool enabled = true;
};
```

第一版可以只实现 `ThirdPersonFollow`，`FirstPerson` 先保留枚举或暂不暴露。跟随相机使用 `PlayerComponent + TransformComponent` 作为目标，计算：

```text
target = player.translation + target_offset
desired_camera_position = target + follow_offset
camera.position = lerp(camera.position, desired_camera_position, follow_speed * delta_time)
view = lookAt(camera.position, target, world_up)
projection = perspective(fov, aspect_ratio, nearClip, farClip)
viewProjection = projection * view
```

注意：

- `aspect_ratio` 第一版可以用组件默认值；后续再由 viewport / renderer size 同步。
- 不建议在 PR-G8 直接让 `GameModule` 依赖 RendererService，只为拿 viewport size。
- 如果要解决 GameView aspect，可以后续开小 PR 做 camera viewport size service。

建议 GameState 数据：

```cpp
struct GameStateSnapshot {
    GameState state = GameState::Playing;
    int score = 0;
    int collectible_count = 0;
    int remaining_collectibles = 0;
    int player_health = 0;
    int player_max_health = 0;
};

class GameStateModel {
public:
    void resetForScene(SceneV2& scene);
    void togglePause();
    void addCollected(int count, int score);
    void updateRules(SceneV2& scene);
    const GameStateSnapshot& getSnapshot() const;
};
```

这里不急着做完整 event bus。`GameModule` 直接按固定顺序把 `CollectibleSystem` 的结果交给 `GameStateModel` 即可。

建议调用顺序：

```text
GameModule::tick()
  if Pause pressed:
    GameStateModel::togglePause()

  if state == Playing:
    PlayerControlSystem
    EnemySystem
    MovementSystem

    TriggerOverlapSystem
    CollectibleSystem -> CollectibleFrameResult
    GameStateModel::addCollected(result.collected_count, result.collected_score)

    GameStateModel::updateRules(scene)
    RuntimeCameraControllerSystem

    LifetimeSystem
    HealthSystem

  else:
    RuntimeCameraControllerSystem  // 可选：暂停时仍保持 camera matrix 有效

RuntimeModule::postUpdate()
  active_scene->extractSceneData()

GameModule::renderUI()
  RuntimeHud::draw(GameStateModel::getSnapshot())
```

这个顺序的关键点：

- camera controller 必须在 `RuntimeModule::postUpdate()` 提取场景前更新 CameraComponent。
- pause 后 gameplay movement / trigger / lifetime 不继续推进。
- HUD 在 UI 阶段读取状态，不影响本帧 gameplay。
- `HealthSystem` 如果会销毁玩家，`GameStateModel::updateRules()` 应该先读取玩家血量。

HUD 第一版建议内容：

```text
Playing:
  Score: current / total
  Health: current / max

Paused:
  PAUSED
  Score / Health

Victory:
  VICTORY
  Final Score

GameOver:
  GAME OVER
  Final Score
```

HUD 不需要做复杂布局。第一版可以是主 viewport 上方的小 overlay，后续如果要严格贴到 GameView bounds，再让 Editor 暴露 viewport bounds 或引入 runtime UI target。

序列化建议：

- `RuntimeCameraControllerComponent` 可以作为可选 scene component 保存 / 加载。
- `GameStateModel` 不保存到 `.nxscene`，score / pause / win / lose 都是运行时状态。
- `HealthComponent` / `CollectibleComponent` 已由 G6 保存，PR-G8 不需要升级 scene version。
- 如果新增 camera controller 字段，旧 scene 缺字段时使用默认值。

测试建议：

- 新增 `Sandbox.exe --runtime-camera-smoke`。
  - 构建内存 scene：Player + ActiveCamera + RuntimeCameraController。
  - tick camera system 后检查 CameraComponent position / view / projection / viewProjection 非默认且稳定。
  - 调用 `SceneV2::extractSceneData()` 后检查 RenderDataPacket camera 与 active camera 一致。
- 新增 `Sandbox.exe --game-state-smoke` 或合并为 `--runtime-game-flow-smoke`。
  - 验证 Pause action 切换 `Playing <-> Paused`。
  - 验证 collectible pickup 后 score 增加。
  - 验证 remaining collectibles 为 0 后进入 Victory。
  - 验证玩家 health <= 0 后进入 GameOver。
  - 验证 paused / victory / game over 时 movement 不继续推进。
- 常规验证继续保留：
  - `cmake --build --preset msvc-vcpkg-debug` 通过。
  - `Sandbox.exe --input-action-smoke` 通过。
  - `Sandbox.exe --gameplay-systems-smoke` 通过。
  - `Sandbox.exe --physics-trigger-smoke` 通过。
  - 常规 Sandbox smoke 通过。

暂时不做：

- 不做完整 Play Mode / Edit Mode 生命周期。
- 不做 raw mouse capture / mouse-look。
- 不做完整 runtime UI 框架、样式系统、按钮导航。
- 不做 gamepad。
- 不做 camera collision / occlusion。
- 不做 checkpoint / level loading / restart flow。
- 不把 score / game state 保存进 `.nxscene`。
- 不让 GameState / RuntimeCamera 依赖 Editor / Vulkan backend。

验收：

- GameView 使用 runtime active camera，camera 能随 player 或目标更新。
- SceneView 仍使用 EditorCamera，不被 RuntimeCameraController 反向污染。
- Pause 能停止 gameplay 推进，并能恢复。
- Collectible pickup 能反映到 score。
- 玩家死亡进入 GameOver，收集完成进入 Victory。
- HUD 能展示 score / health / pause / win / lose。
- GameModule 不 include Editor / Vulkan backend。
- ImGui 依赖只出现在 HUD adapter，不扩散到 gameplay systems。
- RuntimeCameraController / GameState 行为有 smoke 覆盖。

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

下一步建议先执行 PR-G9：PrefabFactory / Spawn templates。

开始 PR-G9 前建议确认两个小决策：

1. 第一版 prefab 是否先用纯代码 factory，不急着做可序列化 prefab asset。
2. Sandbox demo 是否需要同步改成通过 factory 生成 player / enemy / collectible / projectile 样例。

确认后可以直接实现 prefab factory，并补上 factory / spawn smoke。

# NexAur 第一阶段架构优化方案：模块系统主线

日期：2026-06-22

## 1. 目标

第一阶段的目标不是重写引擎，也不是立刻接入 Vulkan、物理引擎或完整插件系统，而是先把 NexAur 的架构整理成一套更清爽、更可读、更容易扩展的模块系统。

这一阶段完成后，NexAur 应该具备下面几个特征：

- Engine 只负责应用生命周期、模块调度和事件路由。
- 各个模块有明确的依赖关系、初始化顺序和关闭顺序。
- 深层代码不再随意访问 `g_runtime_global_context`。
- Scene、Resource、Renderer、Editor、Physics、Audio、Runtime 的职责边界清楚。
- 后续接入 Vulkan 渲染器、物理模块、音频模块、小游戏 gameplay 时，不需要大面积反向修改已有模块。
- 每一步重构都保持当前 OpenGL Sandbox 可运行。

## 2. 为什么采用模块系统

之前文档里提到过 LayerStack，但从引擎主干架构角度看，LayerStack 更适合应用层、编辑器 overlay 或调试工具堆叠，不适合作为 NexAur 后续扩展渲染、物理、资源、编辑器、运行时的主干结构。

模块系统更适合当前目标：

- 它能表达系统级依赖，例如 Renderer 依赖 Platform 和 Resource，Editor 依赖 Runtime 和 Renderer facade。
- 它能管理生命周期，例如 Renderer 必须在 Window context 有效时初始化和销毁。
- 它能为 Vulkan、Physics、Audio 这种后续可替换模块预留边界。
- 它比全局上下文清楚，也比到处传一堆参数更可维护。

可以参考经典引擎的思路：

- Godot 将高层 Scene 与底层 Server 分离，RenderingServer、PhysicsServer、AudioServer 等负责底层能力，Scene 只描述世界。
- Unreal Engine 使用 Module、Subsystem、World、GameInstance 等层次组织功能，模块负责能力边界，Subsystem 负责具体服务。
- NexAur 不需要照搬它们的复杂度，但可以吸收核心思想：能力按模块拥有，模块之间通过稳定接口协作。

## 3. 非目标

本阶段暂时不做：

- 完整动态插件系统。
- 运行时热加载模块。
- 完整 Vulkan 渲染器。
- 完整 RenderGraph。
- 完整物理引擎。
- 完整 Asset Pipeline / 热重载。
- 完整 Prefab / Blueprint / 脚本系统。
- 大规模目录迁移。

第一阶段推荐做“静态模块系统”。也就是说，模块仍然编译进 Engine，但生命周期、依赖关系和对外服务先规范起来。

## 4. 核心设计原则

### 4.1 模块是能力边界

模块负责一类引擎能力的生命周期和对外接口。

推荐模块：

```text
CoreModule
PlatformModule
ResourceModule
RuntimeModule
RendererModule
EditorModule
PhysicsModule
AudioModule
GameModule
```

模块不是每个类的集合，也不是目录名字的简单替代。模块应该回答三个问题：

1. 我拥有哪些资源？
2. 我对外提供什么服务？
3. 我依赖哪些其他模块？

### 4.2 系统是模块内部的执行单元

System 不等于 Module。

例如：

```text
RuntimeModule
  owns SceneManager
  owns GameplaySystem list
  owns SceneSerializer

RendererModule
  owns RendererSystem
  owns RenderResourceCache
  owns RenderDevice
  owns RenderPipeline

PhysicsModule
  owns PhysicsSystem
  owns PhysicsWorld
  owns backend body mapping
```

模块偏生命周期和边界，系统偏具体执行逻辑。

### 4.3 服务是模块对外暴露的窄接口

模块之间不要互相暴露内部细节，而是暴露小服务。

例如 RendererModule 对外不应该暴露 OpenGLTexture、OpenGLFramebuffer，而是暴露：

```cpp
class RendererService {
public:
    virtual void setViewportSize(uint32_t width, uint32_t height) = 0;
    virtual uint32_t getViewportColorAttachment() const = 0;
    virtual int readViewportEntityID(int x, int y) = 0;
};
```

Editor 只依赖 `RendererService`，不依赖 OpenGL 或 Vulkan 后端。

### 4.4 数据跨模块传递，资源由模块拥有

跨模块传递的应该是轻量数据或 handle。

推荐：

```cpp
struct RenderObjectData {
    AssetHandle model;
    glm::mat4 transform;
    int entity_id;
};
```

避免：

```cpp
struct RenderObjectData {
    std::shared_ptr<VertexArray> vao;
    std::shared_ptr<Texture2D> texture;
};
```

同理：

- Scene 输出资产 handle。
- Renderer 内部解析成 GPU resource。
- Physics 读取 Collider / Rigidbody / Transform，写回模拟结果。
- Audio 读取 AudioSourceComponent，内部持有后端音源。

### 4.5 依赖只能朝稳定方向流动

推荐依赖方向：

```text
GameModule / Sandbox
  -> EditorModule
  -> RuntimeModule
  -> RendererModule
  -> PhysicsModule
  -> AudioModule
  -> ResourceModule
  -> PlatformModule
  -> CoreModule
```

更准确地说：

```text
Core
  no engine-level dependency

Platform
  depends on Core

Resource
  depends on Core, FileSystem, import libraries
  must not depend on Renderer/RHI

Runtime / Scene
  depends on Core, Resource handles
  must not depend on Renderer implementation

Renderer
  depends on Core, Platform native window abstraction, Resource CPU data
  owns GPU resources

Physics
  depends on Core, Runtime scene data
  must not depend on Renderer

Audio
  depends on Core, Resource audio assets, Runtime listener/source data
  must not depend on Renderer

Editor
  depends on Runtime public API, Renderer facade, Platform input abstraction
  must not depend on renderer backend internals

Application / Sandbox
  composition root
  configures modules
```

## 5. 目标架构草图

### 5.1 顶层结构

```text
Application / Sandbox
  creates EngineApplication
  selects startup modules
  loads startup scene or demo

EngineApplication
  owns ModuleManager
  owns frame loop
  owns event route

ModuleManager
  registers modules
  resolves dependencies
  initializes modules in order
  shuts modules down in reverse order
  ticks modules by stage

CoreModule
  log
  event
  time
  uuid
  assert

PlatformModule
  window
  input
  native handles

ResourceModule
  AssetManager
  AssetRegistry
  importers

RuntimeModule
  SceneManager
  SceneSerializer
  gameplay systems

RendererModule
  RendererSystem
  RenderDevice
  RenderResourceCache
  render pipelines and passes

EditorModule
  editor services
  panels
  scene view camera

PhysicsModule
  physics world
  physics backend

AudioModule
  audio backend
  audio sources/listeners
```

### 5.2 模块生命周期

推荐生命周期：

```text
registerModules
  create module instances
  declare module dependencies

registerServices
  modules expose narrow service interfaces
  no heavy resource creation yet

initialize
  create runtime resources
  init window, renderer, asset registry, scene manager

postInitialize
  modules that depend on multiple services finish wiring

tick
  per-frame update by stage

shutdown
  reverse dependency order
  release resources before dependent modules disappear
```

接口示例：

```cpp
enum class ModuleStage {
    Core,
    Platform,
    Resource,
    Runtime,
    Physics,
    Audio,
    Renderer,
    Editor,
    Game
};

struct ModuleInfo {
    std::string name;
    ModuleStage stage;
    std::vector<std::string> dependencies;
};

class EngineModule {
public:
    virtual ~EngineModule() = default;

    virtual ModuleInfo getInfo() const = 0;
    virtual void registerServices(ModuleRegistry& registry) {}
    virtual void initialize(ModuleContext& context) {}
    virtual void postInitialize(ModuleContext& context) {}
    virtual void tick(const TickContext& tick_context) {}
    virtual void onEvent(Event& event) {}
    virtual void shutdown() {}
};
```

注意：`ModuleContext` 只建议在模块初始化和模块级协作中使用，不要一路传到深层业务类里，否则会退化成另一个全局上下文。

### 5.3 每帧阶段

推荐使用 frame stage，而不是 LayerStack。

```text
Frame Begin
  PlatformModule begin frame
  InputModule update snapshot

PreUpdate
  EditorModule process editor commands
  RuntimeModule process scene transitions

FixedUpdate
  PhysicsModule step fixed simulation

Update
  RuntimeModule tick gameplay systems
  AudioModule update sources/listener

PostUpdate
  RuntimeModule extract scene changes
  EditorModule sync editor camera or selection

RenderExtract
  RuntimeModule / EditorModule prepare RenderDataPacket

Render
  RendererModule resolve resources and render

UI
  EditorModule draw editor UI
  GameModule draw runtime HUD if needed

Present
  PlatformModule swap buffers
  PlatformModule poll events
```

短期可以先实现少量阶段：

```text
BeginFrame
Update
Render
UI
EndFrame
```

后续再增加 FixedUpdate、RenderExtract、PostUpdate。

## 6. 全局上下文的替代方案

`RunTimeGlobalContext` 当前承担了太多全局访问职责。第一阶段不建议立刻删除它，而是逐步把它降级为模块系统启动时的兼容桥。

### 6.1 保留但限制访问

允许访问：

- `EngineApplication`
- `ModuleManager`
- `Sandbox` / Application 入口
- 迁移期兼容代码

不推荐访问：

- Render Pass
- Editor Panel
- EditorCamera
- Resource Uploader
- IBL Builder
- Gameplay System
- Physics System
- Runtime Component

### 6.2 用 ModuleRegistry 替代万能全局

模块之间通过 Registry 注册服务：

```cpp
class ModuleRegistry {
public:
    template<typename T>
    void registerService(T* service);

    template<typename T>
    T& getService();
};
```

但要注意：

- Registry 只在模块初始化和模块顶层使用。
- 不把 Registry 传进 Entity、Component、Pass、Panel 深处。
- 深层对象仍然通过构造函数、函数参数或小 Context 获取最小依赖。

### 6.3 服务示例

```text
PlatformModule
  provides WindowService
  provides InputService

ResourceModule
  provides AssetService

RuntimeModule
  provides SceneService
  provides SceneEditingService

RendererModule
  provides RendererService

EditorModule
  provides SelectionService
  provides ViewportService
```

## 7. 模块详细设计

### 7.1 CoreModule

职责：

- 日志。
- 断言。
- 事件基础类型。
- 时间。
- UUID。
- 基础宏。
- EngineModule、ModuleManager、ModuleRegistry 基础接口。

不应该包含：

- GLFW。
- OpenGL。
- Vulkan。
- ImGui。
- Assimp。
- Scene。
- Renderer。

建议新增：

```text
Core/Module/engine_module.h
Core/Module/module_manager.h
Core/Module/module_registry.h
Core/Module/tick_context.h
```

### 7.2 PlatformModule

职责：

- 创建窗口。
- 管理 native window。
- poll events。
- present / swap buffers。
- 维护输入快照。
- 屏蔽 GLFW 细节。

建议服务：

```cpp
class WindowService {
public:
    virtual void* getNativeWindow() const = 0;
    virtual std::pair<uint32_t, uint32_t> getSize() const = 0;
    virtual void setTitle(const char* title) = 0;
};

class InputService {
public:
    virtual const InputState& getState() const = 0;
};
```

当前 `WindowSystem` 和 `InputSystemGLFW` 可以先作为 PlatformModule 内部实现，不急着搬目录。

### 7.3 ResourceModule

职责：

- AssetHandle。
- AssetMetadata。
- AssetRegistry。
- AssetManager。
- CPU 资产导入。
- 模型、纹理、音频、材质等资产元数据。

不应该：

- 创建 GPU texture。
- 创建 VertexArray。
- 调用 RendererCommand。
- 访问 ImGui。

建议服务：

```cpp
class AssetService {
public:
    virtual AssetHandle importAsset(const std::string& path, AssetType type) = 0;
    virtual const AssetMetadata* getMetadata(AssetHandle handle) const = 0;
    virtual std::shared_ptr<Model> loadModelCPU(AssetHandle handle) = 0;
};
```

短期保留 `AssetManager::getInstance()` 兼容，但新模块优先通过 AssetService。

### 7.4 RuntimeModule

职责：

- SceneManager。
- Scene lifecycle。
- Scene tick。
- Scene serialization。
- Gameplay system schedule。
- Runtime camera。
- Runtime-side component editing API。

不应该：

- 创建 GPU 对象。
- 访问 ImGui。
- 依赖 OpenGL/Vulkan。
- 直接访问 Editor panel。

建议服务：

```cpp
class SceneService {
public:
    virtual std::shared_ptr<SceneV2> getActiveScene() = 0;
    virtual void setActiveScene(std::shared_ptr<SceneV2> scene) = 0;
};
```

```cpp
class SceneEditingService {
public:
    virtual Entity createEntity(const std::string& name) = 0;
    virtual void destroyEntity(Entity entity) = 0;
    virtual void renameEntity(Entity entity, const std::string& name) = 0;
};
```

后续 PhysicsModule、AudioModule、GameModule 都应该围绕 RuntimeModule 的 scene data 工作。

### 7.5 RendererModule

职责：

- RendererSystem。
- RenderDevice。
- OpenGLRenderDevice。
- 后续 VulkanRenderDevice。
- RenderResourceCache。
- RenderPipeline。
- RenderPassContext。
- IBL、Shadow、Skybox、Picking。

外部只应该看到 RendererService：

```cpp
class RendererService {
public:
    virtual void render(const RenderDataPacket& data) = 0;
    virtual void setViewportSize(uint32_t width, uint32_t height) = 0;
    virtual std::pair<uint32_t, uint32_t> getViewportSize() const = 0;
    virtual uint32_t getViewportColorAttachment() const = 0;
    virtual int readViewportEntityID(int x, int y) = 0;
};
```

RendererModule 可以依赖：

- PlatformModule：获取 native window / context。
- ResourceModule：读取 CPU 资产。
- CoreModule。

RendererModule 不应该依赖：

- EditorModule。
- Physics backend internals。
- GameModule。

### 7.6 EditorModule

职责：

- Editor UI。
- Editor services。
- Editor camera controller。
- Viewport panel。
- Hierarchy panel。
- Properties panel。
- Gizmo。
- Scene editing commands。

建议内部服务：

```text
SelectionService
  current selected entity
  selection source
  select / clear

ViewportService
  viewport bounds
  viewport size
  hovered/focused
  picking request/result

EditorCameraController
  owns editor camera input

EditorCommandStack
  later undo/redo
```

EditorModule 可以依赖：

- RuntimeModule 的 SceneService / SceneEditingService。
- RendererModule 的 RendererService。
- PlatformModule 的 InputService。

EditorModule 不应该依赖：

- OpenGLRendererAPI。
- VulkanRendererAPI。
- Renderer backend object。

### 7.7 PhysicsModule

后续接入物理时，建议作为独立模块，而不是塞进 RuntimeModule 或 RendererModule。

职责：

- PhysicsWorld。
- PhysicsSystem。
- Physics backend。
- Entity 到 physics body handle 的映射。
- 读取 Rigidbody / Collider / Transform。
- 写回模拟结果。

接口：

```cpp
class PhysicsService {
public:
    virtual void step(SceneV2& scene, TimeStep ts) = 0;
};
```

Scene 只保存通用组件：

- RigidbodyComponent
- BoxColliderComponent
- SphereColliderComponent
- CapsuleColliderComponent
- CharacterControllerComponent

不要在组件里保存 Jolt/Bullet 原生指针。

### 7.8 AudioModule

职责：

- Audio backend。
- Audio clip asset。
- Audio source。
- Audio listener。
- 播放、暂停、音量控制。

接口：

```cpp
class AudioService {
public:
    virtual void play(AssetHandle audio_clip) = 0;
    virtual void update(SceneV2& scene, TimeStep ts) = 0;
};
```

Scene 组件：

```cpp
struct AudioSourceComponent {
    AssetHandle audio_clip;
    bool loop = false;
    float volume = 1.0f;
    bool play_on_start = false;
};

struct AudioListenerComponent {
    bool primary = true;
};
```

## 8. 具体改造步骤

### PR1：EngineModule 与 ModuleManager 骨架

状态：已完成（2026-06-22）。

目标：

- 建立静态模块系统骨架，替代 LayerStack 作为主线方案。

任务：

- 新增 `EngineModule`。
- 新增 `ModuleInfo`。
- 新增 `ModuleManager`。
- 新增 `ModuleRegistry`。
- 支持模块注册、依赖声明、初始化顺序、反向关闭顺序。
- 先不做动态加载。

验收：

- Engine 可以注册 Core、FileSystem、Platform、Resource、RenderContext、Renderer、Runtime、UI 等静态模块。
- 初始化顺序可打印日志。
- 关闭顺序与初始化顺序相反。
- EditorLayer 暂时仍由 Engine 兼容创建，完整 EditorModule 接管放到 PR6。

实际落地：

- 新增 `Core/Module/engine_module.h`。
- 新增 `Core/Module/module_manager.h/.cpp`。
- `ModuleManager` 支持模块注册、依赖拓扑排序、初始化、事件分发、tick、反向 shutdown。
- `ModuleRegistry` 支持按类型注册和获取模块服务。
- `ModuleManager` 会输出模块初始化和关闭顺序日志，方便后续 PR 验证生命周期。

### PR2：RunTimeGlobalContext 降级为兼容桥

状态：已完成（2026-06-22）。

目标：

- 不立即删除全局上下文，但减少深层访问。

任务：

- `RunTimeGlobalContext::startSystems()` 逐步迁移到 ModuleManager 初始化。
- `RunTimeGlobalContext` 暂时持有模块创建出的服务指针，兼容旧代码。
- 新代码从 ModuleRegistry 获取模块服务。
- 明确标注不推荐深层 include `global_context.h`。

验收：

- Sandbox 行为不变。
- 新增模块不直接访问 `g_runtime_global_context`。

实际落地：

- `RunTimeGlobalContext::startSystems()` 改为创建 `ModuleManager` 并注册静态模块。
- 当前落地的静态模块包括 Core、FileSystem、Platform、Resource、RenderContext、Renderer、Runtime、UI。
- `RunTimeGlobalContext` 继续保留旧公开指针，作为旧代码兼容桥。
- `RunTimeGlobalContext` 新增 `getModuleManager()` / `getModuleRegistry()`，给迁移期的模块顶层代码使用。
- 模块初始化时同步兼容指针；模块关闭时按依赖反向顺序释放资源。
- `RunTimeGlobalContext::shutdownSystems()` 不再手写系统销毁顺序，而是委托 `ModuleManager`，完成后兜底清空兼容指针。
- `LogSystem::init()` 支持同进程重复调用，避免兼容桥重启时重复创建同名 logger。

### PR3：PlatformModule 与 InputState

状态：已完成（2026-06-22）。

目标：

- 把窗口和输入收口到 PlatformModule。

任务：

- PlatformModule 创建 WindowSystem。
- PlatformModule 创建 InputSystem。
- 新增 `InputState`。
- InputSystem 每帧更新输入快照。
- 注册 WindowService 和 InputService。

验收：

- 窗口创建、事件、present 正常。
- EditorCamera 可以只依赖 InputState。

实际落地：

- 新增 `Function/Input/input_state.h`。
- 新增 `Function/Platform/platform_services.h`，包含 `WindowService` 和 `InputService`。
- `InputSystemGLFW` 改为持有 GLFW window 指针，不再从 `g_runtime_global_context` 获取窗口。
- `InputSystemGLFW::update()` 每帧刷新 `InputState`。
- `PlatformModule` 注册 `WindowService`、`WindowSystem`、`InputService`、`InputSystem`。
- Engine 每帧调用 `ModuleManager::tickModules()`，由 PlatformModule 刷新输入快照。
- `EditorCamera` 通过 `InputService` 读取 `InputState`，不再直接访问全局 InputSystem。

### PR4：RuntimeModule 与 SceneService

状态：已完成（2026-06-22）。

目标：

- SceneManager 和 Scene lifecycle 从全局上下文中抽到 RuntimeModule。

任务：

- RuntimeModule 创建 SceneManager。
- RuntimeModule 注册 SceneService。
- RuntimeModule tick active scene。
- 保持 SceneV2 当前行为不变。

验收：

- 默认场景正常创建。
- Sandbox 场景测试代码可以通过 SceneService 或兼容接口拿 active scene。

实际落地：

- 新增 `Function/Scene/scene_service.h`。
- `SceneManager` 实现 `SceneService`。
- `RuntimeModule` 注册 `SceneService` 和 `SceneManager`。
- Engine 的逻辑更新优先通过 `SceneService` 获取 active scene，旧 `m_scene_manager` 作为兼容 fallback。
- `EditorLayer` / `EditorContext` 接入 `SceneService`，并在同步面板上下文时刷新 active scene。

### PR5：RendererModule 与 RendererService

状态：已完成（2026-06-22）。

目标：

- RendererSystem 成为 RendererModule 内部实现，对外暴露 RendererService。

任务：

- RendererModule 创建 RendererSystem。
- RendererModule 注册 RendererService。
- RendererService 提供 viewport、render、picking 等窄接口。
- Editor 只依赖 RendererService。

验收：

- OpenGL 渲染行为不变。
- Viewport framebuffer 正常显示。
- Picking 正常。

实际落地：

- 新增 `Function/Renderer/RHI/renderer_service.h`。
- `RendererSystem` 实现 `RendererService`。
- `RendererModule` 注册 `RendererService` 和 `RendererSystem`。
- Engine 渲染提交优先调用 `RendererService::render()`。
- `ViewportPanel` 改为依赖 `RendererService` 完成 viewport resize、color attachment 显示和 picking。
- `RendererService::getViewportFramebuffer()` 在 Phase1 期间曾作为兼容接口保留；Phase1.1 PR14 已删除，Editor 现在只通过 color attachment、resize 和 picking 窄接口工作。

### PR6：EditorModule 与 EditorServices

状态：已完成（2026-06-22）。

目标：

- EditorLayer 转为 EditorModule 内部系统，而不是 Engine 主循环硬编码对象。

任务：

- EditorModule 创建 EditorController 或 EditorSystem。
- 新增 SelectionService。
- 新增 ViewportService。
- 新增 EditorCameraController。
- 面板从 EditorContext 获取服务，而不是直接写裸状态。

验收：

- Hierarchy 选择正常。
- Viewport picking 正常。
- Properties 修改 Transform 正常。

实际落地：

- `EditorModule` 创建并持有 `EditorLayer`，`Engine` 不再直接调用 `EditorService::update()` / `renderUI()`。
- `EngineModule` / `ModuleManager` 新增 `postUpdate` 和 `renderUI` 阶段，Editor 更新和 UI 绘制由模块阶段调度。
- `EditorModule` 组装 `EditorContext`，注入 `SceneService`、`RendererService`、`InputService`、`UIService`、`RenderContext` 等窄依赖。
- `EditorLayer` 不再主动从 `RunTimeGlobalContext` 获取服务，改为使用外部注入的 `EditorContext`。
- `EditorModule` 注册 `EditorService`、`SelectionService`、`ViewportService`。
- `SceneHierarchyPanel` 与 `ViewportPanel` 写选择状态时优先通过 `SelectionService`。
- `editor_context.h` 清理为明确字段，修复 `selected_entity` 被注释行吞掉的潜在问题。

保留事项：

- EditorCameraController 仍以内嵌 `EditorCamera` + `EditorLayer::updateViewportCamera()` 的形式存在，后续可在编辑器交互继续复杂化时单独拆类。

### PR7：事件路由与 UI 捕获策略

状态：已完成（2026-06-22）。

目标：

- 模块之间事件传播规则清楚。

任务：

- ModuleManager 负责把事件按阶段传给模块。
- UI 捕获输入时阻断 Runtime/Editor 不该收到的事件。
- Editor Viewport hovered/focused 时允许 viewport 处理输入。
- Event handled 后停止继续传播。

验收：

- UI 输入框里按 WASD 不移动相机。
- 点击普通 UI 不触发 viewport picking。
- Gizmo 操作时不触发 picking。

实际落地：

- `ModuleStage` 新增 `UI` 阶段。
- `ModuleManager` 维护独立事件分发顺序：按模块阶段从高到低分发，同阶段保持后初始化模块优先；`event.handled` 后停止传播。
- `UISystem` 实现新增的 `UIService`，`UIModule` 注册 `UIService` 和 `UISystem`。
- UI 捕获策略从 `Engine::onEvent()` 下沉到 `UIModule::onEvent()`。
- 当 ImGui 捕获键盘或文本输入时，键盘事件会被 UI 阻断，EditorCamera 的轮询更新也会暂停。
- 当 ImGui 捕获鼠标且鼠标不在 Editor Viewport 上时，鼠标事件会被 UI 阻断；Viewport hovered 时仍允许 picking、滚轮和视口交互。
- `RendererModule` 将事件转发给 `RendererSystem`，`RendererSystem` 补齐窗口 resize 事件处理。

### PR8：RenderPassContext 最小实现

状态：已完成（2026-06-22）。

目标：

- Pass 不再直接访问全局 RendererSystem / WindowSystem。

任务：

- 新增 `RenderPassContext`。
- RendererModule / RendererSystem 创建 context。
- IRenderPass::run 接收 context 和 render data。
- ShadowPass 从 context 获取 viewport 和 target。
- SkyboxPass 使用 context。

验收：

- Shadow、Skybox、PBR 渲染正常。
- Pass 源码不 include `global_context.h`。

实际落地：

- 新增 `Function/Renderer/RHI/render_pass_context.h`，用 `RenderPassContext` 显式描述当前 viewport framebuffer 和 viewport 尺寸。
- `RenderPipeline::render()`、`RenderForwardPipeline::render()`、`IRenderPass::run()`、`IRenderPass::execute()` 改为接收 `RenderPassContext`。
- `RendererSystem` 在每帧创建 `RenderPassContext`，并在绑定 viewport framebuffer 时统一设置 viewport。
- `ShadowPassV2` 不再从 `g_runtime_global_context` 读取 `WindowSystem` / `RendererSystem`，而是用 context 恢复 viewport target。
- `SkyboxPassV2` 使用新的 pass 接口，Pass 目录已不再 include `global_context.h`。
- `RenderIBLBuilder` 去掉对 `global_context.h` / `WindowSystem` / `RendererSystem` 的依赖，烘焙后 viewport 恢复交给 `RendererSystem` 的帧入口统一处理。
- `NX_ASSET` 的轻量定义下沉到 `Function/File/file_system.h`，渲染 Pass 获取资源路径不再需要 include `global_context.h`。

### PR9：RenderDevice 最小接口

状态：已完成（2026-06-22）。

目标：

- 为 Vulkan 接入预留后端设备抽象。

任务：

- 新增 `RenderDevice`。
- 新增 `OpenGLRenderDevice`。
- RendererModule 持有 device。
- RenderResourceCache 通过 device 创建资源。
- RendererFactory 静态函数逐步转成兼容 wrapper。

验收：

- OpenGL 行为不变。
- 新代码优先使用 RenderDevice。
- VulkanRenderDevice 后续可以从同一接口开始实现。

实际落地：

- 新增 `Function/Renderer/RHI/render_device.h/.cpp`，定义 `RenderDevice` 最小资源创建接口。
- 新增 `Function/Renderer/Platform/OpenGL/opengl_render_device.h/.cpp`，当前 OpenGL 后端通过该 device 创建 framebuffer、shader、texture、buffer、vertex array、uniform buffer 等资源。
- `RendererFactory` 从 `RendererSystem` 中移出，变成 `RenderDevice` 上方的兼容静态门面；旧调用点可以继续使用，内部会转发到当前 active device。
- `RendererFactory` 不再隐式创建 OpenGL fallback；RendererSystem 必须先设置 active device，避免中立 RHI 层反向依赖具体 OpenGL 后端。
- `RendererSystem` 创建并持有 `OpenGLRenderDevice`，初始化时注册给 `RendererFactory`，关闭时清理 active device。
- `Renderer::init()` 的 camera UBO、`RenderResourceCache` 的 Texture2D、`RenderResourceUploader` 的 mesh GPU 上传、`RenderIBLBuilder` 的离屏 framebuffer 创建都改为走 `RendererFactory -> RenderDevice`。
- `ProceduralModelFactory` 不再 include `renderer_system.h`，改为只依赖 `render_device.h` 的资源创建门面。
- CMake 已加入 `render_device`、`render_pass_context`、`opengl_render_device` 新文件。

保留事项：

- Phase1.1 已将 `AssetManager` 的 GPU 创建路径降级：`loadTexture()` / `loadTextureCube()` / `loadShader()` 只登记资产身份，`getTexture()` / `getTextureCube()` / `getShader()` 仅作为 legacy cache 查询入口保留。新渲染主路径使用 `AssetHandle -> RenderResourceCache -> RenderDevice`。
- `ProceduralModelFactory` 已移动到 `Function/Renderer/Resources`，避免 Resource 目录继续承担 GPU buffer 创建职责。

验证：

- `cmake --build build --config Debug` 已通过。
- 构建仍有既有 C4251 DLL 导出 warning，未发现 PR8/PR9 引入的编译错误。

## 9. 目录与命名建议

第一阶段不建议大规模移动目录，但建议逐步朝这个方向靠拢：

```text
source/Engine/Core/
  Events/
  Log/
  Time/
  UUID.*
  Module/

source/Engine/Function/Platform/
  platform_module.*
  Window/
  Input/

source/Engine/Function/Resource/
  resource_module.*
  asset_handle.h
  asset_metadata.h
  asset_manager.*
  importers/

source/Engine/Function/Scene/
  runtime_module.*
  scene_v2.*        # 后续稳定后重命名为 scene.*
  scene_manager.*
  scene_factory.*
  scene_serializer.*
  component.h
  entity.*

source/Engine/Function/Renderer/
  renderer_module.*
  renderer_system.*
  data/
  RHI/
  Resources/
  Passes/

source/Engine/Editor/
  editor_module.*
  editor_controller.*
  editor_context.*
  Services/
  Panels/
```

命名建议：

- `SceneV2` 后续稳定后重命名为 `Scene`。
- `ShadowPassV2` 后续重命名为 `ShadowPass`。
- `SkyboxPassV2` 后续重命名为 `SkyboxPass`。
- `RunTimeGlobalContext` 保留为兼容桥，但不作为新架构主入口。
- `Function` 目录后续可以逐步拆成 Runtime / Renderer / Platform / Asset，但不是第一阶段重点。

## 10. 模块依赖约束清单

### Core

允许依赖：

- C++ 标准库。

禁止依赖：

- GLFW。
- OpenGL/Vulkan。
- ImGui。
- Assimp。
- Scene。
- Renderer。

### Platform

允许依赖：

- Core。
- GLFW 等窗口库。

禁止依赖：

- RendererModule。
- RuntimeModule。
- EditorModule。

### Resource

允许依赖：

- Core。
- FileSystem。
- Assimp / stb 等导入库。

禁止依赖：

- RendererCommand。
- VertexArray。
- Texture2D GPU object。
- Framebuffer。
- ImGui。

### Runtime / Scene

允许依赖：

- Core。
- AssetHandle。
- EnTT。
- glm。

禁止依赖：

- OpenGL/Vulkan。
- RendererCommand。
- Framebuffer。
- ImGui。
- Editor。

### Renderer

允许依赖：

- Core。
- Platform native handle abstraction。
- Resource CPU assets。

禁止依赖：

- Editor Panel。
- Gameplay System。
- Physics backend internals。

### Editor

允许依赖：

- Runtime public API。
- RendererService。
- InputService。
- ImGui。

禁止依赖：

- OpenGL implementation classes。
- Vulkan implementation classes。
- Renderer backend internals。

### Physics

允许依赖：

- Core。
- Runtime scene public API。
- Transform / Collider / Rigidbody components。

禁止依赖：

- Renderer。
- Editor。
- Audio。

## 11. 架构质量检查表

每次新增模块或重构后，可以问这些问题：

1. 这个模块对外暴露的是服务接口，还是内部实现细节？
2. 这个模块依赖哪些模块？依赖方向是否合理？
3. 这个类真正需要哪些依赖？能否从构造函数看出来？
4. 它是否 include 了不该知道的模块？
5. 它是否直接访问 `g_runtime_global_context`？
6. 它是否持有了别的模块的后端对象？
7. 它是否把 OpenGL/Vulkan 细节暴露给 Scene 或 Resource？
8. 它是否让 Editor 和 Runtime 互相污染？
9. 如果明天替换 Vulkan，这个类要不要改？
10. 如果明天接入物理引擎，这个类要不要改？
11. 如果这个模块单独测试，是否需要启动整个引擎？
12. 这个抽象是不是为了解决当前真实问题，而不是提前想象的复杂度？

## 12. 第一阶段完成标准

第一阶段可以认为完成，当满足：

- Engine 使用 ModuleManager 管理模块生命周期。
- Core、Platform、Resource、Runtime、Renderer、Editor 至少有静态模块边界。
- 初始化顺序由模块依赖决定，关闭顺序自动反向执行。
- `RunTimeGlobalContext` 降级为兼容桥，而不是新代码主入口。
- EditorCamera 不再直接访问全局 InputSystem。
- ViewportPanel 不再直接访问全局 InputSystem。
- Event handled 和 UI capture 策略可预测。
- Render Pass 不再直接访问全局 WindowSystem / RendererSystem。
- RendererModule 内部有最小 RenderPassContext。
- RenderDevice 最小接口存在，OpenGL 走该接口或兼容 wrapper。
- Scene 和 Resource 不创建新的 GPU 对象。
- Sandbox 仍能正常启动、渲染、操作 viewport、picking。

## 13. 推荐优先级总结

最先做：

1. EngineModule。
2. ModuleManager。
3. ModuleRegistry。
4. CoreModule / PlatformModule / ResourceModule / RuntimeModule / RendererModule / EditorModule 静态注册。
5. RunTimeGlobalContext 降级为兼容桥。
6. InputState。
7. EditorCamera 输入解耦。

然后做：

1. SelectionService。
2. ViewportService。
3. RendererService。
4. RenderPassContext。
5. RenderDevice 最小接口。
6. SceneSerializer 第一版。

暂时不做：

1. 完整 Vulkan。
2. 完整物理引擎。
3. 完整 RenderGraph。
4. 动态插件系统。
5. 大规模目录搬迁。

## 14. 结论

NexAur 第一阶段最适合走模块系统，而不是 LayerStack。LayerStack 更像应用层组织方式，模块系统更像引擎能力边界。模块负责生命周期和依赖，系统负责具体执行，服务负责对外窄接口，这会让后续 Vulkan、Physics、Audio、Gameplay 都更自然地接进来。

这一阶段的目标可以概括为一句话：

```text
让模块拥有资源，让服务暴露能力，让数据跨模块流动，让深层代码远离全局上下文。
```

如果能做到这一点，NexAur 后续从 OpenGL 走向 Vulkan、从编辑器预览走向可玩 demo，都会轻松很多。

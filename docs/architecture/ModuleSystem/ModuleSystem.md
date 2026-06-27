# NexAur 模块系统架构说明

日期：2026-06-22

## 1. 范围

本文档说明 NexAur 第一阶段架构优化中已经落地的模块系统。

当前覆盖范围：

- PR1：`EngineModule` 与 `ModuleManager` 骨架。
- PR2：`RunTimeGlobalContext` 降级为兼容桥。
- PR3：`PlatformModule` 与 `InputState`。
- PR4：`RuntimeModule` 与 `SceneService`。
- PR5：`RendererModule` 与 `RendererService`。
- PR6：`EditorModule` 与 Editor services。
- PR7：事件路由与 UI 捕获策略。
- PR8：`RenderPassContext`。
- PR9：`RenderDevice` / `OpenGLRenderDevice`。
- PR10-PR17：Phase1.1 残留旧代码清理。

暂不包含：

- Vulkan 后端完整实现。
- 物理、音频、脚本、Gameplay 模块的完整实现。

## 2. 设计目标

Phase1 的目标不是重写引擎，而是把原先集中在 `RunTimeGlobalContext` 和 `Engine` 主循环里的职责拆成更清晰的模块边界。

核心目标：

- Engine 只负责应用生命周期、帧调度和事件入口。
- Module 拥有系统资源和生命周期。
- Service 暴露跨模块能力。
- 深层代码不要直接访问 `g_runtime_global_context`。
- 后续接入 Vulkan、Physics、Audio、GameModule 时，不需要大面积反向修改 Editor/Runtime/Renderer。

一句话总结：

```text
Engine 调度模块，Module 拥有系统，Service 暴露能力，Context 传递局部依赖。
```

## 3. 核心概念

### 3.1 EngineModule

位置：

```text
source/Engine/Core/Module/engine_module.h
```

`EngineModule` 是所有模块的统一生命周期协议。

```cpp
class EngineModule {
public:
    virtual ModuleInfo getInfo() const = 0;
    virtual void registerServices(ModuleContext& context) {}
    virtual void initialize(ModuleContext& context) {}
    virtual void postInitialize(ModuleContext& context) {}
    virtual void tick(const TickContext& tick_context) {}
    virtual void postUpdate(const TickContext& tick_context) {}
    virtual void renderUI(const TickContext& tick_context) {}
    virtual void onEvent(Event& event) {}
    virtual void shutdown(ModuleContext& context) {}
};
```

生命周期含义：

| 阶段 | 用途 |
| --- | --- |
| `registerServices` | 预留服务注册阶段。适合先注册服务壳，避免模块互相找不到接口。 |
| `initialize` | 创建真实系统资源，例如窗口、输入、渲染器、场景管理器。 |
| `postInitialize` | 所有模块初始化完成后的最终补线阶段。 |
| `tick` | 每帧早期更新。当前主要用于 Platform 刷新输入快照。 |
| `postUpdate` | 逻辑更新后阶段。EditorCamera 覆盖渲染数据放在这里。 |
| `renderUI` | UI 绘制阶段。EditorModule 通过这里绘制编辑器 UI。 |
| `onEvent` | 模块事件入口。事件被 handled 后停止传播。 |
| `shutdown` | 释放模块资源。ModuleManager 会按反向初始化顺序调用。 |

### 3.2 ModuleInfo

`ModuleInfo` 描述模块元信息。

```cpp
struct ModuleInfo {
    std::string name;
    ModuleStage stage;
    std::vector<std::string> dependencies;
};
```

三个字段分别回答：

- `name`：模块唯一名字。
- `stage`：模块所属阶段，主要用于事件优先级。
- `dependencies`：初始化依赖，用于拓扑排序。

例如：

```cpp
return { "Editor", ModuleStage::Editor, { "UI", "Runtime", "Renderer", "Platform" } };
```

表示 EditorModule 需要在 UI、Runtime、Renderer、Platform 之后初始化。

### 3.3 ModuleRegistry

`ModuleRegistry` 是一个轻量服务注册表。

模块把对外能力注册成 Service，其他模块按类型取服务。

```cpp
context.registry.registerService<RendererService>(renderer_service);
auto renderer_service = context.registry.getService<RendererService>();
```

注意：

- `ModuleRegistry` 只建议在模块顶层使用。
- 不要把 `ModuleRegistry` 传进 Entity、Component、Panel、RenderPass 等深层对象。
- 深层对象应该通过构造参数、函数参数或局部 Context 获取最小依赖。

## 4. 当前模块列表

当前内置模块的注册入口在：

```text
source/Engine/Function/Global/global_context.cpp
```

各模块实现已经拆到自己的领域目录，例如：

```text
Core/Module/core_engine_module.*
Function/File/file_system_module.*
Function/Platform/platform_module.*
Function/Resource/resource_module.*
Function/Renderer/renderer_module.*
Function/Scene/runtime_module.*
Function/UI/ui_module.*
Editor/editor_module.*
```

### 4.1 CoreModule

职责：

- 初始化日志系统。
- 作为所有模块的基础依赖。

当前拥有：

- `LogSystem`。

### 4.2 FileSystemModule

职责：

- 创建 `FileSystem`。
- 注册 `FileSystem` 服务。

旧的 `g_runtime_global_context.m_file_system` 兼容指针不由模块直接写入，而是在 `RunTimeGlobalContext` 组合根里统一同步。

### 4.3 PlatformModule

职责：

- 创建窗口。
- 创建输入系统。
- 维护输入快照。
- 隔离 GLFW 细节。

当前拥有：

- `WindowSystem`
- `InputSystemGLFW`

对外服务：

- `WindowService`
- `InputService`

每帧行为：

```cpp
void PlatformModule::tick(const TickContext&) {
    m_input_system->update();
}
```

这意味着 `InputState` 在每帧早期刷新。

### 4.4 ResourceModule

职责：

- 初始化 `AssetManager`。
- 注册资源管理相关服务。

当前状态：

- `AssetManager` 仍是单例。
- Module 中用不拥有删除权的 `shared_ptr` 暴露过渡服务视图。
- `AssetManager` 不再创建 GPU Texture / Shader，对应旧 API 只作为 legacy UUID/cache 查询入口保留。
- 会创建 GPU buffer 的 `ProceduralModelFactory` 已移动到 `Function/Renderer/Resources`，Resource 目录不再作为 GPU 资源创建入口。

后续方向：

- 抽出更窄的 `AssetService`。
- 删除或迁移 `getTexture()` / `getTextureCube()` / `getShader()` 等 legacy GPU cache API。

### 4.5 RenderContextModule

职责：

- 创建 `RenderContext`。
- 管理双缓冲 `RenderDataPacket`。

数据流：

```text
Scene extract -> RenderContext write packet
RenderContext swap
Renderer reads read packet
```

### 4.6 RendererModule

职责：

- 创建 `RendererSystem`。
- 初始化当前 OpenGL 渲染路径。
- 注册渲染门面服务。

当前拥有：

- `RendererSystem`

对外服务：

- `RendererService`
- `RendererSystem` 兼容注册

外部模块应该优先依赖：

```cpp
RendererService
```

而不是：

```cpp
RendererSystem
OpenGLRendererAPI
Framebuffer
Texture
```

### 4.7 RuntimeModule

职责：

- 创建 `SceneManager`。
- 管理 active scene。
- 注册场景服务。

当前拥有：

- `SceneManager`

对外服务：

- `SceneService`

### 4.8 UIModule

职责：

- 创建 `UISystem`。
- 维护 ImGui 帧生命周期。
- 提供 UI 输入捕获状态。
- 在事件路由中阻断不应继续传给 Editor/Runtime 的输入事件。

当前拥有：

- `UISystem`

对外服务：

- `UIService`
- `UISystem` 兼容注册

`UISystem` 初始化时由 `UIModule` 注入 `WindowService`，不再自己访问 `RunTimeGlobalContext`。

输入捕获规则：

```text
UI 捕获键盘或文本输入
  -> 阻断 Editor/Runtime 键盘事件

UI 捕获鼠标，且鼠标不在 Editor viewport 上
  -> 阻断 Editor/Runtime 鼠标事件

鼠标在 Editor viewport 上
  -> 允许 viewport picking、滚轮、相机交互
```

### 4.9 EditorModule

职责：

- 创建 `EditorContext`。
- 创建 `EditorLayer`。
- 管理编辑器更新、UI 绘制和事件。
- 注册编辑器相关服务。

当前拥有：

- `EditorLayer`
- `EditorContext`
- Editor panels
- Editor viewport camera

对外服务：

- `EditorService`
- `SelectionService`
- `ViewportService`

`EditorContext` 中保存编辑器需要的窄依赖：

```cpp
std::shared_ptr<SceneService> scene_service;
std::shared_ptr<RendererService> renderer_service;
std::shared_ptr<InputService> input_service;
std::shared_ptr<RenderContext> render_context;
std::shared_ptr<UIService> ui_service;
```

Panel 不再自己访问全局上下文，而是从 `EditorContext` 读取服务。

## 5. RunTimeGlobalContext 的新定位

`RunTimeGlobalContext` 以前是事实上的全局 Service Locator。

Phase1 后，它的新定位是：

```text
模块组合根 + 旧代码兼容桥
```

兼容桥的写入集中在 `RunTimeGlobalContext::syncCompatibilityServices()`。各个内置模块只注册自己的 Service，不再接收 `RunTimeGlobalContext&` 或直接写旧 public pointer。

允许使用：

- `Engine`
- `RunTimeGlobalContext::startSystems()`
- `RunTimeGlobalContext::shutdownSystems()`
- 模块注册阶段的顶层组合代码
- 迁移期兼容路径

不推荐使用：

- RenderPass
- EditorPanel
- EditorCamera
- GameplaySystem
- Resource 内部导入逻辑
- Physics/Audio 后续模块内部实现

新代码优先使用：

```text
ModuleRegistry -> Service -> 显式依赖注入
```

## 6. 启动流程

入口：

```text
Engine::startEngine()
```

流程：

```text
Engine::startEngine
  -> g_runtime_global_context.startSystems()
      -> create ModuleManager
      -> register Core/FileSystem/Platform/Resource/RenderContext/Renderer/Runtime/UI/Editor
      -> ModuleManager::initializeModules()
      -> sync legacy compatibility service pointers
  -> bind window event callback
  -> enableEditorMode()
```

`ModuleManager::initializeModules()` 内部流程：

```text
resolveInitializationOrder()
  -> 根据 dependencies 做拓扑排序

registerServices()
  -> 预留服务壳注册阶段

initialize()
  -> 创建真实系统资源

postInitialize()
  -> 跨模块最终补线

build event module order
  -> UI 优先于 Editor/Runtime 接收事件
```

## 7. 每帧流程

入口：

```text
Engine::tickOneFrame()
```

当前顺序：

```text
calculateFPS

ModuleManager::tickModules
  -> PlatformModule refresh InputState
  -> RuntimeModule tick active scene
  -> RuntimeModule extract RenderDataPacket

ModuleManager::postUpdateModules
  -> EditorModule update
  -> EditorCamera override camera data

UIService::beginFrame

ModuleManager::renderUIModules
  -> EditorModule render editor UI

RenderContext::swapBuffers

Engine::rendererTick
  -> RendererService::render

UIService::endFrameAndRender

WindowService::present
WindowService::pollEvents
```

可以简化理解为：

```text
输入快照
  -> 场景逻辑
  -> 编辑器后处理
  -> UI
  -> 数据交换
  -> 渲染
  -> 显示与事件采集
```

## 8. 事件流程

入口：

```text
Engine::onEvent(Event& event)
```

流程：

```text
Engine 先处理 WindowClose / WindowResize
  -> 如果 event.handled，停止
  -> ModuleManager::dispatchEvent(event)
```

`ModuleManager` 使用独立事件顺序：

```text
UI
Game
Editor
Renderer
Runtime
Resource
Platform
Core
```

当前没有 GameModule，所以实际大致是：

```text
UI -> Editor -> Renderer -> Runtime -> Platform -> Core
```

为什么 UI 先于 Editor？

因为 ImGui 输入框、菜单、属性面板需要先决定是否捕获输入。

例如：

```text
Properties 面板的 InputText 正在输入
用户按 W
  -> UI 捕获键盘
  -> event.handled = true
  -> EditorCamera 不收到这个按键语义
```

## 9. Service 设计

Service 是模块对外暴露的窄接口。

### 9.1 Platform services

位置：

```text
source/Engine/Function/Platform/platform_services.h
```

包含：

- `WindowService`
- `InputService`

使用者不需要知道 GLFW。

### 9.2 RendererService

位置：

```text
source/Engine/Function/Renderer/RHI/renderer_service.h
```

主要接口：

```cpp
virtual void render(TimeStep ts, const RenderDataPacket& render_data) = 0;
virtual void setViewportSize(uint32_t width, uint32_t height) = 0;
virtual uint32_t getViewportColorAttachment() const = 0;
virtual int readViewportEntityID(int x, int y) = 0;
```

Editor viewport 通过它完成：

- viewport resize
- 显示 color attachment
- picking

### 9.3 SceneService

位置：

```text
source/Engine/Function/Scene/scene_service.h
```

用于读取或切换 active scene。

### 9.4 UIService

位置：

```text
source/Engine/Function/UI/ui_system.h
```

用于：

- UI begin/end frame。
- 查询 ImGui 是否捕获鼠标、键盘、文本输入。

### 9.5 Editor services

位置：

```text
source/Engine/Editor/editor_services.h
```

包含：

- `EditorService`
- `SelectionService`
- `ViewportService`

`SelectionService` 让 Hierarchy、Viewport、Properties 不需要互相知道对方。

`ViewportService` 让 UI 捕获策略知道鼠标是否在 Editor viewport 上。

## 10. 如何新增一个模块

下面以未来的 `AudioModule` 为例。

### 10.1 第一步：定义 Service

先定义模块对外暴露的能力。

例如：

```cpp
// source/Engine/Function/Audio/audio_service.h
class AudioService {
public:
    virtual ~AudioService() = default;

    virtual void play(AssetHandle audio_clip) = 0;
    virtual void stopAll() = 0;
    virtual void update(SceneV2& scene, TimeStep delta_time) = 0;
};
```

注意：

- Service 应该小。
- Service 不应该暴露后端对象，例如 OpenAL/FMOD/Wwise 原生句柄。
- Service 应该表达“能力”，而不是暴露整个系统内部。

### 10.2 第二步：实现具体系统

例如：

```cpp
class AudioSystem : public AudioService {
public:
    void init();
    void shutdown();

    void play(AssetHandle audio_clip) override;
    void stopAll() override;
    void update(SceneV2& scene, TimeStep delta_time) override;
};
```

`AudioSystem` 可以知道音频后端，但外部模块不应该知道。

### 10.3 第三步：实现 Module

每个模块优先放在自己的领域目录，并提供一个工厂函数给组合根注册。

示例：

```cpp
class AudioModule final : public EngineModule {
public:
    ModuleInfo getInfo() const override {
        return {
            "Audio",
            ModuleStage::Audio,
            { BuiltinModuleNames::Core, BuiltinModuleNames::Resource, BuiltinModuleNames::Runtime }
        };
    }

    void initialize(ModuleContext& context) override {
        m_audio_system = std::make_shared<AudioSystem>();
        m_audio_system->init();

        context.registry.registerService<AudioService>(
            std::static_pointer_cast<AudioService>(m_audio_system));
    }

    void tick(const TickContext& tick_context) override {
        // 如果 Audio 只需要每帧更新，可以在这里做。
        // 如果需要读 active scene，可以在 initialize 中缓存 SceneService。
    }

    void shutdown(ModuleContext& context) override {
        if (m_audio_system) {
            m_audio_system->shutdown();
        }

        context.registry.resetService<AudioService>();
        m_audio_system.reset();
    }

private:
    std::shared_ptr<AudioSystem> m_audio_system;
};

std::unique_ptr<EngineModule> createAudioModule() {
    return std::make_unique<AudioModule>();
}
```

### 10.4 第四步：注册模块

在 `RunTimeGlobalContext::startSystems()` 中注册：

```cpp
m_module_manager->registerModule(createAudioModule());
```

注册位置不必强求顺序完全正确，因为初始化顺序由 dependencies 决定。

但为了阅读舒服，建议按大阶段排列：

```text
Core
FileSystem
Platform
Resource
Runtime
Physics
Audio
Renderer
UI
Editor
Game
```

### 10.5 第五步：使用服务

模块顶层可以从 registry 取服务：

```cpp
auto audio_service = context.registry.getService<AudioService>();
```

深层对象不建议直接拿 registry。

如果某个系统需要音频服务，推荐：

```cpp
class SomeSystem {
public:
    explicit SomeSystem(std::shared_ptr<AudioService> audio_service)
        : m_audio_service(std::move(audio_service)) {}

private:
    std::shared_ptr<AudioService> m_audio_service;
};
```

### 10.6 第六步：更新构建和文档

新增文件后需要：

- 更新 `source/Engine/CMakeLists.txt`。
- 更新对应架构文档。
- 写一个最小 smoke test 或构建验证。

## 11. 新增模块的检查清单

新增模块前问自己：

1. 这个模块拥有哪些资源？
2. 它对外暴露什么 Service？
3. 它依赖哪些模块？
4. 它是否真的需要访问 `RunTimeGlobalContext`？
5. 它是否把后端细节暴露给了 Runtime/Editor？
6. 它的初始化和关闭顺序是否能由 dependencies 表达？
7. 它的每帧逻辑应该在 `tick`、`postUpdate`、`renderUI` 还是后续新增阶段？
8. 如果未来替换后端，这个模块的外部调用者是否不用改？

## 12. 代码阅读建议

建议按这个顺序读：

```text
1. engine_module.h
2. module_manager.h / module_manager.cpp
3. global_context.cpp
4. core_engine_module.cpp / platform_module.cpp / renderer_module.cpp / runtime_module.cpp / ui_module.cpp / editor_module.cpp
5. engine.cpp
6. platform_services.h / renderer_service.h / scene_service.h / editor_services.h / ui_system.h
7. editor_context.h / editor_layer.cpp
8. viewport_panel.cpp / scene_hierarchy_panel.cpp
```

读的时候不要先钻进 OpenGL、Framebuffer、Shader、Scene 细节。

先确认这几个问题：

```text
谁拥有这个对象？
谁注册这个 Service？
谁消费这个 Service？
它在一帧的哪个阶段运行？
事件会不会被 UI 提前拦截？
```

## 13. 当前遗留点

Phase1 已经完成模块系统主线，但仍有一些明确遗留：

- `AssetManager` 仍是单例，后续可以抽出 `AssetService`。
- `RendererFactory` 仍是迁移期静态门面，后续可让 Renderer 内部对象显式持有 `RenderDevice`。
- Physics、Audio、GameModule 尚未实现。
- EditorCameraController 还没有单独拆类，目前逻辑在 `EditorLayer::updateViewportCamera()`。

这些遗留是下一阶段演进点，不影响当前 Phase1 的主架构阅读。

## 14. 最小心智模型

最后记住这张图：

```text
Engine
  owns frame loop
  calls ModuleManager

ModuleManager
  owns EngineModule list
  resolves dependencies
  dispatches frame stages and events

EngineModule
  owns concrete systems
  registers services

Service
  is the narrow interface used by other modules

RunTimeGlobalContext
  is only composition root and compatibility bridge
```

如果读代码读乱了，就回到这张图。

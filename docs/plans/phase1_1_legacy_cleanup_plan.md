# NexAur Phase1.1 残留旧代码清理计划

日期：2026-06-22

## 1. 背景

Phase1 已经完成了模块系统主线：

- `EngineModule` / `ModuleManager` / `ModuleRegistry` 已经存在。
- `RunTimeGlobalContext` 已经降级为兼容桥。
- Platform、Runtime、Renderer、UI、Editor 已经有静态模块边界。
- Renderer 侧已经引入 `RenderPassContext` 和最小 `RenderDevice`。
- Render Pass 已经不再直接访问 `g_runtime_global_context`。

不过 Phase1 为了保证每一步都可运行，保留了不少迁移期兼容代码。现在进入 Phase1.1，目标不是增加新功能，而是清理这些残留，让代码结构更清爽、更容易继续接 Vulkan、Physics、Audio 和小游戏 demo。

一句话目标：

```text
把 Phase1 搭出的新架构主线固定下来，把旧路径、兼容桥和深层全局访问逐步收窄。
```

## 2. Phase1.1 的目标

Phase1.1 完成后，希望达到：

- 深层系统不再直接 include `global_context.h`。
- `Engine` 的主循环 fallback 分支更少，优先只通过 `ModuleManager` / `ModuleRegistry` 工作。
- `UISystem` 不再直接访问 `g_runtime_global_context.m_window_system`。
- `global_context.cpp` 不再塞满所有静态 Module 实现。
- Resource 层不再创建新的 GPU 对象。
- Editor 只依赖 `RendererService`，不再依赖 `RendererSystem`。
- Renderer 对外不再暴露 `Framebuffer` 这类后端资源对象。
- Scene 组件逐步移除旧 UUID 渲染字段，统一使用 `AssetHandle`。
- 文档和示例不再反复展示旧的全局访问方式。

## 3. 非目标

Phase1.1 暂时不做：

- 不删除 `RunTimeGlobalContext` 本体。
- 不实现完整 Vulkan 后端。
- 不实现完整 RenderGraph。
- 不做大规模目录重排。
- 不重写 `AssetManager` 全部功能。
- 不重写 Editor UI。
- 不一次性删除所有兼容 API。

`RunTimeGlobalContext` 仍可作为组合根和旧代码兼容桥存在，但不应该继续成为新代码入口。

## 4. 清理原则

### 4.1 先收窄，再删除

旧接口不要一上来全部删除。推荐顺序：

```text
新主路径可用
-> 旧路径不再被新代码调用
-> 标注 legacy / compatibility
-> 移除旧调用点
-> 删除旧接口
```

### 4.2 深层代码不碰全局上下文

允许访问 `RunTimeGlobalContext`：

- `Engine`
- `RunTimeGlobalContext` 自身
- 静态模块注册入口
- Sandbox / 迁移期测试代码

不应该访问：

- UI system
- Render pass
- Render resource uploader
- Editor panel
- Scene component
- Asset import / runtime resource logic
- Gameplay system

### 4.3 Resource 不创建 GPU 对象

Resource 层只负责：

- `AssetHandle`
- metadata
- CPU asset loading
- path / registry

GPU texture、buffer、shader、framebuffer 的创建应该属于 RendererModule / RenderDevice。

### 4.4 Editor 不认识 Renderer 内部实现

Editor 应该依赖：

- `RendererService`
- `SceneService`
- `InputService`
- `UIService`
- `SelectionService`
- `ViewportService`

Editor 不应该依赖：

- `RendererSystem`
- `Framebuffer`
- OpenGL / Vulkan 后端类

### 4.5 每个 PR 都保持 Sandbox 可运行

Phase1.1 是清理，不应该让引擎停在不可运行状态。每个 PR 至少需要：

- 构建通过。
- Sandbox 可以启动。
- Editor viewport 可以显示。
- picking / camera / UI capture 不回退。

## 5. 当前残留旧代码清单

本节是 Phase1.1 执行前的审查清单，用于说明每项清理的来源和原因。当前完成状态以第 6 节的执行结果表为准。

### 5.1 Engine 仍直接访问全局上下文

位置：

- `source/Engine/engine.cpp`

典型残留：

- `g_runtime_global_context.startSystems()`
- `g_runtime_global_context.m_window_system`
- `g_runtime_global_context.m_ui_system`
- `g_runtime_global_context.m_render_context`
- `g_runtime_global_context.m_renderer_system`
- `g_runtime_global_context.m_scene_manager`

分析：

`Engine` 作为组合根访问 `RunTimeGlobalContext` 可以接受，但现在 `Engine` 里存在较多 fallback 分支，例如先尝试服务接口，不存在时再走旧全局指针。这些 fallback 让主循环读起来不够清爽，也容易掩盖模块注册错误。

清理方向：

- 保留 `g_runtime_global_context.startSystems()` 和 `shutdownSystems()`。
- 每帧逻辑优先只走 `ModuleManager` 和服务接口。
- 如果必要服务不存在，直接报错或跳过，而不是回退到旧 public 指针。

### 5.2 UISystem 仍依赖全局 WindowSystem

位置：

- `source/Engine/Function/UI/ui_system.cpp`

典型残留：

- `g_runtime_global_context.m_window_system->getNativeWindow()`
- `g_runtime_global_context.m_window_system->getWindowSize()`

分析：

UI 初始化需要 native window 是合理的，但 `UISystem` 自己去全局上下文里拿 window 不合理。UI 应该由 `UIModule` 注入 `WindowService` 或 native window。

清理方向：

- `UISystem::init()` 接收 `WindowService` 或 `void* native_window`。
- `UIModule` 从 `ModuleRegistry` 获取 `WindowService` 后传入。
- `UISystem` 删除 `global_context.h` include。

### 5.3 global_context.cpp 承担了太多 Module 实现

位置：

- `source/Engine/Function/Global/global_context.cpp`

当前包含：

- `CoreEngineModule`
- `FileSystemModule`
- `PlatformModule`
- `ResourceModule`
- `RenderContextModule`
- `RendererModule`
- `RuntimeModule`
- `UIModule`
- `EditorModule`

分析：

这在 Phase1 中有利于快速落地，但长期会让 `global_context.cpp` 变成新的上帝文件。模块系统已经成立后，模块实现应该移动到各自领域目录。

清理方向：

```text
Core/Module/core_module.*
Function/File/file_system_module.*
Function/Platform/platform_module.*
Function/Resource/resource_module.*
Function/Renderer/renderer_module.*
Function/Scene/runtime_module.*
Function/UI/ui_module.*
Editor/editor_module.*
```

`RunTimeGlobalContext::startSystems()` 最终只负责：

```cpp
m_module_manager->registerModule(createCoreModule());
m_module_manager->registerModule(createPlatformModule());
...
m_module_manager->initializeModules();
syncCompatibilityServices();
```

兼容 public pointer 由 `RunTimeGlobalContext` 统一同步，模块实现不再接收 `RunTimeGlobalContext&`。

### 5.4 AssetManager 仍有 GPU 资源旧接口

位置：

- `source/Engine/Function/Resource/asset_manager.h`
- `source/Engine/Function/Resource/asset_manager.cpp`

典型残留：

- `Texture2D::create(path)`
- `TextureCubeMap::create(path)`
- `Shader::create(...)`
- `getTexture()`
- `getTextureCube()`
- `loadShader()`

分析：

这些接口让 Resource 层仍然知道 GPU 对象。Phase1 的新路径已经是：

```text
Scene / Runtime
  -> AssetHandle
RendererSystem
  -> RenderResourceCache
  -> RenderDevice
```

因此 `AssetManager` 的 GPU 缓存接口应该逐步降级为 legacy，后续删除或移动到 Renderer 侧。

清理方向：

- 保留 CPU model / metadata / AssetHandle API。
- 标注 GPU 加载 API 为 legacy。
- 新代码禁止调用 `AssetManager::getTexture()` / `getShader()`。
- 如果仍有旧调用点，迁移到 `RenderResourceCache`。

### 5.5 RendererService 仍暴露 Framebuffer 兼容接口

位置：

- `source/Engine/Function/Renderer/RHI/renderer_service.h`

残留接口：

```cpp
virtual std::shared_ptr<Framebuffer> getViewportFramebuffer() const = 0;
```

分析：

PR8 后 Render Pass 已经不再需要它。这个接口主要是迁移期留给旧 viewport / editor 代码的兼容桥。长期看，Editor 不应该知道 `Framebuffer` 对象，否则后续 Vulkan 的 swapchain image / image view / descriptor set 会被迫暴露出去。

清理方向：

- Viewport 显示继续使用 `getViewportColorAttachment()`。
- Picking 使用 `readViewportEntityID()`。
- Resize 使用 `setViewportSize()`。
- 删除 Editor / Panel 对 `Framebuffer` 的直接依赖。
- 最后移除 `getViewportFramebuffer()`。

### 5.6 EditorContext 仍暴露 RendererSystem

位置：

- `source/Engine/Editor/editor_context.h`
- `source/Engine/Function/Global/global_context.cpp`

残留字段：

```cpp
std::shared_ptr<RendererSystem> renderer_system;
std::shared_ptr<RendererService> renderer_service;
```

分析：

同时存在 `renderer_system` 和 `renderer_service` 会鼓励 Editor 继续访问 Renderer 内部实现。Phase1.1 应该保留 `RendererService`，删除 `RendererSystem`。

清理方向：

- 查找所有 `m_context->renderer_system` 调用。
- 如果只是 viewport resize / picking / color attachment，全部改成 `renderer_service`。
- `EditorModule` 不再注入 `RendererSystem`。

### 5.7 RendererFactory 是迁移期静态门面

位置：

- `source/Engine/Function/Renderer/RHI/render_device.h`
- `source/Engine/Function/Renderer/RHI/render_device.cpp`

分析：

`RendererFactory` 当前已经不再直接创建 OpenGL fallback，而是转发到 active `RenderDevice`。这比 Phase1 前清爽很多，但它本质上仍然是一个静态入口。短期可以保留，因为它能减少迁移成本；长期最好让 Renderer 侧对象显式持有 `RenderDevice` 或 `RenderResourceFactory`。

清理方向：

- 短期保留。
- 不让 Runtime / Scene / Resource 新代码继续调用它。
- Renderer 内部资源创建可以继续用它。
- Vulkan 接入前后，再考虑把 `RenderResourceCache`、`RenderIBLBuilder`、Pass 构造函数改为显式接收 `RenderDevice&`。

### 5.8 RHI 静态 create 函数仍硬编码 OpenGL

位置：

- `source/Engine/Function/Renderer/RHI/texture.cpp`
- `source/Engine/Function/Renderer/RHI/buffer.cpp`
- `source/Engine/Function/Renderer/RHI/shader.cpp`
- `source/Engine/Function/Renderer/RHI/framebuffer.cpp`
- `source/Engine/Function/Renderer/RHI/vertex_array.cpp`
- `source/Engine/Function/Renderer/RHI/uniform_buffer.cpp`

分析：

这些静态函数目前由 `OpenGLRenderDevice` 调用，本质上是 OpenGL 后端的兼容实现。它们暂时可以保留，但不要让新架构继续把它们当跨后端工厂。

清理方向：

- Vulkan 接入时不要继续扩展这些静态 `create()`。
- 后续可把 OpenGL 创建逻辑移动到 `OpenGLRenderDevice` 或后端私有 factory 中。
- `Texture2D::create()` 等静态函数最终可以降级为 legacy wrapper。

### 5.9 Scene 组件仍有旧 UUID 渲染字段

位置：

- `source/Engine/Function/Scene/component.h`

典型残留：

- `model_id`
- `environment_map_id`

分析：

新路径应该使用 `AssetHandle`，让 Scene 只描述资产引用。旧 UUID 字段会让 Scene 同时存在两套资源引用语义。

清理方向：

- 确认 Sandbox / SceneFactory / PropertiesPanel 是否还读写旧字段。
- 统一迁移到 `model_asset` / `environment_asset`。
- 删除旧 UUID 字段。

### 5.10 Sandbox / SceneFactory 仍有迁移期全局访问

位置：

- `source/Sandbox/scene_test.h`
- `source/Sandbox/scene_test.cpp`
- `source/Engine/Function/Scene/scene_factory.cpp`

分析：

Sandbox 作为外部应用或测试层，短期访问全局兼容桥可以接受。但如果后续要做小游戏 demo，Sandbox 应该逐步变成 `GameModule` 或更清晰的应用层，而不是继续直接拿全局 scene manager。

清理方向：

- Sandbox 优先从 `SceneService` 获取 active scene。
- SceneFactory 不再 include `global_context.h`，改为接收 `AssetManager` / `AssetService` 参数。

### 5.11 文档中仍有旧架构示例

位置：

- `docs/architecture/Architecture.md`
- `docs/architecture/InputSystem/Input.md`
- `docs/architecture/EventSystem/EventSystem.md`
- `docs/architecture/ModuleSystem/ModuleSystem.md`
- `docs/plans/engine_architecture_vulkan_demo_roadmap.md`

分析：

部分文档仍展示旧的全局上下文调用或过期计划，例如 Render Pass 直接访问 `g_runtime_global_context`。这会让后续阅读者困惑。

清理方向：

- 标记旧文档为 historical。
- 或更新示例为 ModuleRegistry / Service / RenderPassContext 风格。
- `ModuleSystem.md` 中关于 PR8 未完成的内容应同步为已完成。

## 6. Phase1.1 推荐 PR 拆分

### Phase1.1 执行结果

| PR | 状态 | 落地结果 |
| --- | --- | --- |
| PR10 | 已完成 | `UISystem` 改为由 `UIModule` 注入 `WindowService`，UI 目录不再 include `global_context.h`。 |
| PR11 | 已完成 | 内置模块实现拆到各自领域目录，`global_context.cpp` 只保留组合根和兼容桥。 |
| PR12 | 已完成 | `Engine` 主循环清理旧 public pointer fallback，按服务访问 UI / Window / Scene / Renderer / RenderContext。 |
| PR13 | 已完成 | `AssetManager` 不再创建 GPU Texture / Shader，`ProceduralModelFactory` 移到 Renderer/Resources，新路径固定为 `AssetHandle -> RenderResourceCache -> RenderDevice`。 |
| PR14 | 已完成 | `RendererService::getViewportFramebuffer()` 已删除，Editor 不再接触 `Framebuffer`。 |
| PR15 | 已完成 | `EditorContext::renderer_system` 已删除，Editor 只依赖 `RendererService`。 |
| PR16 | 已完成 | `MeshRendererComponent` / `EnvironmentComponent` 删除旧 UUID 渲染字段，统一使用 `AssetHandle`。 |
| PR17 | 已完成 | 架构文档、路线图、输入/事件示例和 Sandbox 示例已同步为服务风格。 |

### PR10：UISystem 与 WindowService 解耦（已完成）

目标：

- `UISystem` 不再访问 `RunTimeGlobalContext`。
- UI 初始化依赖从 `UIModule` 显式注入。

任务：

- 修改 `UISystem::init()` 签名，接收 `WindowService` 或 native window。
- `UIModule` 从 `ModuleRegistry` 获取 `WindowService`。
- `UISystem` 内部保存必要的 native window / window size provider。
- 删除 `ui_system.cpp` 对 `global_context.h` 的 include。

验收：

- ImGui 正常初始化。
- UI begin/end frame 正常。
- Docking / viewport panel 正常显示。
- UI capture 策略不回退。
- `rg "global_context" source/Engine/Function/UI` 无结果。

风险：

- ImGui backend 初始化依赖 GLFW window，需要注意生命周期必须晚于 PlatformModule，早于 EditorModule。

### PR11：拆分静态 Module 实现（已完成）

目标：

- `global_context.cpp` 不再包含所有模块实现。
- 模块实现回到各自领域目录。

任务：

- 新增 `platform_module.h/.cpp`。
- 新增 `runtime_module.h/.cpp`。
- 新增 `renderer_module.h/.cpp`。
- 新增 `ui_module.h/.cpp`。
- 新增 `editor_module.h/.cpp`。
- 可选：新增 `resource_module.h/.cpp` 和 `file_system_module.h/.cpp`。
- `RunTimeGlobalContext::startSystems()` 只负责创建和注册模块。
- 旧 public pointer 兼容视图由 `RunTimeGlobalContext::syncCompatibilityServices()` 统一同步。

验收：

- 模块初始化顺序不变。
- 关闭顺序不变。
- Sandbox 行为不变。
- `global_context.cpp` 行数明显下降。

风险：

- 拆分时容易漏 include 或 CMake 文件。
- 建议只移动代码，不改行为。

### PR12：Engine 主循环 fallback 清理（已完成）

目标：

- `Engine` 优先只通过 ModuleRegistry / ModuleManager 工作。
- 减少旧 public pointer fallback。

任务：

- 为 Engine 需要的服务建立清晰 helper：
  - `getModuleService<WindowService>()`
  - `getModuleService<UIService>()`
  - `getModuleService<SceneService>()`
  - `getModuleService<RendererService>()`
  - `getModuleService<RenderContext>()`
- 删除 `Engine` 中不必要的 `m_ui_system` / `m_renderer_system` / `m_scene_manager` fallback。
- 保留启动/关闭对 `g_runtime_global_context` 的调用。

验收：

- `Engine` 仍能启动和关闭。
- 逻辑 tick / render / UI / present 顺序不变。
- `Engine` 里直接访问 `g_runtime_global_context.m_xxx` 的数量显著减少。

风险：

- 某个服务未注册时，以前 fallback 能掩盖问题，清理后会暴露初始化顺序错误。

### PR13：Resource 层 GPU API 降级（已完成）

目标：

- ResourceModule 不再作为 GPU 资源创建入口。
- AssetManager 聚焦 asset identity、metadata 和 CPU data。

任务：

- 标注 `AssetManager::getTexture()` / `getTextureCube()` / `loadShader()` 为 legacy。
- 查找所有调用点并迁移。
- 新渲染路径统一使用：

```text
AssetHandle
  -> RenderResourceCache
  -> RenderDevice
```

- 如果短期不能删接口，至少将其从新代码路径中移除。

验收：

- Runtime / Scene / Editor 不再调用 `AssetManager` 的 GPU cache API。
- `AssetManager::loadModel()` 保持 CPU model 路径。
- `Function/Resource` 目录不再包含会调用 `RendererFactory` 的过程模型 GPU 创建工具。
- RenderResourceCache 仍正常上传 model / texture / environment map。

风险：

- 旧测试或 Sandbox 可能仍依赖 `getTexture()`。
- 需要逐个迁移，不建议一次删除。

### PR14：RendererService 兼容接口收口（已完成）

目标：

- Editor 不再接触 `Framebuffer`。
- `RendererService::getViewportFramebuffer()` 删除或废弃。

任务：

- 查找 `getViewportFramebuffer()` 调用点。
- ViewportPanel 改用：
  - `getViewportColorAttachment()`
  - `readViewportEntityID()`
  - `setViewportSize()`
- 如果需要更多 viewport 信息，新增窄接口，不暴露 `Framebuffer`。

验收：

- `RendererService` 不再暴露 `Framebuffer`。
- Editor viewport 显示正常。
- Picking 正常。
- Renderer 后端对象不泄漏给 Editor。

风险：

- ImGui 显示 texture 目前需要 renderer id，OpenGL 下是 `uint32_t`，Vulkan 下需要重新设计 descriptor / texture handle。短期可继续保留 `getViewportColorAttachment()`，但命名要意识到它是 OpenGL 兼容语义。

### PR15：EditorContext 移除 RendererSystem（已完成）

目标：

- Editor 只依赖 RendererService。

任务：

- 删除 `EditorContext::renderer_system`。
- `EditorModule` 不再注入 `RendererSystem`。
- 面板内所有 renderer 操作改为 `RendererService`。

验收：

- `rg "renderer_system" source/Engine/Editor` 无非必要结果。
- Editor compile / viewport / picking 正常。

风险：

- 如果某些 editor 代码仍需要 RendererSystem 独有接口，说明 RendererService 需要补窄接口，而不是把 RendererSystem 继续暴露出去。

### PR16：Scene 旧 UUID 字段清理（已完成）

目标：

- Scene 渲染资源引用统一为 `AssetHandle`。

任务：

- 迁移 `model_id` 到 `model_asset`。
- 迁移 `environment_map_id` 到 `environment_asset`。
- 更新 SceneFactory / Sandbox / PropertiesPanel。
- 删除旧 UUID 字段。

验收：

- Scene 能正常生成 render data。
- 默认场景模型和环境贴图正常。
- 没有旧 UUID 渲染字段。

风险：

- 如果未来要序列化旧场景文件，需要考虑兼容转换。

### PR17：文档和示例清理（已完成）

目标：

- 文档反映 Phase1 完成后的真实架构。

任务：

- 更新 `docs/architecture/ModuleSystem/ModuleSystem.md` 中 PR8 未完成的描述。
- 更新 `docs/plans/engine_architecture_vulkan_demo_roadmap.md` 中已经完成的部分。
- 将旧全局访问示例标记为 historical 或替换为 service 风格。

验收：

- 新读者不会被旧示例误导。
- 文档中的阶段状态与代码一致。

## 7. 推荐执行顺序

建议顺序：

```text
PR10 UISystem 与 WindowService 解耦
PR11 拆分静态 Module 实现
PR12 Engine 主循环 fallback 清理
PR13 Resource 层 GPU API 降级
PR14 RendererService 兼容接口收口
PR15 EditorContext 移除 RendererSystem
PR16 Scene 旧 UUID 字段清理
PR17 文档和示例清理
```

如果只想先做最有收益的三件事：

1. PR10：UI 去全局上下文。
2. PR11：拆分 `global_context.cpp` 中的模块实现。
3. PR13：Resource 层 GPU API 降级。

这三项完成后，NexAur 的模块边界会明显清爽很多，后续接 Vulkan 的阻力也会小很多。

## 8. Phase1.1 完成标准

Phase1.1 可以认为完成，当满足：

- `source/Engine/Function/UI` 不再 include `global_context.h`。
- `global_context.cpp` 只保留组合根和兼容桥逻辑。
- `Engine` 中直接访问 `g_runtime_global_context.m_xxx` 的 fallback 明显减少。
- `AssetManager` 不再作为新代码 GPU resource 创建入口。
- `RendererService` 不暴露 `Framebuffer`。
- `EditorContext` 不暴露 `RendererSystem`。
- Scene 渲染资源字段统一使用 `AssetHandle`。
- 文档不再把旧全局访问方式作为推荐写法。
- `cmake --build build --config Debug` 通过。
- Sandbox 能启动、渲染、操作 viewport、picking。

## 9. 暂时保留的兼容点

即使 Phase1.1 完成，也可以暂时保留：

- `RunTimeGlobalContext` 本体。
- `g_runtime_global_context.startSystems()` / `shutdownSystems()`。
- `RendererFactory` 静态门面。
- RHI 层旧静态 `Texture2D::create()` 等 OpenGL wrapper。
- Sandbox 少量全局访问，直到正式引入 GameModule 或更清晰的 Application 层。

这些点不是最终理想形态，但只要它们被限制在清晰边界内，就不会继续污染深层模块。

## 10. 结论

Phase1.1 是一轮必要的“架构收边”。

Phase1 的价值是把新主线搭起来；Phase1.1 的价值是让旧路径不要继续和新主线并行生长。清理完成后，NexAur 会更适合继续做两件事：

- 接入 VulkanRenderer。
- 补 gameplay / physics / audio 等模块，开始小游戏 demo。

Phase1.1 完成后，下一步建议转向 Runtime/GameModule、SceneSerializer、AssetService 和 Vulkan 前置渲染抽象的详细方案。

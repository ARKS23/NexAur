# NexAur 开源引擎架构参考与目标设计

日期：2026-05-17

## 目标

这份文档回答两个问题：

1. 从 Godot、O3DE、Bevy、Hazel、Filament、bgfx、Diligent Engine、Wicked Engine、Urho3D、Cocos2d-x、OGRE、Magnum 等开源项目里，NexAur 值得吸收哪些架构思想。
2. 在暂时不重写 Vulkan 渲染器的前提下，NexAur 现在应该怎么改，才能让后续 Vulkan、编辑器、资源系统、场景系统更自然地长出来。

结论先说：NexAur 不应该马上追求“大而全”的插件系统、RenderGraph、异步资产管线或多线程渲染。当前更重要的是把模块所有权整理清楚：

- Scene 描述世界，不持有 GPU 对象。
- Asset 管理资产身份、路径、元数据和 CPU 资产，不直接依赖图形 API。
- Renderer 独占 GPU 资源、Pass、Framebuffer、管线状态和后端选择。
- Editor 通过明确服务访问场景、选择、视口和命令，不把所有面板都接到一个大上下文上。
- Engine 只做应用生命周期、事件分发和每帧调度，不把所有业务细节塞进主循环。

## 当前 NexAur 的主要观察

本次重点阅读了这些模块：

- `source/Engine/engine.*`
- `source/Engine/Function/Global/global_context.*`
- `source/Engine/Function/Scene/scene_v2.*`
- `source/Engine/Function/Scene/component.h`
- `source/Engine/Function/Renderer/data/render_data.h`
- `source/Engine/Function/Renderer/RHI/*`
- `source/Engine/Function/Renderer/Passes/*`
- `source/Engine/Function/Resource/*`
- `source/Engine/Editor/*`
- `source/Engine/Editor/Panels/*`
- `source/Sandbox/*`

目前代码已经有几个好基础：`Engine` 主循环、`RunTimeGlobalContext`、ECS 风格的 `SceneV2`、`RenderDataPacket`、面板化 Editor、`RendererSystem`、Pass、Viewport framebuffer、picking、AssetManager。这说明项目不是“散代码”，而是已经在向引擎结构靠近。

但几个边界还没有稳定：

- `g_runtime_global_context` 被 Engine、UI、Input、Editor、Scene、Pass、IBLBuilder、Sandbox 多处直接访问，已经变成全局 Service Locator。
- `SceneV2::extractSceneData()` 会通过 `AssetManager::getRenderModel()` 把 GPU model 放进 `RenderDataPacket`，Scene 和 Renderer 被 GPU 资源绑在一起。
- `AssetManager` 同时管理路径、CPU model、GPU model、Texture、Shader、EnvironmentMap，职责跨度太大。
- `ProceduralModelFactory` 和 `RenderResourceUploader` 放在 Resource 模块中，但会创建 VAO/VBO/EBO 等 RHI 对象。
- `render_data.h` include 了 `texture.h` 和 `vertex_array.h`，导致渲染数据包不是轻量跨线程数据，而是 GPU 对象集合。
- `RenderPass` 在 target framebuffer 为空时会回到全局窗口/renderer 上下文，Pass 不是显式依赖。
- Editor 的 `EditorContext` 同时持有 scene、renderer、selected entity、selection source，后续面板变多时会逐渐膨胀。
- `Scene` 和 `SceneV2` 并存，旧 `Scene` 中仍残留大量 renderer/IBL/demo 逻辑，会干扰主线认知。

## 开源引擎横向参考

### Godot：高层 Scene 与底层 Server 分离

Godot 的官方架构图把系统分为 Scene layer、Server layer、Drivers / Platform Interface、Core / Main。Scene 是用户组织游戏的高层模型，渲染、音频、物理等能力下沉到 Server，平台和图形 API 放在 Driver/Platform 层。

对 NexAur 的启发：

- `SceneV2` 应该接近 Godot 的 Scene layer：表达 Entity、Component、Transform、Camera、Light、MeshRenderer 等世界信息。
- `RendererSystem` 更接近 Godot 的 Rendering Server：它接收场景抽取后的数据，自己管理 GPU 对象和后端。
- 未来 Vulkan 不应该让 Scene 层感知 `VkImage`、`VkBuffer`、descriptor、command buffer；Scene 只交出 handle、transform、material 参数。
- 可以用“服务器/服务”的思路收敛职责，但不要把所有服务都暴露成随处可拿的全局单例。

### O3DE：Gem、Component、Asset Pipeline、Atom Renderer

O3DE 是大型引擎，不能照搬复杂度，但它的边界很值得看：

- 功能以 Gem 扩展，代码、资产、工具甚至项目都可以作为 Gem 分发。
- O3DE 是 ECS 思路，Entity 更像 ID 和组件容器，功能由 Component 提供。
- Asset Pipeline 区分 source asset 和 product asset，Asset Processor 在后台把源资产处理成运行时优化资产，并支持热更新。
- Atom Renderer 里 RHI 是和后端图形 API 通信的底层层，Pass 系统可以 C++ 或 JSON 数据驱动，Pass 通过 attachment 表达输入输出。

对 NexAur 的启发：

- 现在不要做完整 Gem，但可以先做“静态模块”：Core、Platform、Runtime、Renderer、Editor、Tools，每个模块有清楚的依赖方向。
- AssetManager 应该先从“加载并创建 GPU 对象”改为“资产注册表 + CPU 资产缓存 + handle 查询”。
- Renderer 内部再有 `RenderResourceCache`，把资产 handle 转换为 GPU resource。
- Pass 不要隐式访问全局 framebuffer，而是声明输入输出，短期用 `RenderPassContext`，长期再走 RenderGraph。

### Bevy：插件化 App、Schedule、系统函数

Bevy 的核心思路是所有功能都是 plugin，plugin 修改 App；ECS 系统挂进 schedule，每帧按 schedule 运行。它的重点不是“每个小引擎都要 Rust ECS”，而是“应用由可组合模块搭起来，而不是主循环硬编码一切”。

对 NexAur 的启发：

- 可以把 `RunTimeGlobalContext::startSystems()` 逐渐演进成 `EngineApplication` 安装模块：
  - `CoreModule`
  - `PlatformModule`
  - `RendererModule`
  - `RuntimeModule`
  - `EditorModule`
- 每个模块明确 `onInit/onShutdown/onUpdate/onEvent`，Engine 主循环只调度模块，不知道模块内部细节。
- Input、Scene tick、Editor tick、Render extract、Render execute 可以形成固定阶段，而不是任意类随时做任意事。

### Hazel：适合小型 C++ 引擎的 Application / Layer / Renderer 抽象

Hazel 本身就是教学型 C++ 引擎，和 NexAur 的阶段很接近。它的 Application、LayerStack、RendererAPI 抽象不是终局架构，但非常适合早期项目整理生命周期和编辑器/运行时边界。

对 NexAur 的启发：

- NexAur 可以保留简洁 Application / Layer 模式：EditorLayer、RuntimeLayer、ImGuiLayer、DebugLayer。
- 事件应该由 Engine 进入 LayerStack，再从上层 UI/Editor 往下传播，而不是各模块主动去全局对象拉输入。
- RendererAPI/RendererCommand 早期可以保留，但应该逐步从静态全局命令变成 `RenderDevice` / `RenderCommandContext` 的实例能力。

### Filament：Renderer 是可嵌入库，Engine 管理渲染资源

Filament 是面向应用嵌入的实时 PBR renderer。它强调 Engine、Renderer、Scene、View、Camera 等对象的显式创建和生命周期；渲染时将 View 交给 Renderer。

对 NexAur 的启发：

- `RendererSystem` 应该成为 renderer 的唯一门面，对外暴露少量高层 API：resize viewport、render frame、pick entity、get viewport texture。
- 内部可以拆 `RenderDevice`、`RenderScene`、`RenderView`、`RenderResourceCache`。
- Editor Viewport 本质上是一个 `RenderView`：它有 camera、viewport size、target framebuffer、picking target。
- Runtime 游戏窗口也可以是另一个 `RenderView`。这样未来多视口、多窗口、Scene View / Game View 会更顺。

### bgfx：后端无关、Handle 化、命令提交

bgfx 是 “Bring Your Own Engine/Framework” 风格的跨平台图形库，支持多种后端。它最大的启发是：图形资源通过 handle 抽象，渲染以 view、encoder、submit 的形式表达，不把具体 API 泄漏给上层。

对 NexAur 的启发：

- Scene、Asset、Editor 不要拿 `std::shared_ptr<Texture2D>`、`std::shared_ptr<VertexArray>`，而是拿 `AssetHandle` 或 `RenderHandle`。
- `RenderDataPacket` 应该可复制、轻量，适合作为“场景到渲染器”的一帧快照。
- OpenGL 后端也可以先模拟 Vulkan 风格的 handle/resource ownership，后续迁移会轻很多。

### Diligent Engine：一致前端 API + 现代后端抽象

Diligent Engine 是低层图形 API 抽象和渲染框架，支持 D3D12、Vulkan、Metal、WebGPU，同时兼容 OpenGL/GLES。它的文档和示例把 pipeline state、resource binding、render target、render pass、多线程 command list 等现代概念分得比较清楚。

对 NexAur 的启发：

- 不要把 `RendererFactory` 永远作为一组静态函数放在 `renderer_system.h`。
- 抽一个 `RenderDevice` 更清晰：它创建 buffer、texture、shader、pipeline、framebuffer。
- 抽一个 `RenderCommandContext`：它负责 begin/end pass、set pipeline、bind resources、draw。
- OpenGL 实现也通过这个接口走，而不是由上层直接调用 OpenGL 包装类。

### Wicked Engine：小团队可维护的现代 C++ 引擎参考

Wicked Engine 是 C++ 现代渲染引擎，带 editor、scripting、样例和多平台支持。它不是最“教科书式”的架构，但很适合作为个人/小团队项目的现实参考：功能集中、模块清楚、渲染能力强。

对 NexAur 的启发：

- 不需要过早拆成过多动态库；清楚的命名空间和目录边界已经够用。
- Editor、Runtime、Samples 可以同仓库并存，但入口和依赖目标要清晰。
- 保持能运行的样例很重要，每次重构都要能跑 Sandbox/Editor，不要只追求漂亮图。

### Urho3D：Context + Subsystem 的优点与风险

Urho3D 使用 Context 注册 subsystem，Time、FileSystem、Log、ResourceCache、Input、UI、Graphics、Renderer 等都可以作为 subsystem 获取。这和 NexAur 当前的 `RunTimeGlobalContext` 很像。

对 NexAur 的启发：

- Context/Subsystem 对个人引擎很实用，不必完全否定。
- 风险是深层对象随时 `GetSubsystem()`，依赖关系会变暗。
- 建议保留全局上下文作为 composition root，但禁止新增深层代码直接 include `global_context.h`。
- 需要某个能力时通过构造参数、函数参数或局部 service context 传入。

### Cocos2d-x：Director / Scene / Node / Renderer 的简单运行模型

Cocos2d-x 的基本概念非常朴素：Director 管理游戏流，Scene 是一屏/一关/一个状态，Scene Graph 用 Node 组织，Renderer 负责把可见对象画出来。

对 NexAur 的启发：

- `SceneManager` 可以变得更像一个轻量 Director：负责 active scene、scene switching、runtime/editor play state。
- Editor 的面板不应该直接操心引擎整体状态切换；应通过 `SceneEditingService` 或 `RuntimeController`。
- 如果以后做 2D/UI 场景，Node/SceneGraph 思路也可作为 ECS 之外的局部结构，而不是把所有东西都塞进 ECS。

### OGRE：Root / SceneManager / Resource / RenderSystem / Plugin

OGRE 的核心对象按 Scene Management、Resource Management、Rendering 分工，并通过插件扩展 render system、资源加载等能力。

对 NexAur 的启发：

- `RendererSystem`、`SceneManager`、`AssetManager` 三者应该是平级高层系统，而不是 Scene/Asset 随手创建渲染对象。
- 插件系统可以暂缓，但模块边界要先像插件一样清楚：谁拥有资源，谁能创建，谁能销毁。
- 资源路径配置、插件配置、后端选择可以逐渐从硬编码迁移到项目配置文件。

### Magnum：轻量核心、可选子库、明确 include

Magnum 不是完整游戏引擎，更像轻量图形中间件。它强调核心小、子库可选、插件扩展、文档清楚、include 尽量精确。

对 NexAur 的启发：

- 头文件依赖要收窄。比如 `render_data.h` 现在 include RHI texture/vertex array，这是很强的耦合信号。
- 能 forward declare 就不要 include 全世界。
- 个人引擎尤其要控制编译依赖，否则后期每改一个小头文件都会触发大面积重编。

## 推荐目标架构

### 总体分层

建议把 NexAur 的逻辑目标整理成下面的依赖方向：

```text
Application / Entry
  -> Editor
  -> Runtime
  -> Renderer
  -> Platform
  -> Core

Editor -> Runtime(Scene/Asset) + Renderer public facade + Platform input abstractions
Runtime -> Core + Asset handles + pure scene data
Renderer -> Core + Platform native window + Asset CPU data
Asset -> Core + FileSystem + importers
Platform -> Core
Core -> no engine-level dependency
```

关键点：Runtime/Scene 不能依赖 Renderer/RHI；Asset 不能依赖 Renderer/RHI；Renderer 可以读 Asset 的 CPU 数据，但通过接口/handle，不反向污染 Asset。

### 建议目录

短期不必一次移动所有文件，但目标目录可以先定：

```text
source/Engine/Core/
  Log/
  Events/
  Time/
  UUID.*
  Module/
  Layer/

source/Engine/Platform/
  Window/
  Input/
  OpenGLContext/       # 当前阶段可在这里，后续 Vulkan surface 也在 Platform/RHI 交界

source/Engine/Runtime/
  Scene/
  Components/
  Systems/
  Serialization/

source/Engine/Asset/
  AssetHandle.h
  AssetManager.*
  AssetRegistry.*
  Importers/
  Sources/             # MeshSource, ImageSource, MaterialSource

source/Engine/Renderer/
  RendererSystem.*
  RenderScene.*
  RenderView.*
  RenderData.*
  RHI/
    RenderDevice.*
    RenderCommandContext.*
    Buffer.*
    Texture.*
    Shader.*
    PipelineState.*
  Resources/
    RenderResourceCache.*
    RenderResourceUploader.*
    IBLBuilder.*
  Passes/
    RenderPass.*
    ForwardPass.*
    ShadowPass.*
    SkyboxPass.*
    PickingPass.*

source/Engine/Editor/
  EditorLayer.*
  EditorContext.*
  Services/
    SelectionService.*
    ViewportService.*
    SceneEditingService.*
    EditorCommandStack.*
  Panels/

source/Sandbox/
source/Tools/
```

### Core 模块

Core 只提供基础设施：

- `LogSystem`
- `Event`
- `TimeStep` / `Clock`
- `UUID`
- Assert / Base macro
- `Layer` / `LayerStack`
- `Module` / `EngineApplication` 生命周期接口

Core 不应该知道 Scene、Renderer、Editor、GLFW、OpenGL、Assimp、ImGui。

建议新增：

```cpp
class EngineModule {
public:
    virtual ~EngineModule() = default;
    virtual void onInit() {}
    virtual void onShutdown() {}
    virtual void onUpdate(TimeStep) {}
    virtual void onEvent(Event&) {}
};
```

短期可以只用于整理主循环，不需要完整插件热加载。

### Platform 模块

Platform 负责窗口、输入、native handle、文件对话框等 OS/窗口系统能力。

建议：

- `WindowSystem` 保持窗口和 native window handle。
- `InputSystem` 只维护输入快照，不要让深层对象直接轮询全局 GLFW。
- 增加 `InputState`：

```cpp
struct InputState {
    glm::vec2 mouse_position;
    glm::vec2 mouse_delta;
    bool keys[KeyCount];
    bool mouse_buttons[MouseButtonCount];
};
```

EditorCamera、ViewportPanel、Runtime controller 都吃 `InputState` 或事件，而不是主动访问 `g_runtime_global_context`。

### Runtime / Scene 模块

Scene 的角色：保存世界数据、执行业务系统、提供序列化，不持有 GPU 对象。

建议组件方向：

```cpp
struct MeshRendererComponent {
    AssetHandle mesh;
    AssetHandle material;
    bool transparent = false;
};

struct EnvironmentComponent {
    AssetHandle environment;
    float intensity = 1.0f;
};
```

`SceneV2::extractSceneData()` 的目标应该是：

```cpp
struct RenderObjectData {
    AssetHandle mesh;
    AssetHandle material;
    glm::mat4 transform{1.0f};
    EntityId entity_id = InvalidEntityId;
};
```

不要在这里调用 `asset_manager.getRenderModel()`。这一步应该由 Renderer 的 `RenderResourceCache` 完成。

短期迁移策略：

1. 新增 `using AssetHandle = UUID;`。
2. 保留 `model_id` 字段，但语义上当作 asset handle。
3. 新增轻量 `RenderObjectData::model_handle`，先和旧 `model_data` 并存。
4. RendererSystem 优先用 handle 查 GPU 资源；旧路径作为过渡。
5. 删除 `RenderDataPacket` 中的 RHI include。

### Asset 模块

Asset 的角色：资产身份、路径、元数据、导入、CPU 数据缓存。

建议拆分：

```text
AssetHandle
  UUID 包装，表达资产身份

AssetRegistry
  handle <-> path
  type
  import settings
  dependency

AssetManager
  对外门面：load/get/reload

AssetImporter
  从文件生成 CPU asset

MeshSource / ImageSource / MaterialSource
  可序列化、可缓存、无 GPU 对象
```

当前 `AssetManager` 不宜继续返回 `std::shared_ptr<Texture2D>`、`std::shared_ptr<RenderModelData>` 作为主接口。可以保留一段兼容接口，但新代码应改成：

```cpp
AssetHandle loadTexture(const std::string& path);
std::shared_ptr<ImageSource> getImageSource(AssetHandle handle);
```

GPU 上传由 Renderer 完成：

```cpp
GpuTextureHandle RenderResourceCache::getOrCreateTexture(AssetHandle handle);
GpuMeshHandle RenderResourceCache::getOrCreateMesh(AssetHandle handle);
```

`IBLBuilder` 也建议从 Resource 移到 Renderer/Resources，因为它本质上是 GPU pass 工具，不是纯资产导入器。

### Renderer 模块

Renderer 的目标不是“现在就 Vulkan 化”，而是让 OpenGL 后端也按未来 Vulkan 能接受的边界运行。

建议内部结构：

```text
RendererSystem
  对 Engine / Editor / Runtime 的唯一渲染门面

RenderDevice
  后端资源创建与销毁接口

RenderCommandContext
  draw、dispatch、begin/end pass、bind pipeline、bind resources

RenderResourceCache
  AssetHandle -> GPU resource

RenderScene
  渲染器内部的一帧或持久场景表达

RenderView
  camera + viewport + target + debug/picking settings

RenderPass / RenderPassContext
  pass 的显式输入、输出和执行上下文
```

短期接口可以长这样：

```cpp
struct RenderPassContext {
    RenderDevice& device;
    RenderCommandContext& commands;
    RenderResourceCache& resources;
    Framebuffer* target = nullptr;
    glm::uvec2 viewport_size{0, 0};
};
```

`RendererCommand` 可以先不删，但新 Pass 不再直接调静态全局命令，而是通过 context。

`RenderPass` 的目标：

- 不访问 `g_runtime_global_context`。
- 不自己问 WindowSystem 当前大小。
- 不隐式拿 viewport framebuffer。
- 输入输出由 `RenderPassSpecification` 或 `RenderPassContext` 给出。

更长期可以演进成简化 RenderGraph：

```text
ShadowPass
  writes: shadow_depth

ForwardOpaquePass
  reads: shadow_depth, environment maps
  writes: scene_color, scene_depth

SkyboxPass
  reads: skybox
  writes: scene_color/depth

PickingPass
  writes: entity_id_texture

EditorCompositePass
  reads: scene_color, entity_id_texture
  writes: viewport framebuffer
```

### Editor 模块

Editor 不应该只有一个越来越胖的 `EditorContext`。建议把 Context 变成服务入口：

```text
EditorContext
  当前 editor session 的组合根，不直接堆状态

SelectionService
  单一选择状态写入点

ViewportService
  viewport bounds、size、hover/focus、framebuffer、picking request/result

SceneEditingService
  create/destroy/rename entity，add/remove component，后续接 undo/redo

InspectorRegistry
  component -> property drawer

EditorCommandStack
  undo/redo
```

面板之间不直接共享可变字段：

- `SceneHierarchyPanel` 调 `selection.select(entity, "SceneHierarchy")`。
- `ViewportPanel` picking 后调 `selection.select(entity, "Viewport")`。
- `PropertiesPanel` 只读 `selection.current()`，修改组件时走 `SceneEditingService` 或 `EditorCommandStack`。

这样后续加 gizmo、prefab、undo/redo、multi-selection 时不会重写所有面板。

### Engine / Application 模块

Engine 的目标是稳定调度：

```text
while running:
  platform.pollEvents()
  input.beginFrame()
  layerStack.onUpdate(delta)
  runtime.tick(delta)
  renderer.extract(scene)
  renderer.render()
  editor.renderUI()
  platform.present()
```

当前可以逐步改为：

```text
Engine
  owns EngineApplication / module list
  owns LayerStack
  routes events
  calls update phases

RunTimeGlobalContext
  temporary composition root
  only top-level code can touch it
```

不要急着删除 `RunTimeGlobalContext`。先制定规则：新深层代码不能 include `global_context.h`。当引用数量自然下降后，再决定是否拆成 `EngineServices` 或显式依赖注入。

## 推荐迁移路线

### Phase 1：收窄全局上下文和旧代码入口

目标：不改变用户可见行为，先降低认知成本。

任务：

- 标记旧 `Scene` 为 legacy，或者移动到 `TempTest/Legacy`。
- 明确 `SceneV2` 是主线 Scene。
- 禁止新代码在深层模块 include `global_context.h`。
- `EditorCamera` 改为接收 `InputState` 或由 `EditorCameraController` 驱动。
- `ViewportPanel` 不直接访问全局 InputSystem。
- `RenderPass` 不直接访问全局 WindowSystem/RendererSystem。

### Phase 2：Editor 服务化

目标：让面板协作从共享字段变成共享服务。

任务：

- 新增 `SelectionService`。
- 新增 `ViewportService`。
- 新增 `SceneEditingService`。
- `EditorContext` 只保存服务指针/引用。
- SceneHierarchy、Viewport、Properties 三个面板都通过服务读写状态。

这是低风险、高收益的优先改造。

### Phase 3：RenderDataPacket 轻量化

目标：Scene 不再把 GPU 对象塞进渲染数据。

任务：

- 新增 `AssetHandle` 类型。
- `RenderObjectData` 增加 `model_handle/material_handle`。
- `RendererEnvironmentData` 增加 environment handle，逐渐替代 texture shared_ptr。
- RendererSystem 内部通过 `RenderResourceCache` 查 GPU 资源。
- 删除 `render_data.h` 对 `texture.h` 和 `vertex_array.h` 的直接 include。

验收标准：

- `RenderDataPacket` 可复制、可移动、无 RHI shared_ptr。
- Scene 层不 include RHI 资源头。
- Vulkan 后端替换时 Scene 不需要改。

### Phase 4：Asset 与 Renderer 上传职责分离

目标：Asset 可以在没有 GL context 的情况下加载/导入。

任务：

- 新增 `MeshSource`、`ImageSource`、`MaterialSource`。
- `ProceduralModelFactory` 改为生成 CPU mesh。
- `RenderResourceUploader` 移动到 Renderer/Resources。
- `AssetManager` 主接口返回 handle/CPU asset，不返回 GPU texture/model。
- `IBLBuilder` 移到 Renderer/Resources。

验收标准：

- 资源导入单元测试不需要初始化 OpenGL。
- GPU 创建和销毁只发生在 Renderer 模块。

### Phase 5：RendererDevice / RenderPassContext

目标：OpenGL 后端先按 Vulkan 友好的边界运行。

任务：

- 从 `renderer_system.h` 拆出 `RendererFactory`。
- 新增 `RenderDevice` 接口。
- 新增 `OpenGLRenderDevice`。
- 新增 `RenderCommandContext`。
- `ShadowPassV2`、`SkyboxPassV2`、Forward pipeline 逐渐使用 context。
- Pass 显式声明 target、viewport、输入资源。

验收标准：

- RendererSystem 是外部唯一入口。
- Pass 不访问 global context。
- 未来 Vulkan 后端主要替换 `RenderDevice` 和 resource cache。

### Phase 6：资产元数据、序列化和热重载

目标：让项目资产可管理，而不是纯路径加载。

任务：

- `AssetRegistry` 保存 handle、path、type、import settings。
- 场景序列化保存 handle，不保存绝对路径。
- 支持 asset reload 消息。
- Editor 资源面板可读取 registry。
- 后续再做后台 asset processor。

## 建议优先落地的 8 个 PR

1. `EditorContext` 服务化第一步：加入 `SelectionService`，替代 `selected_entity/selection_source` 直接字段。
2. `EditorCamera` 输入解耦：新增 `InputState`，把 camera 从全局 InputSystem 中剥离。
3. 标记并隔离旧 `Scene`：减少主线代码歧义。
4. `AssetHandle` 类型引入：先用 `UUID` 包装，不大改加载逻辑。
5. `RenderDataPacket` 增加 handle 字段：和旧 GPU pointer 并存一版。
6. `RenderResourceCache` 最小实现：从 handle 查当前已有 GPU model/texture。
7. `RenderPassContext` 最小实现：先让 Pass 不再自己找全局 window/viewport。
8. `ProceduralModelFactory` 拆 CPU/GPU：生成 mesh source，再由 renderer 上传。

## 不建议现在做的事

- 不要现在重写 Vulkan renderer。边界没整理前写 Vulkan，会把旧耦合复制到新后端。
- 不要马上做完整动态插件系统。静态模块 + 清晰依赖已经够当前阶段使用。
- 不要马上上复杂 RenderGraph。先让 Pass 显式输入输出，后面自然能升级。
- 不要马上做异步资产加载。先拆 CPU asset 和 GPU resource，否则异步只会放大生命周期问题。
- 不要把所有管理器都变成单例。单例会让早期舒服，后期定位依赖痛苦。

## 一句话架构蓝图

NexAur 可以向“轻量 Godot/O3DE 分层 + Hazel 式 Application/Layer + bgfx/Diligent 式 RHI 边界 + Bevy 式模块安装”靠拢，但实现上保持个人引擎的克制：先用静态模块、显式服务、轻量 handle、Renderer 独占 GPU 资源，把每一步都压到能构建、能运行、能继续做游戏/编辑器的大小。

## 参考来源

- Godot 架构概览：https://docs.godotengine.org/en/stable/engine_details/architecture/godot_architecture_diagram.html
- O3DE Components：https://docs.o3de.org/docs/user-guide/programming/components/overview/
- O3DE Gems / Key Concepts：https://docs.o3de.org/docs/welcome-guide/key-concepts/
- O3DE Asset Pipeline：https://docs.o3de.org/docs/user-guide/assets/pipeline/asset-processing/
- O3DE Atom Pass System：https://docs.o3de.org/docs/atom-guide/dev-guide/passes/pass-system/
- O3DE Atom RHI：https://docs.o3de.org/docs/atom-guide/dev-guide/rhi/
- Bevy ECS：https://bevy.org/learn/quick-start/getting-started/ecs/
- Bevy Plugins：https://bevy.org/learn/quick-start/getting-started/plugins/
- Hazel repository：https://github.com/TheCherno/Hazel
- Filament repository：https://github.com/google/filament
- Filament PBR / Materials docs：https://google.github.io/filament/Filament.md.html
- bgfx overview：https://bkaradzic.github.io/bgfx/overview.html
- bgfx API reference：https://bkaradzic.github.io/bgfx/bgfx.html
- Diligent Engine repository：https://github.com/DiligentGraphics/DiligentEngine
- Diligent Engine architecture：https://diligentgraphics.com/diligent-engine/architecture/
- Wicked Engine repository：https://github.com/turanszkij/WickedEngine
- Urho3D Subsystems：https://urho3d.io/documentation/1.5/_subsystems.html
- Urho3D Scene Model：https://urho3d.io/documentation/1.7/_scene_model.html
- Cocos2d-x Scene Graph：https://docs.cocos.com/cocos2d-x/manual/en/basic_concepts/scene.html
- OGRE setup / plugins：https://ogrecave.github.io/ogre/api/latest/setup.html
- OGRE core objects：https://www.ogre3d.org/docs/manual18/manual_4.html
- Magnum features：https://magnum.graphics/features/
- Magnum feature guide：https://doc.magnum.graphics/magnum/features.html

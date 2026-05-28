# NexAur 架构解耦改进方案

日期：2026-05-17

## 目标

这份方案的目标不是立刻重写渲染器，也不是提前把项目改成 Vulkan 架构，而是在当前 OpenGL 渲染器还能继续工作的前提下，逐步降低模块耦合，让后续扩展编辑器、资源系统、场景系统和渲染后端时少踩生命周期和依赖边界的坑。

核心目标：

- 让 Engine 只负责应用生命周期和每帧调度，不直接承担所有子系统细节。
- 让 Scene 保持为纯数据和逻辑层，不直接持有 RHI 对象。
- 让 Renderer 独立拥有 GPU 资源、Pass、Framebuffer 和后端状态。
- 让 Editor 通过明确的上下文服务访问场景、选择、输入和视口，不直接穿透到全局系统。
- 让资源系统区分 CPU 资产、GPU 资源和运行时句柄，为以后异步加载和 Vulkan 后端预留空间。

## 当前阅读范围

重点看过这些模块：

- `source/Engine/engine.*`
- `source/Engine/Function/Global/global_context.*`
- `source/Engine/Function/Scene/scene_v2.*`
- `source/Engine/Function/Scene/component.h`
- `source/Engine/Function/Renderer/RHI/*`
- `source/Engine/Function/Renderer/Passes/*`
- `source/Engine/Function/Renderer/data/*`
- `source/Engine/Function/Resource/*`
- `source/Engine/Editor/*`
- `source/Engine/Editor/Panels/*`
- `source/Engine/Function/UI/*`
- `source/Engine/Function/Input/*`
- `source/Sandbox/*`
- 顶层和 source 目录下的 CMake 文件

## 总体判断

项目已经有不错的雏形：Engine 主循环、全局系统启动、ECS 场景、渲染数据包、编辑器面板、viewport framebuffer、对象拾取、Pass 分层都已经出现了。

主要问题是这些模块的边界还没有稳定下来。很多地方能跑通功能，但跨层访问较多，例如 Scene 直接碰 AssetManager，Resource 直接创建 RHI 对象，Pass 直接访问全局 RendererSystem，EditorCamera 直接从全局 InputSystem 拉输入。这些设计在个人项目早期很正常，但如果继续堆功能，后面会让重构成本快速变高。

## 高优先级问题

### 1. 子系统生命周期和 GPU 资源销毁顺序有风险

`RunTimeGlobalContext::shutdownSystems()` 当前先关闭并销毁 `WindowSystem`，再关闭 `RendererSystem`。OpenGL 资源释放通常需要有效 GL context，如果窗口和 context 先销毁，后续 renderer 或 texture/framebuffer 析构可能会在无效 context 下调用 GL 删除函数。

建议：

- Renderer、AssetManager 中的 GPU 资源应先释放。
- UI 应在 renderer/window context 还有效时 shutdown。
- WindowSystem 应最后销毁窗口和图形 context。
- AssetManager 不建议作为无生命周期控制的函数静态单例长期持有 GPU 资源。

推荐顺序：

```text
Engine shutdown
1. EditorLayer shutdown
2. SceneManager shutdown
3. AssetManager release runtime GPU resources
4. RendererSystem shutdown
5. UISystem shutdown
6. InputSystem shutdown
7. WindowSystem shutdown
8. FileSystem shutdown
9. LogSystem shutdown
```

实际顺序可以微调，但原则是：依赖 GPU context 的对象必须在 context 销毁前释放。

### 2. `g_runtime_global_context` 是事实上的全局 Service Locator

全局上下文目前被 Engine、UI、Input、Editor、Scene、Pass、IBLBuilder、Sandbox 多处直接访问。这会导致：

- 调用关系不透明，模块测试困难。
- 系统初始化顺序被隐式依赖。
- 很难知道一个类真正需要哪些能力。
- 后续多场景、多窗口、离线资源构建会被全局单例卡住。

建议不要一次性删除全局上下文，而是先把访问范围收窄：

- Engine 和 composition root 可以访问 `RunTimeGlobalContext`。
- 业务对象通过构造参数或上下文接口拿依赖。
- 禁止新代码在深层模块里直接包含 `global_context.h`。
- 分阶段把 `EditorCamera`、`IBLBuilder`、`IRenderPass`、`ViewportPanel` 中的全局访问替换为显式依赖。

### 3. Scene 数据和 Renderer GPU 资源混在一起

`RenderDataPacket` 里的 `RenderObjectData` 当前持有 `std::shared_ptr<RenderModelData>`，而 `RenderModelData` 持有 `VertexArray`、`Texture2D` 等 GPU/RHI 对象。

这会让 Scene 抽取数据时直接携带后端资源，未来 Vulkan 或多线程渲染时会比较难处理。

建议逐步改成：

```cpp
struct RenderObjectData {
    AssetHandle model;
    glm::mat4 transform;
    int entity_id = -1;
};
```

RendererSystem 内部再通过 `AssetHandle` 查到后端 GPU 资源。

短期可以先引入 `AssetHandle` 类型，但保留旧路径兼容，等资源管理稳定后再移除 `shared_ptr<RenderModelData>`。

### 4. Resource 模块不应直接生成后端 GPU 对象

`ProceduralModelFactory` 当前直接创建 `VertexArray`、`VertexBuffer`、`IndexBuffer`，`RenderResourceUploader` 也直接把 CPU Model 上传成 RHI 对象。这样 Resource 层和 Renderer 层绑定很深。

建议拆成三层：

```text
Importer/Generator
  输出 CPU 资产，例如 MeshSource, ImageSource, MaterialSource

AssetManager
  管理 AssetHandle、路径、缓存、元数据

RendererResourceCache
  在 renderer 内把 AssetHandle 上传为后端 GPU 资源
```

短期可以先做两个类型：

- `MeshSource`：CPU 顶点、索引、材质路径。
- `GpuMesh` 或 `RenderMeshResource`：后端持有的 VAO/VBO/EBO。

这样 procedural mesh 可以先生成 CPU 数据，再交给 renderer 上传。

### 5. 旧 Scene 和 SceneV2 并存，容易制造重复路径

`source/Engine/Function/Scene/scene.*` 和 `scene_v2.*` 同时存在，而且旧 Scene 里仍有大量 RendererFactory、RendererCommand、IBL 烘焙逻辑。

建议：

- 明确 `SceneV2` 为当前主线。
- 将旧 `Scene` 标记为 legacy 或移动到 `TempTest/Legacy`。
- 如果旧 Scene 还有有价值代码，拆到独立工具类，例如 mesh primitive、IBL helper、light demo。
- 防止新功能继续接入旧 Scene。

这是性价比很高的清理，因为它会减少认知分叉。

## 分阶段改进路线

### Phase 0：工程卫生和安全边界

目标：不大改架构，先把明显会拖后腿的东西收紧。

任务：

- 固定 UTF-8 编码，避免中文注释和文档在不同终端下显示乱码。
- 删除或隔离明显未使用的 `TempTest`、空 `source/Editor/main.cpp`、旧 Editor 目录里的重复面板。
- 给 `NexAurEditor` 目标补一个最小入口，或者暂时从 `source/CMakeLists.txt` 移除，避免空目标误导。
- 梳理 CMake target：Engine、Sandbox、Editor、Tests 各自职责清楚。
- 给 `NX_CORE_ASSERT` 做一次安全修正，支持无额外格式参数的调用。
- 给 `RunTimeGlobalContext::shutdownSystems()` 调整销毁顺序，先保证 GL context 生命周期安全。

验收标准：

- Debug 构建能过。
- Sandbox 能正常启动和退出。
- 关闭窗口时不出现 GL 资源析构时机相关问题。
- 目录结构能看出主线代码和 legacy/test 代码。

### Phase 1：收窄全局上下文访问

目标：全局上下文仍存在，但深层模块不再主动穿透它。

任务：

- 为 Editor 创建更明确的 `EditorServices` 或扩展 `EditorContext`：
  - `SceneV2* active_scene`
  - `RendererSystem* renderer`
  - `InputSystem* input`
  - `SelectionService* selection`
  - `ViewportService* viewport`
- `ViewportPanel` 不再直接访问 `g_runtime_global_context.m_input_system`。
- `EditorCamera` 不再直接访问全局 InputSystem，改为：
  - `onUpdate(delta, const InputState&)`
  - 或 `EditorCameraController` 负责输入，Camera 只保存矩阵和参数。
- `IRenderPass` 不再在 `target_framebuffer == nullptr` 时直接从全局 RendererSystem 取 viewport framebuffer，而是由 `RenderPassContext` 提供当前 render target。
- `IBLBuilder` 不再读取全局 WindowSystem 来恢复 viewport，改为由调用者传入 previous viewport 或 command context。

验收标准：

- `global_context.h` 的 include 数量明显下降。
- 新增模块不需要知道全局系统名字。
- Editor、Pass、Resource 的依赖能从构造参数或函数参数看出来。

### Phase 2：Scene 与 RenderData 解耦

目标：Scene 只描述世界状态，Renderer 负责后端资源。

任务：

- 引入统一句柄类型：

```cpp
using AssetHandle = UUID;
```

- `MeshRendererComponent` 保持 `model_id`，但命名逐步改为 `model_handle`。
- `RenderObjectData` 从 `shared_ptr<RenderModelData>` 迁移到 `AssetHandle model_handle`。
- `RenderDataPacket` 只存轻量、可复制、跨线程友好的数据。
- RendererSystem 内维护 `RenderResourceCache`：
  - model handle 到 GPU model
  - texture handle 到 GPU texture
  - material handle 到 GPU material
- `SceneV2::extractSceneData()` 不再直接从 AssetManager 获取 GPU model，只把组件里的 handle 和 transform 打包出去。

验收标准：

- Scene 层不 include `vertex_array.h`、`texture.h`、`renderer_system.h`。
- `RenderDataPacket` 不直接持有 RHI 对象。
- 之后把 OpenGL GPU 资源换成 Vulkan 资源时，Scene 不需要改。

### Phase 3：资源系统分层

目标：区分资产身份、CPU 数据、GPU 数据。

建议结构：

```text
AssetManager
  管路径、UUID、元数据、CPU 资产缓存

AssetImporter
  用 Assimp/stb 把文件导入成 CPU 资产

ProceduralAssetFactory
  生成 CPU mesh/image，不创建 RHI 对象

RenderResourceCache
  RendererSystem 内部拥有，把 CPU 资产上传到当前后端

IBLBuildService
  可以是 renderer 侧工具，因为它强依赖 GPU pass
```

任务：

- 新增 `MeshSource`、`ImageSource`、`MaterialSource`。
- `Model` 继续作为 CPU 模型，但不直接触碰 GPU 上传。
- `RenderResourceUploader` 移到 Renderer 模块或改名为 `OpenGLRenderResourceUploader`。
- `IBLBuilder` 移到 renderer resource/utility 侧，不放在纯 Resource 模块里。
- AssetManager 返回 handle 和 CPU 数据，不返回 OpenGL texture/VAO。

验收标准：

- Resource 模块可以在没有 OpenGL context 的情况下导入模型和图片。
- Renderer 模块负责所有 GPU 创建和销毁。
- 资源加载失败能返回明确错误，而不是只打日志后返回 `INVALID_UUID`。

### Phase 4：Renderer 内部边界整理

目标：在不写 Vulkan 的情况下，让 OpenGL 后端也更像一个真正的后端。

任务：

- 将 `RendererFactory` 从 `renderer_system.h` 中拆出到独立文件，例如：
  - `renderer_factory.h`
  - `render_device.h`
  - `opengl_render_device.h`
- 引入 `RenderDevice` 接口，短期只实现 OpenGL：

```cpp
class RenderDevice {
public:
    virtual std::shared_ptr<VertexBuffer> createVertexBuffer(...) = 0;
    virtual std::shared_ptr<Texture2D> createTexture2D(...) = 0;
    virtual std::shared_ptr<Framebuffer> createFramebuffer(...) = 0;
};
```

- `RendererCommand` 从静态全局命令变成 `RenderCommandList` 或 `RenderContext` 的方法。
- `RenderPass` 接收 `RenderPassContext`：

```cpp
struct RenderPassContext {
    RenderDevice& device;
    RendererState& state;
    Framebuffer* target;
    glm::uvec2 viewport_size;
};
```

- Pass 不直接改全局 viewport，所有状态修改集中通过 context。
- `ShadowPassV2` 和 `SkyboxPassV2` 统一使用 `IRenderPass::run()` 流程，不再混用 `run_without_begin_end`。

验收标准：

- RendererSystem 是 renderer 的唯一入口。
- Pass 不直接访问 WindowSystem 或 global context。
- OpenGL 仍能跑 viewport、shadow、skybox、picking。

### Phase 5：Editor 模块服务化

目标：让编辑器面板之间通过明确服务协作，而不是共享一个过大的上下文。

建议拆分：

```text
EditorContext
  生命周期和服务入口

SelectionService
  当前选中实体、选择来源、选择变更事件

SceneEditingService
  创建、删除、重命名实体，添加/删除组件

ViewportService
  viewport bounds、size、hover/focus、framebuffer handle、picking request

InspectorRegistry
  组件属性绘制器注册

EditorCommandStack
  undo/redo 命令栈
```

任务：

- `SceneHierarchyPanel` 不直接改 `selected_entity`，改为 `selection.select(entity, source)`。
- `ViewportPanel` picking 后提交选择请求。
- `PropertiesPanel` 修改 Transform 时走 `EditorCommand` 或至少走 `SceneEditingService`，为 undo/redo 留入口。
- `EditorCamera` 移出 `SceneV2`，由 `ViewportService` 或 `EditorLayer` 拥有。
- Runtime camera 和 editor camera 区分清楚。

验收标准：

- 新增一个面板时，只需要拿它真正需要的服务。
- 选择状态有单一写入口。
- 后续实现 undo/redo 不需要重写全部面板。

### Phase 6：事件和输入系统整理

目标：输入行为能按上下文启用，不互相抢。

任务：

- Engine 事件流改成 LayerStack：

```text
WindowSystem -> Engine -> UISystem -> EditorLayer -> RuntimeLayer
```

- ImGui 捕获输入时，Editor/Runtime 是否还能收到事件由策略决定。
- Viewport 只有在 hovered/focused 且鼠标在 viewport 内时才驱动 editor camera。
- `InputSystem` 保留 polling，但增加 `InputState` 快照，避免每个模块自己查 GLFW。
- 鼠标拾取改成显式 request：

```cpp
viewportPicking.requestPick(mouse_position);
```

验收标准：

- 在 PropertiesPanel 输入文本时，WASD 不移动相机。
- 在 ImGuizmo 操作时，不触发对象拾取。
- 输入规则集中可查，不散落在各个系统里。

## 推荐优先级

优先做：

1. 修正 shutdown 顺序和 GPU 资源生命周期。
2. 清理空 Editor 目标、legacy Scene、TempTest 的目标边界。
3. 把 EditorCamera 输入从全局 InputSystem 中剥离。
4. 把 `RenderDataPacket` 改轻，先引入 `AssetHandle`。
5. 把 `ProceduralModelFactory` 改成输出 CPU mesh，再交给 renderer 上传。

暂时不急：

- 完整 RenderGraph。
- 异步资源加载。
- 多线程渲染。
- Vulkan 后端抽象。
- 完整 prefab/scene 序列化。

## 建议的近期工作拆分

### PR 1：生命周期安全和工程清理

范围：

- 调整 `RunTimeGlobalContext::shutdownSystems()`。
- 给 `AssetManager` 增加明确 `init/shutdown` 调用点。
- 处理空 `NexAurEditor` 目标。
- 标记 legacy Scene。

风险：低。

### PR 2：Editor 输入边界

范围：

- 新增 `EditorInputContext` 或 `InputState`。
- `EditorCamera` 不直接访问 global context。
- Viewport focus/hover 控制相机移动。

风险：低到中。

### PR 3：SelectionService

范围：

- 新增 `SelectionService`。
- SceneHierarchyPanel、ViewportPanel、PropertiesPanel 通过服务读写选择。
- 保留当前 UI 行为。

风险：低。

### PR 4：RenderDataPacket 轻量化

范围：

- 新增 `AssetHandle`。
- `RenderObjectData` 从 GPU model 指针迁移到 model handle。
- RendererSystem 侧解析 handle 到 GPU model。

风险：中。会碰 Scene、AssetManager、RendererSystem、Sandbox。

### PR 5：Resource 和 Renderer 上传职责拆分

范围：

- 新增 CPU `MeshSource`。
- ProceduralModelFactory 输出 CPU mesh。
- RendererResourceCache 负责上传。

风险：中。

## 长期方向

当上面这些完成后，未来 Vulkan 渲染器会容易很多，因为：

- Scene 不知道 OpenGL/Vulkan。
- Resource 不要求有图形 context。
- Renderer 独占 GPU 对象。
- Pass 通过 context 操作 render target，而不是访问全局系统。
- Editor viewport 和 picking 已经是清晰服务，不绑定 `glReadPixels` 模型。

届时 Vulkan 重构可以从 `RenderDevice`、`RenderResourceCache`、`RenderPassContext` 三个点切入，而不是推翻整个引擎。

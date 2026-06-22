# NexAur 架构优化、Vulkan 接入与小游戏 Demo 工作路线

日期：2026-06-22

## 1. 文档目标

这份文档用于整理 NexAur 接下来的总体工作方向。它不是每个任务的详细执行方案，而是后续拆分具体方案、PR 或阶段计划时的总纲。

当前期望路线是：

1. 先优化现有引擎架构设计，降低模块耦合，稳定生命周期和依赖边界。
2. 在不破坏现有 OpenGL 渲染路径的前提下，为后续 Vulkan 渲染器接入预留清晰接口。
3. 等基础架构稳定后，补全 runtime/gameplay 模块，用 NexAur 写一个可玩的小游戏 demo。

总体原则：

- 不急着重写整套引擎。
- 不把 Vulkan 接入当成第一步。
- 每个阶段都保持 Sandbox 或最小样例可运行。
- 优先处理会阻塞后续扩展的设计问题。
- 让 Scene、Resource、Renderer、Editor、Runtime 的职责边界逐步清楚起来。

## 2. 当前代码状态判断

NexAur 目前已经具备一个早期 3D 引擎雏形：

- `Engine` 已有主循环、事件入口、FPS 统计和逻辑/渲染调度。
- `RunTimeGlobalContext` 已能统一初始化 File、Window、Input、Renderer、Asset、Scene、UI。
- `SceneV2` 使用 EnTT 作为 ECS 基础。
- `RenderDataPacket` 已经承担 Scene 到 Renderer 的帧数据传递。
- `AssetHandle`、`AssetMetadata`、`RenderResourceCache` 已经开始把资产身份和 GPU 资源分开。
- Renderer 已有 OpenGL RHI 包装、Forward Pipeline、Shadow Pass、Skybox Pass、PBR、IBL、Viewport framebuffer 和 picking。
- Editor 已有 DockSpace、Viewport、Hierarchy、Properties、EditorCamera 和 ImGuizmo。

这说明项目主线是健康的。当前最大问题不是缺少某个单点功能，而是几个核心边界还没有完全稳定：

- 全局上下文访问范围偏大。
- Engine 主循环对 Editor、UI、Renderer 的调度比较硬编码。
- Editor 与 Runtime 的职责边界还不够清楚。
- Input/Event 还缺少统一消费策略和 action mapping。
- Renderer Pass 仍会直接访问全局窗口或 renderer。
- RenderDevice / RenderCommandContext / RenderPassContext 还没有形成。
- Scene 序列化、Prefab、Gameplay System、Physics、Audio、Runtime UI 等 demo 所需模块还未建立。

## 3. 第一阶段：架构优化优先方向

### 3.1 收窄 `RunTimeGlobalContext` 的使用范围

当前 `g_runtime_global_context` 是事实上的全局 Service Locator。它适合作为早期引擎的组合根，但不适合被深层模块随处访问。

优化方向：

- 保留 `RunTimeGlobalContext`，不要立即删除。
- 明确它主要服务于 Engine、Sandbox、Application 启动层。
- 新增深层模块时，避免直接 include `global_context.h`。
- 对 Editor、Renderer Pass、Input、IBLBuilder 等模块逐步改为显式依赖。

建议拆分上下文：

```text
EngineServices
  FileSystem
  WindowSystem
  InputSystem
  AssetManager
  SceneManager
  RendererSystem
  UISystem

EditorContext
  active scene
  viewport camera
  selection service
  viewport service
  scene editing service

RenderPassContext
  render device
  command context
  resource cache
  target framebuffer
  viewport size
```

验收目标：

- 深层模块不再主动穿透全局上下文。
- 一个类需要什么依赖，可以从构造函数或函数参数看出来。
- 后续多窗口、多视口、Vulkan 后端不会被全局单例卡住。

### 3.2 整理 Engine 主循环与模块调度

当前 `Engine::tickOneFrame()` 直接调度逻辑、Editor、UI、Renderer 和窗口更新。短期清晰，但后续 Runtime、Editor、Game Demo、Tools 都会挤进这里。

建议以 `EngineModule` / `ModuleManager` 作为第一阶段主线：

```text
Engine
  owns ModuleManager
  starts modules by dependency order
  routes events to module-level systems
  keeps frame loop readable

ModuleManager
  CoreModule
  FileSystemModule
  PlatformModule
  ResourceModule
  RenderContextModule
  RendererModule
  RuntimeModule
  UIModule
```

当前已完成：

- PR1：新增 `EngineModule`、`ModuleInfo`、`ModuleRegistry`、`ModuleManager` 骨架。
- PR2：`RunTimeGlobalContext` 降级为兼容桥，由 `ModuleManager` 管理系统初始化和关闭。
- PR3：`PlatformModule` 注册 `WindowService` / `InputService`，`InputSystemGLFW` 维护 `InputState`。
- PR4：`RuntimeModule` 注册 `SceneService`，Engine 和 Editor 顶层优先通过服务获取 active scene。
- PR5：`RendererModule` 注册 `RendererService`，Viewport resize、显示和 picking 走渲染服务入口。

优先级：

1. 让 Engine 只保留 frame loop、事件入口和少量兼容调度。
2. 将 Platform、Runtime、Renderer、UI 的生命周期继续收口到模块。
3. 后续 PR6 再让 EditorLayer 进入 EditorModule。
4. Game Demo 逻辑后续作为 GameModule 或 Sandbox application module 接入。

### 3.3 明确 Editor 与 Runtime 边界

当前 EditorCamera 会把视口相机数据写入渲染包，这对编辑器预览是合理的，但游戏运行时需要自己的 CameraComponent。

建议区分：

- `SceneView`：编辑器视口，使用 EditorCamera。
- `GameView`：运行时视口，使用场景里的 CameraComponent。
- `EditorCamera`：只属于 Editor。
- `CameraComponent`：只描述游戏/运行时相机。

后续 Play Mode 可以采用：

```text
Edit Mode
  SceneView uses EditorCamera

Play Mode
  GameView uses active CameraComponent
```

短期目标：

- EditorCamera 输入逻辑不直接访问全局 InputSystem。
- EditorLayer 不直接修改 runtime camera component。
- Renderer 接收当前 View 所选择的 camera data。

### 3.4 输入系统从 KeyCode 轮询升级到 Action Mapping

当前输入系统可以查询键盘和鼠标状态，但 gameplay 直接判断 `KeyCode::W` 会让游戏逻辑绑定具体输入设备。

建议增加 `InputAction`：

```text
MoveForward -> W
MoveBackward -> S
MoveLeft -> A
MoveRight -> D
Jump -> Space
Attack -> MouseLeft
Pause -> Escape
```

模块建议：

- `InputState`：每帧输入快照。
- `InputActionMap`：动作到按键/鼠标/手柄的映射。
- `InputActionSystem`：查询 action 的 pressed、released、held、axis。

验收目标：

- Gameplay 不直接依赖 GLFW key code。
- Editor 输入和 Runtime 输入可以按上下文启用或屏蔽。
- UI 输入文字时不会驱动游戏或编辑器相机。

### 3.5 事件分发增加明确消费策略

当前事件对象有 `handled` 字段，UI 也能判断 ImGui 是否想捕获输入，但事件流还没有形成统一策略。

建议事件链路：

```text
WindowSystem
  -> Engine
  -> UISystem / ImGui
  -> EditorModule / EditorLayer
  -> RuntimeModule / GameModule
```

需要明确：

- UI 捕获鼠标时，Viewport picking 是否还能触发。
- UI 捕获键盘时，EditorCamera 是否还能移动。
- Editor 消费事件后，Runtime 是否还能收到。
- Runtime 游戏暂停时，哪些输入仍然有效。

短期可以先在 Engine 层做简单策略：

```text
if UI wants keyboard:
  block keyboard events from Editor/Runtime

if UI wants mouse and mouse is not inside viewport:
  block mouse events from Editor/Runtime
```

### 3.6 Renderer Pass 改为显式上下文

当前部分 Pass 会直接从全局上下文中获取 RendererSystem、WindowSystem 或 viewport framebuffer。这会阻碍后续 Vulkan、RenderGraph、多 viewport。

建议引入：

```cpp
struct RenderPassContext {
    RenderDevice* device = nullptr;
    RenderCommandContext* commands = nullptr;
    RenderResourceCache* resources = nullptr;
    Framebuffer* target = nullptr;
    uint32_t viewport_width = 0;
    uint32_t viewport_height = 0;
};
```

目标：

- `IRenderPass` 不访问全局系统。
- `ShadowPass` 不自己恢复全局 viewport。
- `SkyboxPass`、`ShadowPass`、Forward Pass 统一执行模型。
- Pass 的输入输出由 context 或 specification 提供。

这一步是 Vulkan 接入前的重要准备。

### 3.7 Resource、Scene、Renderer 边界继续收紧

当前代码已经开始用 `AssetHandle` 和 `RenderResourceCache` 分离职责，这是正确方向。后续应继续推进：

Scene 层：

- 只描述实体、组件、Transform、Camera、Light、MeshRenderer、Environment。
- 不持有 RHI 对象。
- 不创建 GPU buffer、texture、framebuffer。

Resource 层：

- 管理资产身份、路径、元数据、CPU 数据。
- 模型导入、图片导入、材质描述都属于 Resource。
- 不要求存在 OpenGL/Vulkan context。

Renderer 层：

- 独占 GPU 资源。
- 把 AssetHandle 解析成后端资源。
- 管理 Pass、Framebuffer、Pipeline、RenderTarget、IBL。

验收目标：

- 没有 GL context 时，Resource 仍能导入模型或读取资产元数据。
- 替换 Vulkan 后端时，Scene 和 Asset 不需要大改。

## 4. 第二阶段：Vulkan 渲染器接入前置设计

在接入 Vulkan 前，不建议直接把现有 OpenGL 类平移成 Vulkan 类。更好的方式是先让 OpenGL 也走一层后端无关接口。

### 4.1 引入 RenderDevice

`RendererFactory` 当前是静态工厂，后续可以演进为实例化设备接口：

```cpp
class RenderDevice {
public:
    virtual ~RenderDevice() = default;
    virtual std::shared_ptr<VertexBuffer> createVertexBuffer(...) = 0;
    virtual std::shared_ptr<IndexBuffer> createIndexBuffer(...) = 0;
    virtual std::shared_ptr<Texture2D> createTexture2D(...) = 0;
    virtual std::shared_ptr<TextureCubeMap> createTextureCube(...) = 0;
    virtual std::shared_ptr<Shader> createShader(...) = 0;
    virtual std::shared_ptr<Framebuffer> createFramebuffer(...) = 0;
};
```

短期实现：

```text
OpenGLRenderDevice
  wraps current OpenGL buffer/texture/shader/framebuffer creation
```

后续实现：

```text
VulkanRenderDevice
  wraps Vulkan buffer/image/pipeline/descriptors/render pass creation
```

### 4.2 引入 RenderCommandContext

当前 `RendererCommand` 是静态命令转发。OpenGL 阶段能用，但 Vulkan 需要更明确的 command buffer / frame context。

目标接口：

```cpp
class RenderCommandContext {
public:
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void beginRenderPass(...) = 0;
    virtual void endRenderPass() = 0;
    virtual void setViewport(...) = 0;
    virtual void bindShader(...) = 0;
    virtual void bindVertexArray(...) = 0;
    virtual void drawIndexed(...) = 0;
};
```

短期可以让 OpenGLCommandContext 内部仍然调用现有 `RendererCommand`。

### 4.3 抽象 RenderTarget 与 Framebuffer

OpenGL 的 framebuffer 和 Vulkan 的 render target/swapchain image 不是完全相同模型。

建议上层逐渐使用：

```text
RenderTarget
  color attachments
  depth attachment
  width / height
  sample count
```

Editor viewport、shadow map、picking texture、game view 都应被看成 render target，而不是直接默认窗口 framebuffer。

### 4.4 材质和资源绑定不要写死 OpenGL texture slot

当前 PBR pipeline 中有大量：

```cpp
texture->bind(slot);
shader->setInt("u_AlbedoMap", slot);
```

Vulkan 接入后，这些概念会变成 descriptor set / binding。建议逐步收口到 Material 或 RenderResourceBinding：

```cpp
material.setTexture("u_AlbedoMap", albedo_handle);
commands.bindMaterial(material);
```

短期不需要完整材质系统，但要避免新的高层代码继续写死 slot。

### 4.5 Shader 与 PipelineState 分离

OpenGL 中 shader 可以直接绑定并设置 uniform；Vulkan 中 pipeline state 更固定。

后续建议：

- `ShaderAsset`：着色器源码或 SPIR-V 资产。
- `PipelineState`：shader、blend、depth、rasterizer、layout 的组合。
- `MaterialInstance`：具体贴图和参数。

这不是第一阶段必须完成，但设计新接口时要给它留空间。

### 4.6 Vulkan 接入推荐顺序

建议顺序：

1. OpenGL 先接入 `RenderDevice` 和 `RenderCommandContext`。
2. Pass 改为使用 `RenderPassContext`。
3. ResourceCache 只依赖 RenderDevice，不直接调用 OpenGL 静态工厂。
4. RendererSystem 增加后端选择枚举：

```cpp
enum class RenderBackend {
    OpenGL,
    Vulkan
};
```

5. 接入 VulkanRenderDevice 的最小资源创建。
6. 先跑最小三角形或简单 mesh。
7. 再接 viewport framebuffer、depth、PBR、IBL、shadow、picking。

不要一开始就要求 Vulkan 后端完整复刻当前 OpenGL 效果。

## 5. 第三阶段：小游戏 Demo 所需模块

如果目标是写一个小型 3D 游戏 demo，例如角色移动、敌人、收集物、简单关卡、计分和胜负条件，当前引擎还需要补齐下面模块。

### 5.1 GameModule / GameplaySystem

需要有一个独立于 Editor 的游戏运行模块：

```text
GameModule
  load scene
  update gameplay systems
  process runtime input
  submit render camera
  draw runtime UI
```

Sandbox 不应该长期承担游戏逻辑。Sandbox 更适合作为样例入口或测试工程。

### 5.2 Scene Serialization

小游戏需要能保存和加载关卡。第一版可以只支持 JSON 或 YAML。

优先序列化：

- Entity ID
- Tag
- Transform
- CameraComponent
- MeshRendererComponent
- DirectionalLightComponent
- PointLightComponent
- EnvironmentComponent
- 后续 gameplay components

第一版可以保存资产路径，后续再过渡到稳定 AssetHandle / AssetRegistry。

### 5.3 Gameplay Component 与 System

建议建立简单 runtime systems：

```text
PlayerControlSystem
EnemySystem
ProjectileSystem
CollectibleSystem
LifetimeSystem
HealthSystem
GameRuleSystem
```

示例组件：

```cpp
struct PlayerComponent {};
struct EnemyComponent {};
struct VelocityComponent { glm::vec3 velocity; };
struct HealthComponent { int current; int max; };
struct CollectibleComponent { int score; };
struct LifetimeComponent { float seconds; };
```

这一步能让 Demo 从“场景展示”变成“可玩游戏”。

### 5.4 简单碰撞与触发器

小游戏第一版不一定需要完整物理引擎，但至少需要：

- SphereCollider
- AABBCollider
- Trigger overlap
- 简单 collision layer
- 基础 raycast 或 ground check

推荐先做自研轻量碰撞系统。等玩法需要复杂刚体、约束、角色控制器时，再考虑接 Bullet、Jolt 或 PhysX。

### 5.5 Runtime Camera Controller

至少实现一种游戏相机：

- 第一人称相机
- 第三人称跟随相机
- 俯视角相机
- 固定轨道相机

不要复用 EditorCamera 作为游戏相机。EditorCamera 只服务编辑器观察。

### 5.6 Runtime UI / HUD

小游戏需要：

- 分数
- 血量
- 暂停菜单
- 胜利/失败界面
- 简单按钮

第一版可以临时使用 ImGui 绘制 HUD。后续再考虑独立 runtime UI 系统。

### 5.7 Audio

最少需要：

- 播放背景音乐
- 播放一次性音效
- 控制音量
- 暂停/恢复

可选方案：

- miniaudio：轻量，适合个人引擎。
- SoLoud：易用，游戏音频能力更完整。
- OpenAL：传统方案。

### 5.8 Prefab / Spawn 模板

Demo 会反复创建敌人、子弹、收集物。没有 prefab 会大量重复代码。

第一版可以很简单：

```text
PrefabFactory
  createPlayer()
  createEnemy()
  createCollectible()
  createProjectile()
```

后续再演进成可序列化 prefab。

### 5.9 资源面板与资产清单

为了 Demo 制作效率，后续 Editor 可以增加：

- Asset Browser
- 模型/贴图预览
- 拖拽模型到 Scene
- 材质配置
- 缺失资产提示

这不一定是 Demo 第一阶段必须项，但会明显提升开发体验。

### 5.10 Game State 与关卡流程

需要管理：

- MainMenu
- Playing
- Paused
- GameOver
- Victory
- LevelLoading

可以先做简单状态机：

```cpp
enum class GameState {
    MainMenu,
    Playing,
    Paused,
    GameOver,
    Victory
};
```

## 6. 推荐阶段路线

### Phase 0：工程与文档整理

目标：

- 明确主线代码。
- 整理计划文档位置。
- 确认旧计划、legacy、临时代码是否保留。

建议任务：

- 将后续计划统一放到 `docs/plans`。
- 检查根目录 `plans` 是否继续保留，避免重复。
- 保持 README 指向最新架构文档。
- 确认 CMake target：Engine、Sandbox、未来 Editor/Game Demo。

### Phase 1：架构边界优化

目标：

- 收窄全局上下文。
- 引入 EngineModule / ModuleManager 主线。
- 初步整理 Editor 与 Runtime 边界。
- 输入和事件规则更清楚。

建议任务：

1. 完成 EngineModule、ModuleManager、ModuleRegistry 骨架。
2. 将 RunTimeGlobalContext 降级为兼容桥。
3. 继续收口 Platform、Runtime、Renderer、UI 模块边界。
4. EditorCamera 输入改为依赖 InputState。
5. ViewportPanel 不直接访问全局 InputSystem。
6. UI 捕获输入时阻断不应继续传播的事件。

### Phase 2：Renderer 接口前置重构

目标：

- 为 Vulkan 接入准备后端无关接口。
- 保持 OpenGL 路径可运行。

建议任务：

1. 拆出 `RenderDevice`。
2. 让 OpenGL 资源创建走 `OpenGLRenderDevice`。
3. 新增 `RenderCommandContext`。
4. 新增 `RenderPassContext`。
5. Shadow/Skybox/Forward Pass 改为显式 context。
6. ResourceCache 不再直接依赖静态工厂。

### Phase 3：Scene 与 Asset 基础能力

目标：

- 支持场景保存加载。
- 稳定资产引用。

建议任务：

1. 新增 `SceneSerializer`。
2. 保存和加载基础组件。
3. 增加 path fallback。
4. 初步建立 AssetRegistry。
5. Editor 接入 Save / Load。

### Phase 4：Vulkan 渲染器最小接入

目标：

- Vulkan 后端跑通最小渲染路径。
- 不要求立即完整支持现有全部 PBR/IBL 功能。

建议任务：

1. 增加 RenderBackend 配置。
2. 创建 VulkanRenderDevice。
3. 跑通 window surface / swapchain。
4. 跑通 basic mesh。
5. 接入 depth buffer。
6. 接入 viewport render target。
7. 接入基础 material / texture。
8. 再逐步迁移 shadow、skybox、IBL、picking。

### Phase 5：小游戏 Demo 模块

目标：

- 从引擎样例转向可玩 demo。

建议任务：

1. 新建 GameDemo target 或 Sandbox game mode。
2. 新增 Gameplay Systems。
3. 新增 InputAction。
4. 新增简单碰撞系统。
5. 新增 RuntimeCameraController。
6. 新增 HUD。
7. 新增 Audio。
8. 新增 PrefabFactory。
9. 制作第一版关卡。

## 7. 推荐优先级

近期优先：

1. PlatformModule / InputState。
2. EditorCamera 输入解耦。
3. ViewportPanel 输入和 picking 解耦。
4. RendererService / RenderPassContext。
5. RenderDevice 最小接口。
6. SceneSerializer 第一版。

中期优先：

1. AssetRegistry。
2. Editor Save / Load。
3. InputActionSystem。
4. GameplaySystem 框架。
5. 简单 Collider / Trigger。

后期优先：

1. VulkanRenderDevice。
2. Vulkan resource cache。
3. Material/PipelineState 抽象。
4. Runtime UI。
5. Audio。
6. Prefab/Asset Browser。

暂时不急：

- 完整 RenderGraph。
- 完整异步资源加载。
- 完整物理引擎。
- 动态插件系统。
- 大规模目录重排。
- 复杂材质编辑器。

## 8. 最小可执行目标

为了避免架构优化变成长期空转，建议设置几个可验证里程碑。

### Milestone A：架构整理后仍能跑 Sandbox

标准：

- OpenGL 路径正常启动。
- Editor viewport 正常显示。
- 相机移动、picking、gizmo 不退化。
- UI 输入不会误触发相机移动。

### Milestone B：Scene 可以保存和加载

标准：

- 保存当前默认场景。
- 重新加载后实体、Transform、模型、灯光、环境基本一致。
- 缺失资产给出 warn，不崩溃。

### Milestone C：OpenGL 走 RenderDevice / PassContext

标准：

- RendererSystem 是外部唯一渲染入口。
- Pass 不访问全局 WindowSystem / RendererSystem。
- ResourceCache 通过 RenderDevice 创建 GPU 资源。

### Milestone D：Vulkan 最小路径跑通

标准：

- 能创建 Vulkan window surface / swapchain。
- 能清屏。
- 能绘制一个 mesh。
- 能和 OpenGL 后端在配置上切换。

### Milestone E：小游戏 Demo 可玩

标准：

- 有玩家控制。
- 有关卡加载。
- 有简单碰撞或触发器。
- 有胜负条件。
- 有 HUD。
- 有音效。

## 9. 总结

NexAur 现在最适合的路线不是立刻扩展大量功能，也不是马上切 Vulkan，而是先把现有 OpenGL 引擎的边界整理成 Vulkan 也能接受的形状。

推荐主线：

```text
架构边界优化
  -> Renderer 后端接口整理
  -> Scene / Asset 可保存可引用
  -> Vulkan 最小后端接入
  -> Gameplay 模块补全
  -> 小游戏 Demo
```

这条路线的好处是每一步都有可运行结果，同时每一步都在为下一步降低风险。等 Vulkan 接入时，它会接到一个职责相对清楚的 Renderer 层，而不是把当前 OpenGL 的隐式全局状态和模块耦合复制过去。

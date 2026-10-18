# 模块系统

最后核对：2026-10-18。源码入口：[engine_module.h](../../../source/Engine/Core/Module/engine_module.h)、[module_manager.cpp](../../../source/Engine/Core/Module/module_manager.cpp)、[global_context.cpp](../../../source/Engine/Function/Global/global_context.cpp)。

## 职责与依赖

Engine 调度模块，Module 拥有系统，Service 暴露跨模块能力，局部 Context 传递依赖。

`ModuleInfo` 包含唯一名称、`ModuleStage` 和依赖名称列表。初始化依赖按拓扑排序解析；重复名称、缺失依赖和依赖环会报错。初始化后不允许再注册模块。

`ModuleRegistry` 按类型保存 `shared_ptr` 服务，支持 `registerService<T>()`、`getService<T>()`、`getRequiredService<T>()` 和 `resetService<T>()`。它是模块顶层的装配工具，不应传入 Component、Panel 或 RenderPass。

## 生命周期

| 阶段 | 作用 |
| --- | --- |
| `registerServices` | 所有模块先注册服务壳，当前多数模块在 initialize 中注册实际服务 |
| `initialize` | 按依赖顺序创建系统与资源 |
| `postInitialize` | 所有模块初始化后进行最终连接 |
| `tick` | 输入、运行时和玩法等每帧更新 |
| `postUpdate` | 场景提取，以及 Editor 预览相机覆盖 |
| `renderUI` | 提交各模块的 ImGui 内容 |
| `onEvent` | 接收同步事件；handled 后停止继续分发 |
| `shutdown` | 按实际初始化顺序反向释放依赖 |

tick、postUpdate 和 renderUI 均沿初始化顺序调用。ModuleStage 当前主要决定事件优先级，不是另一套 tick 排序。

## 内置模块

| 模块 | 拥有/管理 | 主要对外能力 |
| --- | --- | --- |
| Core | 日志初始化 | `LogSystem` 静态接口 |
| FileSystem | 文件路径服务 | `FileSystem` |
| Platform | GLFW 窗口、InputSystemGLFW | `WindowService`、`InputService` |
| InputAction | InputActionSystem | `InputActionService` |
| Resource | AssetManager 生命周期 | `AssetManager` 服务视图 |
| Audio | miniaudio AudioSystem | `AudioService` |
| RenderContext | 双缓冲数据包与渲染设置 | `RenderContext` |
| Renderer | VulkanRendererSystem | `RendererService`、`ViewportRendererService`、`ReflectionProbeCaptureService`、`RendererDebugService` |
| Runtime | SceneManager | `SceneService` |
| Game | Gameplay systems、GameState、RuntimeHud | 更新当前场景中的玩法组件 |
| UI | UISystem / ImGui 生命周期 | `UIService` |
| Editor | EditorContext、EditorLayer、面板和观察相机 | `EditorService`、`SelectionService`、`ViewportService` |

部分模块还注册实现类型用于内部或兼容访问，新调用方优先使用 Service。Resource 对单例 AssetManager 的 shared_ptr 使用不删除对象的 deleter，不代表所有权已迁入 Module。

`ModuleStage::Physics` 是预留阶段；当前没有独立注册的 PhysicsModule，TriggerOverlapSystem 由 GameModule 驱动。

## 关键服务边界

- [RendererService](../../../source/Engine/Function/Renderer/renderer_service.h)：帧渲染和 UI 后端生命周期。
- [ViewportRendererService](../../../source/Engine/Function/Renderer/viewport_renderer_service.h)：视口尺寸、输出和拾取。
- [ReflectionProbeCaptureService](../../../source/Engine/Function/Renderer/reflection_probe_capture_service.h)：Probe capture 请求与状态。
- [RendererDebugService](../../../source/Engine/Function/Renderer/renderer_debug_service.h)：只读诊断快照。
- [SceneService](../../../source/Engine/Function/Scene/scene_service.h)：当前场景和场景切换。
- [EditorContext](../../../source/Engine/Editor/editor_context.h)：面板需要的服务、相机、选择、工具和视口状态。

Renderer 的这些窄接口已经替代早期把全部功能集中到 RendererService 的设计；外部不再通过 OpenGL texture ID 或旧 RendererSystem 访问渲染。

## 事件排序

ModuleManager 先反转初始化列表，再按 ModuleStage 降序稳定排序。因此 UI 优先于 Game、Editor、Renderer 和 Runtime；同 stage 中后初始化的模块先接收事件。

Engine 先处理 WindowClose/WindowResize，再把未消费事件交给 ModuleManager。UIModule 根据 ImGui capture 状态和 viewport focus/hover 决定是否拦截输入，具体规则见[事件系统](../EventSystem/EventSystem.md)。

事件拦截和 InputAction 的轮询是两条路径，不应把 handled 当作所有输入状态的过滤器。

## 全局上下文

RunTimeGlobalContext 只组合模块，并在初始化后同步旧的 FileSystem、WindowSystem、InputSystem、SceneManager、RenderContext、UISystem 兼容指针。各模块注册自身服务，不反向写全局指针。

新代码中的依赖获取应停留在模块边界：

```cpp
void ExampleModule::initialize(ModuleContext& context) {
    auto scene_service = context.registry.getRequiredService<SceneService>();
    m_system = std::make_unique<ExampleSystem>(scene_service);
}
```

这是装配示意；ExampleSystem 只接收所需服务，不继续读取全局上下文或 ModuleRegistry。

## 新增模块

1. 明确模块拥有的系统/资源和最小对外 Service。
2. 在领域目录实现 EngineModule，声明名称、stage 和依赖。
3. 在 initialize 注册服务，在 shutdown 释放服务与系统。
4. 选择 tick、postUpdate、renderUI 或 onEvent 中合适的阶段。
5. 在 global_context.cpp 注册工厂，在 Engine CMake 中显式加入源码。
6. 增加与变更匹配的验证，并更新当前架构说明。

当前模块为静态注册；动态插件加载不是本系统已实现的功能。早期 PR1～PR17 的迁移过程保留在[历史计划](../../plans/README.md)中。

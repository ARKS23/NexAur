# 编辑器架构

最后核对：2026-10-18。编辑器实现位于 Engine/Editor，由 EditorModule 管理，没有单独参与构建的 Editor executable。

## 对象与服务

- [EditorModule](../../../source/Engine/Editor/editor_module.cpp)创建 EditorContext 和 EditorLayer，并注册 EditorService、SelectionService、ViewportService。
- [EditorLayer](../../../source/Engine/Editor/editor_layer.cpp)调度面板、菜单、工具栏、命令、布局、场景保存/加载、Probe bake 和观察相机。
- [EditorContext](../../../source/Engine/Editor/editor_context.h)保存场景、资产及渲染窄服务、选择、工具和视口状态；Panel 不读取全局 ModuleRegistry。
- SelectionService 统一选择变更，Hierarchy、Viewport、Inspector 不直接互相调用。

## 当前面板

| 面板 | 作用 |
| --- | --- |
| Viewport | 展示渲染输出、resize、picking、Gizmo、视口覆盖层 |
| SceneHierarchy | 展示实体并通过 SelectionService 修改选择 |
| Properties / Inspector | 编辑组件、Transform、材质实例和贴图槽 |
| RenderSettings | 调整渲染设置 |
| RendererDebug | 查看后端、资源、渲染效果与光追等诊断 |
| Project | 浏览 assets 目录并过滤文件；不等于完整资产数据库 |
| Console | 显示和过滤 LogSystem 的近期日志 |

EditorLayer 还注册了 PlaceholderPanel；出现面板标签不代表对应工具已实现。

## 相机与运行时边界

SceneView 在 postUpdate 中用 EditorCamera 覆盖 RenderDataPacket 的预览相机。GameView 保留 SceneV2 提取的 active CameraComponent。EditorCamera 不改写场景 runtime camera。

Play/Pause/Stop 命令目前只记录占位日志，尚未建立运行场景副本、恢复编辑场景等完整生命周期。GameModule 已有自己的玩法更新和暂停状态；它与编辑器工具栏尚未接通。

## 命令和持久化

EditorCommandRegistry 为菜单、工具栏和快捷键共享 command id、enabled/selected 条件及执行函数。当前可保存编辑器配置、布局、快捷键和场景，场景数据通过 SceneSerializer 处理。

Inspector 已支持材质 slot override、Make Instance/Use Default、参数编辑与贴图 Import/Clear。独立持久化 `.nxmat` 资产和完整 Undo/Redo 仍在后续设计范围，不能把场景中的材质数据保存等同于完整材质资产系统。

## 输入与渲染

- UIModule 优先决定事件是否被 UI 捕获；Editor 相机还检查视口状态。
- ViewportRendererService 负责输出和拾取，不向面板暴露 Vk 对象。
- ReflectionProbeCaptureService 负责 bake 请求和状态，RendererDebugService 提供诊断。
- 当前输入轮询与事件捕获的区别见[输入系统](../InputSystem/Input.md)。

后续工作及历史完成记录见 [Editor UI 计划](../../plans/Editor/editor_ui_polish_plan.md)和[材质编辑计划](../../plans/Editor/material_editing_design_plan.md)。

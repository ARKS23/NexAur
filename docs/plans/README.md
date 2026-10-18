# 开发计划索引

最后核对：2026-10-18。当前实现概览见[文档导航](../README.md)，实际运行命令见[构建与测试](../development/build_and_test.md)。

## 进行中的主线

| 文档 | 当前状态 | 下一步 |
| --- | --- | --- |
| [Ray Tracing 总计划](Renderer/RayTracing/ray_tracing_development_plan.md) | RT-00～RT-09 已完成；RT-10.0～RT-10.2 已实现 | RT-10.3 时空降噪 |
| [RT-10 反射专项](Renderer/RayTracing/ray_traced_reflection_foundation_plan.md) | 已有 scene tables、surface/history foundation 和 raw trace/hit shading | RT-10.3 降噪，随后 RT-10.4 合成 |

RT-10 尚未整体完成。当前 raw RT 反射通过 debug view 查看，默认关闭；目标帧图中的 temporal resolve、spatial filter 和独立 hybrid composite 不能当作已实现能力。

## 后续设计参考

| 文档 | 使用边界 |
| --- | --- |
| [材质编辑设计](Editor/material_editing_design_plan.md) | Primitive、slot override、runtime instance 和基础贴图编辑已有实现；独立 .nxmat 与完整 UX/Undo 仍属后续 |

## 历史方案与实施记录

以下文档保留当时的需求、取舍、工作包和验证记录，不再承担全项目当前进度入口。正文中的旧 API、源码路径、分支和测试命令应在历史语境中阅读。

| 阶段 | 文档 | 当前阅读入口 |
| --- | --- | --- |
| 最初总体路线 | [架构/Vulkan/Demo 路线](engine_architecture_vulkan_demo_roadmap.md) | [整体架构](../architecture/Architecture.md) |
| 模块系统 | [Phase1](phase1_architecture_optimization_plan.md)、[Phase1.1 清理](phase1_1_legacy_cleanup_plan.md) | [模块系统](../architecture/ModuleSystem/ModuleSystem.md) |
| Runtime foundation | [Scene/Gameplay 计划](runtime_scene_gameplay_foundation_plan.md)、[Phase1.2 收口](phase1_2_runtime_gameplay_closeout_plan.md) | [整体架构](../architecture/Architecture.md) |
| Vulkan 迁移方案 | [ARKRenderer 参考方案](Renderer/ark_renderer_integration_plan.md)、[接口设计](Renderer/renderer_interface_redesign_plan.md)、[依赖/CMake](Renderer/renderer_dependency_cmake_plan.md) | [渲染架构](../architecture/RendererSystem/Renderer.md)、[构建说明](../development/build_and_test.md) |
| Vulkan 接入 | [开发路线](Renderer/renderer_vulkan_development_roadmap.md)、[接入后工作](Renderer/renderer_post_vulkan_integration_plan.md)、[目录重组](Renderer/renderer_module_reorganization_plan.md) | [渲染架构](../architecture/RendererSystem/Renderer.md) |
| 画面效果迭代 | [Visual Effects 计划](Renderer/renderer_visual_effects_development_plan.md) | [渲染架构](../architecture/RendererSystem/Renderer.md) |
| Renderer Rebuild | [RR 实施方案](Renderer/renderer_rebuild.md)、[RR-00/02 历史基线](Renderer/renderer_rebuild_baseline.md) | [当前同步与生命周期](../architecture/RendererSystem/Renderer.md) |
| 模型导入 | [glTF runtime import](Loader/gltf_runtime_import_development_plan.md) | [资产清单](../../assets/asset_manifest.md)及当前 Resource/Import 源码 |
| 编辑器 UI | [UI Polish 计划](Editor/editor_ui_polish_plan.md) | [编辑器架构](../architecture/EditorSystem/Editor.md) |

历史分类不表示文档里的所有候选功能都已实现。例如旧 Visual Effects 计划包含后来换方案或仍未开展的效果，旧 Editor 计划中的工具栏按钮也不能证明 Play Mode 已完成。

## 更新约定

- 进行中的专项在首页标明已完成范围和下一工作包，工作包小节标明状态。
- 实施记录保留日期、验证范围和已知限制；不把一次 GPU smoke 扩写成所有场景验证通过。
- 新计划加入此索引，完成或被替代后标为历史；当前架构同步到 architecture 文档。
- 历史方案保留原有路径以便追溯。新开发不继续更新已经退役的总体路线作为进度主文档。

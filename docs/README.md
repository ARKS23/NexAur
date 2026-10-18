# NexAur 文档导航

最后核对：2026-10-18。当前实现依据文档整理前的提交 `695e1d1`；未提交的实验改动不计入已完成能力。

## 阅读顺序

1. [项目简介](../README.md)：项目定位和能力概览。
2. [构建、运行与测试](development/build_and_test.md)：环境、预设、产物和验证入口。
3. [整体架构](architecture/Architecture.md)：模块边界、启动和帧数据流。
4. [模块系统](architecture/ModuleSystem/ModuleSystem.md)：生命周期、Service 与新增模块方式。
5. [渲染架构](architecture/RendererSystem/Renderer.md)和[编辑器架构](architecture/EditorSystem/Editor.md)：领域职责和当前限制。
6. [开发计划索引](plans/README.md)：进行中的工作、后续候选和历史记录。

## 专题文档

| 主题 | 入口 |
| --- | --- |
| 输入快照与动作映射 | [输入系统](architecture/InputSystem/Input.md) |
| 同步事件分发与 UI 捕获 | [事件系统](architecture/EventSystem/EventSystem.md) |
| 日志与编辑器 Console | [日志系统](architecture/LogSystem/Log.md) |
| 代码约定 | [编码规范](CodingGuideLines/CodingGuideLines.md) |
| Shader、纹理与可选模型 | [资产清单](../assets/asset_manifest.md) |
| 可选硬件光追 | [Ray Tracing 总计划](plans/Renderer/RayTracing/ray_tracing_development_plan.md) |
| 当前反射工作包 | [RT-10 反射专项](plans/Renderer/RayTracing/ray_traced_reflection_foundation_plan.md) |

## 当前开发状态

- 构建主线为 C++20、CMake、vcpkg 和 Vulkan 1.3；旧 OpenGL 后端已退役。
- 模块系统、Runtime/Gameplay foundation、glTF 导入、编辑器基础和 Renderer Rebuild 已有落地实现。
- 光追 RT-00～RT-09 已完成，包含可选能力协商、BLAS/TLAS、同步、方向光阴影、资源生命周期和 RTAO。
- RT-10.0 GPU Scene Tables、RT-10.1 Surface/Compute/History、RT-10.2 Trace/Hit Shading 已实现。
- 下一工作包为 RT-10.3 时空降噪；RT-10.4 最终合成尚未完成。当前 raw RT 结果用于调试视图，默认关闭 RT 反射。
- SceneView/GameView 已区分相机；完整 Play Mode、持久化独立材质资产和完整 RT Pipeline/SBT 仍属于后续工作。

## 文档维护规则

- `architecture/` 描述已实现行为；新增功能同时更新对应入口和边界说明。
- `development/` 提供可在当前构建上使用的命令。测试名称以 CMake 注册为准。
- `plans/` 保存设计与实施记录。首页状态、工作包状态和末尾实施记录应同步更新；目标流程不等于当前实现。
- 标为历史的文档保留当时的 API、路径、测试命令和验证结果，不能作为当前操作手册。历史验证不代表本次重新执行过。
- 当前实现优先核对源码，再参考专项实施记录；旧文档中的“未来”“当前”和分支名只适用于其编写阶段。
- 文档内使用相对链接，不写开发机绝对路径；更改文件位置时更新入口。

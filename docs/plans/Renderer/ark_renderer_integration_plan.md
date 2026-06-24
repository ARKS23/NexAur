# NexAur 渲染模块重构与 ARKRenderer 接入总方案

日期：2026-06-24

进度主文档：

```text
docs/plans/Renderer/renderer_vulkan_development_roadmap.md
```

本文是总体架构参考文档；后续开发进度、阶段状态和完成记录以进度主文档为准。

## 1. 文档目标

本文档记录 NexAur 渲染模块从当前 OpenGL 实现重构到新 Vulkan 渲染器 `ARKRenderer` 的总体路线。

相关专项文档：

- `renderer_interface_redesign_plan.md`：渲染模块对外接口重构方案。
- `renderer_dependency_cmake_plan.md`：vcpkg 包管理和 CMake 构建集成方案。

本文只保留总体判断、模块边界、阶段拆分和里程碑，具体接口与构建细节放到专项文档中维护。

## 2. 核心判断

当前 NexAur 渲染模块已经通过 Phase1 / Phase1.1 做了一轮模块化整理：

- `RendererService` 已经成为 Editor / Runtime 访问渲染能力的窄接口。
- `RenderDataPacket` 已经承担 Scene 到 Renderer 的跨模块帧数据传递。
- `AssetHandle` / `AssetMetadata` 已经开始把资产身份和 GPU 资源分离。
- `RenderPassContext` 已经让 OpenGL pass 不再直接访问全局上下文。
- `RenderDevice` / `OpenGLRenderDevice` 已经把一部分资源创建入口收口。

这些改动是正确的，但它们仍然服务于旧 OpenGL 渲染结构。继续把旧 OpenGL RHI 打磨成完整通用 RHI，价值不高。

更推荐的方向是：

```text
NexAur 保留引擎侧边界
  RendererService
  RenderDataPacket
  PlatformModule
  AssetManager / AssetHandle
  EditorModule

ARKRenderer 成为新的 Vulkan 渲染核心
  Renderer
  RenderScene
  RenderView
  renderer/resources/*
  Vulkan RHI

中间通过 adapter 连接
  VulkanRendererSystem
  VulkanRenderResourceCache
  VulkanRenderDataTranslator
```

### 2.1 现代引擎架构参考

这次重构可以参考 Godot / Unreal / Unity 的共同点，但不需要照搬它们的完整复杂度。

- Godot 的思路更接近“渲染服务 + 后端 driver”：上层通过 RenderingServer / RenderingDevice 风格的边界提交资源和绘制意图，Vulkan / OpenGL 等 API 细节留在后端内部。对应到 NexAur，就是上层只面向 `RendererService`、`RenderDataPacket` 和 `AssetHandle`，不直接持有 Vulkan image、buffer、descriptor 或 OpenGL texture id。
- Unreal 的思路强调模块边界和渲染线程数据隔离：Game / Editor 模块不直接操作 RHI resource，而是把场景代理、材质、mesh 等数据交给 Renderer 模块。对应到 NexAur，就是 `RendererModule` 拥有 `VulkanRendererSystem`、`VulkanRenderResourceCache`、pass、target 和 GPU resource 生命周期，Scene / Editor 只提交后端无关帧数据。
- Unity 的思路强调渲染管线接管 GPU 表达：Scene / Component 层描述对象和资产，SRP / render pipeline 决定如何转换成 draw list、pass 和 render target。对应到 NexAur，就是 `RenderDataPacket -> RenderSceneFrame -> VulkanDrawList -> VulkanForwardPass` 的内部链路，避免把 draw call 细节散落在 Editor 或 Scene 中。

因此 NexAur 的目标不是做一个过早通用化的双后端 RHI，而是形成清爽的三层：

```text
Engine-facing contract
  RendererService / RenderDataPacket / AssetHandle / ViewportOutput

Renderer module orchestration
  RendererModule / VulkanRendererSystem / translators / resource cache

Backend internal implementation
  Vulkan device / swapchain / resources / passes / targets / ImGui renderer
```

这套结构的判断标准很简单：删除 OpenGL 后，上层模块不应该需要大面积改动；未来接入新物理、音频或脚本模块时，也应该沿用同样的“模块服务 + 私有实现”边界。

## 3. 为什么不继续扩展旧 OpenGL 渲染模块

当前旧渲染模块的问题不是缺少一个 Vulkan backend 类，而是整体结构仍带有 OpenGL 时代的隐式假设：

- `RendererSystem` 同时负责 device、viewport framebuffer、forward pipeline、资源解析、IBL、picking 和事件处理。
- `RenderResourceCache` 缓存的是 OpenGL GPU 对象，例如 `Texture2D`、`TextureCubeMap`、`VertexArray`、`RenderModelData`。
- `RendererService::getViewportColorAttachment()` 暴露的是 OpenGL texture id。
- `RendererFactory` 和 `RendererCommand` 仍然带有静态 OpenGL 风格。
- Editor viewport、picking、gizmo 显示路径都默认自己拿到的是 OpenGL texture。

如果把 Vulkan 塞进这套接口，最终会得到一个很别扭的双后端系统：

```text
OpenGL 模型强行抽象成 RHI
Vulkan 被迫模拟 OpenGL framebuffer / texture id / binding slot
Editor 继续依赖 OpenGL 语义
资源缓存需要同时理解两套 GPU 对象
```

这会让代码更复杂，而不是更清爽。

因此推荐：

```text
旧 OpenGL 后端只保留为过渡 fallback
不继续大规模增强旧 OpenGL RHI
新的主线渲染模块围绕 ARKRenderer adapter 重建
Vulkan 跑通后逐步退役 OpenGL
```

## 4. ARKRenderer 接入边界

NexAur 应优先依赖 ARKRenderer 的 public facade：

```text
externalRenderer/src/renderer/Renderer.h
externalRenderer/src/renderer/RenderScene.h
externalRenderer/src/renderer/RenderView.h
externalRenderer/src/renderer/resources/*
```

NexAur 不应该直接依赖：

```text
externalRenderer/src/renderer/core/*
externalRenderer/src/renderer/passes/*
externalRenderer/src/renderer/effects/*
externalRenderer/src/rhi/vulkan/*
```

更理想的边界是：这些 ARKRenderer 头文件只出现在 `VulkanRendererSystem`、`VulkanRenderResourceCache` 和 `VulkanRenderDataTranslator` 的实现中。NexAur Editor、Scene、Resource、Gameplay 仍然只看 `RendererService` 和 `RenderDataPacket`。

## 5. 目标架构

### 5.1 模块关系

```text
PlatformModule
  owns WindowSystem / InputSystem
  exposes WindowService / InputService

ResourceModule
  owns AssetManager
  exposes AssetService later

RuntimeModule / EditorModule
  produce RenderDataPacket

RendererModule
  owns RendererService implementation
  chooses active renderer backend during transition
```

### 5.2 渲染模块内部

过渡期：

```text
RendererModule
  OpenGLLegacyRendererSystem
    old OpenGL renderer path

  VulkanRendererSystem
    owns ark::Renderer
    owns VulkanRenderResourceCache
    owns VulkanRenderDataTranslator
```

目标期：

```text
RendererModule
  VulkanRendererSystem
    the only runtime renderer implementation

  Legacy OpenGL code removed or isolated outside default build
```

### 5.3 一帧数据流

```text
Scene / EditorCamera / GameCamera
  -> RenderContext write buffer
  -> RenderDataPacket
  -> RendererService::render(ts, render_data)
  -> VulkanRendererSystem::render()
      -> VulkanRenderDataTranslator builds RenderView
      -> VulkanRenderResourceCache resolves AssetHandle
      -> VulkanRenderDataTranslator builds RenderScene
      -> ark::Renderer::render(scene, view)
```

### 5.4 资源流

```text
AssetHandle
  -> AssetManager owns asset identity / metadata / path
  -> VulkanRenderResourceCache owns renderer GPU resources
  -> ark::ModelResource / TextureResource / EnvironmentResource
```

短期可以让 Vulkan 路径直接复用 ARKRenderer 的 glTF / texture loader，优先跑通渲染闭环。长期再把 NexAur AssetManager 和 ARKRenderer asset loader 整合成统一 CPU asset contract。

## 6. 新增模块建议

推荐新增目录：

```text
source/Engine/Function/RendererV2/
  vulkan_renderer_system.h
  vulkan_renderer_system.cpp
  vulkan_render_resource_cache.h
  vulkan_render_resource_cache.cpp
  vulkan_render_data_translator.h
  vulkan_render_data_translator.cpp
```

### 6.1 VulkanRendererSystem

职责：

- 实现 NexAur `RendererService`。
- 创建 / 销毁 `ark::Renderer`。
- 从 `WindowService` 获取 native window。
- 管理 viewport 尺寸。
- 持有 `VulkanRenderResourceCache`。
- 调用 `VulkanRenderDataTranslator`。
- 提供 viewport output / picking 的 Vulkan 过渡实现。

### 6.2 VulkanRenderResourceCache

职责：

- `AssetHandle -> ark::ModelResource`。
- `AssetHandle -> ark::EnvironmentResource`。
- 后续支持 `AssetHandle -> ark::TextureResource`。
- 管理 renderer resource 生命周期。
- 隐藏 ARKRenderer resource contract，不让 Editor / Scene 直接持有。

### 6.3 VulkanRenderDataTranslator

职责：

```text
NexAur::RenderDataPacket
  -> ark::RenderView
  -> ark::RenderScene
```

转换集中在这里，避免 `VulkanRendererSystem::render()` 膨胀。

## 7. 关键专项问题

### 7.1 接口设计

旧接口中的 OpenGL texture id、readPixel 语义需要拆掉。新的 `RendererService` 应该暴露后端无关的 viewport output、picking request/result、backend type 和基础 frame stats。

详细方案见：

```text
docs/plans/Renderer/renderer_interface_redesign_plan.md
```

### 7.2 包管理与 CMake

NexAur 当前使用 `external/` vendored/submodule 依赖，ARKRenderer 使用 vcpkg manifest。长期不建议混用两套依赖系统。

推荐最终统一为 NexAur 顶层 vcpkg manifest，并逐步停止构建 `external/` 中与 vcpkg 重复的第三方库。

详细方案见：

```text
docs/plans/Renderer/renderer_dependency_cmake_plan.md
```

### 7.3 Window 创建策略

Vulkan 后端需要 GLFW no-api window，旧 OpenGL 后端需要 OpenGL context。后端选择必须早于窗口创建。

建议让 `WindowSpecification` 增加 graphics API / client API 描述：

```text
OpenGL legacy -> GLFW creates OpenGL context
Vulkan -> GLFW_CLIENT_API = GLFW_NO_API
```

### 7.4 Editor viewport

OpenGL 旧路径可以直接把 framebuffer color attachment texture id 交给 ImGui。

Vulkan 新路径不能沿用这个接口。建议阶段推进：

```text
早期：Vulkan 直接渲染到 swapchain，Editor viewport 显示 placeholder / backend status
中期：ARKRenderer 支持 offscreen viewport output
后期：ViewportPanel 使用 Vulkan ImGui descriptor 显示 image
```

### 7.5 Picking

旧 OpenGL picking 基于 RED_INTEGER framebuffer attachment。

Vulkan 路径后续需要 ARKRenderer 增加 ObjectId / Picking pass，支持：

```text
entity id -> integer target -> readback -> RendererService::pickEntity()
```

第一版 Vulkan backend 可以返回空结果，Editor 做 graceful fallback。

## 8. 分阶段实施计划

### PR-R1：文档拆分与方向确认

目标：

- 主方案、接口方案、CMake/vcpkg 方案拆分完成。
- 明确旧 OpenGL 不再作为长期双后端主线。

验收：

- 三份文档完成。
- 后续 PR 边界清楚。

### PR-R2：RendererService 接口重构

目标：

- 移除或隔离 OpenGL texture id 风格的接口。
- 新增后端无关 viewport output / picking contract。

参考：

```text
renderer_interface_redesign_plan.md
```

验收：

- OpenGL legacy backend 仍能通过兼容 adapter 工作。
- Editor 不直接假设 viewport output 是 OpenGL texture id。

### PR-R3：顶层 vcpkg / CMake 预研落地

目标：

- NexAur 能以统一依赖策略构建 ARKRenderer。

参考：

```text
renderer_dependency_cmake_plan.md
```

验收：

- NexAur CMake 能提供 RendererV2 后续需要的 Vulkan / shader / asset 相关依赖。
- 不重复构建 glfw / glm / spdlog / imgui 等依赖。

### PR-R4：WindowSystem graphics API 策略

目标：

- 根据 renderer backend 创建 OpenGL context 或 Vulkan no-api window。

验收：

- OpenGL legacy 仍能启动。
- Vulkan path 能拿到 GLFW native window 并创建 surface。

### PR-R5：VulkanRendererSystem 骨架

目标：

- 新增实现 `RendererService` 的 Vulkan adapter。

任务：

- 创建 `ark::Renderer`。
- 实现初始化、resize、shutdown。
- `render()` 先提交空场景或 fallback scene。
- picking 暂时返回空结果。

验收：

- Vulkan backend 可以启动和关闭。
- Vulkan instance / device / swapchain 创建成功。

### PR-R6：RenderDataPacket -> RenderView

目标：

- NexAur 相机数据驱动 ARKRenderer view。

任务：

- 实现 `VulkanRenderDataTranslator::buildRenderView()`。
- 核对 RH/LH、depth range、Vulkan Y flip。

验收：

- 相机移动方向和投影正确。

### PR-R7：VulkanRenderResourceCache 第一版

目标：

- 通过 `AssetHandle` 创建并缓存 ARK model resource。

验收：

- 同一模型资产不会重复创建 GPU resource。
- 可以释放或清空缓存。

### PR-R8：RenderDataPacket -> RenderScene

目标：

- NexAur scene objects 能提交给 ARKRenderer。

任务：

- opaque / transparent objects 转为 `ark::RenderScene::addModel()`。
- directional light / environment intensity 转为 ARK scene data。

验收：

- Vulkan 后端能显示 NexAur 场景模型。

### PR-R9：Editor viewport 过渡

目标：

- Vulkan backend 下 Editor 不崩溃，并有明确显示策略。

验收：

- viewport panel 对空输出有 graceful fallback。
- 如果尚未支持 Vulkan viewport image，UI 显示明确提示。

### PR-R10：Picking / ObjectId Pass

目标：

- 恢复 Vulkan backend 下 entity picking。

验收：

- `RendererService::pickEntity()` 可返回实体 ID。

### PR-R11：OpenGL legacy 隔离与退役

目标：

- Vulkan backend 覆盖核心能力后，旧 OpenGL 代码移入 legacy 或删除。

验收：

- 默认 renderer backend 为 Vulkan。
- OpenGL-only 类型不再出现在 Scene / Resource / Editor 公开接口。

## 9. 推荐优先级

近期优先：

1. 文档拆分。
2. `RendererService` 接口清理。
3. 顶层 vcpkg / CMake 方案。
4. Window no-api 支持。
5. VulkanRendererSystem 骨架。

中期优先：

1. RenderView 转换。
2. VulkanRenderResourceCache。
3. RenderScene 转换。
4. Editor viewport fallback。

后期优先：

1. Vulkan viewport 嵌入 Editor。
2. Vulkan picking。
3. OpenGL legacy 退役。
4. 统一 AssetManager 和 ARKRenderer asset loader。

## 10. 当前推荐结论

推荐主线：

```text
不要继续扩展旧 OpenGL 渲染模块
  -> 先重构 RendererService 为干净的后端无关接口
  -> 使用 ARKRenderer 作为新的 Vulkan 渲染核心
  -> NexAur 只保留 RenderDataPacket / AssetHandle / Editor / Runtime 边界
  -> 中间用 VulkanRendererSystem adapter 连接
  -> Vulkan 稳定后退役 OpenGL
```

简短总结：

```text
让 NexAur 继续做引擎，让 ARKRenderer 专心做渲染器，中间用清爽的 RendererService + adapter 连接。
```

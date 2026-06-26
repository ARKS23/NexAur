# Renderer Post Vulkan Integration Plan

日期：2026-06-26

## 1. 文档定位

本文档记录 NexAur 在 Vulkan 后端完成主线接入后的渲染模块后续工作。

当前结论：

```text
Vulkan backend 已经作为主线后端接入完成。
当前 renderer 已具备最小可运行 Vulkan 渲染链路。
后续重点从“接入后端”转向“补齐游戏渲染能力”和“继续保持模块边界清爽”。
```

相关文档：

- `renderer_vulkan_development_roadmap.md`：Vulkan 接入阶段进度记录。
- `renderer_module_reorganization_plan.md`：Renderer 目录和模块边界收口方案。
- `renderer_interface_redesign_plan.md`：RendererService 和 viewport / picking 契约。
- `engine_architecture_vulkan_demo_roadmap.md`：引擎架构、Vulkan 接入和小游戏 demo 总路线。

## 2. 当前已完成能力

当前 Vulkan renderer 已具备：

- 默认构建路径收口为 vcpkg + Vulkan。
- OpenGL legacy 已从主线退役。
- `RendererService` 作为 Editor / Runtime 的后端无关入口。
- `RenderDataPacket -> RenderView -> RenderSceneFrame -> VulkanDrawList -> VulkanForwardPass` 数据链路。
- Vulkan instance / surface / device / swapchain / dynamic rendering baseline。
- Vulkan model resource cache 和基础 mesh buffer 上传。
- Editor viewport Vulkan offscreen image 显示。
- Vulkan ObjectId picking。
- Renderer 目录已收口为：

```text
Renderer/
  data/
  frontend/
  Vulkan/
```

这些说明 Vulkan 后端已经“接入完成”，但它还不是完整游戏渲染器。

## 3. 后续设计原则

### 3.1 不回退到旧式全局渲染器

后续新增功能必须继续走清晰数据流：

```text
Scene / Editor / Runtime
  -> RenderDataPacket
  -> RenderView
  -> RenderSceneFrame
  -> VulkanDrawList
  -> Vulkan passes / graph
```

不要让 Scene / Editor / Resource 直接 include Vulkan backend 类型。

### 3.2 不急着做庞大通用 RHI

当前只有 Vulkan 后端，先把 Vulkan backend 做顺。第一版 RenderGraph / PassGraph 建议 Vulkan-only。等真的出现第二后端需求，再从稳定 Vulkan 实现中提炼通用 RHI。

### 3.3 Resource 管 CPU 资产，Renderer 管 GPU 资源

`AssetManager` 负责资产身份、路径、metadata 和 CPU asset。  
`VulkanRenderResourceCache` 负责 GPU buffer / image / descriptor / material resource。

### 3.4 HLSL 是 Vulkan 主线 shader 语言

后续新增 Vulkan shader 统一使用 HLSL，并通过 DXC 编译为 SPIR-V。不要重新引入 GLSL / OpenGL shader 作为主线。

## 4. 第一优先级：架构收尾

### PR-R13：Camera 所有权收口

执行状态：已完成。

目标：

- `EditorCamera` 归 Editor，只服务编辑器 SceneView 导航。
- `CameraComponent` 归 Scene / Runtime，后续服务 GameView / Play Mode。
- Renderer 不持有完整 camera 行为，只消费 `RenderDataPacket` / `RenderView` 中已经提取好的相机数据。

完成记录：

- `EditorCamera` 已移动到 `source/Engine/Editor/Camera`。
- `Function/Renderer/camera.h` 空基类已移除，不再保留跨模块 camera 抽象。
- `EditorLayer`、`ViewportPanel`、CMake include/source list 已更新。
- Renderer 目录不再保存 editor camera 行为类。
- `EditorCamera` 中未使用的 pan / calculate 占位方法已删除，避免继续把 controller 行为堆在相机数据类里。

后续边界：

- PR-R13 只做所有权收口，不实现完整 runtime camera controller。
- `EditorLayer` 当前仍负责把 SceneView camera 写入 `RenderDataPacket`，这是编辑器预览覆盖逻辑。
- PR-R14 再拆分 SceneView / GameView 语义，让 Runtime 可以稳定选择 active `CameraComponent`。

验收：

- `Renderer` 目录不再保存 EditorCamera 行为类。
- Editor viewport 相机移动不受影响。
- Scene / Runtime 可独立提供 active CameraComponent。
- `rg "Function/Renderer/editor_camera|Function/Renderer/camera" source/Engine` 无命中。
- `rg "EditorCamera" source/Engine/Function/Renderer` 无命中。

### PR-R14：SceneView / GameView 语义拆分

目标：

- Editor SceneView 使用 `EditorCamera`。
- Runtime GameView 使用场景 active `CameraComponent`。
- Renderer 不关心 view 来源，只消费最终 `RenderDataPacket`。

验收：

- Editor 下仍可自由观察场景。
- Runtime 下可从场景 camera 渲染。
- 后续 Play Mode 不需要改 RendererService。

## 5. 第二优先级：材质与纹理最小链路

### PR-R15：Texture asset 和 VulkanTextureResource

目标：

- Resource 层能表达 texture asset。
- Vulkan backend 能创建 `VulkanTextureResource`。
- 支持 base color texture 最小采样。

建议结构：

```text
Resource/
  texture_asset.h
  texture_importer.h

Renderer/Vulkan/resources/
  vulkan_texture_resource.h
  vulkan_texture_resource.cpp
```

第一版只做：

- 2D texture。
- SRGB / linear 基础区分。
- sampler 默认配置。
- CPU image -> staging buffer -> Vulkan image。

暂时不做：

- bindless。
- texture streaming。
- virtual texture。
- compressed texture 全格式支持。

### PR-R16：Material asset 和 VulkanMaterialResource

目标：

- Resource 层能表达最小材质。
- Vulkan backend 能把 material asset 转为 descriptor 可用的 GPU resource。

第一版材质字段：

```text
base_color_factor
base_color_texture
metallic_factor
roughness_factor
alpha_mode
```

验收：

- 模型可使用不同 base color。
- 模型可采样 base color texture。
- Scene / Editor 不直接操作 descriptor set。

## 6. 第三优先级：Shader / Pipeline / Descriptor 管理

### PR-R17：ShaderLibrary 和 PipelineCache

目标：

- 不让 pass 直接散落 shader path 和 pipeline creation 细节。
- 建立 Vulkan backend 内部 shader / pipeline 管理入口。

建议结构：

```text
Renderer/Vulkan/shaders/
  vulkan_shader_library.h
  vulkan_shader_library.cpp

Renderer/Vulkan/pipelines/
  vulkan_pipeline_cache.h
  vulkan_pipeline_cache.cpp
```

第一版只需要：

- 读取已编译 SPIR-V。
- 管理 shader module 生命周期。
- 管理 forward / object id pipeline。
- pipeline rebuild 时释放旧资源。

### PR-R18：DescriptorAllocator / DescriptorLayoutCache

目标：

- 把 descriptor pool / layout / set 分配从 pass 中收口。
- 为材质、纹理、camera uniform、light uniform 做准备。

第一版只需要支持：

- frame global descriptor。
- material descriptor。
- ImGui / viewport texture descriptor 保持在 Vulkan UI bridge 内。

## 7. 第四优先级：轻量 Vulkan PassGraph

### PR-R19：Vulkan-only PassGraph 骨架

目标：

让 `VulkanRendererSystem` 从巨型执行类逐步变成 frame orchestrator。

推荐数据流：

```text
prepare frame data
prepare resources
build draw list
build graph
execute graph
present
```

第一版 graph 只解决：

- pass 顺序。
- pass 输入输出声明。
- color / depth / object id target 管理。
- image layout transition。
- Vulkan 1.3 dynamic rendering begin/end。

暂时不做：

- async compute。
- multi-queue scheduling。
- resource aliasing。
- 通用跨后端 RenderGraph。

## 8. 第五优先级：基础视觉能力

### PR-R20：Lighting baseline

目标：

- 稳定 directional light。
- 支持多个 point lights 的基础 forward lighting。
- light data 进入 uniform / storage buffer，而不是硬编码 push constant。

验收：

- Scene 中灯光组件变化能影响画面。
- Forward pass 不再只依赖固定颜色。

### PR-R21：Skybox / Environment

目标：

- 支持 environment asset。
- 支持简单 skybox 或 environment color。
- 为 IBL 预留资源结构。

第一版可以只做：

- cubemap 或纯色环境。
- 不做完整 prefilter / BRDF LUT。

### PR-R22：Shadow 第一版

目标：

- directional light shadow map。
- shadow pass 写 depth。
- forward pass 采样 shadow map。

暂时不做：

- cascaded shadow maps。
- point light shadow cube map。
- contact shadow / screen-space shadow。

## 9. 第六优先级：Editor 和 Debug 工具

### PR-R23：Debug draw

目标：

- 线段。
- AABB。
- camera frustum。
- light gizmo 辅助显示。

Debug draw 对 demo 和后续物理调试很重要，建议在完整后处理之前做。

### PR-R24：Renderer debug panel

目标：

- 显示 backend、swapchain、viewport target、frame time。
- 显示 resource cache 数量。
- 显示当前 RenderView / viewport size。

第一版只做只读信息，不做复杂 profiling。

## 10. 推荐执行顺序

建议下一阶段按下面顺序推进：

```text
PR-R13 Camera 所有权收口
PR-R14 SceneView / GameView 语义拆分
PR-R15 Texture asset + VulkanTextureResource
PR-R16 Material asset + VulkanMaterialResource
PR-R17 ShaderLibrary + PipelineCache
PR-R18 Descriptor 管理收口
PR-R19 Vulkan-only PassGraph
PR-R20 Lighting baseline
PR-R21 Skybox / Environment
PR-R22 Shadow 第一版
PR-R23 Debug draw
PR-R24 Renderer debug panel
```

如果目标是尽快进入小游戏 demo，建议先做：

```text
PR-R13
PR-R14
PR-R15
PR-R16
PR-R20
```

也就是先保证 runtime camera、材质纹理和基础灯光可用。RenderGraph、阴影和 debug panel 可以稍后做。

## 11. 完成标准

进入小游戏 demo 前，Renderer 至少应满足：

- Runtime camera 可以独立于 EditorCamera 工作。
- MeshRenderer 可以引用 model + material。
- Material 可以显示 base color 和 base color texture。
- Directional light / point light 能影响画面。
- Editor viewport 和 runtime view 都能稳定显示。
- Vulkan backend 不向 Scene / Editor / Resource 暴露 Vulkan 类型。
- 构建、shader 编译、Sandbox smoke 稳定通过。

达到这些后，Renderer 就足够支撑第一版小游戏 demo。后续再逐步补 RenderGraph、shadow、post process 和更完整的 PBR。

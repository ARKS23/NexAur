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

执行状态：已完成。

背景：

当前每帧数据链路是：

```text
SceneV2::extractSceneData()
  -> 从 CameraComponent 写入 RenderDataPacket
EditorLayer::syncViewportCameraToRenderPacket()
  -> 用 EditorCamera 覆盖 RenderDataPacket
Renderer
  -> 消费最终 RenderDataPacket
```

这个流程对编辑器 SceneView 是正确的，但对后续 GameView / Play Mode 不够清晰：运行时相机明明属于场景，却可能在编辑器路径下被 `EditorCamera` 覆盖。PR-R14 的目标不是改 Renderer，而是把“编辑器观察视角”和“游戏运行视角”显式分开。

目标：

- Editor SceneView 使用 `EditorCamera`。
- Runtime GameView 使用场景 active `CameraComponent`。
- Renderer 不关心 view 来源，只消费最终 `RenderDataPacket`。

完成记录：

- 新增 `EditorViewportViewMode`，当前支持 `SceneView` / `GameView`。
- `ViewportPanel` 增加轻量 Scene / Game 切换。
- `SceneView` 下继续使用 `EditorCamera`，并保留 gizmo / picking / editor camera event。
- `GameView` 下不再用 `EditorCamera` 覆盖 `RenderDataPacket`，保留 `SceneV2` 提取出的 runtime camera。
- 新增 `ActiveCameraComponent`，默认场景 `MainCamera` 会被标记为 active camera。
- `SceneV2::extractSceneData()` 优先导出 active `CameraComponent`，找不到时 fallback 到第一个 `CameraComponent`。
- RendererService 和 Vulkan backend 接口未改动。

语义定义：

```text
SceneView
  - 编辑器观察场景的视图。
  - 使用 EditorCamera。
  - 支持 WASD / 鼠标观察、gizmo、picking、scene hierarchy selection。
  - 不代表游戏真实镜头。

GameView
  - 游戏运行视图。
  - 使用场景中的 active CameraComponent。
  - 不被 EditorCamera 覆盖。
  - 后续 Play Mode 和小游戏 demo 都以它作为运行时画面来源。
```

推荐拆分：

1. 增加 viewport view mode。
   - 在 Editor 侧新增轻量枚举，例如 `EditorViewportViewMode { SceneView, GameView }`。
   - 默认保持 `SceneView`，避免改变当前编辑器交互。
   - 第一版可以只放在 `EditorContext` 或 `ViewportPanel` 状态里，不急着做复杂 UI。

2. 给场景相机 active 语义。
   - 推荐新增轻量 marker，例如 `ActiveCameraComponent`。
   - 也可以短期在 `CameraComponent` 中加入 `bool primary`，但 marker 更符合 ECS 语义。
   - `scene_factory.cpp` 创建默认相机时同步标记 active camera。

3. 收口 `SceneV2::extractSceneData()` 的相机选择。
   - 优先导出 active `CameraComponent`。
   - 找不到 active camera 时，可以 fallback 到第一个 `CameraComponent` 或默认相机数据。
   - 不再把“第一个 CameraComponent”当成长期主相机语义。

4. 修改 EditorLayer 的相机覆盖策略。
   - `SceneView`：继续用 `EditorCamera` 覆盖 `RenderDataPacket`。
   - `GameView`：不覆盖，保留 `SceneV2` 提取出的 runtime camera。
   - `EditorCamera` 仍可继续更新自己的状态，但不能影响 GameView。

5. 保持 Renderer 边界不变。
   - 不新增 `RendererService` 接口。
   - 不让 Renderer 知道 SceneView / GameView。
   - Renderer 只消费最终 `RenderDataPacket -> RenderView`。

暂不做：

- 不实现完整 Play Mode 生命周期。
- 不实现 runtime camera controller。
- 不做多 viewport / 多 camera editor UI。
- 不改 Renderer 后端接口。

验收：

- Editor 下仍可自由观察场景。
- Runtime 下可从场景 camera 渲染。
- GameView 不会被 `EditorCamera` 覆盖。
- SceneView / GameView 切换不需要改 RendererService。
- 后续 Play Mode 不需要改 RendererService。

## 5. 第二优先级：材质与纹理最小链路

### PR-R15：Texture asset 和 VulkanTextureResource

执行状态：已完成。

目标：

- Resource 层能表达 texture asset。
- Vulkan backend 能创建 `VulkanTextureResource`。
- 为 PR-R16 的 base color texture 采样准备稳定 CPU / GPU 资源链路。

当前状态：

- `AssetManager` 已有 `importTextureAsset()` / `loadTextureAsset()`，但当前只登记路径和 `AssetHandle`。
- Resource 层还没有 `TextureAsset` / CPU image 数据对象。
- Vulkan backend 还没有 `VulkanTextureResource`。
- `VulkanMaterialResource` 目前只保存 albedo / normal / metallic / roughness / ao 的路径字符串。
- Forward pass 还没有材质 descriptor / texture sampling 完整链路。

完成记录：

- 新增 `TextureColorSpace` / `TexturePixelFormat`，固定第一版 texture import 的基础格式语义。
- 新增 `TextureAsset`，保存 CPU 侧 2D RGBA8 texture 数据、尺寸、格式、color space 和 source path。
- 新增 `TextureLoader`，通过 stb_image 加载普通 2D texture，并可创建 1x1 solid color runtime texture。
- `AssetManager` 增加 `loadTextureCPU()` 和 CPU texture cache；`importTextureAsset()` 继续只登记资产身份。
- `AssetMetadata` 记录 texture color space。
- 新增 `VulkanTextureResource`，负责 CPU image -> staging buffer -> Vulkan image / image view / sampler。
- `VulkanRenderResourceCache` 增加 texture cache 和 `getOrCreateTexture()`。
- `VulkanRenderResourceCache` 初始化时创建 fallback white texture。
- CMake 已加入 Resource texture 文件和 Vulkan texture resource 文件。
- Forward shader / material descriptor 采样链路仍保留给 PR-R16。

建议结构：

```text
Resource/
  texture_asset.h
  texture_asset.cpp
  texture_loader.h
  texture_loader.cpp

Renderer/Vulkan/resources/
  vulkan_texture_resource.h
  vulkan_texture_resource.cpp
```

推荐拆分：

1. Resource 层新增 CPU texture 表达。
   - 新增 `TextureAsset`，保存 width / height / channels / format / color space / source path / pixel data。
   - 第一版只支持 2D texture。
   - 用明确字段区分 sRGB 和 linear，避免后续材质采样时靠路径猜。

2. AssetManager 支持加载 CPU texture。
   - 新增 `std::shared_ptr<TextureAsset> loadTextureCPU(AssetHandle handle)`。
   - 新增 CPU texture cache，例如 `m_uuid_cpu_texture_cache`。
   - `importTextureAsset()` 继续只负责登记资产身份，真正读取磁盘放到 `loadTextureCPU()`。
   - 使用 stb_image 加载 png / jpg 等普通 2D 贴图。

3. Vulkan 后端新增 `VulkanTextureResource`。
   - 从 `TextureAsset` 创建 GPU texture。
   - 创建 staging buffer 并上传 pixel data。
   - 创建 `VkImage` / `VmaAllocation`。
   - 创建 `VkImageView`。
   - 创建默认 `VkSampler`。
   - 执行 layout transition：
     ```text
     UNDEFINED -> TRANSFER_DST_OPTIMAL
     copy buffer to image
     TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
     ```

4. `VulkanRenderResourceCache` 增加 texture cache。
   - 新增：
     ```cpp
     VulkanTextureResource* getOrCreateTexture(AssetHandle texture_asset, AssetManager& asset_manager);
     VulkanTextureResource* getTexture(AssetHandle texture_asset) const;
     ```
   - 内部缓存：
     ```cpp
     std::unordered_map<AssetHandle, std::unique_ptr<VulkanTextureResource>> m_texture_cache;
     ```
   - `clear()` / `shutdown()` 需要释放 texture cache。

5. 准备默认 fallback texture。
   - 建议创建 1x1 white texture。
   - 贴图路径为空或加载失败时，后续材质系统可以使用 fallback，避免 pass 到处判空。

第一版只做：

- 2D texture。
- SRGB / linear 基础区分。
- sampler 默认配置。
- CPU image -> staging buffer -> Vulkan image。
- `AssetHandle -> TextureAsset -> VulkanTextureResource` 缓存链路。
- fallback white texture。

暂时不做：

- 不接入完整材质 descriptor set。
- 不修改 forward shader 去采样材质贴图，采样链路放到 PR-R16。
- 不做 normal / metallic / roughness / ao 全套材质贴图绑定。
- bindless。
- mipmap 生成。
- texture streaming。
- virtual texture。
- compressed texture 全格式支持。
- cubemap / environment map。

与 PR-R16 的边界：

```text
PR-R15:
  texture 从磁盘加载到 CPU，再上传成 Vulkan GPU resource。

PR-R16:
  Material asset / VulkanMaterialResource 引用这些 texture，
  建立 descriptor layout / descriptor set / shader sampling 链路。
```

验收：

- `AssetManager::loadTextureCPU()` 可从已登记的 `Texture2D` asset 加载 CPU image。
- `VulkanRenderResourceCache::getOrCreateTexture()` 可创建并缓存 `VulkanTextureResource`。
- texture resource 能正确释放，不泄漏 Vulkan image / image view / sampler / allocation。
- 空路径或加载失败时有 fallback white texture。
- RendererService / Scene / Editor 不暴露 Vulkan texture 句柄。
- 构建和 Sandbox smoke 通过。

### PR-R16：Material asset 和 VulkanMaterialResource

目标：

- Resource 层能表达最小材质。
- Vulkan backend 能把 material asset 转为 descriptor 可用的 GPU resource。
- Forward shader 能使用材质 base color 和 base color texture，而不是继续输出调试法线色。
- Scene / Editor / Resource 仍只通过 `AssetHandle` 和渲染数据描述材质，不接触 Vulkan descriptor。

当前状态：

- `MaterialData` 仍在 `mesh.h` 中，主要服务模型导入阶段的贴图路径记录。
- `VulkanMaterialResource` 目前只保存 albedo / normal / metallic / roughness / ao 的路径字符串。
- `MeshRendererComponent` 和 `RenderObjectData` 当前只有 `model_asset`，还没有材质覆盖入口。
- `VulkanForwardPass` 还没有材质 descriptor set layout / descriptor pool / descriptor set 绑定。
- `vulkan_forward.hlsl` 目前输出调试色，没有采样材质贴图。

第一版材质字段：

```text
base_color_factor
base_color_texture
metallic_factor
roughness_factor
alpha_mode
alpha_cutoff
```

推荐结构：

```text
Resource/
  material_asset.h
  material_asset.cpp
  material_types.h        # 可选：MaterialAlphaMode / MaterialImportData 等轻量类型

Renderer/Vulkan/resources/
  vulkan_material_resource.h
  vulkan_material_resource.cpp
```

建议拆分：

1. Resource 层新增 CPU material 表达。
   - 新增 `MaterialAsset`，保存 debug name、base color factor、base color texture handle、metallic / roughness factor 和 alpha mode。
   - 新增 `MaterialAlphaMode`，第一版支持 `Opaque` / `Mask` / `Blend` 语义，但渲染实现可以先只完整支持 `Opaque`。
   - `MaterialAsset` 不保存 Vulkan 类型，也不保存 GPU resource 指针。

2. 收口导入材质描述。
   - 将当前 `mesh.h` 中的 `MaterialData` 移到 Resource 材质相关头文件，或重命名为 `MaterialImportData`。
   - `Mesh` 可以继续保存模型导入得到的材质描述，但不要让 `Mesh` 成为长期材质系统的所有者。
   - 模型导入阶段读取到的 base color texture 路径，应在后续资源解析阶段转为 `AssetHandle`。

3. `AssetManager` 支持 material asset。
   - `AssetType` 增加 `Material`。
   - 增加 `registerRuntimeMaterial()` / `loadMaterialCPU()`。
   - 对模型导入产生的材质，可以先注册为 runtime material，后续再扩展到 `.material` 序列化资产。
   - base color texture 路径通过 `importTextureAsset(path, TextureColorSpace::SRGB)` 转为 texture asset handle。

4. Vulkan 后端升级 `VulkanMaterialResource`。
   - 从 `MaterialAsset` 创建 GPU 侧材质资源。
   - 引用 `VulkanTextureResource`，空贴图或加载失败时使用 fallback white texture。
   - 创建材质常量 buffer，保存 base color factor、metallic、roughness、alpha cutoff 等数据。
   - 分配并更新材质 descriptor set。
   - 资源释放必须跟随 `VulkanRenderResourceCache::clear()` / `shutdown()`。

5. 材质 descriptor 最小链路。
   - 第一版可以先使用一个 material descriptor set：
     ```text
     binding 0: material constants uniform buffer
     binding 1: combined image sampler base color texture
     ```
   - `VulkanForwardPass` 的 pipeline layout 增加 material descriptor set layout。
   - draw item 有材质时绑定材质 descriptor；没有材质时绑定 fallback material。
   - Descriptor 管理暂时可以放在 `VulkanRenderResourceCache` 或 material resource 内，但接口要为 PR-R18 的 descriptor 管理收口预留迁移空间。

6. Forward shader 接入材质。
   - vertex shader 继续传递 texcoord。
   - pixel shader 采样 base color texture。
   - 第一版输出：
     ```text
     sampled_base_color * base_color_factor
     ```
   - 不在 PR-R16 内实现完整 PBR；directional / point light 留给 PR-R20。

7. 可选：材质覆盖入口。
   - 如果小游戏 demo 需要“同一个模型换不同材质”，可以给 `MeshRendererComponent` / `RenderObjectData` 增加可选 `material_asset`。
   - 第一版也可以只使用模型自带材质，避免过早处理 submesh material override。

暂时不做：

- 不做完整 PBR。
- 不做 normal / metallic / roughness / ao 全套贴图绑定。
- 不做 bindless material / bindless texture。
- 不做材质编辑器 UI。
- 不做完整透明排序和 blend pipeline。
- 不做 `.material` 文件序列化格式。
- 不做 RenderGraph 迁移。

验收：

- 模型可使用不同 base color。
- 模型可采样 base color texture。
- Scene / Editor 不直接操作 descriptor set。
- 空材质或贴图加载失败时使用 fallback material / fallback white texture，不崩溃。
- `RendererService`、Scene、Editor、Resource 不暴露 `VkDescriptorSet` / `VkImageView` / `VkSampler`。
- 构建和 Sandbox smoke 通过。

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

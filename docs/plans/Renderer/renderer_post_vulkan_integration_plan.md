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

执行状态：已完成。

改造前状态：

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

执行状态：已完成。

目标：

- Resource 层能表达最小材质。
- Vulkan backend 能把 material asset 转为 descriptor 可用的 GPU resource。
- Forward shader 能使用材质 base color 和 base color texture，而不是继续输出调试法线色。
- Scene / Editor / Resource 仍只通过 `AssetHandle` 和渲染数据描述材质，不接触 Vulkan descriptor。

完成记录：

- 新增 `MaterialAlphaMode` / `MaterialImportData`，将旧 `MaterialData` 从 `mesh.h` 收口为 Resource 层导入材质描述。
- 新增 `MaterialAsset`，保存 base color factor、base color texture handle、metallic / roughness factor 和 alpha mode。
- `AssetManager` 增加 `createMaterialFromImportData()`、`registerRuntimeMaterial()` 和 `loadMaterialCPU()`。
- `VulkanMaterialResource` 升级为真实 GPU 材质资源，创建材质常量 buffer 和 descriptor set。
- `VulkanRenderResourceCache` 持有 material descriptor layout / pool，并创建 fallback material。
- `VulkanModelResource` 从 CPU model 的 mesh import material 创建 `MaterialAsset`，再创建 `VulkanMaterialResource`。
- `VulkanForwardPass` 的 pipeline layout 预留 set 0 给 frame/view，set 1 绑定 material descriptor。
- `vulkan_forward.hlsl` 已从调试法线色切换为 base color texture * base color factor。
- 当前第一版仍只使用模型自带材质，`MeshRendererComponent` / `RenderObjectData` 材质覆盖入口保留给后续需求。

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
   - 已将旧 `MaterialData` 从 `mesh.h` 收口为 Resource 层的 `MaterialImportData`。
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
     binding 1: sampled image base color texture
     binding 2: sampler
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
- Forward / ObjectId pass 只负责 render command 记录，不再直接读 `.spv` 或创建 shader module。
- 为 PR-R20 lighting、PR-R21 skybox、PR-R22 shadow 的 pipeline 扩展打基础。

执行状态：已完成。

改造前状态：

- `VulkanForwardPass` / `VulkanObjectIdPass` 各自重复实现 shader 路径拼接、SPIR-V 读取和 `VkShaderModule` 创建。
- pipeline create info 直接散落在 pass 中，后续新增 lighting / shadow / skybox 时会继续膨胀。
- CMake HLSL 编译命令仍直接硬编码在 `source/Engine/CMakeLists.txt` 中。

建议结构：

```text
Renderer/Vulkan/shaders/
  vulkan_shader_library.h
  vulkan_shader_library.cpp

Renderer/Vulkan/pipeline/
  vulkan_pipeline_types.h
  vulkan_pipeline_cache.h
  vulkan_pipeline_cache.cpp
```

建议拆分：

1. `VulkanShaderLibrary`。
   - 统一运行时 shader 根目录，例如 `bin/.../shaders/Renderer/Vulkan`。
   - 用稳定 `ShaderProgramId` 或 shader 名称管理 shader。
   - 读取已编译 SPIR-V。
   - 创建并缓存 `VkShaderModule`。
   - pass 不再保存 `readBinaryFile()` / `shaderPath()` / `createShaderModule()`。

2. `VulkanPipelineCache`。
   - 包装 Vulkan 原生 `VkPipelineCache`。
   - 用 `VulkanGraphicsPipelineDesc` 描述 pipeline。
   - 第一版至少纳入 shader program、color/depth format、descriptor set layouts、push constants、vertex layout、rasterization/depth/blend/dynamic state。
   - `ForwardPass` 和 `ObjectIdPass` 通过 desc 获取 pipeline / pipeline layout。

3. 迁移 `VulkanForwardPass`。
   - 保留 render target / depth resource / draw command 记录。
   - 移除 shader module 创建和大部分 pipeline create boilerplate。
   - 保持 set 0 预留 frame/view，set 1 绑定 material。

4. 迁移 `VulkanObjectIdPass`。
   - 使用同一个 shader library / pipeline cache。
   - 保持 picking pass 的 object id push constants 和 target format 语义。

5. CMake HLSL 编译 helper。
   - 可新增轻量 helper，减少后续 shader 编译命令复制。
   - 仍然使用 HLSL -> DXC -> SPIR-V。

生命周期：

```text
shader_library init
pipeline_cache init
resource_cache init
passes init/recreate

shutdown:
passes shutdown
pipeline_cache shutdown
shader_library shutdown
resource_cache shutdown
```

完成记录：

- 新增 `Renderer/Vulkan/shaders/VulkanShaderLibrary`，统一管理运行时 `bin/.../shaders/Renderer/Vulkan` 下的 SPIR-V 读取和 `VkShaderModule` 缓存。
- 新增 `Renderer/Vulkan/pipeline/VulkanPipelineCache`，封装原生 `VkPipelineCache`，并通过 `VulkanGraphicsPipelineDesc` 创建 / 复用 graphics pipeline 与 pipeline layout。
- `VulkanRendererSystem::Backend` 现在持有 shader library 和 pipeline cache，生命周期收口在 Vulkan backend 内部。
- `VulkanForwardPass` 通过 pipeline desc 获取 forward opaque pipeline，不再自己创建 shader module / pipeline layout / graphics pipeline。
- `VulkanObjectIdPass` 已迁移到同一套 shader library / pipeline cache，不再重复 shader path、SPIR-V 读取和 pipeline 创建样板。
- `source/Engine/CMakeLists.txt` 增加 `nexaur_add_vulkan_hlsl_compile()` helper，后续新增 HLSL shader 只需要登记 source / profile / entry / output。

第一版只需要：

- 读取已编译 SPIR-V。
- 管理 shader module 生命周期。
- 管理 forward / object id pipeline。
- pipeline 由 cache 统一持有，按 desc 复用，并在 cache shutdown 时集中释放。

暂时不做：

- runtime HLSL 编译。
- shader hot reload。
- shader reflection 自动生成 descriptor layout。
- pipeline cache 磁盘序列化。
- bindless pipeline variant。
- 多线程 pipeline compilation。
- RenderGraph 迁移。

验收：

- `VulkanForwardPass` / `VulkanObjectIdPass` 中不再出现重复的 `readBinaryFile()` / `shaderPath()` / `createShaderModule()`。
- Forward pass 仍能显示 base color texture。
- ObjectId picking 仍可用。
- 新增 shader 只需要在 shader library / CMake helper 中登记，不需要复制 pass 内读取逻辑。
- 构建和 Sandbox smoke 通过。

### PR-R18：DescriptorAllocator / DescriptorLayoutCache

目标：

- 把 descriptor pool / layout / set 分配从资源缓存、材质资源和 pass 中收口。
- 建立 Vulkan backend 内部统一 descriptor 管理入口。
- 为材质、纹理、camera uniform、light uniform、shadow map、skybox / environment 等后续资源绑定做准备。
- 让 pass 只声明自己需要的 descriptor set layout，不直接管理 descriptor pool。

执行状态：已完成。

当前问题：

- `VulkanRenderResourceCache` 当前直接创建 material descriptor layout / pool。
- `VulkanMaterialResource` 当前直接依赖 material descriptor layout / pool，并手写 descriptor set update。
- `VulkanForwardPass` 需要拿 material descriptor layout 去创建 pipeline layout。
- 后续 lighting / skybox / shadow / debug draw 如果继续各自创建 descriptor pool 和 layout，会迅速产生重复代码和生命周期混乱。
- descriptor lifetime 还没有区分长期资源和每帧临时资源，后续 frame global / light buffer 接入时容易把资源缓存和帧调度耦合在一起。

建议结构：

```text
Renderer/Vulkan/descriptors/
  vulkan_descriptor_types.h
  vulkan_descriptor_layout_cache.h
  vulkan_descriptor_layout_cache.cpp
  vulkan_descriptor_allocator.h
  vulkan_descriptor_allocator.cpp
  vulkan_descriptor_writer.h
  vulkan_descriptor_writer.cpp
```

核心类：

1. `VulkanDescriptorLayoutCache`。
   - 根据 descriptor set layout 描述创建 / 复用 `VkDescriptorSetLayout`。
   - layout desc 中需要记录 binding、descriptor type、descriptor count、shader stages。
   - binding 应排序后参与 hash / equality，避免同一 layout 因声明顺序不同重复创建。
   - 第一版可提供 named builtin layout，例如 `Material`；后续再加入 `FrameGlobal`、`Lighting`、`Shadow`、`Environment`。

2. `VulkanDescriptorAllocator`。
   - 管理一个或多个 `VkDescriptorPool`。
   - 支持 descriptor set 分配失败时自动扩容新 pool，避免硬编码一个 material pool 后被资源数量打爆。
   - 第一版至少支持 persistent descriptor，也就是 material / texture / fallback material 这类长期资源。
   - 后续扩展 per-frame transient descriptor，用于 camera uniform、light uniform、pass uniform 等每帧更新数据。

3. `VulkanDescriptorWriter`。
   - 封装 `VkWriteDescriptorSet`。
   - 提供 `writeBuffer()`、`writeImage()`、`update()` 之类的轻量接口。
   - `VulkanMaterialResource` 只描述 binding 写入意图，不再手写重复 `VkWriteDescriptorSet` 数组。

推荐职责边界：

```text
VulkanRendererSystem::Backend
  owns VulkanDescriptorLayoutCache
  owns VulkanDescriptorAllocator
  owns VulkanPipelineCache
  owns VulkanRenderResourceCache

VulkanRenderResourceCache
  owns GPU resources
  requests descriptor layout / descriptor set allocation from descriptor services

VulkanMaterialResource
  owns material constants buffer
  owns descriptor set handle
  does not own descriptor pool / descriptor layout

VulkanForwardPass
  receives descriptor set layouts for pipeline desc
  does not create descriptor layout / descriptor pool
```

建议生命周期：

```text
init:
shader_library init
descriptor_layout_cache init
descriptor_allocator init
pipeline_cache init
resource_cache init(descriptor services)
passes init/recreate

shutdown:
passes shutdown
resource_cache shutdown
pipeline_cache shutdown
descriptor_allocator shutdown
descriptor_layout_cache shutdown
shader_library shutdown
```

说明：
- `resource_cache shutdown` 需要早于 `descriptor_allocator shutdown`，因为 material resource 持有 descriptor set handle。
- `pipeline_cache shutdown` 需要早于 `descriptor_layout_cache shutdown`，因为 pipeline layout 创建时依赖 descriptor set layout。
- ImGui / viewport texture descriptor 暂时继续由 `VulkanImGuiRenderer` 和 ImGui Vulkan backend 管理，不纳入 PR-R18。

建议拆分：

1. PR-R18-A：新增 descriptor types / layout desc。
   - 定义 descriptor binding desc。
   - 定义 descriptor set layout desc。
   - 实现 hash / equality。

2. PR-R18-B：实现 `VulkanDescriptorLayoutCache`。
   - 支持 `getOrCreateLayout(desc)`。
   - 支持 material builtin layout。
   - shutdown 时统一销毁 layout。

3. PR-R18-C：实现 `VulkanDescriptorAllocator`。
   - 管理 persistent descriptor pools。
   - 支持 pool exhausted 时创建新 pool。
   - shutdown 时统一销毁 pools。

4. PR-R18-D：实现 `VulkanDescriptorWriter`。
   - 支持 buffer / image / sampler 写入。
   - 用它替换 `VulkanMaterialResource` 内部手写 descriptor update。

5. PR-R18-E：迁移 material descriptor。
   - 从 `VulkanRenderResourceCache` 移除 material descriptor layout / pool 所有权。
   - `VulkanRenderResourceCache` 改为从 descriptor services 获取 material layout 并分配 descriptor set。
   - `VulkanForwardPass` 使用同一份 material layout 创建 pipeline desc。

6. PR-R18-F：验证和文档。
   - Forward pass 仍能采样 base color texture。
   - fallback material / fallback white texture 仍稳定。
   - Scene / Editor / Resource 不暴露 descriptor 类型。
   - 构建和 Sandbox smoke 通过。

完成记录：

- 新增 `Renderer/Vulkan/descriptors/`，包含 descriptor types、layout cache、allocator 和 writer。
- `VulkanDescriptorLayoutCache` 负责创建 / 缓存 descriptor set layout，并提供 `FrameGlobal` / `Material` builtin layout。
- `VulkanDescriptorAllocator` 负责 persistent descriptor pool 管理，支持 pool exhausted 时扩容新 pool。
- `VulkanDescriptorWriter` 负责封装 buffer / image descriptor 写入。
- `VulkanRenderResourceCache` 不再创建 material descriptor layout / pool，只从 descriptor services 获取 layout 和 allocator。
- `VulkanMaterialResource` 改为通过 descriptor allocator 分配 descriptor set，并通过 descriptor writer 更新 material constants、base color image 和 sampler。
- `VulkanRendererSystem::Backend` 持有 descriptor layout cache / allocator，并收口初始化与关闭顺序。
- `VulkanPipelineCache` 不再创建 empty descriptor set layout；set 0 的空 `FrameGlobal` layout 改由 descriptor layout cache 提供。
- `VulkanForwardPass` 使用 descriptor layout cache 提供的 `FrameGlobal` / `Material` layouts 创建 pipeline desc。
- ImGui viewport texture descriptor 仍保持在 `VulkanImGuiRenderer` / ImGui Vulkan backend 内部。
- 验证通过：`cmake --preset msvc-vcpkg`、`cmake --build build/msvc-vcpkg --config Debug --target Sandbox --`、`Sandbox.exe` 5 秒 smoke check。

第一版只需要支持：

- material descriptor。
- `FrameGlobal` descriptor 保留结构和命名，当前为空 layout，不强制在本 PR 内把 camera / light 数据迁移出 push constants。
- ImGui / viewport texture descriptor 保持在 Vulkan UI bridge 内。

暂时不做：

- bindless texture。
- descriptor indexing。
- shader reflection 自动生成 descriptor layout。
- material graph。
- RenderGraph descriptor 自动绑定。
- 完整 per-frame descriptor ring。
- 多线程 descriptor allocation。

验收：

- material descriptor layout / pool 不再由 `VulkanRenderResourceCache` 直接创建和销毁。
- `VulkanMaterialResource` 使用 descriptor allocator / writer 完成 descriptor set 分配和更新。
- `VulkanForwardPass` 和 `VulkanPipelineCache` 使用 descriptor layout cache 提供的 layout。
- 新增 descriptor 绑定时不需要在 pass / resource cache 中复制 pool / layout 创建样板。
- Resource / Scene / Editor 不 include `Renderer/Vulkan/descriptors`。
- 构建、shader 编译和 Sandbox smoke 通过。

## 7. 第四优先级：轻量 Vulkan PassGraph

### PR-R19：Vulkan-only PassGraph 骨架

目标：

让 `VulkanRendererSystem` 从巨型执行类逐步变成 frame orchestrator，避免后续 skybox、shadow、post process、debug draw 继续堆进同一个 command recording 流程里。

当前问题：

- `VulkanRendererSystem` 仍直接串联 swapchain acquire / command buffer begin / viewport target transition / forward pass / object id pass / ImGui composite / present transition。
- viewport image、picking image、swapchain image 的 layout transition 逻辑分散在 Backend 私有函数中。
- 后续新增 pass 时，很容易继续扩大 `recordCommandBuffer()` 或类似帧录制函数。
- 当前 pass 之间的资源读写关系主要靠代码顺序表达，缺少显式“这个 pass 读什么、写什么”的结构。

PR-R19 的目标不是实现完整现代 RenderGraph，而是先建立 Vulkan-only 的轻量 PassGraph：

```text
VulkanRendererSystem
  prepare frame data
  prepare resources
  build VulkanPassGraph
  execute VulkanPassGraph
  present
```

建议目录：

```text
Renderer/Vulkan/graph/
  vulkan_graph_resource.h
  vulkan_pass_graph.h
  vulkan_pass_graph.cpp
  vulkan_graph_executor.h
  vulkan_graph_executor.cpp
```

建议核心概念：

1. `VulkanGraphResource`。
   - 表达当前帧 graph 中可读写的 Vulkan image resource。
   - 第一版只引用已有资源，不拥有资源。
   - 可包装 swapchain image、viewport color/depth、picking object id/depth。
   - 记录 image、image view、format、extent、aspect mask、current layout。

2. `VulkanGraphPass`。
   - 表达一个 pass 的名字、读资源、写资源和 execute callback。
   - 第一版不需要复杂反射或自动调度；按添加顺序执行即可。
   - 读写声明用于 executor 插入 layout transition 和辅助调试。

3. `VulkanPassGraph`。
   - 每帧临时构建。
   - 输入来自已经准备好的 `VulkanDrawList`、viewport target、picking target、swapchain image。
   - 不回头访问 Scene / Editor / Resource。

4. `VulkanGraphExecutor`。
   - 遍历 pass。
   - 根据资源使用声明插入 image layout transition。
   - 调用 pass execute callback。
   - 更新 graph resource 的 current layout。

第一版推荐 pass：

```text
ForwardScene
  read: draw_list
  write: viewport_color or swapchain_color
  write: scene_depth

ObjectIdPicking
  read: draw_list
  write: picking_object_id
  write: picking_depth

ImGuiComposite
  read: viewport_color as shader read
  write: swapchain_color

PresentTransition
  write: swapchain_color -> present layout
```

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
- 现有 pass record 调用的统一编排。

第一版可以继续让 `VulkanForwardPass` / `VulkanObjectIdPass` 自己负责 dynamic rendering begin/end。PassGraph 先收口 pass 顺序和 image layout 状态，后续再逐步把 dynamic rendering attachment begin/end 进一步下沉到 graph executor。

示例形态：

```cpp
graph.addPass("ForwardScene")
    .writeColor(viewport_color)
    .writeDepth(viewport_depth)
    .execute([&](VkCommandBuffer command_buffer) {
        forward_pass.record(command_buffer, viewport_target.getRenderTarget(), draw_list);
    });

graph.addPass("ObjectIdPicking")
    .writeColor(picking_object_id)
    .writeDepth(picking_depth)
    .execute([&](VkCommandBuffer command_buffer) {
        object_id_pass.record(command_buffer, picking_target.getRenderTarget(), draw_list);
    });

graph.addPass("ImGuiComposite")
    .read(viewport_color)
    .writeColor(swapchain_color)
    .execute([&](VkCommandBuffer command_buffer) {
        imgui_renderer.renderDrawData(command_buffer);
    });
```

建议拆分：

1. PR-R19-A：盘点并封装当前 frame flow。
   - 明确现在 command buffer recording 中的 pass 顺序。
   - 明确 viewport / picking / swapchain 的 layout 状态转换。
   - 提取 graph execute 所需的轻量 frame context。

2. PR-R19-B：新增 graph resource / usage 类型。
   - 定义 color attachment、depth attachment、shader read、transfer src、present 等 usage。
   - 定义 usage 到 Vulkan layout / access / stage 的映射。
   - 第一版继续使用 `vkCmdPipelineBarrier`，后续可切换到 synchronization2。

3. PR-R19-C：新增 `VulkanPassGraph` 和 pass declaration。
   - 支持 `addPass()`。
   - 支持 read/write resource 声明。
   - 支持 execute callback。
   - 第一版按添加顺序执行，不做复杂拓扑排序。

4. PR-R19-D：新增 `VulkanGraphExecutor`。
   - 在 pass 前插入必要 layout transition。
   - 调用 pass execute。
   - 更新 resource layout。

5. PR-R19-E：迁移当前 pass flow。
   - viewport target 路径：ForwardScene -> ObjectIdPicking -> ImGuiComposite -> Present。
   - fallback swapchain 路径：ForwardScene -> ObjectIdPicking -> Present。
   - 保持现有 viewport output / picking 行为不变。

6. PR-R19-F：清理 `VulkanRendererSystem`。
   - `recordCommandBuffer()` 或等价函数只负责构建 graph、执行 graph 和 end command buffer。
   - transition helper 从 Backend 私有散落逻辑收口到 graph executor。

7. PR-R19-G：验证和文档。
   - Editor viewport 仍正常显示。
   - ObjectId picking 仍可用。
   - ImGui composite 和 swapchain present 正常。
   - 构建和 Sandbox smoke 通过。

暂时不做：

- async compute。
- multi-queue scheduling。
- resource aliasing。
- 通用跨后端 RenderGraph。
- transient render target allocator。
- pass 自动裁剪。
- 完整拓扑排序。
- render thread / job system。
- shader reflection 驱动的资源自动绑定。

验收：

- `VulkanRendererSystem` 不再直接堆大量 pass 顺序和 image layout transition 细节。
- Forward / ObjectId / ImGui 的现有行为保持不变。
- 新增一个 pass 时优先注册 graph pass，而不是继续扩大 Backend command recording 逻辑。
- Resource / Scene / Editor 不 include `Renderer/Vulkan/graph`。
- 构建、shader 编译和 Sandbox smoke 通过。

完成状态：

- 新增 `Renderer/Vulkan/graph`，包含 graph image resource、pass declaration、pass graph 和 graph executor。
- `VulkanGraphExecutor` 统一维护 image usage 到 Vulkan layout / access / stage 的转换，第一版继续使用 `vkCmdPipelineBarrier`。
- `VulkanRendererSystem::recordDrawCommands()` 已改为构建并执行 graph，viewport 路径为 `ForwardScene -> ObjectIdPicking -> ImGuiComposite -> PresentTransition`。
- fallback swapchain 路径为 `ForwardScene -> ObjectIdPicking -> PresentTransition`。
- viewport color/depth、picking object id/depth、swapchain color 的 layout 状态通过 graph commit 回写到各自 target / swapchain layout cache。
- `VulkanForwardPass` / `VulkanObjectIdPass` 仍负责各自 dynamic rendering begin/end；PassGraph 第一版只收口帧内顺序和 image transition。
- 构建、HLSL shader 编译和 Sandbox smoke 已通过。

## 8. 第五优先级：基础视觉能力

### PR-R20：Lighting baseline

目标：

- 稳定 directional light。
- 支持多个 point lights 的基础 forward lighting。
- light data 进入 uniform / storage buffer，而不是硬编码 push constant。
- 建立 PBR-ready 的直接光照 baseline，但不在本 PR 内实现完整 PBR 生态。

PR-R20 开始前状态：

- Scene 层已有 `DirectionalLightComponent` / `PointLightComponent`。
- `RenderDataPacket -> RenderSceneFrame -> VulkanDrawList` 已经携带 directional light 和 point lights。
- `vulkan_forward.hlsl` 当前只采样 base color texture 并乘以 base color factor，没有使用灯光数据。
- `MaterialAsset` / `VulkanMaterialResource` 已经具备 base color、metallic、roughness 等 PBR-ready 材质参数。
- `FrameGlobal` descriptor set layout 当时仍为空；camera / light 数据还没有正式进入 GPU frame descriptor。

推荐设计：

```text
set 0: FrameGlobal
  binding 0: frame / camera / directional light uniform buffer
  binding 1: point light uniform/storage buffer

set 1: Material
  binding 0: material constants
  binding 1: base color image
  binding 2: base color sampler
```

`FrameGlobal` 第一版建议包含：

- `view_projection`。
- `camera_position`。
- directional light direction / color / intensity。
- point light count。
- environment intensity 或 fallback ambient intensity。

point light 第一版建议：

- 保持当前 `RenderSceneFrameBuilder` 的 64 个 point light 上限。
- 可以先使用 uniform buffer 数组；如果后续灯光数量增加，再切换到 storage buffer。
- HLSL / C++ 结构必须按 `float4` 对齐，避免 `vec3` packing 造成跨语言布局问题。

Forward pass 调整：

- `VulkanForwardPass` 每帧绑定 set 0。
- 每个 draw item 继续绑定 set 1 material descriptor。
- push constants 建议只保留 per-object 数据：
  - `model`。
  - 可选 `normal_matrix`。
- `view_projection` 从 push constants 迁移到 frame uniform，避免后续 camera / light 数据继续挤压 push constant 上限。

Shader 策略：

- PR-R20 可以实现 direct-light PBR baseline：
  - base color。
  - metallic。
  - roughness。
  - directional light。
  - point lights attenuation。
  - camera vector 用于 specular。
- 不建议在 PR-R20 做完整 PBR：
  - 不做 IBL。
  - 不做 BRDF LUT。
  - 不做 irradiance / prefilter cubemap。
  - 不做 normal map / AO / emissive。
  - 不做 shadow。
  - 不做 HDR / tonemapping。

拆分：

1. PR-R20-A：确认 light data contract。
   - 明确 directional light direction 语义。
   - 明确 point light attenuation 参数语义。
   - 保持 Scene / Renderer data 结构不暴露 Vulkan 类型。

2. PR-R20-B：新增 Vulkan frame lighting resource。
   - 负责创建 / 更新 frame global buffer。
   - 负责创建 / 更新 point light buffer。
   - 第一版可以按当前单帧 in-flight 设计，但类名和接口要为多帧资源预留空间。

3. PR-R20-C：完善 `FrameGlobal` descriptor layout。
   - `VulkanDescriptorLayoutCache` 的 `FrameGlobal` builtin layout 从空 layout 升级为正式 layout。
   - descriptor allocation / update 仍使用 PR-R18 的 allocator / writer。

4. PR-R20-D：迁移 `VulkanForwardPass` 绑定。
   - pipeline layout 继续使用 set 0 + set 1。
   - draw 前绑定 frame descriptor set。
   - material descriptor set 仍按 draw item 绑定。

5. PR-R20-E：更新 HLSL forward lighting。
   - VS 输出 world position / world normal / texcoord。
   - PS 读取 frame globals、point lights、material constants。
   - 实现 directional + point lights 的 direct lighting。
   - 无灯光或环境很弱时保留一个很小的 fallback ambient，避免默认场景全黑。

6. PR-R20-F：默认场景、验证和文档。
   - 默认 directional light / point light 能明显影响画面。
   - 摄像机移动时 specular 方向变化合理。
   - base color texture 仍正常。
   - 构建、shader 编译和 Sandbox smoke 通过。

验收：

- Scene 中灯光组件变化能影响画面。
- Directional light 和至少多个 point lights 能同时参与 Forward lighting。
- `VulkanForwardPass` 不再依赖硬编码固定颜色或只返回 base color。
- camera / light 数据进入 frame descriptor，而不是继续塞进 push constants。
- Material 的 base color / metallic / roughness 参与 direct-light shading。
- 本 PR 不要求完整 IBL / shadow / normal map / tonemapping。

完成状态：

- 新增 `VulkanFrameLightingResource`，负责 frame global uniform buffer、point light storage buffer 和 set 0 descriptor set。
- `FrameGlobal` builtin descriptor layout 已升级为：
  - binding 0：frame / camera / directional light uniform buffer。
  - binding 1：point light storage buffer。
- `VulkanRendererSystem` 在 fence wait 之后更新 frame lighting buffer，避免覆盖 GPU 仍在读取的 frame data。
- `VulkanForwardPass` 每帧绑定 set 0 frame descriptor，每个 draw item 继续绑定 set 1 material descriptor。
- Forward push constants 已收口为 per-object `model` + `normal_matrix`，`view_projection` 迁移到 frame uniform。
- `vulkan_forward.hlsl` 已实现 directional light + point lights 的 direct-light PBR baseline，使用 base color、metallic、roughness、camera vector 和 fallback ambient。
- 构建、HLSL shader 编译和 Sandbox smoke 已通过。

### PR-R21：Skybox / Environment

执行状态：已完成。

完成记录：

- `EnvironmentComponent`、`RendererEnvironmentData`、`RenderSceneFrame` 和 `VulkanDrawList` 已增加 background / environment color 数据链路。
- `SceneV2::extractSceneData()` 不再要求 environment asset 必须有效；procedural skybox 可以只依赖 background color 和 intensity。
- 新增 `assets/shaders/Renderer/Vulkan/vulkan_skybox.hlsl`，使用 HLSL + DXC 编译到 Vulkan 1.3 SPIR-V。
- `VulkanShaderLibrary` 增加 Skybox shader program id，`VulkanPipelineCache` 增加 no-vertex-input pipeline 支持。
- 新增 `VulkanSkyboxPass`，使用 Vulkan dynamic rendering 和 full-screen triangle 绘制 procedural sky / environment color。
- `VulkanForwardPass` 增加 render options，支持 color attachment `LOAD`，Skybox 之后的 ForwardScene 只清 depth。
- PassGraph 已接入 `Skybox -> ForwardScene`，viewport target 和 fallback swapchain 两条路径都覆盖。
- Editor `PropertiesPanel` 已为 `EnvironmentComponent` 暴露 background color 和 intensity。
- 构建、HLSL shader 编译和 Sandbox smoke 已通过。

目标：

- 支持 environment asset。
- 支持简单 skybox 或 environment color。
- 为 IBL 预留资源结构。

- 第一版建议只做 baseline：

- environment color / procedural skybox。
- 不消费真实 HDR / cubemap asset。
- 不做完整 prefilter / BRDF LUT。

当前状态：

- Scene 层已有 `EnvironmentComponent`。
- `RenderDataPacket -> RenderSceneFrame -> VulkanDrawList` 已经携带 `environment_asset` 和 `environment_intensity`。
- `SceneFactory` 会默认注册一个 HDR environment asset。
- `AssetManager` 当前只登记 `EnvironmentMap` / `TextureCube` 身份，还没有真实 HDR / cubemap CPU loader。
- Vulkan 侧当前只有 2D texture resource，还没有 cube texture resource。
- 旧 `assets/shaders/skybox` 是 GLSL 风格，不能直接复用到当前 HLSL Vulkan 路线。

R21 第一版推荐路线：

```text
RenderSceneFrame.environment
  -> VulkanDrawList.environment
  -> VulkanSkyboxPass
  -> procedural sky / environment color
```

推荐 pass 顺序：

```text
Skybox
  -> ForwardScene
  -> ObjectIdPicking
  -> ImGuiComposite
  -> PresentTransition
```

关键设计：

1. `EnvironmentComponent` / renderer environment data。
   - 可以补充 `environment_color` 或 `background_color`。
   - `environment_intensity` 继续保留。
   - `environment_asset` 继续作为后续 HDR / cubemap 入口，但第一版不强制加载。

2. `VulkanSkyboxPass`。
   - 放在 `Renderer/Vulkan/passes`。
   - 第一版可以画 full-screen triangle 或 procedural sky。
   - 不需要 mesh vertex buffer。
   - 不需要 cube texture。
   - 只负责写 color attachment，不写 depth。

3. `VulkanForwardPass` 支持 color load op。
   - 当前 Forward pass 会 clear color。
   - 如果 Skybox 先画，ForwardScene 必须 `LOAD` color，否则 skybox 会被清掉。
   - depth attachment 仍然可以 `CLEAR`。
   - 推荐通过轻量 record options 或 render target options 表达，不要把 skybox 特例写死进 ForwardPass。

4. `VulkanPipelineCache` 支持无 vertex input pipeline。
   - 给 `VulkanPipelineVertexLayout` 增加 `None`。
   - Skybox full-screen triangle 通过 `vkCmdDraw(3, 1, 0, 0)` 绘制。

5. 新增 HLSL shader。
   - 新增 `assets/shaders/Renderer/Vulkan/vulkan_skybox.hlsl`。
   - 第一版使用 procedural gradient / environment color。
   - HLSL 编译接入 CMake。
   - `VulkanShaderLibrary` 增加 Skybox shader program id。

6. PassGraph 接入。
   - viewport target 路径：`Skybox -> ForwardScene -> ObjectIdPicking -> ImGuiComposite -> PresentTransition`。
   - fallback swapchain 路径：`Skybox -> ForwardScene -> ObjectIdPicking -> PresentTransition`。
   - Skybox 和 ForwardScene 共享同一个 color target。
   - Skybox 写 color 后，ForwardScene 对 color 使用 load，不需要额外 image layout transition。

建议拆分：

1. PR-R21-A：Environment data contract。
   - 增加 environment color / background color。
   - 保持 `environment_asset` 只作为 identity，不在本 PR 强制加载。

2. PR-R21-B：Skybox shader 和 pipeline。
   - 新增 HLSL skybox shader。
   - `VulkanShaderLibrary` / CMake 接入 skybox shader。
   - `VulkanPipelineCache` 支持 `VulkanPipelineVertexLayout::None`。

3. PR-R21-C：新增 `VulkanSkyboxPass`。
   - 使用 dynamic rendering。
   - 绘制 full-screen triangle。
   - 写入当前 viewport / swapchain color target。

4. PR-R21-D：ForwardPass load op 支持。
   - color attachment 支持 `LOAD`。
   - depth attachment 仍默认 `CLEAR`。
   - 保持没有 Skybox 时仍可 clear 到 fallback background。

5. PR-R21-E：PassGraph flow 迁移。
   - 在 ForwardScene 前注册 Skybox pass。
   - viewport 和 fallback swapchain 两条路径都覆盖。

6. PR-R21-F：验证和文档。
   - Editor viewport 能看到背景色 / procedural sky。
   - Forward mesh 能叠在 skybox 上。
   - ObjectId picking 不受影响。
   - 构建、shader 编译和 Sandbox smoke 通过。

暂时不做：

- HDR 加载。
- equirectangular HDR -> cubemap。
- Vulkan cube texture resource。
- samplerCube skybox。
- irradiance / prefilter cubemap。
- BRDF LUT。
- IBL diffuse / specular。
- environment asset 热更新。

后续建议：

- PR-R21.1：TextureCube / EnvironmentResource。
- PR-R21.2：HDR equirectangular import 和 cubemap conversion。
- PR-R21.3：IBL diffuse irradiance / specular prefilter / BRDF LUT。

### PR-R22：Shadow 第一版

执行状态：已完成。

完成记录：

- `DirectionalLightComponent`、`RendererDirectionalLightData`、`RenderFrameDirectionalLight` 和 `VulkanDrawList` 已增加 cast shadow、strength、bias、distance 等后端无关阴影字段。
- 新增 `VulkanShadowMapTarget`，负责 sampled depth image / image view / sampler、shadow map extent 和 image layout 记录。
- 新增 `assets/shaders/Renderer/Vulkan/vulkan_shadow_depth.hlsl`，通过 HLSL + DXC 编译为 vertex-only shadow depth shader。
- `VulkanShaderLibrary` 增加 `ShadowDepth` shader program id，`VulkanPipelineCache` 支持 depth-only / vertex-only pipeline。
- 新增 `VulkanShadowPass`，使用 Vulkan dynamic rendering 写入 directional light shadow depth。
- `FrameGlobal` descriptor 增加 shadow sampled image / sampler，frame uniform 增加 shadow light VP 和 shadow params。
- `vulkan_forward.hlsl` 增加 shadow depth sampling 和 3x3 PCF，第一版只衰减 directional direct light。
- PassGraph 已接入 `DirectionalShadowMap -> Skybox -> ForwardScene`，viewport target 和 fallback swapchain 两条路径都覆盖。
- 构建、HLSL shader 编译和 Sandbox smoke 已通过。

目标：

- directional light shadow map。
- shadow pass 写 depth。
- forward pass 采样 shadow map。

建议范围：

- 第一版只支持一个 directional light shadow map。
- shadow map 分辨率先固定为 `2048x2048`，后续再暴露为项目配置或组件参数。
- 使用 Vulkan dynamic rendering 的 depth-only pass。
- 使用 HLSL shader，编译到 Vulkan 1.3 SPIR-V。
- 使用最小 shadow compare，可先做 hard shadow；如果实现成本很低，可以加 3x3 PCF，但不要扩展成完整阴影质量系统。
- shadow 只影响 direct light，不影响 procedural skybox / ambient / environment baseline。

建议数据流：

```text
DirectionalLightComponent
  -> RenderDataPacket
  -> RenderSceneFrame
  -> VulkanDrawList
  -> VulkanShadowPass
  -> VulkanForwardPass
```

Scene / Resource / Editor 不直接 include Vulkan shadow 类型。Scene 层只表达“是否投射阴影”和少量后端无关参数，GPU shadow map、sampler、descriptor、pipeline 都留在 Vulkan backend 内部。

建议给 directional light 增加的最小参数：

```cpp
bool cast_shadow = true;
float shadow_strength = 0.65f;
float shadow_bias = 0.002f;
float shadow_distance = 30.0f;
```

这些参数进入 `RendererDirectionalLightData` / `RenderFrameDirectionalLight` / `VulkanDrawList`，但不携带 Vulkan handle。

建议新增结构：

```text
Renderer/Vulkan/targets/
  vulkan_shadow_map_target.h
  vulkan_shadow_map_target.cpp

Renderer/Vulkan/passes/
  vulkan_shadow_pass.h
  vulkan_shadow_pass.cpp

assets/shaders/Renderer/Vulkan/
  vulkan_shadow_depth.hlsl
```

`VulkanShadowMapTarget` 职责：

- 创建 depth image / image view / sampler。
- 维护 shadow map image layout。
- 提供 depth-only render target 描述。
- 提供 shader read image view / sampler 给 frame descriptor。

`VulkanShadowPass` 职责：

- 只消费 `VulkanDrawList.opaque_items`。
- 只绑定 mesh vertex / index buffer。
- 不绑定 material descriptor。
- 不采样 texture。
- 使用 depth-only pipeline 记录 draw command。
- push constants 可以先使用 `light_view_projection + model`，两个 `mat4` 正好 128 bytes，保持在 Vulkan 最小 push constant 限制内。

`VulkanPipelineCache` 需要补充：

- 支持 depth-only pipeline，也就是 `colorAttachmentCount = 0`。
- 支持 vertex-only shader program，shadow depth pass 不需要 fragment shader。
- `VulkanGraphicsPipelineDesc` 的合法性检查不能强制要求 color format。

`VulkanShaderLibrary` 需要补充：

- 增加 `VulkanShaderProgramId::ShadowDepth`。
- 支持只加载 vertex stage，或让 shader program 显式声明 stage 列表。
- 不建议为了 shadow pass 伪造无意义 fragment shader。

Frame descriptor 建议：

当前 `VulkanFrameLightingResource` 实际已经管理 camera / light / environment 等 frame-global 数据。接入 shadow 后建议重命名或逐步迁移为：

```text
VulkanFrameGlobalResource
```

如果不想在 PR-R22 扩大改名范围，也可以先保留旧类名，但要避免继续把大量非 lighting 职责塞进类内部。较清爽的长期边界是：

```text
VulkanFrameGlobalResource
  - frame uniform buffer
  - point light storage buffer
  - shadow map image descriptor
  - shadow sampler descriptor
```

FrameGlobal descriptor 第一版可以扩展为：

```text
set 0 binding 0: FrameGlobals uniform buffer
set 0 binding 1: PointLights storage buffer
set 0 binding 2: ShadowMap sampled image
set 0 binding 3: ShadowSampler
```

Forward shader 需要补充：

- `FrameGlobals` 增加 `shadow_light_view_projection`。
- `FrameGlobals` 或 shadow params 增加 enabled / strength / bias / map size。
- VS 输出 world position。
- PS 将 world position 变换到 light clip space。
- light clip space 转 shadow map uv / depth。
- 采样 shadow map 并比较深度。
- shadow 只衰减 directional / point direct lighting 中需要的部分；第一版建议先只衰减 directional light。

Light view projection 计算：

- 第一版可以在 Vulkan backend 内部基于 directional light direction、camera position 和 `shadow_distance` 计算一个固定 orthographic shadow box。
- 当前相机投影已有 Vulkan clip space 转换逻辑；shadow orthographic projection 也需要同样进入 Vulkan clip space，避免 Y / depth 范围错误。
- 不建议第一版做 scene bounds 精确包围，因为当前 mesh resource 还没有稳定 bounds 数据。
- 后续可在 model / mesh resource 中补 bounds，再做 tighter shadow frustum 或 CSM。

PassGraph 接入：

```text
DirectionalShadowMap
  writes ShadowDepth as DepthStencilAttachment

Skybox
  writes ViewportColor / SwapchainColor as ColorAttachment

ForwardScene
  reads ShadowDepth as ShaderRead
  writes scene color / depth
```

需要让 `VulkanGraphImageUsage` 覆盖 shadow depth 的 shader read 状态。当前已有 `ShaderRead` usage，可先复用；如果后续需要更准确的 depth aspect / sampled depth 状态，再细化 usage 名称。

建议拆分：

1. PR-R22-A：Shadow data contract。
   - `DirectionalLightComponent` 增加 cast shadow / strength / bias / distance。
   - `RendererDirectionalLightData`、`RenderFrameDirectionalLight`、`VulkanDrawList` 同步字段。
   - 默认场景 directional light 默认开启 shadow。

2. PR-R22-B：新增 `VulkanShadowMapTarget`。
   - 创建 `VK_FORMAT_D32_SFLOAT` 或支持格式探测。
   - usage 包含 `VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT`。
   - 创建 sampler，第一版可以使用 clamp-to-border。
   - 维护 depth image layout。

3. PR-R22-C：Pipeline / Shader 基础能力。
   - `VulkanPipelineCache` 支持 depth-only pipeline。
   - `VulkanShaderLibrary` 支持 `ShadowDepth` vertex-only program。
   - CMake HLSL 编译 helper 增加 shadow depth shader。

4. PR-R22-D：新增 `VulkanShadowPass`。
   - 使用 dynamic rendering 写 shadow depth。
   - 使用 mesh vertex / index buffer 绘制 opaque items。
   - 不绑定 material / texture。
   - 验证 shadow pass 可以单独录制不崩溃。

5. PR-R22-E：FrameGlobal descriptor 接入 shadow map。
   - 扩展 builtin `FrameGlobal` descriptor layout。
   - 更新 frame resource descriptor set。
   - 在 frame uniform 中写入 shadow light VP 和 shadow params。

6. PR-R22-F：Forward shader shadow sampling。
   - `vulkan_forward.hlsl` 采样 shadow depth。
   - 对 directional direct light 应用 shadow attenuation。
   - 调整 bias，避免明显 shadow acne。

7. PR-R22-G：PassGraph flow 迁移。
   - viewport target 路径：`DirectionalShadowMap -> Skybox -> ForwardScene -> ObjectIdPicking -> ImGuiComposite -> PresentTransition`。
   - fallback swapchain 路径：`DirectionalShadowMap -> Skybox -> ForwardScene -> ObjectIdPicking -> PresentTransition`。
   - shadow depth pass 后进入 shader read，再给 ForwardScene 使用。

8. PR-R22-H：验证和文档。
   - 默认场景能看到物体投影。
   - 关闭 directional shadow 后画面恢复无阴影。
   - camera 移动时 shadow 稳定，不出现明显闪烁。
   - ObjectId picking 不受 shadow pass 影响。
   - 构建、HLSL shader 编译和 Sandbox smoke 通过。

暂时不做：

- cascaded shadow maps。
- point light shadow cube map。
- contact shadow / screen-space shadow。
- transparent shadow。
- shadow atlas。
- contact hardening shadow / VSM / EVSM。
- editor shadow map debug viewer。

## 9. 第六优先级：Editor 和 Debug 工具

### PR-R23：Debug draw

执行状态：已完成。

完成记录：

- 新增 `RenderDebugLine` / `RenderDebugDrawData`，debug draw 数据从 `RenderDataPacket -> RenderSceneFrame -> VulkanDrawList` 进入渲染器。
- 新增 `RenderDebugDrawBuilder`，支持 line、AABB、camera frustum、directional light gizmo 和 point light gizmo 展开。
- `SceneV2::extractSceneData()` 默认输出 active camera frustum、directional light gizmo 和 point light gizmo。
- 新增 `VulkanDebugDrawBuffer`，将后端无关 debug lines 上传为 `position + color` vertex buffer。
- 新增 `assets/shaders/Renderer/Vulkan/vulkan_debug_draw.hlsl`，通过 HLSL + DXC 编译为 DebugDraw VS / PS。
- `VulkanShaderLibrary` 增加 DebugDraw shader program id，`VulkanPipelineCache` 增加 debug position/color line-list vertex layout。
- 新增 `VulkanDebugDrawPass`，使用 scene color / depth dynamic rendering 绘制 depth-tested line list。
- `VulkanForwardPass` 暴露 swapchain scene render target / depth layout，让 fallback swapchain 路径也能通过 PassGraph 接入 DebugDraw。
- PassGraph 已接入 `ForwardScene -> DebugDraw -> ObjectIdPicking`，DebugDraw 不参与 ObjectId picking。
- 构建、HLSL shader 编译和 Sandbox smoke 已通过。

目标：

- 线段。
- AABB。
- camera frustum。
- light gizmo 辅助显示。

Debug draw 对 demo 和后续物理调试很重要，建议在完整后处理之前做。

设计原则：

- Debug draw 是后端无关的调试图元数据，不是 Vulkan pass 的私有功能。
- Scene / Editor / Physics 只提交 line、color、depth test 等语义，不接触 Vulkan buffer、descriptor、pipeline。
- Renderer frontend 负责把高级 debug shape 展开为线段列表。
- Vulkan backend 只消费已经展开好的 debug line list。
- Debug draw 不参与 ObjectId picking，避免调试线影响实体选中。

建议数据流：

```text
Editor / Scene / Physics
  -> DebugDrawData / RenderDataPacket
  -> RenderSceneFrame
  -> VulkanDrawList
  -> VulkanDebugDrawBuffer
  -> VulkanDebugDrawPass
```

第一版建议新增后端无关数据：

```cpp
struct RenderDebugLine {
    glm::vec3 start;
    glm::vec3 end;
    glm::vec4 color;
    bool depth_test = true;
};

struct RenderDebugDrawData {
    std::vector<RenderDebugLine> lines;
};
```

如果希望第一版更简单，可以只支持 depth-tested lines，把 xray / always-on-top line 留给后续。更清爽的长期结构是按 render mode 分组：

```text
depth_tested_lines
overlay_lines
```

建议 helper：

- `addLine(start, end, color)`。
- `addAABB(min, max, color)`，展开为 12 条线。
- `addFrustum(corners, color)` 或 `addFrustum(view_projection_inverse, color)`，展开为 12 条线。
- `addDirectionalLightGizmo(position, direction, color)`，第一版用 arrow + small cross 即可。
- `addPointLightGizmo(position, radius, color)`，第一版可用三轴 circle 或 cross，不要求光照体积精确。

建议新增文件：

```text
Renderer/data/
  render_debug_draw.h

Renderer/data/
  render_debug_draw_builder.h
  render_debug_draw_builder.cpp

Renderer/Vulkan/passes/
  vulkan_debug_draw_pass.h
  vulkan_debug_draw_pass.cpp

Renderer/Vulkan/resources/
  vulkan_debug_draw_buffer.h
  vulkan_debug_draw_buffer.cpp

assets/shaders/Renderer/Vulkan/
  vulkan_debug_draw.hlsl
```

`VulkanDebugDrawBuffer` 职责：

- 管理一块 host-visible dynamic vertex buffer。
- 每帧上传 `position + color` 顶点。
- 按 debug line count 自动扩容，但不要每条线创建 buffer。
- 第一版可以单 buffer + fence wait 后更新；后续再升级为 per-frame ring buffer。

`VulkanDebugDrawPass` 职责：

- 使用 `VK_PRIMITIVE_TOPOLOGY_LINE_LIST`。
- 绑定 `FrameGlobal` descriptor，复用当前 view projection。
- 不绑定 material descriptor。
- 不采样 texture。
- 关闭 blend 或使用 alpha blend，第一版建议先 alpha blend 开启，方便半透明调试线。
- depth-tested 模式使用现有 scene depth，`depthWriteEnable = false`。

PassGraph 接入建议：

```text
DirectionalShadowMap
  -> Skybox
  -> ForwardScene
  -> DebugDraw
  -> ObjectIdPicking
  -> ImGuiComposite
  -> PresentTransition
```

说明：

- DebugDraw 在 ForwardScene 之后绘制，才能叠加在模型上。
- DebugDraw 读取 / 使用 scene depth，但不写 depth。
- ObjectIdPicking 仍使用独立 picking target，不消费 debug draw。
- viewport target 路径第一版必须支持，因为 Debug draw 首先服务 Editor。
- fallback swapchain 路径可以同步支持；如果现有 swapchain depth 仍由 `VulkanForwardPass` 私有持有，需要先给 DebugDraw 暴露只读 depth target，或把 swapchain scene depth 从 ForwardPass 中抽出成更明确的 target。

建议拆分：

1. PR-R23-A：Debug draw data contract。
   - 新增 `RenderDebugLine` / `RenderDebugDrawData`。
   - `RenderDataPacket -> RenderSceneFrame -> VulkanDrawList` 携带 debug lines。
   - 不引入 Vulkan 类型。

2. PR-R23-B：Debug draw builder helpers。
   - 实现 line / AABB / frustum / light gizmo 展开。
   - 第一版可以由 Editor 临时注入选中实体 AABB、camera frustum、light gizmo。

3. PR-R23-C：Vulkan debug draw shader / pipeline。
   - 新增 `vulkan_debug_draw.hlsl`。
   - `VulkanShaderLibrary` 增加 DebugDraw program id。
   - `VulkanPipelineCache` 使用 line list topology 创建 pipeline。

4. PR-R23-D：VulkanDebugDrawBuffer。
   - host-visible dynamic vertex buffer。
   - 每帧上传 debug vertices。
   - 空 line list 时 pass 直接跳过。

5. PR-R23-E：VulkanDebugDrawPass。
   - 绑定 FrameGlobal descriptor。
   - 绑定 debug vertex buffer。
   - 使用 scene color / depth dynamic rendering 绘制 line list。

6. PR-R23-F：PassGraph 接入。
   - viewport target 路径：`ForwardScene -> DebugDraw -> ObjectIdPicking -> ImGuiComposite`。
   - fallback swapchain 路径尽量同步；如果成本过高，文档标明第一版只支持 embedded viewport debug draw。

7. PR-R23-G：验证和文档。
   - 默认场景能显示 selected entity AABB。
   - SceneView 能显示 active camera frustum。
   - Directional / point light 有简单 gizmo。
   - debug draw 不影响 picking。
   - 构建、HLSL shader 编译和 Sandbox smoke 通过。

暂时不做：

- 文字标签。
- debug draw 分类面板。
- 持久化 lifetime / duration。
- physics debug draw 完整适配。
- navmesh / path finding debug draw。
- GPU profiler。
- shadow map debug viewer。
- gizmo 交互逻辑替代 ImGuizmo。

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

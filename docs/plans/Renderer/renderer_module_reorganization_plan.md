# Renderer Module Reorganization Plan

## 1. 背景

D12 之后，NexAur 已经完成旧 OpenGL legacy renderer 的退役：

- 默认构建路径已经收口为 vcpkg + Vulkan。
- `RendererService` 不再暴露 OpenGL texture id / readPixel 语义。
- 旧 OpenGL RHI、OpenGL platform implementation、legacy render pass 和 legacy GPU resource cache 已从主线删除。
- PR-R12.1-B 后，当前 Vulkan 后端已位于 `source/Engine/Function/Renderer/Vulkan`。

这意味着“功能迁移”已经完成，但“目录语义”和“模块所有权”还没有完全收口。

当前主要问题不是功能重复，而是命名和职责边界仍带有迁移期痕迹：

- `RendererV2` 是迁移期版本名，长期保留会让读代码的人误以为还有 RendererV1 / RendererV2 双线架构；PR-R12.1-B 已将源码目录收口为 `Renderer/Vulkan`。
- PR-R12.1-A 前 `Renderer/RHI` 只剩 `RendererService`，它已经不是传统 RHI，而是引擎侧 renderer facade；PR-R12.1-A 后已移动到 `Renderer` 根目录。
- PR-R12.1-D 前 `WindowSystem` 仍在 `Renderer` 目录下，但窗口和图形 API 创建策略更属于 Platform 层；PR-R12.1-D 后已移动到 Platform。
- `Camera` / `EditorCamera` 的所有权需要进一步明确，避免 Renderer 目录变成“和画面有关的所有东西”的杂物间。

本阶段目标是让渲染模块的目录结构和架构语义更清爽、稳定、易读，为后续接入更完整的 Vulkan renderer、RenderGraph、材质系统、阴影、后处理和小游戏 demo 打基础。

## 2. 参考架构原则

### 2.1 Godot：RenderingServer / Driver 边界

Godot 的优秀思路是上层模块不直接依赖具体图形 API。Scene、Editor、Resource 等模块通过服务边界提交渲染意图，Vulkan/OpenGL 等后端细节留在 renderer driver 内部。

对应到 NexAur：

- Scene 只输出 `RenderDataPacket`、`AssetHandle`、相机、灯光和 transform。
- Editor 只依赖 `RendererService`、`ViewportOutput`、`ViewportPickResult`。
- Vulkan image、buffer、descriptor、pipeline、command buffer 不进入 Scene / Resource / Editor 的公开接口。

### 2.2 Unreal Engine：RendererModule / RHI / Render Pass 分层

Unreal 的 Renderer 和 RHI 分层很强，但 NexAur 当前阶段不适合立刻做一个庞大的通用 RHI。旧 OpenGL 代码退役后，最稳妥的方向是：

- 保留清晰的 RendererModule facade。
- 后端内部可以有 Vulkan-specific resource、pass、target、pipeline。
- 不为了“未来可能多后端”提前抽象所有 Vulkan 对象。
- 如果未来真的需要第二后端，再从稳定的 Vulkan backend 中提炼最小必要 RHI。

对应到 NexAur：

- `RendererService` 是模块外部契约。
- `VulkanRendererSystem` 是 Vulkan backend 入口。
- `RenderView` / `RenderSceneFrame` / `VulkanDrawList` 是 renderer 内部 frame representation。
- Vulkan pass / resource / target 是 backend-private implementation。

### 2.3 Unity：Scene Data -> Render Pipeline Data

Unity 的 SRP 思路强调“引擎对象数据”和“渲染管线数据”分离。Scene 不应该直接变成 GPU command，渲染器需要一个明确的转换阶段。

对应到 NexAur：

```text
Scene / Editor
  -> RenderDataPacket
  -> RenderView
  -> RenderSceneFrame
  -> VulkanDrawList
  -> Vulkan command buffer
```

这条链路应该保持稳定。目录整理时不要把这些阶段混在一个大文件夹里。

## 3. 设计目标

### 3.1 对外简单

非 Renderer 模块只应该知道：

- `RendererService`
- `RendererBackendType`
- `ViewportOutput`
- `ViewportPickRequest` / `ViewportPickResult`
- `RenderDataPacket`
- `RenderContext`

它们不应该 include Vulkan backend header。

### 3.2 对内清晰

Renderer 内部按照数据流组织：

```text
RenderDataPacket
  -> RenderView
  -> RenderSceneFrame
  -> VulkanDrawList
  -> Pass execution
```

每一层都应该有明确职责：

- `RenderDataPacket`：Scene / Editor 提交的轻量帧数据。
- `RenderView`：视口、相机、投影、渲染区域等 view 相关数据。
- `RenderSceneFrame`：renderer 消费的场景快照。
- `VulkanDrawList`：已经绑定 Vulkan GPU resource 的绘制列表。
- Vulkan passes：执行具体 GPU 渲染逻辑。

### 3.3 后端私有

Vulkan backend 内部类型不应该泄漏到公共接口：

- `VkImage`
- `VkBuffer`
- `VkDescriptorSet`
- `VkPipeline`
- `VulkanMeshResource`
- `VulkanModelResource`
- `VulkanRenderTarget`

这些类型只允许出现在 `Renderer/Vulkan` 目录和必要的内部实现文件中。

### 3.4 目录语义不要带版本号

`RendererV2` 是迁移阶段的名字，不适合长期保留。版本信息应该存在于 git 历史、tag、roadmap 中，而不是长期目录名中。PR-R12.1-B 已将源码目录收口为 `Renderer/Vulkan`。

### 3.5 当前架构审查结论

当前 D12 后的渲染架构方向是正确的，已经可以支撑后续 Vulkan renderer 接入、Editor viewport 迭代和小游戏 demo 开发。

已经比较健康的部分：

- `RendererService` 成为 Editor / Runtime 面向渲染器的唯一主要门面。
- `RenderDataPacket -> RenderView -> RenderSceneFrame -> DrawList` 的数据流已经形成，Scene 不直接生成 GPU command。
- Vulkan backend 资源、pass、target、UI renderer 已经基本私有化。
- UI 系统不再拥有 OpenGL renderer backend，也不直接管理 Vulkan backend。
- `AssetManager` 开始回到 CPU 资产身份管理，GPU 资源创建交给 renderer backend。
- Scene / Resource / Editor 公开接口没有继续依赖 OpenGL legacy 类型。

但它还不是最终清爽状态。剩余风险主要来自目录语义和职责边界：

- `RendererV2` 是迁移期名字，长期保留会让后续代码读者误以为引擎存在 RendererV1 / RendererV2 双线架构；该风险已通过 PR-R12.1-B 收口。
- PR-R12.1-A 前 `Renderer/RHI` 只剩 facade 类型，名字已经不准确，容易被误解成 UE 风格通用 RHI；PR-R12.1-A 后该 facade 已移出 `RHI`。
- PR-R12.1-D 前 `WindowSystem` 仍放在 Renderer 目录下，容易让 Platform 和 Renderer 的职责重新缠在一起；该风险已通过 PR-R12.1-D 收口。
- `Camera` / `EditorCamera` 的所有权还不够清晰，后续容易把和画面相关的工具类都塞进 Renderer。
- shader source、compiled SPIR-V output、runtime asset load path 需要固定边界，否则后续材质和 shader pipeline 容易混乱。
- 当前还没有稳定的 RenderGraph / PassGraph 组织方式，后续添加 shadow、post process、skybox、debug draw 时需要避免直接堆在 `VulkanRendererSystem` 中。

因此，本阶段的结论是：

```text
当前架构：可继续开发，方向正确。
当前问题：目录和职责边界仍带有迁移期痕迹。
下一步重点：不要急着加复杂功能，先把目录、命名和所有权收口。
```

后续开发需要守住几条硬规则：

- 对外只暴露 `RendererService` 和 backend-neutral 数据结构。
- Vulkan 类型只能出现在 `Renderer/Vulkan` 内部。
- Editor / Scene / Resource 不 include Vulkan backend header。
- Platform 只提供窗口、输入和系统能力，不知道 renderer pass / swapchain frame / ImGui renderer backend。
- Resource 只管理资产身份和 CPU 资产，不创建 GPU 对象。
- 新 render feature 必须进入清晰的数据流：`RenderDataPacket -> RenderView -> RenderSceneFrame -> DrawList -> Pass`。
- 新 pass 不直接堆进 `VulkanRendererSystem`；`VulkanRendererSystem` 应该逐步变成 orchestrator，而不是巨型 renderer god object。

## 4. 推荐目标目录

迁移前存在两个渲染源码入口：

```text
source/Engine/Function/Renderer/
source/Engine/Function/RendererV2/  // 历史迁移期目录
```

PR-R12.1-B 后已收口为：

```text
source/Engine/Function/Renderer/
  renderer_module.h
  renderer_module.cpp
  renderer_service.h
  renderer_service_types.h

  data/
    render_context.h
    render_data.h
    render_view.h
    render_scene_frame.h

  frontend/
    render_scene_frame_builder.h
    render_scene_frame_builder.cpp

  Vulkan/
    vulkan_renderer_system.h
    vulkan_renderer_system.cpp
    vulkan_resource_context.h
    vulkan_render_target.h
    vulkan_render_resource_cache.h
    vulkan_render_resource_cache.cpp

    frontend/
      vulkan_render_data_translator.h
      vulkan_render_data_translator.cpp
      vulkan_draw_list.h
      vulkan_draw_list_builder.h
      vulkan_draw_list_builder.cpp

    passes/
      vulkan_forward_pass.h
      vulkan_forward_pass.cpp
      vulkan_object_id_pass.h
      vulkan_object_id_pass.cpp

    resources/
      vulkan_material_resource.h
      vulkan_material_resource.cpp
      vulkan_mesh_resource.h
      vulkan_mesh_resource.cpp
      vulkan_model_resource.h
      vulkan_model_resource.cpp

    targets/
      vulkan_viewport_target.h
      vulkan_viewport_target.cpp
      vulkan_picking_target.h
      vulkan_picking_target.cpp

    ui/
      vulkan_imgui_renderer.h
      vulkan_imgui_renderer.cpp

assets/
  shaders/
    Renderer/
      Vulkan/
        vulkan_forward.hlsl
        vulkan_object_id.hlsl
```

说明：

- `Renderer` 是模块名，不是 backend 名。
- `Vulkan` 是唯一当前后端。
- `Data` 放 backend-neutral frame data。
- `Frontend` 放从 backend-neutral data 到 backend draw data 的转换逻辑。
- `Vulkan` 放 Vulkan implementation。
- `assets/shaders/Renderer/Vulkan` 放作者可编辑的 HLSL 源码。
- 构建阶段生成的 SPIR-V 继续放到运行时输出目录，例如 `bin/.../shaders/Renderer/Vulkan`。

## 5. 需要迁出的文件

### 5.1 WindowSystem 迁到 Platform

迁移前：

```text
source/Engine/Function/Renderer/window_system.h
source/Engine/Function/Renderer/window_system.cpp
source/Engine/Function/Platform/window_graphics_api.h
```

当前已变为：

```text
source/Engine/Function/Platform/window_system.h
source/Engine/Function/Platform/window_system.cpp
source/Engine/Function/Platform/window_graphics_api.h
```

原因：

- Window 是平台能力，不是渲染器能力。
- Vulkan renderer 只通过 `WindowService` 使用 native window 和 instance extensions。
- 未来如果加入 headless、multi-window、平台抽象，放在 Platform 更自然。

### 5.2 RendererService 移出 RHI

当前：

```text
source/Engine/Function/Renderer/RHI/renderer_service.h
source/Engine/Function/Renderer/RHI/renderer_service_types.h
```

建议变为：

```text
source/Engine/Function/Renderer/renderer_service.h
source/Engine/Function/Renderer/renderer_service_types.h
```

原因：

- 当前 `RendererService` 是模块 facade，不是低层 RHI。
- 保留 `RHI` 名字会让人误解为 UE 风格通用 RHI。
- D12 后没有 OpenGL backend，不需要继续维护旧 RHI 语义。

### 5.3 Camera 所有权再确认

PR-R13 前：

```text
source/Engine/Function/Renderer/camera.h
source/Engine/Function/Renderer/editor_camera.h
source/Engine/Function/Renderer/editor_camera.cpp
```

PR-R13 后：

```text
source/Engine/Editor/Camera/editor_camera.h
source/Engine/Editor/Camera/editor_camera.cpp
```

处理结果：

- 旧 `Function/Renderer/camera.h` 空基类已删除，不再保留没有行为的跨模块抽象。
- `EditorCamera` 已移动到 Editor 侧，作为编辑器 SceneView 的观察相机。
- `CameraComponent` 继续属于 Scene / Runtime，后续 GameView / Play Mode 从 active `CameraComponent` 提取相机数据。
- Renderer 目录只保留 `RenderDataPacket`、`RenderView` 这类渲染输入，不持有 camera controller 或输入行为。

### 5.4 Camera 归属建议补充

更明确地说，Camera 最好按“谁拥有行为，谁就持有它”来划分：

```text
Scene / Runtime
  Camera

Editor
  EditorCamera / CameraController

Renderer
  RenderView
```

- `Camera` 作为世界/场景中的观察对象，应该归 Scene 或 Runtime。
- `EditorCamera` 带输入、轨道、缩放、平移，应该归 Editor。
- `Renderer` 只保留 `RenderView`、`ViewProjection` 这类纯渲染输入，不持有完整 camera 行为。

这个划分的好处是：渲染器、编辑器和场景系统各自只管自己的职责，后续新增轨道相机、自由相机、预览相机时也不会把 Renderer 目录再拉回“相机工具箱”。

## 6. 推荐迁移步骤

### PR-R12.1-A：移动 RendererService

执行状态：已完成。

目标：

- `Renderer/RHI/renderer_service.*` 移到 `Renderer/renderer_service.*`。
- 更新 include。
- 删除空的 `Renderer/RHI` 目录。

验收：

- `rg "Renderer/RHI" source/Engine` 无命中。
- 构建通过。

### PR-R12.1-B：移动 RendererV2 到 Renderer/Vulkan

执行状态：已完成。

目标：

- `RendererV2` 整体移动到 `Renderer/Vulkan`。
- 更新 include path。
- 更新 CMake source list。
- 将 HLSL 源码移动到 `assets/shaders/Renderer/Vulkan`，并更新 CMake shader source/output path。

验收：

- `rg "RendererV2" source/Engine/CMakeLists.txt source/Engine` 无主线命中。
- HLSL shader 仍能编译到运行时可加载路径。
- Sandbox smoke 通过。

注意：

- 运行时 shader 输出目录已同步改成 `shaders/Renderer/Vulkan`。
- Vulkan pass 中的 shader load path 已同步更新。

### PR-R12.1-C：整理 Renderer Data / Frontend

执行状态：已完成。

完成记录：

- `RenderView` / `RenderSceneFrame` 已移动到 `Renderer/data`。
- `RenderSceneFrameBuilder` 已移动到 `Renderer/frontend`。
- `VulkanRenderDataTranslator` / `VulkanDrawList` / `VulkanDrawListBuilder` 已移动到 `Renderer/Vulkan/frontend`。
- `source/Engine/CMakeLists.txt` 和相关 include path 已同步更新。
- 已验证中立 `Renderer/data` / `Renderer/frontend` 不依赖 Vulkan backend。

目标：

- 固定 `Renderer/data` 作为上层可理解的 frame data 层。
- 固定 `Renderer/frontend` 作为 backend-neutral frame builder 层。
- 固定 `Renderer/Vulkan/frontend` 作为 Vulkan draw data 构建层。
- 让 `VulkanRendererSystem` 只编排 frame flow，不继续承担数据清洗和 draw list 构建职责。

建议：

- `render_data.h`、`render_context.h` 已在 `Renderer/data`，继续保留为模块外部数据契约。
- `render_view.h`、`render_scene_frame.h` 不包含 Vulkan handle，应该移动到 `Renderer/data`。
- `render_scene_frame_builder.*` 只把 `RenderDataPacket` 清洗成 `RenderSceneFrame`，应该移动到 `Renderer/frontend`。
- `vulkan_render_data_translator.*` 处理 Vulkan clip space / projection policy，应该移动到 `Renderer/Vulkan/frontend`。
- `vulkan_draw_list.*`、`vulkan_draw_list_builder.*` 绑定 Vulkan GPU resource，必须留在 `Renderer/Vulkan/frontend`。
- 如果后续引入 backend-neutral RenderGraph，只能依赖 `Renderer/data` 里的数据，不依赖 `Renderer/Vulkan/frontend`。

更明确的推荐目标结构：

```text
source/Engine/Function/Renderer/
  data/
    render_context.h
    render_data.h
    render_view.h
    render_scene_frame.h

  frontend/
    render_scene_frame_builder.h
    render_scene_frame_builder.cpp

  Vulkan/
    frontend/
      vulkan_render_data_translator.h
      vulkan_render_data_translator.cpp
      vulkan_draw_list.h
      vulkan_draw_list_builder.h
      vulkan_draw_list_builder.cpp

    passes/
    resources/
    targets/
    ui/
```

分层原则：

- `Renderer/data` 只放 backend-neutral frame data，不包含 Vulkan 类型。
- `Renderer/frontend` 只负责把 Scene / Editor 提交的数据整理成 renderer 可消费的场景帧。
- `Renderer/Vulkan/frontend` 只负责把 renderer frame data 转换成 Vulkan draw data。
- `Renderer/Vulkan/passes` 只负责具体 pass 的 GPU 执行逻辑。
- `Renderer/Vulkan/resources` 只负责 GPU 资源缓存和创建。
- `Renderer/Vulkan/targets` 只负责 viewport、picking、swapchain-adjacent render target。
- `Renderer/Vulkan/ui` 只负责 ImGui draw data 到 Vulkan command 的提交，不拥有 UI 系统生命周期。

推荐数据流：

```text
RenderDataPacket
  -> RenderView
  -> RenderSceneFrame
  -> VulkanDrawList
  -> Vulkan pass execution
```

PR-R12.1-C 不应该改变渲染行为，也不应该开始实现 RenderGraph。它只是把现有数据阶段摆正，让后续 RenderGraph 有一个干净的插入点。

#### 拆分步骤

1. C1：移动 backend-neutral data。
   - 将 `Renderer/Vulkan/render_view.h` 移到 `Renderer/data/render_view.h`。
   - 将 `Renderer/Vulkan/render_scene_frame.h` 移到 `Renderer/data/render_scene_frame.h`。
   - 更新 include path，确保 `Renderer/data` 不 include `Renderer/Vulkan`。

2. C2：移动 backend-neutral frontend。
   - 将 `Renderer/Vulkan/render_scene_frame_builder.*` 移到 `Renderer/frontend/render_scene_frame_builder.*`。
   - `RenderSceneFrameBuilder` 只依赖 `RenderDataPacket`、`RenderView`、`RenderSceneFrame`、`AssetHandle` 和 math 类型。
   - 不允许 builder 访问 `VulkanRenderResourceCache`、`Vk*`、descriptor、pipeline 或 command buffer。

3. C3：移动 Vulkan frontend。
   - 将 `vulkan_render_data_translator.*` 移到 `Renderer/Vulkan/frontend`。
   - 将 `vulkan_draw_list.*`、`vulkan_draw_list_builder.*` 移到 `Renderer/Vulkan/frontend`。
   - `VulkanDrawList` 可以持有 `VulkanMeshResource` / `VulkanMaterialResource` 指针，但不能暴露到 `RendererService`、Scene、Resource、Editor。

4. C4：收口 `VulkanRendererSystem` include。
   - `VulkanRendererSystem` 只 include `Renderer/data`、`Renderer/frontend`、`Renderer/Vulkan/frontend` 和 Vulkan backend 内部服务。
   - frame flow 保持为 `build view -> build scene frame -> prepare resources -> build draw list -> execute passes`。
   - 不在 `VulkanRendererSystem` 内部直接解析 `RenderDataPacket` 的 mesh / material / light 细节。

5. C5：更新 CMake 和文档。
   - 更新 `source/Engine/CMakeLists.txt` 文件列表。
   - 更新本文件和 roadmap 的 PR 状态。
   - 保留 `Renderer/data` / `Renderer/frontend` / `Renderer/Vulkan/frontend` 的边界说明。

验收：

- `rg "Function/Renderer/Vulkan/render_view|Function/Renderer/Vulkan/render_scene_frame|Function/Renderer/Vulkan/render_scene_frame_builder" source/Engine` 无命中。
- `rg "Vk|Vulkan" source/Engine/Function/Renderer/data source/Engine/Function/Renderer/frontend` 无 Vulkan native 类型或 Vulkan backend include 命中。
- `rg "vulkan_draw_list|VulkanDrawList|VulkanMeshResource|VulkanMaterialResource" source/Engine/Editor source/Engine/Function/Scene source/Engine/Function/Resource` 无命中。
- 构建通过，Sandbox smoke 通过。

#### 后续 RenderGraph 接入方向

后续如果要做 RenderGraph，建议接在 `VulkanDrawList` 之后、具体 pass 执行之前：

```text
RenderDataPacket
  -> RenderView
  -> RenderSceneFrame
  -> VulkanDrawList
  -> VulkanRenderGraph
  -> Vulkan command buffer
```

第一版 RenderGraph 建议做成 Vulkan-only，不急着抽象成通用 RHI graph：

```text
Renderer/Vulkan/graph/
  vulkan_render_graph.h
  vulkan_render_graph.cpp
  vulkan_render_graph_builder.h
  vulkan_render_graph_builder.cpp
  vulkan_render_graph_pass.h
  vulkan_graph_resource.h
  vulkan_graph_executor.h
  vulkan_graph_executor.cpp
```

RenderGraph 的输入不应该回头读取 `RenderDataPacket`，也不应该直接访问 Scene / Editor。它只消费已经准备好的 renderer frame 数据：

- view：来自 `RenderView`。
- scene：来自 `RenderSceneFrame`。
- draw data：来自 `VulkanDrawList`。
- resources：来自 `VulkanRenderResourceCache` 和 graph 内部声明的 transient resources。

第一版只解决必要问题：

- pass 顺序声明。
- pass 读写资源声明。
- color / depth / object id target 管理。
- image layout transition。
- Vulkan 1.3 dynamic rendering begin/end。
- transient render target 生命周期。

暂时不要做：

- async compute。
- multi-queue scheduling。
- resource aliasing。
- 完整通用 RHI。
- 复杂 shader reflection binding 系统。

最小可用 pass 模型可以类似：

```cpp
graph.addPass("ObjectId")
    .write(object_id_target)
    .write(depth_target)
    .execute(...);

graph.addPass("Forward")
    .read(draw_list)
    .write(scene_color)
    .write(depth_target)
    .execute(...);

graph.addPass("ImGui")
    .read(scene_color)
    .write(swapchain)
    .execute(...);
```

这样 `VulkanRendererSystem` 后续可以逐步变成 frame orchestrator：

```text
prepare frame data
prepare resources
build graph
execute graph
present
```

而不是继续把 pass 调度、资源转换、layout barrier、UI 合成、swapchain present 全部堆在一个类里。

### PR-R12.1-D：移动 WindowSystem 到 Platform

执行状态：已完成。

完成记录：

- `window_system.h` / `.cpp` 已移动到 `Function/Platform`。
- `platform_module.cpp` 和 `global_context.cpp` 已改用 `Function/Platform/window_system.h`。
- `source/Engine/CMakeLists.txt` 已将 `window_system.*` 从 Renderer 文件组移动到 Function / Platform 文件组。
- Renderer 源码不再 include 或引用 concrete `WindowSystem`。

背景：

`WindowSystem` 已经由 `PlatformModule` 创建、持有、注册，`RendererModule` 也只通过 `WindowService` 消费窗口信息。PR-R12.1-D 将源码文件从 `Renderer/window_system.*` 移到 `Platform/window_system.*`，只做所有权和目录语义收口，不改变窗口创建、输入、事件和 Vulkan surface 初始化行为。

目标：

- 将 `Function/Renderer/window_system.h` / `.cpp` 移到 `Function/Platform/window_system.h` / `.cpp`。
- `PlatformModule` 使用新路径 include `WindowSystem`。
- `GlobalContext` 兼容桥使用新路径 include `WindowSystem`。
- `source/Engine/CMakeLists.txt` 将 `window_system.*` 从 Renderer 文件组移动到 Platform / Function 文件组。
- Renderer 模块继续只依赖 `WindowService`，不直接 include `WindowSystem`。

拆分步骤：

1. D1：移动源码文件。
   - `source/Engine/Function/Renderer/window_system.h` -> `source/Engine/Function/Platform/window_system.h`。
   - `source/Engine/Function/Renderer/window_system.cpp` -> `source/Engine/Function/Platform/window_system.cpp`。
   - namespace 和类名保持 `NexAur::WindowSystem` 不变。

2. D2：更新 include path。
   - `platform_module.cpp` 改为 include `Function/Platform/window_system.h`。
   - `global_context.cpp` 改为 include `Function/Platform/window_system.h`。
   - 搜索并清理所有 `Function/Renderer/window_system.h` 和 `Renderer/window_system` 命中。

3. D3：更新 CMake 文件分组。
   - 从 `NEXAUR_ENGINE_RENDERER_FILES` 移除 `Function/Renderer/window_system.h` / `.cpp`。
   - 加入 `NEXAUR_ENGINE_FUNCTION_FILES` 的 Platform 区域。
   - 保持 `WindowGraphicsAPI` 和 `WindowService` 仍位于 Platform。

4. D4：保持兼容桥不变。
   - `PlatformModule` 仍可临时注册 concrete `WindowSystem` 服务。
   - `RunTimeGlobalContext` / `GlobalContext` 兼容桥仍可保存 `std::shared_ptr<WindowSystem>`。
   - 本 PR 不删除 `WindowSystem` concrete service 注册，避免把兼容桥清理混入目录迁移。

5. D5：验证依赖方向。
   - Renderer backend 只能通过 `WindowService` 获取 native window、graphics API 和 Vulkan instance extensions。
   - `WindowSystem` 不知道 renderer pass、swapchain frame、viewport target、ImGui renderer backend。
   - `InputSystemGLFW` 仍从 `PlatformModule` 拿 GLFW native window 初始化。

不做：

- 不修改 `WindowService` 接口。
- 不修改 GLFW no-api window 创建策略。
- 不修改 Vulkan surface / swapchain 初始化逻辑。
- 不删除 `RunTimeGlobalContext` 对 `WindowSystem` 的兼容字段。
- 不把 `WindowSystem` 暴露给 Renderer / Editor 新代码使用。

验收：

- `rg "Renderer/window_system|Function/Renderer/window_system" source/Engine` 无命中。
- `rg "WindowSystem" source/Engine/Function/Renderer` 只允许出现在明确历史文档或无命中；Renderer 源码应继续依赖 `WindowService`。
- Window 仍创建 GLFW no-api window。
- VulkanRendererSystem 仍通过 `WindowService` 初始化 surface。
- `cmake --preset msvc-vcpkg` 通过。
- `cmake --build --preset msvc-vcpkg-debug` 通过。
- Sandbox 3 秒 smoke 通过。

### PR-R12.1-E：文档与命名清理

执行状态：已完成。

完成记录：

- `source/Engine`、`CMakeLists.txt`、`CMakePresets.json`、`vcpkg.json` 中已无 `RendererV2` / `renderer-v2` / `NEXAUR_BUILD_RENDERER_V2` 主线命中。
- `renderer_vulkan_development_roadmap.md` 当前态、总体路线和阶段看板已改为 `Renderer/Vulkan` / `Vulkan renderer` 表达。
- `renderer_interface_redesign_plan.md` 已标记 OpenGL 接口泄漏为历史问题，并同步当前 `ViewportOutputKind`。
- `renderer_dependency_cmake_plan.md` 和 `ark_renderer_integration_plan.md` 已补充当前落地状态与迁移期命名说明。

目标：

- 更新 `renderer_vulkan_development_roadmap.md`。
- 更新接口设计文档和 CMake 依赖文档。
- 将 `RendererV2` 统一替换为 `Vulkan renderer`、`Renderer/Vulkan` 或 `Vulkan backend`。

验收：

- 文档中只在历史记录段落保留 `RendererV2`。
- 代码和 CMake 中不再出现 `RendererV2`。

## 7. 不建议做的事情

### 7.1 不要立刻设计庞大通用 RHI

当前只有 Vulkan 后端，而且旧 OpenGL RHI 已经证明容易堆出不清爽的抽象。建议先把 Vulkan backend 做顺，等出现真实第二后端需求时再提炼通用层。

### 7.2 不要让 Editor 直接依赖 Vulkan

Editor viewport 应继续只依赖：

- `RendererService::getViewportOutput()`
- `RendererService::pickViewport()`

不要在 Editor 中 include Vulkan header，也不要让 Editor 持有 `VkDescriptorSet`。

### 7.3 不要让 Resource 创建 GPU 对象

`AssetManager` 继续只管理资产身份和 CPU 资产。GPU resource cache 由 Renderer backend 私有管理。

### 7.4 不要把 Platform 和 Renderer 重新绑死

`WindowSystem` 可以提供 Vulkan required extensions，但不应该知道 renderer pass、swapchain frame、ImGui renderer backend 等逻辑。

## 8. 目标依赖方向

推荐最终依赖方向：

```text
Scene / Editor / Runtime
  -> RendererService + RenderDataPacket

RendererModule
  -> RendererService
  -> VulkanRendererSystem

VulkanRendererSystem
  -> WindowService
  -> RenderDataPacket
  -> AssetManager
  -> Vulkan backend internals

PlatformModule
  -> WindowSystem
  -> WindowService
```

禁止方向：

```text
Scene -> Vulkan/*
Editor -> Vulkan/*
Resource -> Vulkan/*
Platform -> Renderer/Vulkan/*
```

## 9. 验收标准

- `RendererV2` 不再作为源码目录存在。
- `Renderer/RHI` 不再作为 facade 目录存在。
- `RendererService` 位于清晰的 renderer public boundary。
- Vulkan backend 位于 `Renderer/Vulkan`。
- WindowSystem 位于 Platform。
- Scene / Resource / Editor 公开接口不 include Vulkan backend header。
- `cmake --preset msvc-vcpkg` 通过。
- `cmake --build --preset msvc-vcpkg-debug` 通过。
- Sandbox smoke 启动通过。

## 10. 建议优先级

建议按以下顺序执行：

1. 移动 `RendererService`，先消除 `RHI` 命名误导。
2. 移动 `RendererV2` 到 `Renderer/Vulkan`，消除版本目录。（已完成）
3. 修正 CMake 和 shader 输出路径。
4. 移动 `WindowSystem` 到 Platform。
5. 评估 `Camera` / `EditorCamera` 所有权。
6. 更新文档和 roadmap。

这样做的好处是每一步都比较机械、可验证，不会在目录移动时混入新的渲染功能风险。

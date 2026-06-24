# NexAur 渲染模块重构与 Vulkan 接入开发路线

日期：2026-06-24
分支：`vulkanRenderer`

## 1. 文档定位

本文档是渲染模块重构与 ARKRenderer 接入的开发进度同步文档。后续每一轮 PR / 阶段完成后，都优先更新本文档的状态。

参考文档：

- `ark_renderer_integration_plan.md`：总体架构和接入边界。
- `renderer_interface_redesign_plan.md`：渲染模块对外接口设计。
- `renderer_dependency_cmake_plan.md`：vcpkg 包管理和 CMake 构建方案。

本文档不重复展开所有设计细节，而是回答：

```text
下一步做什么？
做到什么程度算完成？
完成后需要验证什么？
当前进度在哪里？
```

## 2. 总体路线

推荐路线：

```text
文档与方向确认
  -> RendererService 接口清理
  -> vcpkg / CMake 构建统一
  -> WindowSystem 支持 OpenGL / Vulkan 创建策略
  -> RendererV2 / ArkVulkanRendererSystem 空后端
  -> RenderDataPacket -> RenderView
  -> ArkRenderResourceCache
  -> RenderDataPacket -> RenderScene
  -> Editor viewport 过渡
  -> Vulkan viewport image
  -> Vulkan picking
  -> OpenGL legacy 退役
```

核心原则：

- 不继续把旧 OpenGL RHI 打磨成长期通用 RHI。
- `RendererService` 是 NexAur 对外唯一渲染入口。
- `RenderDataPacket` 继续作为 Scene / Editor / Runtime 到 Renderer 的数据契约。
- ARKRenderer 通过 adapter 接入，不把 Vulkan 类型泄漏到 Scene / Resource / Editor。
- `externalRenderer/` 只作为临时本地参考目录，不作为最终引擎模块源码边界。
- 新渲染模块落在 `source/Engine/Function/RendererV2/`，参考 ARKRenderer 架构并按 NexAur 模块系统重新适配。
- 顶层工程升级到 C++20，避免引擎和新渲染模块出现语言标准双轨。
- 顶层统一使用 vcpkg 管理第三方依赖，逐步移除或隔离重复的 vendored 依赖。
- RendererV2 / Vulkan 路径后续统一使用 HLSL 编写新着色器，并通过 DXC 编译为 SPIR-V。
- OpenGL 只作为迁移期 fallback，Vulkan 稳定后退役。
- 每个阶段都保持可构建，可回退，可验证。

## 3. 当前状态

```text
当前分支：vulkanRenderer
当前阶段：D4 Window graphics API 策略已完成
代码状态：窗口系统已支持 OpenGL context 与 Vulkan no-api window 两种创建策略
OpenGL 后端：仍为当前可运行主路径，并保留 legacy fallback
ARKRenderer：`externalRenderer/` 仅作为临时本地参考目录
```

已完成：

- 新增总体方案文档。
- 新增接口重构方案文档。
- 新增 vcpkg / CMake 构建方案文档。
- 新建 `vulkanRenderer` 开发分支。
- D1：完成 `RendererService` 后端无关 viewport / picking 契约。
- D2：完成 `ViewportPanel` 对新 viewport output / picking 契约的适配。
- D3：完成顶层 C++20、vcpkg manifest / preset、依赖来源分支和特殊依赖收口。
- D4：完成 Window graphics API 策略，OpenGL / Vulkan no-api window 可按配置选择。

已确认：

- `externalRenderer/` 不直接作为最终集成目录，后续只作为架构和实现参考。
- 新渲染模块在 `source/Engine/Function/RendererV2/` 中重构，不在旧 `Renderer/` 目录里继续堆叠大改。
- 顶层直接升级到 C++20，作为 NexAur 后续主线标准。
- 顶层统一改为 vcpkg manifest 管理第三方依赖；旧 `external/` 依赖按阶段隔离、替换或移除。

需要注意：

- `RendererV2` 初期可以和旧 `Renderer` 并存，避免一次性破坏当前 OpenGL 可运行路径。
- vcpkg 迁移时要先处理依赖重复问题，尤其是 GLFW、glm、spdlog、imgui、assimp、Vulkan SDK 相关依赖。
- C++20 升级应在 D3 集中落地并验证全量构建，避免某些旧第三方头文件或编译选项在中途散落修改。

## 4. 阶段看板

| 阶段 | 状态 | 目标 | 主要参考 |
| --- | --- | --- | --- |
| D0 文档与方向确认 | 已完成 | 固定路线、拆分参考文档、决定 externalRenderer 归属 | 本文档 / 总方案 |
| D1 RendererService 接口清理 | 已完成 | 降级 OpenGL texture id 接口，新增后端无关 viewport / picking 契约 | 接口文档 |
| D2 ViewportPanel 适配新输出 | 已完成 | Editor 不再假设 viewport 是 OpenGL texture | 接口文档 |
| D3 vcpkg / CMake 预研落地 | 已完成 | 顶层 C++20 + vcpkg 方案可支撑 RendererV2 | 构建文档 |
| D4 Window graphics API 策略 | 已完成 | OpenGL context 与 Vulkan no-api window 可按 backend 选择 | 总方案 / 构建文档 |
| D5 RendererV2 / ArkVulkanRendererSystem 骨架 | 待开始 | 在 RendererV2 中创建新 Vulkan 渲染模块骨架，跑通空场景 / swapchain | 总方案 |
| D6 RenderView 转换 | 待开始 | NexAur 相机驱动 ARKRenderer | 总方案 |
| D7 ArkRenderResourceCache | 待开始 | AssetHandle 解析到 ARK renderer resources | 总方案 |
| D8 RenderScene 转换 | 待开始 | NexAur 场景模型提交给 ARKRenderer | 总方案 |
| D9 Editor viewport 过渡 | 待开始 | Vulkan backend 下 Editor 不崩溃，有明确输出策略 | 接口文档 |
| D10 Vulkan viewport image | 待开始 | Editor viewport 显示 Vulkan offscreen image | 接口文档 |
| D11 Vulkan picking | 待开始 | ObjectId pass / readback 接入 | 接口文档 |
| D12 OpenGL legacy 退役 | 待开始 | 默认 Vulkan，旧 OpenGL 移除或隔离 | 总方案 |

状态定义：

```text
待开始：没有代码改动
进行中：已开始实现或文档同步
阻塞：需要决策或外部条件
已完成：代码、验证和文档同步均完成
```

## 5. 阶段细化

### D0：文档与方向确认

目标：

- 三份参考文档完成。
- 本开发路线文档完成。
- 明确 `externalRenderer/`、C++20、vcpkg 和 `RendererV2` 的基础决策。
- 明确下一步从接口清理和构建基线开始。

任务：

- 新增 `renderer_vulkan_development_roadmap.md`。
- 在三份参考文档中标注本文档为进度主文档。
- 确认当前分支为 `vulkanRenderer`。
- 确认 `externalRenderer/` 为临时参考目录，新实现落在 `RendererV2`。
- 确认顶层升级 C++20，并统一迁移到 vcpkg manifest。

验收：

- `docs/plans/Renderer/` 下文档结构清楚。
- 后续 PR 可以按本文档逐项推进。

当前状态：

```text
已完成
```

### D1：RendererService 接口清理

目标：

- `RendererService` 建立后端无关的 viewport / picking 契约。
- OpenGL texture id 接口从核心接口降级为兼容 wrapper。
- 新接口可以表达 OpenGL legacy、Vulkan external swapchain、Vulkan viewport image、无输出等状态。

任务：

- 新增 `RendererBackendType`。
- 新增 `ViewportOutputKind`。
- 新增 `ViewportOutput`。
- 新增 `ViewportPickRequest` / `ViewportPickResult`。
- `RendererService` 增加：
  - `getBackendType()`
  - `getViewportOutput()`
  - `pickViewport()`
- 旧 `getViewportColorAttachment()` / `readViewportEntityID()` 暂时保留为 legacy wrapper，避免 D1 同时改动 Editor viewport。
- 当前 OpenGL `RendererSystem` 实现新接口：
  - `getBackendType()` 返回 `OpenGLLegacy`
  - `getViewportOutput()` 返回 `OpenGLTexture`
  - `pickViewport()` 包装旧 readPixel picking

验收：

- OpenGL 当前行为保持不变。
- 新接口不暴露 OpenGL / Vulkan 原生类型。
- `ViewportPanel` 可以暂时继续使用旧 wrapper，真正迁移放到 D2。
- 构建通过。

当前状态：

```text
已完成
验证：cmake --build build --config Debug 通过
备注：现有 C4251 等 DLL 导出警告仍存在，非本次 D1 新增问题
```

建议验证：

```powershell
cmake --build build --config Debug
```

### D2：ViewportPanel 适配新输出

目标：

- Editor viewport 通过 `ViewportOutput` 显示内容。
- Vulkan backend 未提供 viewport image 时，Editor 可以显示 placeholder 或状态文本。

任务：

- `ViewportPanel` 改用 `RendererService::getViewportOutput()`。
- OpenGL legacy 路径使用 `ViewportOutputKind::OpenGLTexture`。
- Vulkan 早期路径支持 `None` 或 `ExternalSwapchain`。
- picking 改用 `pickViewport()`。

验收：

- OpenGL viewport 正常显示。
- texture id 为 0 或无输出时 Editor 不崩溃。
- picking 仍能在 OpenGL 路径工作。

当前状态：

```text
已完成
验证：cmake --build build --config Debug 通过
备注：现有 C4251 等 DLL 导出警告仍存在，非本次 D2 新增问题
```

### D3：vcpkg / CMake 预研落地

目标：

- NexAur 顶层统一具备 vcpkg 构建路径。
- NexAur 顶层升级到 C++20。
- `RendererV2` 能使用 ARKRenderer 所需的第三方依赖。
- vcpkg 成为新默认依赖路径，旧 `external/` 只作为过渡 fallback 保留。
- 尽量在 D3 解决构建地基问题，避免后续接 RendererV2 时混入双依赖、双 ImGui、双标准等隐患。

任务：

- 新增顶层 `vcpkg.json`。
- 新增顶层 `CMakePresets.json`。
- CMake 最低版本评估并提升到 3.25。
- C++ 标准提升到 C++20。
- 新增：
  - `NEXAUR_USE_VCPKG_DEPS`
  - `NEXAUR_BUILD_RENDERER_V2`
  - `NEXAUR_BUILD_LEGACY_OPENGL`
- 梳理旧 `external/` 与 vcpkg 重复依赖，先隔离重复构建，再逐步替换。
- 不把 `externalRenderer/` 直接作为长期子目录接入；只抽取或重写需要进入 `RendererV2` 的代码和设计。

建议拆分：

```text
D3-A：C++20 基线
  -> 顶层 CMake 提升到 3.25
  -> 顶层 / NexAurEngine 使用 C++20
  -> 仍走当前 external 构建，先确认旧 OpenGL 路径不坏

D3-B：vcpkg manifest / preset
  -> 新增 vcpkg.json
  -> 新增 CMakePresets.json
  -> preset 使用 build/msvc-vcpkg，避免污染当前 build/

D3-C：依赖来源分支
  -> NEXAUR_USE_VCPKG_DEPS=ON 默认走 find_package
  -> NEXAUR_USE_VCPKG_DEPS=OFF 保留 add_subdirectory(external) fallback
  -> vcpkg 模式不再构建 external/glfw、external/glm、external/spdlog、external/imgui、external/assimp

D3-D：特殊依赖收口
  -> stb：vcpkg 通常只提供 header，需要 NexAur 自己提供一个 stb_image implementation translation unit
  -> glad：OpenGL legacy 仍需要 loader，优先尝试 vcpkg glad；如果 target/profile 不匹配，只允许作为 legacy isolated exception 记录下来
  -> imgui：vcpkg feature 同时启用 glfw/opengl3/vulkan binding，但 D3 不初始化 Vulkan ImGui backend

D3-E：验证和文档同步
  -> vcpkg preset 配置通过
  -> vcpkg Debug 构建通过
  -> 必要时 legacy external 构建仍可回退
  -> 更新本文档状态和遗留风险
```

主要改动点：

- `CMakeLists.txt`
  - `cmake_minimum_required(VERSION 3.25)`
  - C++20 基线
  - 新增构建选项
  - vcpkg 模式下不 `add_subdirectory(external)`
- `source/Engine/CMakeLists.txt`
  - vcpkg 模式下 `find_package`
  - vcpkg 模式下链接 imported targets
  - legacy 模式保留当前 external include / link
- `vcpkg.json`
  - 合并当前 NexAur 依赖和后续 RendererV2 依赖
- `CMakePresets.json`
  - 新增 `msvc-vcpkg` 配置和 Debug / Release build preset
- 可能新增 `source/Engine/ThirdParty/stb_image.cpp`
  - 只负责 `STB_IMAGE_IMPLEMENTATION`
  - 避免继续依赖 `external/stb/stb_image.cpp`

D3 不做：

- 不 `add_subdirectory(externalRenderer)`。
- 不创建完整 `RendererV2` 后端。
- 不初始化 Vulkan ImGui backend。
- 不删除旧 `external/` 目录和 submodule。
- 不删除 OpenGL legacy。

验收：

- 默认 OpenGL 路径仍可构建。
- vcpkg preset 可配置。
- 不重复构建 glfw / glm / spdlog / imgui 等依赖。
- 顶层 C++20 构建通过。
- `cmake --preset msvc-vcpkg` 通过。
- `cmake --build --preset msvc-vcpkg-debug` 通过。
- 如果保留 legacy fallback，则 `NEXAUR_USE_VCPKG_DEPS=OFF` 路径仍可构建。

当前状态：

```text
已完成
验证：
- cmake -S . -B build -DNEXAUR_USE_VCPKG_DEPS=OFF 通过
- cmake --build build --config Debug 通过
- cmake --preset msvc-legacy 通过
- cmake --build --preset msvc-legacy-debug 通过
- cmake --preset msvc-vcpkg 通过
- cmake --build --preset msvc-vcpkg-debug 通过
备注：
- vcpkg 模式已使用 find_package，不再 add_subdirectory(external)
- legacy external fallback 保留，用于 OpenGL 迁移期回退
- preset 产物已通过 NEXAUR_OUTPUT_ROOT 分流到 bin/msvc-vcpkg 与 bin/msvc-legacy
- ImGui backend include 已改成兼容 vcpkg / vendored 布局的写法
- legacy vendored Assimp 仍可能保留自身历史输出到 bin/Debug，后续 external cleanup 阶段处理
```

### D4：Window graphics API 策略

目标：

- Window 创建策略根据 renderer backend 选择。
- OpenGL 路径继续创建 OpenGL context。
- Vulkan 路径创建 GLFW no-api window，不再由 WindowSystem 负责 swap buffers。
- UI 在 Vulkan no-api 过渡期不初始化 OpenGL ImGui renderer backend，避免错误绑定图形 API。

任务：

- `WindowSpecification` 增加 graphics API / client API 字段。
- OpenGL legacy 创建 OpenGL context。
- ArkVulkan 创建 GLFW no-api window。
- `WindowService::getNativeWindow()` 保持稳定。
- `WindowService` 暴露当前 `WindowGraphicsAPI`。
- `WindowService` 提供 GLFW 所需 Vulkan instance extensions 查询。
- 新增 `NEXAUR_DEFAULT_GRAPHICS_API` 构建配置，默认 `OpenGL`，可用 `Vulkan` 验证 no-api window 编译路径。

验收：

- OpenGL legacy 仍能启动。
- ArkVulkan path 能创建 Vulkan surface。
- vcpkg / OpenGL 默认路径构建通过。
- legacy / OpenGL fallback 构建通过。
- `NEXAUR_DEFAULT_GRAPHICS_API=Vulkan` 编译路径通过。

当前状态：

```text
已完成
验证：
- cmake --preset msvc-vcpkg 通过
- cmake --build --preset msvc-vcpkg-debug 通过
- cmake --preset msvc-legacy 通过
- cmake --build --preset msvc-legacy-debug 通过
- cmake -S . -B build/msvc-vcpkg-vulkan-window ... -DNEXAUR_DEFAULT_GRAPHICS_API=Vulkan 通过
- cmake --build build/msvc-vcpkg-vulkan-window --config Debug 通过
备注：
- 默认仍为 OpenGL，D4 不默认切换 Vulkan runtime。
- Vulkan no-api 下 UI 只启用 ImGui GLFW platform backend；正式 Vulkan ImGui renderer 留到 D10。
- Vulkan surface 创建留给 D5 RendererV2 后端完成。
```

### D5：RendererV2 / ArkVulkanRendererSystem 骨架

目标：

- 在 `source/Engine/Function/RendererV2/` 下新增 Vulkan 渲染模块骨架。
- 参考 ARKRenderer 的 facade / scene / view / resource cache 结构，但按 NexAur 模块系统重新组织。
- 先跑通 renderer 生命周期。

任务：

- 新增 `source/Engine/Function/RendererV2/`。
- 新增 `ArkVulkanRendererSystem`。
- 新增 `ArkRenderDataTranslator`。
- 新增 `ArkRenderResourceCache`。
- 实现 `RendererService`。
- 初始化 Vulkan renderer facade。
- `render()` 提交空场景或 fallback scene。
- `getViewportOutput()` 返回 `ExternalSwapchain` 或 `None`。
- `pickViewport()` 返回 unsupported。

验收：

- ArkVulkan backend 能启动和关闭。
- Vulkan device / swapchain 创建成功。
- 无资源泄漏或关闭崩溃。

### D6：RenderDataPacket -> RenderView

目标：

- NexAur 相机数据驱动 ARKRenderer。

任务：

- 新增 `ArkRenderDataTranslator`。
- 实现 `buildRenderView()`。
- 核对相机矩阵、投影矩阵、Vulkan Y flip、depth range。

验收：

- 相机方向正确。
- 窗口 resize 后 projection 正确。
- 空场景渲染稳定。

### D7：ArkRenderResourceCache

目标：

- Vulkan 路径拥有独立 renderer resource cache。

任务：

- 新增 `ArkRenderResourceCache`。
- `AssetHandle -> ark::ModelResource`。
- 使用 AssetManager 获取路径。
- 第一版可复用 ARKRenderer loader。
- 支持 `clear()` / shutdown。

验收：

- 同一模型不会重复创建 GPU resource。
- 关闭时资源释放稳定。

### D8：RenderDataPacket -> RenderScene

目标：

- NexAur scene objects 提交给 ARKRenderer。

任务：

- 遍历 opaque / transparent objects。
- 调用 `RenderScene::addModel()`。
- 转换 directional light。
- 转换 environment intensity。
- 后续接 environment resource。

验收：

- Vulkan backend 显示 NexAur 场景模型。
- Transform 正确。
- 光照方向基本正确。

### D9：Editor viewport 过渡

目标：

- Vulkan backend 下 Editor 可以稳定运行。

任务：

- ViewportPanel 识别 Vulkan external swapchain / no output。
- 显示明确状态。
- 禁用或提示 picking 暂不可用。

验收：

- Editor UI 不崩溃。
- 用户能理解当前 Vulkan viewport 状态。

### D10：Vulkan viewport image

目标：

- Editor viewport 显示 Vulkan offscreen image。

任务：

- ARKRenderer 支持 offscreen viewport target。
- 暴露 backend-owned image / descriptor token。
- NexAur UI 使用 Vulkan ImGui descriptor 显示。
- 统一 UI backend 生命周期。

验收：

- ViewportPanel 显示 Vulkan 渲染画面。
- Resize 稳定。
- 不与 ImGui backend 冲突。

### D11：Vulkan picking

目标：

- Vulkan backend 支持 entity picking。

任务：

- ARKRenderer 增加 ObjectId pass。
- RenderScene / draw item 携带 entity id。
- 输出 integer target。
- readback 到 CPU。
- `pickViewport()` 返回实体 ID。

验收：

- Editor 点击 viewport 可选中实体。

### D12：OpenGL legacy 退役

目标：

- Vulkan 成为默认主路径。
- OpenGL 代码移入 legacy 或删除。

任务：

- 删除旧 OpenGL-only public interface。
- 删除或隔离 `RendererFactory` / `RendererCommand`。
- 清理 OpenGL resource cache。
- 清理 glad / OpenGL ImGui backend 依赖。

验收：

- 默认 backend 为 ArkVulkan。
- OpenGL-only 类型不再出现在 Scene / Resource / Editor 公开接口。

## 6. 每个 PR 的固定流程

每个阶段建议按下面流程推进：

```text
1. 更新本文档：将阶段标记为进行中
2. 实现代码
3. 自查接口边界
4. 构建验证
5. 必要时运行 Sandbox / Editor 手动验证
6. 代码审查
7. 更新本文档：记录完成内容、验证结果、遗留问题
8. 提交
```

代码审查重点：

- 是否引入新的 OpenGL / Vulkan 类型泄漏。
- 是否让 Editor / Scene / Resource 依赖 backend internal。
- 是否保留了清晰 fallback。
- 是否更新了对应文档。
- 是否保持当前可运行路径。

## 7. 风险登记

| 风险 | 影响 | 当前策略 |
| --- | --- | --- |
| vcpkg 与 external 依赖重复 | 构建冲突、符号重复 | 顶层统一 vcpkg，external 逐步退役 |
| C++17 / C++20 不一致 | ARKRenderer header 编译失败 | 推荐 NexAur 升级 C++20 |
| ImGui OpenGL / Vulkan backend 冲突 | Editor viewport 无法显示或崩溃 | Vulkan 早期不嵌入 Editor viewport |
| viewport output 仍绑定 OpenGL texture id | Vulkan 被迫模拟 OpenGL | D1 / D2 优先清理接口 |
| AssetManager 与 ARK loader 双轨 | 重复加载、缓存复杂 | 第一版接受重复，中期统一 CPU asset contract |
| picking 延迟或不可用 | Editor 交互退化 | 先 graceful fallback，后续 D11 实现 |

## 8. 当前下一步

推荐下一步：

```text
D5 开始：RendererV2 / ArkVulkanRendererSystem 骨架
D6 准备：RenderDataPacket -> RenderView
```

D5 / D6 开始前的已确认前提：

- `externalRenderer/` 仅作为临时本地参考目录。
- 新渲染模块在 `source/Engine/Function/RendererV2/` 中重构。
- NexAur 顶层已升级到 C++20。
- NexAur 顶层已具备 vcpkg manifest / preset 构建路径。
- RendererV2 新增 shader 统一使用 HLSL -> SPIR-V 流程。
- WindowService 已能提供 graphics API 状态和 Vulkan instance extensions。
- Vulkan no-api window 编译路径已验证通过。
- OpenGL legacy 只作为过渡 fallback，不再继续增强。

## 9. 进度记录

### 2026-06-24

- 创建 `vulkanRenderer` 分支。
- 新增 ARKRenderer 接入总方案。
- 新增 RendererService 接口重构方案。
- 新增 vcpkg / CMake 构建方案。
- 新增本文档作为后续开发路线与进度同步入口。
- 确认 `externalRenderer/` 为临时参考目录，后续实现落在 `source/Engine/Function/RendererV2/`。
- 确认顶层升级 C++20，并统一迁移到 vcpkg manifest。
- 完成 D1：新增 `RendererBackendType`、`ViewportOutput`、`ViewportPickRequest` / `ViewportPickResult`。
- 完成 D1：`RendererService` 新增 `getBackendType()`、`getViewportOutput()`、`pickViewport()`。
- 完成 D1：旧 `getViewportColorAttachment()` / `readViewportEntityID()` 降级为迁移期 wrapper。
- 完成 D1：OpenGL `RendererSystem` 实现新 viewport / picking 契约。
- 验证 D1：`cmake --build build --config Debug` 通过。
- 完成 D2：`ViewportPanel` 改用 `RendererService::getViewportOutput()` 显示 viewport。
- 完成 D2：OpenGL 路径通过 `ViewportOutputKind::OpenGLTexture` 继续显示。
- 完成 D2：`None` / `ExternalSwapchain` / `VulkanImGuiTexture` 输出走安全 placeholder。
- 完成 D2：picking 改用 `RendererService::pickViewport()`，unsupported / not ready 不会清空当前选择。
- 验证 D2：`cmake --build build --config Debug` 通过。
- 完成 D3：顶层 `cmake_minimum_required` 提升到 3.25，C++ 标准提升到 C++20。
- 完成 D3：新增顶层 `vcpkg.json`，合并 OpenGL legacy 与后续 RendererV2 需要的第三方依赖。
- 完成 D3：新增 `CMakePresets.json`，提供 `msvc-vcpkg` 与 `msvc-legacy` 两条固定构建入口。
- 完成 D3：新增 `NEXAUR_USE_VCPKG_DEPS`、`NEXAUR_BUILD_LEGACY_OPENGL`、`NEXAUR_BUILD_RENDERER_V2` 构建选项。
- 完成 D3：新增 `NEXAUR_OUTPUT_ROOT`，让 vcpkg / legacy preset 的主产物输出目录互相隔离。
- 完成 D3：vcpkg 模式改用 `find_package` / imported targets，legacy 模式继续 `add_subdirectory(external)`。
- 完成 D3：新增 `source/Engine/ThirdParty/stb_image.cpp`，在 vcpkg 模式下由引擎侧提供 `STB_IMAGE_IMPLEMENTATION`。
- 完成 D3：`ui_system.cpp` 的 ImGui backend include 改为兼容 vcpkg / vendored 布局的写法。
- 确认后续 RendererV2 新增 shader 统一使用 HLSL，并通过 DXC 编译为 SPIR-V。
- 验证 D3：`cmake --preset msvc-vcpkg` 与 `cmake --build --preset msvc-vcpkg-debug` 通过。
- 验证 D3：`cmake --preset msvc-legacy` 与 `cmake --build --preset msvc-legacy-debug` 通过。
- 完成 D4：新增 `WindowGraphicsAPI`，窗口可选择 OpenGL 或 Vulkan。
- 完成 D4：`WindowSpecification` 增加 `graphics_api` 与 `enable_vsync`。
- 完成 D4：`WindowSystem` 在 OpenGL 路径创建 OpenGL context，在 Vulkan 路径创建 GLFW no-api window。
- 完成 D4：`WindowService` 增加 `getGraphicsAPI()` 与 `getRequiredVulkanInstanceExtensions()`。
- 完成 D4：新增 `NEXAUR_DEFAULT_GRAPHICS_API`，默认 OpenGL，可配置 Vulkan 验证 no-api window 编译路径。
- 完成 D4：UI 系统在 Vulkan no-api 过渡期只启用 ImGui GLFW platform backend，不初始化 OpenGL renderer backend。
- 验证 D4：`cmake --preset msvc-vcpkg` 与 `cmake --build --preset msvc-vcpkg-debug` 通过。
- 验证 D4：`cmake --preset msvc-legacy` 与 `cmake --build --preset msvc-legacy-debug` 通过。
- 验证 D4：`NEXAUR_DEFAULT_GRAPHICS_API=Vulkan` 独立 vcpkg 编译路径通过。

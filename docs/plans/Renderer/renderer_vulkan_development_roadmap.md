# NexAur 渲染模块重构与 Vulkan 接入开发路线

日期：2026-06-24
分支：`vulkanRenderer`

## 1. 文档定位

本文档是渲染模块重构与 Vulkan 渲染器接入的开发进度同步文档。后续每一轮 PR / 阶段完成后，都优先更新本文档的状态。

参考文档：

- `ark_renderer_integration_plan.md`：总体架构和 externalRenderer 参考边界。
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
  -> RendererV2 / VulkanRendererSystem 空后端
  -> RenderDataPacket -> RenderView
  -> VulkanRenderResourceCache
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
- 新 Vulkan renderer 通过 RendererV2 adapter 接入，不把 Vulkan 类型泄漏到 Scene / Resource / Editor。
- `externalRenderer/` 只作为临时本地参考目录，不作为最终引擎模块源码边界。
- 新渲染模块落在 `source/Engine/Function/RendererV2/`，参考 `externalRenderer/` 架构并按 NexAur 模块系统重新适配。
- 顶层工程升级到 C++20，避免引擎和新渲染模块出现语言标准双轨。
- 顶层统一使用 vcpkg 管理第三方依赖，逐步移除或隔离重复的 vendored 依赖。
- RendererV2 / Vulkan 路径后续统一使用 HLSL 编写新着色器，并通过 DXC 编译为 SPIR-V。
- OpenGL 只作为迁移期 fallback，Vulkan 稳定后退役。
- 每个阶段都保持可构建，可回退，可验证。

## 3. 当前状态

```text
当前分支：vulkanRenderer
当前阶段：D6 RenderDataPacket -> RenderView 已完成
代码状态：RendererModule 可在 OpenGL legacy 与 RendererV2 Vulkan 后端之间选择；Vulkan 后端可创建 instance / surface / device / swapchain，提交清屏帧，并把 RenderDataPacket 相机数据转换为 RendererV2 RenderView
OpenGL 后端：仍为当前可运行主路径，并保留 legacy fallback
externalRenderer：仅作为临时本地参考目录
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
- D5：完成 RendererV2 / VulkanRendererSystem 骨架，Vulkan 后端可启动、清屏、present 和关闭。
- D6：完成 `RenderDataPacket -> RenderView`，相机矩阵、clip range 和 viewport 尺寸进入 RendererV2 内部 view。

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
| D5 RendererV2 / VulkanRendererSystem 骨架 | 已完成 | 在 RendererV2 中创建新 Vulkan 渲染模块骨架，跑通空场景 / swapchain | 总方案 |
| D6 RenderView 转换 | 已完成 | NexAur 相机驱动新 Vulkan renderer | 总方案 |
| D7 VulkanRenderResourceCache | 待开始 | AssetHandle 解析到 Vulkan renderer resources | 总方案 |
| D8 RenderScene 转换 | 待开始 | NexAur 场景模型提交给新 Vulkan renderer | 总方案 |
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
- `RendererV2` 能使用新 Vulkan renderer 所需的第三方依赖。
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
- Vulkan 创建 GLFW no-api window。
- `WindowService::getNativeWindow()` 保持稳定。
- `WindowService` 暴露当前 `WindowGraphicsAPI`。
- `WindowService` 提供 GLFW 所需 Vulkan instance extensions 查询。
- 新增 `NEXAUR_DEFAULT_GRAPHICS_API` 构建配置，默认 `OpenGL`，可用 `Vulkan` 验证 no-api window 编译路径。

验收：

- OpenGL legacy 仍能启动。
- Vulkan path 能创建 Vulkan surface。
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

### D5：RendererV2 / VulkanRendererSystem 骨架

目标：

- 在 `source/Engine/Function/RendererV2/` 下新增 Vulkan 渲染模块骨架。
- 参考 `externalRenderer/` 的 facade / scene / view / resource cache 结构，但按 NexAur 模块系统重新组织。
- 使用 NexAur 自己的命名，不在新代码中使用 `ArkVulkanRendererSystem`、`ArkRenderResourceCache` 这类外部项目风格命名。
- 第一版先跑通 renderer 生命周期、Vulkan window / surface / swapchain 和空帧提交，不急着接完整场景资源。

任务：

- D5-A：目录与命名落地
  - 新增 `source/Engine/Function/RendererV2/`。
  - 新增 `VulkanRendererSystem`，实现 `RendererService`。
  - 预留 `VulkanRenderDataTranslator`，负责后续 `RenderDataPacket -> RenderView / RenderScene`。
  - 预留 `VulkanRenderResourceCache`，负责后续 `AssetHandle -> renderer GPU resource`。
  - 如需隐藏 externalRenderer / Vulkan 细节，优先在 `VulkanRendererSystem` 中使用私有 `Impl` 或 backend 内部类，避免公开头文件 include Vulkan / externalRenderer 内部头。
- D5-B：CMake 与构建开关
  - `NEXAUR_BUILD_RENDERER_V2=ON` 时编译 `RendererV2` 源码。
  - RendererV2 依赖以 `PRIVATE` 方式链接 `Vulkan::Vulkan`、`volk`、`vk-bootstrap`、`VulkanMemoryAllocator` 等后端依赖。
  - 旧 OpenGL legacy 仍保留，D5 不删除旧 `Renderer/`。
  - 不 `add_subdirectory(externalRenderer)`；需要的结构按 NexAur 风格重写或抽取到 RendererV2。
- D5-C：RendererModule 服务选择
  - RendererModule 根据配置选择 OpenGL legacy 或 `VulkanRendererSystem`。
  - 对外仍只注册 `RendererService`，Editor / Runtime 不感知具体后端类。
  - 过渡期可保留 `RendererSystem` 兼容服务注册，但新调用点必须依赖 `RendererService`。
- D5-D：Window / Vulkan surface 接入
  - 初始化前确认 `WindowService::getGraphicsAPI() == WindowGraphicsAPI::Vulkan`。
  - 通过 `WindowService::getNativeWindow()` 获取 GLFW window。
  - 通过 `WindowService::getRequiredVulkanInstanceExtensions()` 获取 instance extensions。
  - 在 renderer 后端内部创建 Vulkan instance、surface、physical device、logical device、swapchain。
  - Window resize 时重建 swapchain。
  - `RendererService::setViewportSize()` 只表示 Editor / Runtime 的逻辑 viewport 尺寸，不直接驱动 external swapchain 尺寸。
- D5-E：最小渲染生命周期
  - 实现 `init()` / `shutdown()` / `render()` / `setViewportSize()`。
  - `render()` 第一版只提交空场景或清屏帧，目标是验证 acquire / submit / present 生命周期。
  - 关闭时等待 device idle，并按依赖顺序释放 swapchain、device、surface、instance。
- D5-F：RendererService 输出契约
  - `getBackendType()` 返回 `RendererBackendType::Vulkan`。
  - `getViewportOutput()` 第一版返回 `ExternalSwapchain` 或 `None`，不伪装成 OpenGL texture。
  - `pickViewport()` 返回 `supported = false`，Editor 保持 graceful fallback。
- D5-G：日志、断言与失败回退
  - Vulkan 后端初始化失败时输出明确日志，说明缺少 instance extension、surface 创建失败、device 不满足条件或 swapchain 创建失败。
  - 默认构建仍可回退 OpenGL legacy，避免 D5 影响当前可运行编辑器路径。
  - 不在 D5 引入大范围 RenderDataPacket / AssetManager 变更。

验收：

- Vulkan backend 能启动和关闭。
- Vulkan device / swapchain 创建成功。
- 无资源泄漏或关闭崩溃。
- `RendererService` 对外接口不暴露 Vulkan / externalRenderer 原生类型。
- `ViewportPanel` 面对 `ExternalSwapchain` 或 `None` 不崩溃。
- OpenGL legacy 默认路径仍可构建运行。
- `NEXAUR_BUILD_RENDERER_V2=ON` 与 `NEXAUR_DEFAULT_GRAPHICS_API=Vulkan` 的 vcpkg Debug 构建通过。

建议验证：

```powershell
cmake -S . -B build/msvc-vcpkg-renderer-v2 -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_MANIFEST_MODE=ON `
  -DNEXAUR_USE_VCPKG_DEPS=ON `
  -DNEXAUR_BUILD_RENDERER_V2=ON `
  -DNEXAUR_DEFAULT_GRAPHICS_API=Vulkan
cmake --build build/msvc-vcpkg-renderer-v2 --config Debug
```

D5 不做：

- 不接完整 `RenderDataPacket -> RenderScene`，放到 D8。
- 不接 `AssetHandle -> renderer GPU resource`，放到 D7。
- 不做 Vulkan viewport image 嵌入 ImGui，放到 D10。
- 不做 Vulkan picking，放到 D11。
- 不删除 OpenGL legacy。

当前状态：

```text
已完成
验证：
- cmake --build --preset msvc-vcpkg-debug 通过
- cmake --build --preset msvc-legacy-debug 通过
- cmake --preset msvc-vcpkg-renderer-v2 通过
- cmake --build --preset msvc-vcpkg-renderer-v2-debug 通过
- bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe 3 秒 smoke check 通过
备注：
- 新增 RendererV2 独立 preset：msvc-vcpkg-renderer-v2
- Vulkan surface 创建当前使用 Win32 native handle，后续可抽成跨平台 surface factory
- 第一版输出为 ExternalSwapchain，Editor viewport 仍显示 placeholder；正式 Vulkan viewport image 留到 D10
- picking 暂时返回 unsupported，留到 D11
```

### D6：RenderDataPacket -> RenderView

目标：

- NexAur 当前帧相机数据能够稳定驱动 RendererV2 的内部 `RenderView`。
- 为 D7 资源缓存、D8 场景对象提交提前固定相机和 projection 契约。
- 保持 `RendererService` 对外接口不变，Scene / Editor / Runtime 仍然只提交 `RenderDataPacket`。

当前状态：

```text
已完成
验证：
- cmake --build --preset msvc-vcpkg-debug 通过
- cmake --build --preset msvc-vcpkg-renderer-v2-debug 通过
- cmake --build --preset msvc-legacy-debug 通过
- bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe 3 秒 smoke check 通过
备注：
- 新增 RendererV2 内部 RenderView，不暴露给 Scene / Editor / Gameplay
- RendererCameraData / CameraComponent 补充 near / far clip
- VulkanRenderDataTranslator 可构建 RenderView，并集中处理 Vulkan projection Y flip / 0..1 depth range
- VulkanRendererSystem::render() 已消费 RenderDataPacket 并生成 RenderView，D6 仍不提交模型 draw call
```

设计原则：

- `RenderView` 是 RendererV2 内部每帧视角数据，不暴露给 Scene / Editor / Gameplay。
- D6 只做相机和 viewport 翻译，不做资源加载、模型提交、draw item 构建。
- `RenderDataPacket` 继续作为跨模块契约；RendererV2 内部可以把它翻译成更适合渲染器使用的 `RenderView`。
- 不直接引入 externalRenderer 的 `ark::RenderView` 命名或类型；只参考它的职责边界。

建议拆分：

#### D6-A：新增 RendererV2 内部 RenderView

新增文件：

```text
source/Engine/Function/RendererV2/render_view.h
```

第一版只保留后续渲染必需的 per-view 数据：

- `view_matrix`
- `projection_matrix`
- `view_projection_matrix`
- `inverse_view_matrix`
- `inverse_projection_matrix`
- `camera_position`
- `near_clip`
- `far_clip`
- `viewport_width`
- `viewport_height`

暂不放入 tone mapping、shadow、postprocess、visibility 等高级设置。这些可以在 D8 之后按功能逐步扩展，避免第一版 `RenderView` 变成大杂烩。

#### D6-B：补齐 RendererCameraData 的最小相机元数据

建议在 `RendererCameraData` 中补充：

```cpp
float near_clip = 0.1f;
float far_clip = 1000.0f;
```

同步更新：

- `EditorCamera` 提供 near / far getter。
- `EditorLayer::syncViewportCameraToRenderPacket()` 写入 near / far。
- `CameraComponent` 增加 near / far 默认值。
- `SceneV2::extractSceneData()` 从 `CameraComponent` 写入 `RenderDataPacket`。
- `SceneFactory` 创建默认相机时写入 near / far。

这属于轻量跨模块契约补充，不应该改变 OpenGL legacy 的渲染行为。

#### D6-C：实现 VulkanRenderDataTranslator::buildRenderView()

建议接口：

```cpp
RenderView buildRenderView(
    const RenderDataPacket& render_data,
    uint32_t viewport_width,
    uint32_t viewport_height) const;
```

职责：

- 从 `render_data.camera_data` 读取 view、projection、camera position、near / far。
- 使用 renderer 当前 viewport 尺寸填充 `RenderView`。
- 修正或重建 Vulkan 路径需要的 projection。
- 重新计算 `view_projection_matrix`，不要直接复用旧 packet 中可能属于 OpenGL 约定的缓存 VP。
- 计算 inverse view / inverse projection，供后续 skybox、picking、postprocess、shadow 使用。
- 对 viewport size、near / far、矩阵中的 NaN / Inf 做基础保护。

禁止在 translator 里做：

- Vulkan resource 创建。
- AssetManager 查询。
- 模型加载。
- draw call 提交。
- Editor / Scene 状态修改。

#### D6-D：明确 projection 与 Vulkan 坐标约定

D6 必须明确一条规则，避免后续出现双重翻转或 depth range 错误：

```text
RenderDataPacket 保存引擎侧相机输入。
VulkanRenderDataTranslator 负责输出 Vulkan / HLSL / SPIR-V 需要的 RenderView projection。
```

重点检查：

- 当前 `EditorCamera` 使用 `glm::perspective()`，更接近旧 OpenGL 路径。
- Vulkan renderer 应使用 0..1 depth range。
- Vulkan viewport / clip space 需要处理 Y flip。
- externalRenderer 的参考做法是 `glm::perspectiveRH_ZO(...)` 后对 `projection[1][1]` 做一次翻转。

短期策略：

- D6 先在 translator 内集中处理 Vulkan projection 约定。
- 不让 EditorCamera 为 Vulkan 特判 projection。
- 不让 Scene / Editor 知道当前后端是否 Vulkan。

长期策略：

- 将 `RendererCameraData` 从“只传 baked projection matrix”逐步升级为“相机投影描述 + view matrix”。
- 例如后续可增加 perspective / orthographic 描述，让每个后端自行构建 projection matrix。

#### D6-E：接入 VulkanRendererSystem::render()

`VulkanRendererSystem::Backend::render()` 应从：

```cpp
translator.resetFrame();
drawFrame();
```

演进为：

```cpp
translator.resetFrame();
RenderView render_view = translator.buildRenderView(
    render_data,
    viewport_width,
    viewport_height);
drawFrame(render_view);
```

D6 阶段 `drawFrame()` 仍然可以只 clear swapchain，但后端已经拿到了正确的 `RenderView`。D8 接模型渲染时，forward pass 可以直接消费该 view。

D6 不做：

- 不接 `AssetHandle -> GPU resource`，放到 D7。
- 不接完整 `RenderDataPacket -> RenderScene`，放到 D8。
- 不提交模型 draw call。
- 不实现 Vulkan viewport image 嵌入 ImGui，放到 D10。
- 不实现 picking，放到 D11。
- 不删除 OpenGL legacy。

验收：

- `RenderView` 可以从 `RenderDataPacket` 稳定构建。
- 相机移动后 `RenderView.view_matrix`、`camera_position` 跟随变化。
- 窗口或 viewport resize 后 `RenderView.viewport_width / viewport_height` 和 projection 正确更新。
- projection 没有双重 Y flip。
- depth range 符合 Vulkan / HLSL / SPIR-V 约定。
- 空场景 Vulkan clear swapchain 继续稳定。
- OpenGL legacy 构建不受影响。

建议验证：

```powershell
cmake --build --preset msvc-vcpkg-renderer-v2-debug
cmake --build --preset msvc-legacy-debug
```

如新增 translator 单元级辅助函数，应覆盖：

- 默认相机数据生成有效 `RenderView`。
- viewport 宽高为 0 时会被夹到安全值。
- near / far 非法时回退到默认值。
- Vulkan projection 只做一次 Y flip。

### D7：VulkanRenderResourceCache

目标：

- Vulkan 路径拥有独立 renderer resource cache。

任务：

- 新增 `VulkanRenderResourceCache`。
- `AssetHandle -> Vulkan renderer model resource`。
- 使用 AssetManager 获取路径。
- 第一版可参考 externalRenderer loader。
- 支持 `clear()` / shutdown。

验收：

- 同一模型不会重复创建 GPU resource。
- 关闭时资源释放稳定。

### D8：RenderDataPacket -> RenderScene

目标：

- NexAur scene objects 提交给新 Vulkan renderer。

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

- Vulkan renderer 支持 offscreen viewport target。
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

- Vulkan renderer 增加 ObjectId pass。
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

- 默认 backend 为 Vulkan。
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
| C++17 / C++20 不一致 | RendererV2 / externalRenderer 参考代码编译失败 | 推荐 NexAur 升级 C++20 |
| ImGui OpenGL / Vulkan backend 冲突 | Editor viewport 无法显示或崩溃 | Vulkan 早期不嵌入 Editor viewport |
| viewport output 仍绑定 OpenGL texture id | Vulkan 被迫模拟 OpenGL | D1 / D2 优先清理接口 |
| AssetManager 与 externalRenderer loader 双轨 | 重复加载、缓存复杂 | 第一版接受重复，中期统一 CPU asset contract |
| picking 延迟或不可用 | Editor 交互退化 | 先 graceful fallback，后续 D11 实现 |

## 8. 当前下一步

推荐下一步：

```text
D7 开始：VulkanRenderResourceCache
D8 准备：RenderDataPacket -> RenderScene
```

D7 / D8 开始前的已确认前提：

- `externalRenderer/` 仅作为临时本地参考目录。
- 新渲染模块在 `source/Engine/Function/RendererV2/` 中重构。
- NexAur 顶层已升级到 C++20。
- NexAur 顶层已具备 vcpkg manifest / preset 构建路径。
- RendererV2 新增 shader 统一使用 HLSL -> SPIR-V 流程。
- WindowService 已能提供 graphics API 状态和 Vulkan instance extensions。
- Vulkan no-api window 编译路径已验证通过。
- RendererV2 Vulkan 后端已能启动、清屏、present 和关闭。
- OpenGL legacy 只作为过渡 fallback，不再继续增强。

## 9. 进度记录

### 2026-06-24

- 创建 `vulkanRenderer` 分支。
- 新增 externalRenderer 参考接入总方案。
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
- 完成 D5：新增 `source/Engine/Function/RendererV2/`，并创建 `VulkanRendererSystem`、`VulkanRenderDataTranslator`、`VulkanRenderResourceCache` 骨架。
- 完成 D5：`VulkanRendererSystem` 实现 `RendererService`，对外返回 `RendererBackendType::Vulkan`、`ExternalSwapchain` 输出和 unsupported picking。
- 完成 D5：Vulkan 后端内部创建 instance、Win32 surface、physical device、logical device、swapchain、command buffer、semaphore / fence。
- 完成 D5：`render()` 第一版提交清屏帧，验证 acquire / command buffer / submit / present 生命周期。
- 完成 D5：RendererModule 根据 `WindowGraphicsAPI` 选择 OpenGL legacy 或 RendererV2 Vulkan 后端，对外仍只注册 `RendererService`。
- 完成 D5：新增 `msvc-vcpkg-renderer-v2` / `msvc-vcpkg-renderer-v2-debug` preset，专门验证 RendererV2 Vulkan 路径。
- 验证 D5：`cmake --build --preset msvc-vcpkg-debug` 通过，OpenGL legacy 默认路径未被破坏。
- 验证 D5：`cmake --build --preset msvc-legacy-debug` 通过，vendored external 回退路径未被破坏。
- 验证 D5：`cmake --preset msvc-vcpkg-renderer-v2` 与 `cmake --build --preset msvc-vcpkg-renderer-v2-debug` 通过。
- 验证 D5：`bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe` 3 秒 smoke check 通过。
- 完成 D6：新增 RendererV2 内部 `RenderView`，保存 view / projection / inverse matrices、camera position、clip range 和 viewport size。
- 完成 D6：`RendererCameraData`、`CameraComponent`、`EditorCamera` 同步链路补充 near / far clip。
- 完成 D6：`VulkanRenderDataTranslator::buildRenderView()` 将 `RenderDataPacket` 转换为 RendererV2 `RenderView`。
- 完成 D6：Vulkan projection 转换集中在 translator 内处理，Editor / Scene 不需要感知 Vulkan 后端。
- 完成 D6：`VulkanRendererSystem::render()` 已消费 `RenderDataPacket` 并生成 `RenderView`，清屏帧生命周期保持稳定。
- 验证 D6：`cmake --build --preset msvc-vcpkg-renderer-v2-debug` 通过。
- 验证 D6：`cmake --build --preset msvc-vcpkg-debug` 通过。
- 验证 D6：`cmake --build --preset msvc-legacy-debug` 通过。
- 验证 D6：`bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe` 3 秒 smoke check 通过。

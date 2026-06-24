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
  -> RenderDataPacket -> RenderSceneFrame -> VulkanDrawList
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
当前阶段：D12 OpenGL legacy 退役已完成
代码状态：默认构建已切换为 vcpkg + Vulkan 主路径；RendererModule 只创建 VulkanRendererSystem；RenderDataPacket / RendererService / ViewportOutput 已去除 OpenGL-only 语义；旧 OpenGL RHI / pass / resource / platform implementation 已从主线删除
OpenGL 后端：已退役，不再作为默认构建或 fallback 参与主线
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
- D7：完成 `VulkanRenderResourceCache`，模型 AssetHandle 可解析为 RendererV2 内部 Vulkan model resource。
- D8：完成 `RenderDataPacket -> RenderSceneFrame -> VulkanDrawList -> VulkanForwardPass` 最小绘制链路。
- D9：完成 Editor viewport 过渡，ExternalSwapchain 下有明确状态、尺寸策略和交互降级。
- D10：完成 Vulkan viewport image，Editor viewport 可显示 RendererV2 offscreen image。
- D11：完成 Vulkan picking，Editor viewport 可通过 ObjectId pass / readback 选中实体。
- D12：完成 OpenGL legacy 退役，默认 Vulkan/vcpkg 主路径，旧 OpenGL 实现和依赖已从主线移除。

已确认：

- `externalRenderer/` 不直接作为最终集成目录，后续只作为架构和实现参考。
- 新渲染模块在 `source/Engine/Function/RendererV2/` 中重构，不在旧 `Renderer/` 目录里继续堆叠大改。
- 顶层直接升级到 C++20，作为 NexAur 后续主线标准。
- 顶层统一改为 vcpkg manifest 管理第三方依赖；旧 `external/` 依赖按阶段隔离、替换或移除。

需要注意：

- `RendererV2` 仍是当前目录名，D12 先完成 OpenGL 退役；是否收口为 `Renderer/Vulkan` 可放到 D12.1 单独处理。
- 顶层依赖现在以 vcpkg manifest 为唯一主路径，旧 vendored external fallback 不再进入默认构建图。
- GLFW 的 vcpkg 输出仍可能在说明文本里提到 OpenGL，这是 GLFW 包自身说明，不代表 NexAur 主线仍依赖 OpenGL backend。

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
| D7 VulkanRenderResourceCache | 已完成 | AssetHandle 解析到 Vulkan renderer resources | 总方案 |
| D8 SceneFrame / DrawList / ForwardPass | 已完成 | Vulkan 1.3 + dynamic rendering 下提交并显示 NexAur 场景模型 | 总方案 |
| D9 Editor viewport 过渡 | 已完成 | Vulkan backend 下 Editor 不崩溃，有明确输出策略 | 接口文档 |
| D10 Vulkan viewport image | 已完成 | Editor viewport 显示 Vulkan offscreen image | 接口文档 |
| D11 Vulkan picking | 已完成 | ObjectId pass / readback 接入 | 接口文档 |
| D12 OpenGL legacy 退役 | 已完成 | 默认 Vulkan，旧 OpenGL 移除或隔离 | 总方案 |

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

- Vulkan 路径拥有独立 RendererV2 resource cache。
- 建立 `AssetHandle -> RendererV2 内部资源对象` 的稳定链路。
- 为 D8 的 `RenderDataPacket -> RenderScene` 提供可引用的 `VulkanModelResource`。
- 保持 Resource / Scene / Editor 不感知 Vulkan 原生类型。

当前状态：

```text
已完成
完成内容：
- D5 已创建 VulkanRenderResourceCache 生命周期骨架
- D6 已完成 RenderDataPacket -> RenderView
- D7 已新增 RendererV2 resource 类型、VMA allocator、专用 upload command pool 和 AssetHandle -> VulkanModelResource 缓存
- Vulkan device / queue 仍封装在 VulkanRendererSystem::Backend 内部，resource cache 只通过 VulkanResourceContext 获取最小必要句柄
```

设计原则：

- `VulkanRenderResourceCache` 归 RendererV2 后端持有，不做全局 singleton。
- cache key 使用 `AssetHandle`，资源对象生命周期覆盖后续 `RenderScene` 提交与绘制。
- `AssetManager` 只负责资产身份、路径和 CPU 数据，GPU resource 创建属于 RendererV2。
- 不复用旧 OpenGL `RenderModelData / Texture2D / VertexArray`。
- externalRenderer 的 `ModelResource / MeshResource / TextureCache` 只作为职责参考，不直接暴露 `ark::` 类型。

建议拆分：

#### D7-A：定义 RendererV2 资源类型

新增目录建议：

```text
source/Engine/Function/RendererV2/resources/
```

第一版建议新增：

```text
vulkan_model_resource.h / .cpp
vulkan_mesh_resource.h / .cpp
vulkan_material_resource.h / .cpp
```

最低字段：

```cpp
struct VulkanMeshResource {
    uint32_t index_count = 0;
};

struct VulkanModelResource {
    std::vector<VulkanMeshResource> meshes;
    std::string debug_name;
};
```

如果 D7 直接做几何 GPU 上传，则 `VulkanMeshResource` 后续放入 vertex buffer / index buffer / allocation 信息。即使加入 Vulkan handle，也只能停留在 RendererV2 内部。

#### D7-B：新增 VulkanResourceContext

当前 `VulkanRenderResourceCache::init()` 没有参数，不足以创建 GPU resource。建议新增 RendererV2 内部上下文：

```cpp
struct VulkanResourceContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    uint32_t graphics_queue_family = 0;
};
```

本次 D7 让 `VulkanRenderResourceCache` 自己持有 VMA allocator 和专用 upload command pool。单次资源创建时由 cache 组装内部上传上下文：

```cpp
struct VulkanResourceUploadContext {
    VmaAllocator allocator = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
};
```

上下文由 `VulkanRendererSystem::Backend` 在 device 创建完成后组装，并传给 `resource_cache.init(context)`。

#### D7-C：实现 cache API

建议接口：

```cpp
void init(const VulkanResourceContext& context);
void clear();
void shutdown();

VulkanModelResource* getOrCreateModel(
    AssetHandle model_asset,
    AssetManager& asset_manager);

VulkanModelResource* getModel(AssetHandle model_asset) const;
```

内部缓存：

```cpp
std::unordered_map<AssetHandle, std::unique_ptr<VulkanModelResource>> m_model_cache;
```

要求：

- 无效 handle 返回 `nullptr` 并输出 warn。
- 非 Model 资产返回 `nullptr` 并输出 warn。
- 加载失败返回 `nullptr` 并输出 error。
- 同一个 `AssetHandle` 多次请求必须返回同一个资源对象。

#### D7-D：AssetHandle -> CPU model

第一版建议复用 NexAur 现有资源系统：

```cpp
std::shared_ptr<Model> cpu_model = asset_manager.loadModelCPU(model_asset);
```

不要在 D7 主路径中直接引入 externalRenderer `GltfLoader`，避免出现两套 asset pipeline。externalRenderer 的 glTF 数据结构可以作为后续 CPU asset contract 重构参考，但 D7 先保持 NexAur `AssetManager` 是唯一资产入口。

#### D7-E：资源上传策略

有两种可选落地方式。

保守方案：

- D7 只把 CPU model 转换为 `VulkanModelResource` 的内部描述。
- 暂不创建真实 VkBuffer。
- D8 可以先完成 RenderScene 提交流程。
- 优点是风险低；缺点是 D8 后仍不能真正画模型。

推荐方案：

- D7 直接接入 VMA。
- 为模型几何创建 vertex / index buffer。
- 新增最小 immediate upload helper 或专用 upload command pool。
- 材质和纹理先做 placeholder 或只记录路径。
- D8 接 RenderScene 时，模型资源已经接近可绘制。

推荐方案更接近最终 Vulkan renderer，但 D7 必须控制范围：只做几何 buffer 上传，不同时做完整 PBR 材质、纹理采样、shader、pipeline。

本次落地选择推荐方案的受控版本：

- `VulkanRenderResourceCache` 创建并销毁 VMA allocator。
- `VulkanRenderResourceCache` 创建专用 transient upload command pool。
- `VulkanMeshResource` 使用 staging buffer 上传 CPU mesh 顶点 / 索引数据。
- 最终 vertex / index buffer 使用 device-local 倾向分配，供 D8 draw item 直接引用。
- `VulkanMaterialResource` 第一版只记录材质调试名和纹理路径，不创建 texture / sampler。

#### D7-F：生命周期和清理顺序

资源释放顺序应保持：

```text
vkDeviceWaitIdle
  -> resource_cache.shutdown()
  -> command resources cleanup
  -> swapchain cleanup
  -> device destroy
```

当前 D5 的 shutdown 顺序已经接近这个结构。D7 需要让 `clear()` 真正释放缓存里的 RendererV2 resource，确保关闭时不泄漏 Vulkan buffer / allocation。

本次实现中，`resource_cache.shutdown()` 会先清空 model cache，触发 mesh buffer / allocation 释放，再销毁 upload command pool，最后销毁 VMA allocator。

D7 不做：

- 不做 `RenderDataPacket -> RenderScene`，放到 D8。
- 不提交模型 draw call。
- 不做完整材质系统。
- 不做 HDR / IBL。
- 不做 Vulkan viewport image，放到 D10。
- 不做 picking，放到 D11。
- 不删除 OpenGL legacy。

验收：

- 同一个 `AssetHandle` 多次请求不会重复创建 RendererV2 model resource。
- 无效 handle / 错误资产类型 / 加载失败都有明确日志且不会崩溃。
- `clear()` 和 `shutdown()` 能稳定释放缓存。
- RendererV2 Vulkan 构建通过。
- OpenGL legacy 构建仍通过。
- Sandbox smoke check 不崩溃。
- 暂时不要求画出模型；视觉提交留给 D8 及后续 pass。

建议验证：

```powershell
cmake --build --preset msvc-vcpkg-renderer-v2-debug
cmake --build --preset msvc-vcpkg-debug
cmake --build --preset msvc-legacy-debug
```

当前状态：

```text
已完成
验证：
- cmake --build --preset msvc-vcpkg-renderer-v2-debug 通过
- cmake --build --preset msvc-vcpkg-debug 通过
- cmake --build --preset msvc-legacy-debug 通过
- bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe 3 秒 smoke check 通过
备注：
- D7 已创建真实 Vulkan vertex / index buffer，但暂不提交 draw call。
- 材质资源第一版只保存路径和调试名，texture / sampler 留给后续材质系统。
```

### D8：RenderDataPacket -> RenderSceneFrame -> VulkanDrawList

目标：

- RendererV2 明确对齐 Vulkan 1.3+ baseline，后续默认使用 dynamic rendering。
- 建立现代引擎式三段数据流：`RenderSceneFrame -> VulkanDrawList -> VulkanForwardPass`。
- 让 `RenderDataPacket` 的模型、transform、entity id、灯光和环境强度先进入后端无关的帧场景，再由 Vulkan 后端解析成 draw item。
- 第一版用最小 unlit / debug forward path 显示 opaque 模型，验证 transform 和相机方向。

当前状态：

```text
已完成
前置：
- D6 已完成 RenderDataPacket -> RenderView
- D7 已完成 AssetHandle -> VulkanModelResource
- D8 已将 RendererV2 baseline 对齐到 Vulkan 1.3+，并启用 dynamic rendering 所需特性
```

设计原则：

- `RenderSceneFrame` 是 RendererV2 内部每帧语义场景描述，不暴露给 Scene / Editor / Gameplay。
- `RenderSceneFrame` 不保存 Vulkan resource 指针，只保存 asset handle、transform、entity id、灯光等后端无关数据。
- `VulkanDrawList` 是 Vulkan 后端解析后的 draw item 列表，只在 RendererV2 / Vulkan 内部消费。
- `VulkanForwardPass` 只负责 dynamic rendering 和 draw command，不负责解析 `RenderDataPacket`。
- `RenderDataPacket` 继续作为跨模块契约；Scene 仍然只输出 `AssetHandle`、transform、灯光等后端无关数据。
- D8 使用 Vulkan 1.3 dynamic rendering，不新增旧式 `VkRenderPass` / `VkFramebuffer` 长期路径。
- 第一版只保证 opaque 模型可见；transparent sorting、PBR 材质、纹理采样、IBL、picking 和 viewport image 继续后移。
- shader 使用 HLSL，并通过 DXC 编译为 SPIR-V。

推荐数据流：

```text
RenderDataPacket
  -> RenderView
  -> RenderSceneFrame      // 每帧语义场景，仍使用 AssetHandle
  -> VulkanDrawList        // Vulkan 后端资源解析后的 mesh draw item
  -> VulkanForwardPass     // dynamic rendering + vkCmdDrawIndexed
```

建议拆分：

#### D8-0：Vulkan 1.3 baseline 对齐

当前 RendererV2 骨架还有 Vulkan 1.1 临时代码，需要先修正：

```text
source/Engine/Function/RendererV2/vulkan_renderer_system.cpp
  -> require_api_version(1, 3, 0)
  -> set_minimum_version(1, 3)

source/Engine/Function/RendererV2/vulkan_render_resource_cache.cpp
  -> VMA allocator vulkanApiVersion 改为 1.3 或实际 physical device apiVersion
```

设备特性建议显式查询和启用：

```cpp
VkPhysicalDeviceVulkan13Features features13{};
features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
features13.dynamicRendering = VK_TRUE;
features13.synchronization2 = VK_TRUE;
features13.shaderDemoteToHelperInvocation = VK_TRUE; // 使用 alpha discard / demote shader 时需要
```

说明：

- `dynamicRendering` 是 D8 绘制路径的核心前提。
- `synchronization2` 不一定要在 D8 第一版大量使用，但建议 baseline 中一起打开，避免后续 barrier 体系双轨。
- `shaderDemoteToHelperInvocation` 对 externalRenderer 的 alpha mask / discard 路径很重要；D8 unlit shader 可以暂时不用，但 baseline 可以提前对齐。
- 初始化失败时应输出清晰日志，而不是静默 fallback 到 1.1。

#### D8-A：新增 RenderSceneFrame

建议新增：

```text
source/Engine/Function/RendererV2/render_scene_frame.h
```

`RenderSceneFrame` 是 RendererV2 内部的每帧语义场景。它仍然接近引擎提交数据，不直接绑定 Vulkan resource。

```cpp
struct RenderSceneFrameObject {
    AssetHandle model_asset;
    glm::mat4 transform{1.0f};
    int entity_id = -1;
};

struct RenderSceneFrame {
    RenderView view;
    std::vector<RenderSceneFrameObject> opaque_objects;
    std::vector<RenderSceneFrameObject> transparent_objects;

    RenderFrameDirectionalLight directional_light;
    std::vector<RenderFramePointLight> point_lights;

    AssetHandle environment_asset;
    float environment_intensity = 1.0f;
};
```

注意：

- `render_scene_frame.h` 尽量不要 include 旧 `Renderer/data/render_data.h`，因为旧文件里还带着 OpenGL legacy 的 RHI 类型。
- 可以在 RendererV2 中定义轻量 `RenderFrameDirectionalLight` / `RenderFramePointLight`，由 builder 从 `RenderDataPacket` 拷贝。
- `transparent_objects` 第一版可以构建但不绘制，避免一开始引入排序和混合。
- `RenderSceneFrame` 每帧重建，不做持久 scene registry。后续如果要做大型场景和增量更新，再引入 persistent renderer scene。

#### D8-B：新增 RenderSceneFrameBuilder

建议新增：

```text
source/Engine/Function/RendererV2/render_scene_frame_builder.h / .cpp
```

职责：

- 输入 `RenderDataPacket` 和 `RenderView`。
- 输出 `RenderSceneFrame`。
- 只做数据清洗、复制和分流，不解析 GPU resource。

建议接口：

```cpp
RenderSceneFrame buildRenderSceneFrame(
    const RenderDataPacket& render_data,
    const RenderView& render_view) const;
```

转换规则：

- `scene_frame.view = render_view`。
- 遍历 `opaque_objects`，复制有效 `model_asset`、`transform`、`entity_id`。
- 遍历 `transparent_objects`，复制有效对象，但第一版可以只进入列表不绘制。
- 无效 handle 直接跳过并记录 warning。
- 拷贝 `transform` 和 `entity_id`。
- 拷贝 directional light。
- 拷贝 point lights，必要时限制最大数量，避免后续 uniform 爆掉。
- 拷贝 environment asset 和 intensity；D8 不烘焙 IBL。

#### D8-C：新增 VulkanDrawList 和 DrawListBuilder

建议新增：

```text
source/Engine/Function/RendererV2/vulkan_draw_list.h
source/Engine/Function/RendererV2/vulkan_draw_list_builder.h / .cpp
```

`VulkanDrawList` 是 Vulkan 后端真正提交给 pass 的 draw item 列表。

```cpp
struct VulkanMeshDrawItem {
    const VulkanMeshResource* mesh = nullptr;
    glm::mat4 transform{1.0f};
    int entity_id = -1;
    uint64_t sort_key = 0;
};

struct VulkanDrawList {
    RenderView view;
    std::vector<VulkanMeshDrawItem> opaque_items;
    std::vector<VulkanMeshDrawItem> transparent_items;
};
```

`VulkanDrawListBuilder` 负责：

- 输入 `RenderSceneFrame`。
- 通过 `VulkanRenderResourceCache::getOrCreateModel()` 解析 `AssetHandle`。
- 把 model flatten 成 mesh draw item。
- 生成第一版 sort key。
- 加载失败的对象不进入 draw list，并记录 warning / error。

建议接口：

```cpp
VulkanDrawList buildDrawList(
    const RenderSceneFrame& scene_frame,
    VulkanRenderResourceCache& resource_cache,
    AssetManager& asset_manager);
```

边界：

- 资源解析只发生在 `VulkanDrawListBuilder`，不放进 `RenderSceneFrameBuilder`。
- `VulkanForwardPass` 不关心 `AssetHandle`，只消费已经解析好的 `VulkanMeshDrawItem`。
- 这一步类似 UE 的 MeshDrawCommand / Unity 的 RendererList / Godot 的 renderer draw list。

#### D8-D：Backend 帧流程接入

当前流程：

```cpp
translator.resetFrame();
RenderView render_view = translator.buildRenderView(...);
drawFrame(render_view);
```

D8 后演进为：

```cpp
translator.resetFrame();

RenderView render_view = translator.buildRenderView(...);
RenderSceneFrame scene_frame = scene_frame_builder.buildRenderSceneFrame(render_data, render_view);
VulkanDrawList draw_list = draw_list_builder.buildDrawList(
    scene_frame,
    resource_cache,
    asset_manager);

drawFrame(draw_list);
```

实现要求：

- `VulkanRendererSystem::Backend` 仍然是后端细节集中点。
- `AssetManager` 第一版可以通过现有资源入口获取；后续如果 ResourceModule 暴露服务，再替换为服务注入。
- 不要让 Scene / Editor include `RendererV2/render_scene_frame.h` 或 `vulkan_draw_list.h`。
- resource upload 发生在 draw command recording 之前；上传失败的对象不进入 draw list。
- `Backend` 只串联 view builder、scene frame builder、draw list builder 和 pass，不把所有逻辑写成一个巨型函数。

#### D8-E：新增 VulkanForwardPass

为了满足“Vulkan backend 显示 NexAur 场景模型”，D8 需要补最小 forward 绘制闭环。

建议新增：

```text
source/Engine/Function/RendererV2/passes/vulkan_forward_pass.h / .cpp
```

需要新增或改造：

- swapchain image view 创建和销毁。
- depth image / depth image view 创建和销毁。
- dynamic rendering begin/end：
  - `VkRenderingAttachmentInfo`
  - `VkRenderingInfo`
  - `vkCmdBeginRendering`
  - `vkCmdEndRendering`
- graphics pipeline：
  - `VkPipelineRenderingCreateInfo`
  - color format = swapchain format
  - depth format = depth image format
- 最小 HLSL shader：
  - vertex shader：position / normal / uv 输入，输出 clip position。
  - fragment shader：先输出固定颜色或简单 lambert，不接纹理。
- camera / model 数据：
  - 第一版可以用 push constants 传 `view_projection` 和 `model`，保持 descriptor 系统后移。
  - 如果 push constant 超限，再改为 camera uniform buffer + model push constant。
- draw：
  - 遍历 `draw_list.opaque_items`。
  - bind vertex / index buffer。
  - `vkCmdDrawIndexed`。

不建议在 D8 第一版引入完整 descriptor/material system。D8 的目标是“能看见模型并验证提交链路”，不是完成最终 forward renderer。

#### D8-F：验证和调试

建议验证：

```powershell
cmake --build --preset msvc-vcpkg-renderer-v2-debug
cmake --build --preset msvc-vcpkg-debug
cmake --build --preset msvc-legacy-debug
```

运行验证：

```text
bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe 3 秒 smoke check
```

手动观察：

- Vulkan backend 能启动并显示模型。
- 相机移动后模型视角变化正确。
- 模型 transform 正确。
- resize 后 dynamic rendering 目标重建稳定。
- 资源加载失败时不会崩溃。

D8 不做：

- 不做完整 PBR。
- 不做 texture / sampler。
- 不做 IBL / HDR。
- 不做 transparent sorting 和 blending。
- 不做 object picking。
- 不做 Vulkan viewport image 嵌入 ImGui。
- 不删除 OpenGL legacy。
- 不让 Scene / Editor 依赖 Vulkan 或 RendererV2 内部类型。

验收：

- RendererV2 baseline 已切到 Vulkan 1.3+。
- dynamic rendering feature 已启用，绘制路径不依赖旧式 `VkRenderPass` / `VkFramebuffer`。
- `RenderSceneFrame` 可以从 `RenderDataPacket` 稳定构建，且不保存 Vulkan resource 指针。
- `VulkanDrawList` 可以从 `RenderSceneFrame` 稳定构建，并完成 `AssetHandle -> VulkanModelResource -> mesh draw item` 解析。
- 同一模型多次提交不会重复创建 `VulkanModelResource`。
- Vulkan backend 能显示 NexAur 场景 opaque 模型。
- Transform 正确。
- 相机 view / projection 与模型绘制匹配。
- resize 后 swapchain / image view / depth / dynamic rendering 目标重建稳定。
- OpenGL legacy 构建仍通过。

### D9：Editor viewport 过渡

目标：

- Vulkan backend 下 Editor 可以稳定运行，不因为 viewport 输出形态变化而崩溃或误导用户。
- 明确 D8 之后的过渡状态：Vulkan 画面仍然直接输出到主窗口 swapchain，尚未嵌入 ImGui viewport panel。
- 为 D10 的 Vulkan offscreen viewport image 和 D11 picking 留出清晰接口边界。

当前状态：

```text
- D8 后 VulkanRendererSystem 已经可以把 scene draw list 直接绘制到主窗口 swapchain。
- VulkanRendererSystem::getViewportOutput() 当前返回 ExternalSwapchain。
- ViewportPanel 已能识别 ExternalSwapchain / None 并走 placeholder，不会把 Vulkan 输出伪装成 OpenGL texture。
- Vulkan pickViewport() 仍返回 unsupported，正式 object id pass / readback 留给 D11。
- Vulkan 下 UISystem 当前只初始化 ImGui GLFW platform backend，尚未初始化 Vulkan ImGui renderer backend，因此 Editor UI draw data 还没有真正提交到 Vulkan swapchain。
```

设计边界：

- D9 不做 Vulkan offscreen viewport image；这属于 D10。
- D9 不做 Vulkan picking；这属于 D11。
- D9 不做完整 Vulkan ImGui texture descriptor 集成。
- D9 的重点是“过渡期行为正确”：不崩溃、不假装可交互、不让 camera / viewport size 语义错位。
- 如果希望 Vulkan backend 下 Editor UI 可见，需要单独引入最小 Vulkan ImGui overlay 或调整 frame order；这件事接近 D10，D9 只做必要预留和风险记录。

关键问题：

1. ExternalSwapchain 的 viewport size 不能继续被 ImGui panel 尺寸驱动。

   当前 `ViewportPanel::syncViewportResize()` 会用 panel size 调用 `renderer_service->setViewportSize()`。这对 OpenGL offscreen framebuffer 是正确的，但 D8 Vulkan 是直接绘制到主窗口 swapchain。若继续用 panel size 作为 Vulkan `RenderView` 的 viewport size，会导致 projection aspect 和实际 swapchain render area 不一致。

   D9 建议：

   ```text
   OpenGLTexture / VulkanImGuiTexture:
     panel size -> RendererService::setViewportSize()
     panel size -> EditorCamera::setViewportSize()

   ExternalSwapchain:
     不用 panel size 驱动 renderer viewport
     renderer viewport / editor camera aspect 使用 swapchain 或 window surface size

   None:
     不改变 renderer viewport
   ```

2. ExternalSwapchain 下 picking / gizmo 不应该假装可用。

   当前 Vulkan 画面不在 ImGui viewport panel 内，panel 坐标无法对应 Vulkan swapchain 像素。D9 需要确保：

   - `ViewportPanel::pickEntityAtMouse()` 只在 embedded viewport 输出时执行。
   - `ExternalSwapchain` 下点击 placeholder 不触发 picking，不清空已有 selection。
   - `drawGizmo()` 只在 viewport image 真正嵌入 panel 时启用。
   - `ExternalSwapchain` 下可以显示状态提示，但不绘制会误导用户的 gizmo overlay。

3. ViewportPanel 状态显示需要更明确。

   推荐把当前 placeholder 分成明确状态：

   ```text
   OpenGLTexture:
     正常 ImGui::Image 显示

   VulkanImGuiTexture:
     D10 后显示 Vulkan descriptor-backed viewport image

   ExternalSwapchain:
     显示过渡期提示：Vulkan scene is rendered to the main swapchain.
     说明 viewport image / picking / gizmo 暂不可用

   None:
     显示 no viewport output
   ```

4. Vulkan Editor UI 可见性需要独立决策。

   目前 `UISystem` 在 Vulkan 模式下只调用 `ImGui_ImplGlfw_InitForVulkan()`，`endFrameAndRender()` 不会提交 ImGui draw data。这意味着即使 Editor panel 逻辑运行，Vulkan 后端也不会显示 Editor UI。

   D9 有两个可选策略：

   ```text
   保守策略：
     D9 只保证不崩溃，并在日志 / 文档中明确 Vulkan Editor UI 尚未呈现。
     优点是改动小，不打乱 D8 绘制链路。
     缺点是 Vulkan 模式下用户主要看到主 swapchain 场景，Editor UI 不可见。

   增强策略：
     D9 引入最小 Vulkan ImGui overlay，把 ImGui draw data 合成到主 swapchain。
     优点是 Editor UI 可见。
     缺点是需要调整 frame order 或让 VulkanRendererSystem 接收 ImGui draw data，容易提前触碰 D10 的生命周期和同步问题。
   ```

   建议采用保守策略完成 D9，再在 D10 统一处理 Vulkan ImGui renderer、offscreen viewport image 和 descriptor 生命周期。

建议任务拆分：

- D9-A：Viewport output policy 整理
  - 明确 `ExternalSwapchain` 不代表 embedded viewport。
  - `VulkanRendererSystem::getViewportOutput()` 返回 swapchain / surface 尺寸，而不是被 panel size 污染的尺寸。
  - `ViewportPanel::syncViewportResize()` 根据 output kind 决定是否调用 `setViewportSize()`。

- D9-B：ViewportPanel 状态 UI 清理
  - 拆出 `drawExternalSwapchainNotice()` / `drawNoViewportOutputNotice()` 等小函数。
  - 保持 placeholder 占位尺寸稳定，避免 layout 抖动。
  - 状态文字短而明确，不写成大段教程。

- D9-C：交互降级
  - `OpenGLTexture` / `VulkanImGuiTexture` 才允许 picking 坐标换算。
  - `ExternalSwapchain` / `None` 下 picking 直接跳过。
  - `ExternalSwapchain` / `None` 下不绘制 gizmo。
  - 不改变当前 selection，避免用户误点 placeholder 清空选择。

- D9-D：Editor camera 与 RenderData 同步
  - OpenGL embedded viewport 继续使用 panel size。
  - Vulkan ExternalSwapchain 使用 renderer output size / window surface size。
  - 相机移动仍能驱动 Vulkan swapchain 场景。

- D9-E：文档和日志同步
  - 文档明确 D9 不等于 Vulkan viewport image。
  - Vulkan UI renderer 未接入时输出一次清晰 warning，避免用户误以为 Editor 崩溃。
  - 更新 roadmap 进度记录。

验收：

- Editor UI 不崩溃。
- 用户能理解当前 Vulkan viewport 状态。
- Vulkan ExternalSwapchain 下不会执行错误的 panel-size resize、picking 或 gizmo。
- OpenGL legacy viewport 显示、picking 和 gizmo 行为不退化。
- Vulkan backend 下相机移动仍能驱动主 swapchain 画面。

### D10：Vulkan viewport image

目标：

- Editor viewport 显示 Vulkan offscreen image。
- Vulkan scene 不再只直接输出到主窗口 swapchain，而是先绘制到 RendererV2 内部 viewport target。
- `ViewportPanel` 通过 `ViewportOutputKind::VulkanImGuiTexture` 显示 backend-owned descriptor。
- 为 D11 Vulkan picking 预留同尺寸 ObjectId / readback pass 的接入位置。

设计边界：

- D10 不做 picking；ObjectId pass / readback 留到 D11。
- Editor / Scene / Resource 不 include Vulkan 头文件，不解释 `native_handle`。
- `externalRenderer/` 仍只作参考，不直接作为长期子目录接入。
- Vulkan 模式下可以先关闭 ImGui multi-viewport，优先保证主窗口 docking 和 embedded viewport image 稳定。
- resize 可以先用 `vkDeviceWaitIdle()` 做保守重建，后续再优化为延迟销毁。

关键问题：

1. 当前 Engine frame order 不适合 Vulkan UI 合成。

   当前主循环大致是：

   ```text
   UI begin
   Editor 提交 ImGui
   rendererTick()
   UI endFrameAndRender()
   window present
   ```

   OpenGL legacy 可以这样，因为 scene framebuffer 和 ImGui OpenGL backend 都在同一个 OpenGL context 下工作。Vulkan D10 需要让 `ImGui::Render()` 先生成 draw data，再由 `VulkanRendererSystem` 在同一个 swapchain command buffer 中提交 scene viewport image 和 ImGui draw data。

2. D10 需要明确 Vulkan GPU ownership。

   不建议让 `UISystem` 自己持有 Vulkan device / swapchain / command buffer，否则会形成第二套 renderer。推荐：

   ```text
   UISystem
     owns ImGui context
     owns GLFW platform backend
     builds ImGui draw data

   RendererV2 / VulkanRendererSystem
     owns Vulkan device / swapchain / command buffer
     owns Vulkan ImGui renderer backend bridge
     records ImGui draw data into swapchain command buffer
   ```

3. `ViewportOutput` 仍然保持后端无关。

   D10 完成后 Vulkan backend 返回：

   ```cpp
   output.backend = RendererBackendType::Vulkan;
   output.kind = ViewportOutputKind::VulkanImGuiTexture;
   output.width = viewport_width;
   output.height = viewport_height;
   output.native_handle = imgui_descriptor_token;
   ```

   `native_handle` 是不透明 token，只有 ImGui backend 解释，Editor 不知道它是 `VkDescriptorSet`。

建议任务拆分：

- D10-A：UI frame phase 拆分
  - 将 `UIService::endFrameAndRender()` 拆成更明确的阶段，例如：

    ```text
    finalizeFrame()      // ImGui::Render(), 生成 ImDrawData
    renderBackend()      // OpenGL legacy 提交 draw data；Vulkan 路径 no-op
    ```

  - Engine 推荐顺序调整为：

    ```text
    UI begin
    Editor 提交 ImGui
    UI finalizeFrame
    RenderContext swapBuffers
    rendererTick
    UI renderBackend       // OpenGL only
    Window present         // OpenGL only；Vulkan 由 RendererV2 present
    ```

  - `RendererV2` 可以在 `rendererTick()` 内部读取 `ImGui::GetDrawData()`，但调用点集中在 Vulkan ImGui bridge 中。

- D10-B：新增 Vulkan viewport target
  - 新增 RendererV2 内部资源，例如：

    ```text
    source/Engine/Function/RendererV2/targets/vulkan_viewport_target.h / .cpp
    ```

  - 负责创建：
    - color image / image view / allocation
    - depth image / image view / allocation
    - sampler
    - extent / format / layout 状态
  - color image usage 建议包含：

    ```text
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    VK_IMAGE_USAGE_SAMPLED_BIT
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT   // 后续调试 / picking 辅助可用
    ```

  - depth image usage：

    ```text
    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    ```

- D10-C：`VulkanForwardPass` 支持通用 render target
  - 不再只围绕 swapchain image index 录制。
  - 抽出轻量 target 描述：

    ```text
    VulkanRenderTarget
      color_image
      color_view
      color_format
      depth_view
      depth_format
      extent
    ```

  - Forward pass 先绘制到 viewport target，结束后把 color image transition 到 `SHADER_READ_ONLY_OPTIMAL`。

- D10-D：新增 Vulkan ImGui backend bridge
  - 建议放在 RendererV2 内部，例如：

    ```text
    source/Engine/Function/RendererV2/ui/vulkan_imgui_renderer.h / .cpp
    ```

  - 负责：
    - `ImGui_ImplVulkan_Init`
    - descriptor pool
    - font upload
    - `ImGui_ImplVulkan_AddTexture`
    - `ImGui_ImplVulkan_RemoveTexture`
    - `ImGui_ImplVulkan_RenderDrawData`
    - shutdown / swapchain recreate 资源同步
  - 使用 Vulkan 1.3 dynamic rendering 路径初始化 ImGui Vulkan backend。

- D10-E：`VulkanRendererSystem` frame flow 重排
  - 每帧流程推荐：

    ```text
    build RenderView
    build RenderSceneFrame
    build VulkanDrawList

    acquire swapchain image
    record command buffer:
      render scene into viewport target
      transition viewport target -> shader read
      render ImGui draw data into swapchain image
      transition swapchain image -> present
    submit
    present
    ```

  - 若没有有效 ImGui draw data，仍能 fallback 到 clear / direct swapchain path，避免 runtime-only 模式崩溃。

- D10-F：`RendererService::setViewportSize()` 恢复为 Vulkan viewport target resize
  - D9 中 ExternalSwapchain 不响应 panel size。
  - D10 中一旦返回 `VulkanImGuiTexture`，`ViewportPanel::syncViewportResize()` 会重新调用 `setViewportSize(panel_width, panel_height)`。
  - VulkanRendererSystem 应把这个尺寸用于 offscreen viewport target，而不是 swapchain。

- D10-G：文档和示例同步
  - 更新本文档 D10 进度。
  - 明确 D10 只是 viewport image，不包含 Vulkan picking。
  - Sandbox / Editor 示例说明 Vulkan viewport image 的状态。

验收：

- ViewportPanel 显示 Vulkan 渲染画面。
- Resize 稳定。
- 不与 ImGui backend 冲突。
- Vulkan backend 下 `getViewportOutput()` 返回 `VulkanImGuiTexture`，且 `native_handle` 非空。
- OpenGL legacy viewport 显示、UI render 和 window present 不退化。
- `cmake --build --preset msvc-vcpkg-renderer-v2-debug` 通过。
- `cmake --build --preset msvc-vcpkg-debug` 通过。
- `cmake --build --preset msvc-legacy-debug` 通过。
- `bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe` smoke check 通过。

### D11：Vulkan picking

目标：

- Vulkan backend 支持 entity picking。
- Editor 点击 Vulkan embedded viewport 后可以选中实体。
- picking 坐标、viewport 显示方向和 ObjectId target 像素语义一致。

D11 开始前基础：

- `RenderObjectData.entity_id` 已经存在。
- `RenderSceneFrameObject.entity_id` 已经存在。
- `VulkanMeshDrawItem.entity_id` 已经存在。
- `ViewportPanel` 已经通过 `RendererService::pickViewport()` 发起 picking 请求。
- `VulkanRendererSystem::pickViewport()` 在 D11 前仍返回 `supported = false`。

设计边界：

- D11 只做 Editor viewport picking，不做 GPU selection outline / hover highlight。
- D11 不把 Vulkan 类型暴露到 Editor / Scene / Resource。
- 第一版允许同步 readback 和保守等待，优先保证行为正确；后续再优化为异步 readback。
- transparent picking 第一版可以先不做或按 opaque 后续扩展，避免提前引入透明排序语义。

前置修复：viewport Y 轴显示和坐标约定

- 当前 `VulkanRenderDataTranslator::toVulkanProjection()` 已经集中处理 Vulkan projection Y flip。
- `ViewportPanel::drawVulkanImGuiViewport()` 不应继续沿用 OpenGL texture 的 UV 翻转。
- OpenGL legacy viewport 仍使用：

```cpp
ImVec2(0.0f, 1.0f),
ImVec2(1.0f, 0.0f)
```

- Vulkan ImGui viewport 应使用：

```cpp
ImVec2(0.0f, 0.0f),
ImVec2(1.0f, 1.0f)
```

- 不要通过删除 projection Y flip 来修复显示方向；projection flip 是 Vulkan clip space 修正，ImGui UV flip 是显示层问题。
- picking 坐标也要跟随这个约定：
  - OpenGL framebuffer 坐标按 bottom-left 语义转换。
  - Vulkan viewport image 坐标按 top-left 语义转换。
- 更清爽的长期方案是在 `ViewportOutput` 中补充 coordinate origin 描述，避免 `ViewportPanel` 硬编码 backend 判断。

任务：

- D11-A：修复 viewport Y 轴显示与 picking 坐标基础
  - `drawVulkanImGuiViewport()` 改为不翻转 UV。
  - `ViewportPanel::pickEntityAtMouse()` 根据 output coordinate origin 计算 framebuffer y。
  - 推荐新增轻量类型：

    ```cpp
    enum class ViewportCoordinateOrigin {
        TopLeft,
        BottomLeft,
    };
    ```

  - `ViewportOutput` 设置默认 origin，OpenGL 为 `BottomLeft`，Vulkan 为 `TopLeft`。

- D11-B：新增 Vulkan picking target
  - 建议新增：

    ```text
    source/Engine/Function/RendererV2/targets/vulkan_picking_target.h / .cpp
    ```

  - color format 使用 `VK_FORMAT_R32_SINT`，clear value 为 `-1`。
  - usage 至少包含：

    ```text
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    ```

  - depth target 可以第一版独立创建，避免复用 viewport target depth 时引入复杂 layout / loadOp 语义。
  - target extent 必须跟 viewport target 保持一致，`setViewportSize()` resize 时同步重建。

- D11-C：新增 ObjectId pass
  - 建议新增：

    ```text
    source/Engine/Function/RendererV2/passes/vulkan_object_id_pass.h / .cpp
    source/Engine/Function/RendererV2/shaders/vulkan_object_id.hlsl
    ```

  - vertex shader 使用与 forward pass 相同的 mesh position / model / view_projection。
  - pixel shader 输出 `int entity_id` 到 `VK_FORMAT_R32_SINT` target。
  - 每个 draw item 通过 push constant 传：

    ```text
    view_projection
    model
    entity_id
    ```

  - clear 为 `-1`，代表未命中。
  - 第一版只绘制 opaque items，后续再处理 transparent / gizmo / helper geometry。

- D11-D：readback helper
  - picking 请求触发时，从 ObjectId target copy 1x1 pixel 到 host visible buffer。
  - 第一版可以在 `pickViewport()` 中保守 `vkDeviceWaitIdle()` 或等待当前 frame fence，保证上一帧 ObjectId target 可读。
  - copy 流程：

    ```text
    transition ObjectId image -> TRANSFER_SRC_OPTIMAL
    vkCmdCopyImageToBuffer(1x1)
    submit and wait
    map/read int32
    ```

  - 读完后可以保持 tracked layout，下一帧 object pass 再转回 color attachment。

- D11-E：`VulkanRendererSystem` frame flow 接入
  - 每帧在 forward viewport pass 附近记录 ObjectId pass。
  - 推荐流程：

    ```text
    render scene color -> viewport target
    render object id -> picking target
    transition viewport color -> shader read
    render ImGui -> swapchain
    present
    ```

  - `pickViewport()` 读取最近一帧 picking target。
  - target 未 ready 时返回 `supported = true, ready = false`，不要清空 selection。

- D11-F：文档和验证
  - 更新本 roadmap 的 D11 进度。
  - 验证 Vulkan clicking selection。
  - 验证 OpenGL legacy picking 不退化。

验收：

- Editor 点击 viewport 可选中实体。
- 点击空白处返回 `entity_id = -1`，清空 selection 的行为与 OpenGL legacy 一致。
- Vulkan viewport 显示方向正确。
- Vulkan picking 坐标不再上下颠倒。
- resize 后 picking target 和 viewport target 尺寸一致。
- `cmake --build --preset msvc-vcpkg-renderer-v2-debug` 通过。
- `cmake --build --preset msvc-vcpkg-debug` 通过。
- `cmake --build --preset msvc-legacy-debug` 通过。
- `bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe` smoke check 通过。

### D12：OpenGL legacy 退役

目标：

- Vulkan 成为默认主路径。
- OpenGL 代码从默认构建和主线架构中移除。
- `RendererService` / `RenderDataPacket` / `ViewportOutput` 保持为上层唯一可见契约。
- Editor / Scene / Resource / Gameplay 不再感知 OpenGL 语义，也不直接感知 Vulkan 内部类型。

设计原则：

- 参考 Godot 的 RenderingServer / driver 边界：引擎上层只面向渲染服务契约，具体图形 API 只存在于后端实现内部。
- 参考 Unreal 的模块边界：RendererModule 对外暴露稳定服务，backend 作为模块内部实现，退役一个 backend 不应牵动 Editor / Scene。
- 参考 Unity 的渲染管线思路：Scene 提交的是后端无关帧数据，GPU resource / pass / target 生命周期由渲染后端自己管理。
- D12 的目标是“删除历史假设”，不是继续维护一个通用双后端 RHI。

任务：

- D12-A：OpenGL 引用盘点与删除边界确认
  - 使用 `rg` 盘点 `OpenGL`、`glad`、`GL_`、`RendererCommand`、`RendererFactory`、`OpenGLTexture`、`Texture2D`、`VertexArray`、`RendererSystem` 等旧路径引用。
  - 将引用分成四类：
    - public contract：必须删除或改成后端无关表达。
    - Editor / Scene 调用点：必须迁移到 `RendererService` / `ViewportOutput` / `pickViewport()`。
    - OpenGL backend implementation：可以整体删除。
    - CMake / vcpkg / docs / sample：随代码删除同步清理。
  - 若发现 OpenGL 类型仍出现在 public header 中，优先修契约再删除实现。

- D12-B：冻结渲染模块 public contract
  - 保留 `RendererService` 作为唯一渲染服务入口。
  - 保留 `RenderDataPacket` 作为上层提交帧数据的契约。
  - 保留 `ViewportOutput` / `ViewportCoordinateOrigin` / `ViewportPickRequest` / `ViewportPickResult`。
  - 删除或下沉兼容期 OpenGL wrapper，例如 `getViewportColorAttachment()`、`readViewportEntityID()` 这类 OpenGL framebuffer / readPixel 语义接口。
  - `RendererBackendType` 最终只保留 `Unknown` / `Vulkan`，或把 `OpenGLLegacy` 放到临时 legacy 编译开关内。

- D12-C：默认 Vulkan 构建策略切换
  - 顶层默认改为：
    - `NEXAUR_BUILD_RENDERER_V2=ON`
    - `NEXAUR_DEFAULT_GRAPHICS_API=Vulkan`
    - `NEXAUR_BUILD_LEGACY_OPENGL=OFF` 或直接删除该选项。
  - `msvc-vcpkg-debug` 默认应启动 Vulkan 路径。
  - RendererV2 专用 preset 可以在稳定后并入默认 preset，避免长期保留“新后端专用构建”。
  - 删除或改名旧 OpenGL legacy preset，避免误以为 OpenGL 仍是主路径。

- D12-D：删除旧 OpenGL backend implementation
  - 删除旧 OpenGL 渲染系统、OpenGL render device、OpenGL resource cache、OpenGL forward pipeline、OpenGL framebuffer / picking target。
  - 删除或下沉 `RendererFactory` / `RendererCommand`，不要再保留静态 OpenGL 风格入口。
  - 删除旧 `Renderer/Platform/OpenGL/` 相关实现。
  - 删除旧 GPU resource wrapper，例如只服务 OpenGL 的 `Texture2D`、`TextureCubeMap`、`VertexArray`、`Shader`、`Framebuffer` 等。
  - 若担心一次性删除回滚困难，可以先在删除前打一个 git tag 或保留远端分支作为历史回退，不建议在主线继续保留 inactive OpenGL 代码。

- D12-E：UI backend 收口
  - UI 模块只负责 ImGui context、platform backend、frame begin / finalize 和编辑器 UI 构建。
  - Vulkan ImGui renderer 继续归 `VulkanRendererSystem` / RendererV2 内部管理。
  - 删除 `ImGui_ImplOpenGL3` 初始化、渲染和 shutdown 路径。
  - 评估是否移除 `UIService::renderBackend()`；如果 Vulkan 是唯一后端，Engine frame order 可以收口为：
    ```text
    UI beginFrame
    Editor / Runtime build UI
    UI finalizeFrame
    RendererService::render consumes ImGui draw data
    ```

- D12-F：CMake / vcpkg / external 依赖清理
  - 从 `vcpkg.json` 移除 `glad`、OpenGL-only ImGui backend feature、OpenGL-only loader 依赖。
  - 从 CMake 删除 OpenGL backend source list、include path、compile definition 和 link target。
  - 清理 `external/` 中只为 OpenGL legacy 服务的依赖入口。
  - 确保 Vulkan / ImGui / GLFW / VMA / DXC / shader compile path 仍由 vcpkg 统一管理。

- D12-G：目录和命名收口
  - D12 内先保证删除 OpenGL 后项目结构可读。
  - 删除后再评估是否把 `RendererV2` 收口为长期命名，例如：
    ```text
    source/Engine/Function/Renderer/
      renderer_module.*
      RHI/renderer_service.*
      data/render_data.*
      Vulkan/
        vulkan_renderer_system.*
        resources/
        passes/
        targets/
        ui/
        shaders/
    ```
  - 如果重命名影响面过大，可以把目录收口拆到 D12.1；但 D12 需要先把“旧 Renderer == OpenGL”的历史语义清干净。

- D12-H：文档、示例和验证清理
  - 更新本 roadmap，把 D12 进度从“待开始”推进到“进行中 / 已完成”。
  - 更新架构文档，说明当前项目只有 Vulkan 主后端。
  - 删除旧 OpenGL 示例、旧 GLSL shader、旧调试说明，或明确标为历史资料。
  - 用 `rg` 确认 public header 中不再出现 OpenGL-only 类型。

验收：

- 默认 backend 为 Vulkan。
- OpenGL-only 类型不再出现在 Scene / Resource / Editor 公开接口。
- 默认 vcpkg Debug 构建通过。
- Vulkan renderer preset 构建通过；若已并入默认 preset，则默认 preset 即为 Vulkan 验证路径。
- Sandbox / Editor smoke check 通过，viewport image、picking、resize、UI docking 行为不退化。
- `rg "glad|ImGui_ImplOpenGL3|OpenGLTexture|RendererCommand|RendererFactory|GL_" source/Engine` 不再命中主线 public contract；若仍有命中，必须只存在于明确标记的历史文档或临时 legacy 目录中。
- `RendererService` 对外接口不包含 OpenGL texture id / OpenGL framebuffer / readPixel 语义。

当前状态：

```text
已完成
验证：
- cmake --preset msvc-vcpkg 通过
- cmake --build --preset msvc-vcpkg-debug 通过
- bin/msvc-vcpkg/Debug/Sandbox.exe 5 秒 smoke check 通过
- rg "OpenGL|OpenGLLegacy|OpenGLTexture|numeric_handle|getViewportColorAttachment|readViewportEntityID|ImGui_ImplOpenGL3|glad|RendererCommand|RendererFactory|\bRendererSystem\b|ResolvedRenderDataPacket|RenderModelData|VertexArray|TextureCubeMap|std::shared_ptr<Texture2D>" source/Engine CMakeLists.txt CMakePresets.json vcpkg.json 无主线命中
备注：
- 旧 Renderer/Passes、Renderer/Resources、Renderer/Platform/OpenGL 和旧 OpenGL RHI 文件已删除。
- AssetType::Texture2D 仍保留，它是资产类型，不是 OpenGL GPU wrapper。
- D12 未进行 RendererV2 目录改名；如需收口目录，可拆 D12.1。
```

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
| ImGui backend 生命周期冲突 | Editor viewport 无法显示或崩溃 | D12 后 UI 只保留 GLFW platform backend，Vulkan ImGui renderer 归 Renderer 后端 |
| viewport output 重新绑定图形 API 原生对象 | 新后端被迫模拟旧接口 | 保持 `ViewportOutput` 不透明 token，不在 Editor / Scene 解释后端资源 |
| AssetManager 与 externalRenderer loader 双轨 | 重复加载、缓存复杂 | D7 已保持 AssetManager 为唯一入口，externalRenderer loader 只作参考；中期继续统一 CPU asset contract |
| picking 延迟或不可用 | Editor 交互退化 | D11 已实现 Vulkan ObjectId pass / readback；后续可优化同步策略 |
| OpenGL 删除后缺少历史回退 | 调试旧行为时需要查历史 | 使用 git 历史 / 分支回退；主线不继续保留 inactive OpenGL |

## 8. 当前下一步

推荐下一步：

```text
D12.1 可选：RendererV2 目录命名收口
或进入后续：补全材质、纹理、灯光、场景模块，准备小游戏 demo
```

D12 完成后的已确认状态：

- `externalRenderer/` 仅作为临时本地参考目录。
- 新渲染模块在 `source/Engine/Function/RendererV2/` 中重构。
- NexAur 顶层已升级到 C++20。
- NexAur 顶层已具备 vcpkg manifest / preset 构建路径。
- RendererV2 新增 shader 统一使用 HLSL -> SPIR-V 流程。
- WindowService 已能提供 graphics API 状态和 Vulkan instance extensions。
- Vulkan no-api window 编译路径已验证通过。
- RendererV2 Vulkan 后端已能启动、清屏、present 和关闭。
- RendererV2 已具备 `RenderDataPacket -> RenderView` 转换。
- RendererV2 已具备 `AssetHandle -> VulkanModelResource` 资源缓存链路。
- RendererV2 已具备 `RenderDataPacket -> RenderSceneFrame -> VulkanDrawList -> VulkanForwardPass` 的最小绘制链路。
- RendererV2 已具备 Vulkan viewport image，可嵌入 Editor viewport。
- RendererV2 已具备 Vulkan ObjectId picking。
- OpenGL legacy 已从默认构建、vcpkg 依赖、UI backend 和主线源码中退役。

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
- 完成 D7：新增 RendererV2 内部资源目录 `resources/`，包含 `VulkanModelResource`、`VulkanMeshResource`、`VulkanMaterialResource`。
- 完成 D7：新增 `VulkanResourceContext` 与 `VulkanResourceUploadContext`，将 Backend 的 Vulkan 句柄以最小上下文传给 resource cache。
- 完成 D7：`VulkanRenderResourceCache` 持有 VMA allocator、专用 upload command pool 和 `AssetHandle -> VulkanModelResource` 缓存。
- 完成 D7：`VulkanMeshResource` 使用 staging buffer 上传 CPU mesh 顶点 / 索引数据，并创建 device-local 倾向的 vertex / index buffer。
- 完成 D7：`VulkanMaterialResource` 第一版只记录材质名和纹理路径，不创建 texture / sampler。
- 完成 D7：`VulkanRendererSystem::Backend` 初始化时创建 resource cache，shutdown 时在 device 销毁前释放缓存资源。
- 验证 D7：`cmake --build --preset msvc-vcpkg-renderer-v2-debug` 通过。
- 验证 D7：`cmake --build --preset msvc-vcpkg-debug` 通过。
- 验证 D7：`cmake --build --preset msvc-legacy-debug` 通过。
- 验证 D7：`bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe` 3 秒 smoke check 通过。
- 补齐 D8 方案：明确 D8-0 先把 RendererV2 Vulkan baseline 从临时 1.1 对齐到 1.3+，后续绘制路径使用 dynamic rendering。
- 补齐 D8 方案：拆分 `RenderSceneFrame`、`VulkanDrawList`、dynamic rendering 最小绘制路径和验收标准。
- 改进 D8 方案：参考现代引擎分层，将 D8 数据流调整为 `RenderSceneFrame -> VulkanDrawList -> VulkanForwardPass`，避免 `VulkanRendererSystem` 和 `RenderScene` 职责过重。
- 完成 D8-0：RendererV2 Vulkan baseline 对齐到 1.3+，启用 `dynamicRendering`、`synchronization2` 和 `shaderDemoteToHelperInvocation`。
- 完成 D8-A/B：新增 RendererV2 内部 `RenderSceneFrame` 和 `RenderSceneFrameBuilder`，从 `RenderDataPacket` 复制、清洗并分流对象、灯光和环境数据。
- 完成 D8-C/D：新增 `VulkanDrawList` 和 `VulkanDrawListBuilder`，由 draw-list builder 负责 `AssetHandle -> VulkanModelResource -> VulkanMeshDrawItem` 解析，Backend 只做帧流程编排。
- 完成 D8-E：新增 `VulkanForwardPass`，使用 HLSL -> DXC -> SPIR-V、Vulkan 1.3 dynamic rendering、swapchain image view、depth attachment 和最小 debug forward pipeline 绘制 opaque mesh。
- 完成 D8 支撑项：新增 CPU runtime procedural model 路径，Sandbox 示例不再通过 legacy `RendererFactory` 创建过程模型 GPU 资源。
- 验证 D8：`cmake --build --preset msvc-vcpkg-renderer-v2-debug` 通过。
- 验证 D8：`bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe` 3 秒 smoke check 通过。
- 验证 D8：`cmake --build --preset msvc-vcpkg-debug` 通过。
- 验证 D8：`cmake --build --preset msvc-legacy-debug` 通过。
- 补充 D9 方案：明确 Editor viewport 过渡期的 ExternalSwapchain 语义、viewport size 策略、picking/gizmo 降级和 Vulkan UI 可见性边界。
- 完成 D9-A：Vulkan `ExternalSwapchain` 的 viewport output 改为描述 swapchain / surface 尺寸，`setViewportSize()` 不再被 panel size 污染。
- 完成 D9-B：`ViewportPanel` 拆分 OpenGL texture、Vulkan ImGui texture、ExternalSwapchain 和 None 的显示策略。
- 完成 D9-C：ExternalSwapchain / None 下跳过 picking 和 gizmo，不清空当前 selection。
- 完成 D9-D：EditorCamera 在 OpenGL embedded viewport 下继续使用 panel size，在 Vulkan ExternalSwapchain 下使用 renderer output size。
- 完成 D9-D：Vulkan ExternalSwapchain 过渡期可作为全窗口临时输入目标，相机移动仍能驱动主 swapchain 场景。
- 验证 D9：`cmake --build --preset msvc-vcpkg-renderer-v2-debug` 分步验证通过。
- 验证 D9：`bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe` 3 秒 smoke check 通过。
- 完成 D10-A：拆分 `UIService` frame phase，新增 `finalizeFrame()` / `renderBackend()`，Engine 调度改为先生成 ImGui draw data，再由 Renderer 消费。
- 完成 D10-B：新增 RendererV2 内部 `VulkanViewportTarget`，持有 offscreen color/depth image、image view 和 sampler。
- 完成 D10-C：`VulkanForwardPass` 新增通用 `VulkanRenderTarget` record 路径，旧 swapchain record 保留为 fallback wrapper。
- 完成 D10-D：新增 RendererV2 内部 `VulkanImGuiRenderer`，负责 ImGui Vulkan backend、viewport texture descriptor 和 draw data 提交。
- 完成 D10-E：`VulkanRendererSystem` frame flow 改为 viewport target scene pass -> shader read transition -> ImGui swapchain pass -> present。
- 完成 D10-F：Vulkan backend 在 descriptor ready 后返回 `ViewportOutputKind::VulkanImGuiTexture`，`native_handle` 为 backend-owned ImGui texture token。
- 完成 D10-F：`RendererService::setViewportSize()` 在 Vulkan path 下驱动 offscreen viewport target resize，不再驱动 swapchain 尺寸。
- 修复 D10 启动断言：Vulkan 路径不再手动调用 `io.Fonts->Build()`，字体纹理由 ImGui Vulkan backend 接管。
- 验证 D10：`cmake --build --preset msvc-vcpkg-renderer-v2-debug` 通过。
- 验证 D10：`cmake --build --preset msvc-vcpkg-debug` 通过。
- 验证 D10：`cmake --build --preset msvc-legacy-debug` 通过。
- 验证 D10：`bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe` 3 秒 smoke check 通过。
- 完成 D11-A：新增 `ViewportCoordinateOrigin`，OpenGL viewport 使用 bottom-left 坐标，Vulkan viewport 使用 top-left 坐标。
- 完成 D11-A：修复 Vulkan viewport image 的 ImGui UV，避免沿用 OpenGL framebuffer 的上下翻转。
- 完成 D11-B：新增 `VulkanPickingTarget`，持有 `VK_FORMAT_R32_SINT` ObjectId image、独立 depth image 和 1 像素 readback buffer。
- 完成 D11-C：新增 `VulkanObjectIdPass` 和 `vulkan_object_id.hlsl`，通过 `firstInstance` / `SV_InstanceID` 写入实体 ID，避免 push constants 超过 Vulkan 最小 128 bytes。
- 完成 D11-D：`VulkanRendererSystem::pickViewport()` 使用同步 readback 从上一帧 ObjectId target 读取实体 ID。
- 完成 D11-E：Vulkan frame flow 增加 ObjectId pass，viewport resize 时同步重建 picking target。
- 验证 D11：`cmake --build --preset msvc-vcpkg-renderer-v2-debug` 通过。
- 验证 D11：`cmake --build --preset msvc-vcpkg-debug` 通过。
- 验证 D11：`cmake --build --preset msvc-legacy-debug` 通过。
- 验证 D11：`bin/msvc-vcpkg-renderer-v2/Debug/Sandbox.exe` 5 秒 smoke check 通过，stderr 为空。
- 补充 D12 方案：按 OpenGL 引用盘点、public contract 冻结、默认 Vulkan 切换、OpenGL backend 删除、UI backend 收口、CMake / vcpkg 清理、目录命名收口、文档验证八步拆分 OpenGL legacy 退役工作。
- 完成 D12-A/B：`RenderDataPacket` 去除旧 GPU pointer resolved data，`RendererService` 删除 OpenGL texture id / readPixel 兼容 wrapper，`ViewportOutput` 删除 `OpenGLTexture` / `numeric_handle`。
- 完成 D12-C/F：顶层默认构建收口为 vcpkg + Vulkan 主路径，删除 legacy / RendererV2 专用 preset，`vcpkg.json` 移除 `glad` 和 ImGui `opengl3-binding`。
- 完成 D12-D：删除旧 OpenGL RHI、`RendererCommand` / `RendererFactory`、旧 OpenGL passes、旧 OpenGL resource cache 和 `Renderer/Platform/OpenGL` implementation。
- 完成 D12-E：UI 模块只保留 ImGui GLFW platform backend，移除 OpenGL ImGui backend 和 `UIService::renderBackend()`。
- 完成 D12-H：主线 `rg` 检查无 OpenGL-only public contract 命中。
- 验证 D12：`cmake --preset msvc-vcpkg` 通过。
- 验证 D12：`cmake --build --preset msvc-vcpkg-debug` 通过。
- 验证 D12：`bin/msvc-vcpkg/Debug/Sandbox.exe` 5 秒 smoke check 通过。

# 构建、运行与测试

最后核对：2026-10-18。对应 [CMakePresets.json](../../CMakePresets.json)、[依赖清单](../../vcpkg.json)和当前 CMake 测试注册。

## 环境

当前仓库提供 Windows x64 / Visual Studio 2022 预设：

- Visual Studio 2022，安装 C++ 桌面开发工具链与 Windows SDK。
- CMake 3.25 或更高版本。
- vcpkg；环境变量 `VCPKG_ROOT` 指向包含 `scripts/buildsystems/vcpkg.cmake` 的目录。
- Vulkan SDK，以及支持 Vulkan 1.3 的 GPU/驱动。Debug 构建会请求 validation layer，实际启用情况须查看日志。
- DXC 通过 vcpkg 的 `directx-dxc` 包提供；HLSL 在构建中编译成 SPIR-V。

硬件 Ray Query 是可选能力。缺少光追支持时保留 Raster 路径；RT 反射还需要独立的 descriptor-indexing 能力。

首次 configure 会解析并安装 manifest 依赖，需要相应网络和磁盘空间。主线不使用 `external/` 的旧依赖 fallback；`NEXAUR_USE_VCPKG_DEPS=OFF` 会直接报错。

## 配置与构建

在仓库根目录运行：

```powershell
cmake --list-presets
cmake --preset msvc-vcpkg
cmake --build --preset msvc-vcpkg-debug
```

Release 构建：

```powershell
cmake --build --preset msvc-vcpkg-release
```

预设的构建目录为 `build/msvc-vcpkg/`，默认 Debug/Release 产物分别在 `bin/msvc-vcpkg/Debug/` 和 `bin/msvc-vcpkg/Release/`。

| Target | 用途 |
| --- | --- |
| `NexAurEngine` | 引擎；默认共享库，可通过 `NEXAUR_BUILD_SHARED` 调整 |
| `NexAurRendererFrontend` | 后端无关的帧数据整理 |
| `NexAurRendererVulkan` | Vulkan 后端静态库 |
| `NexAurVulkanShaders` | HLSL → SPIR-V |
| `Sandbox` | 编辑器/样例入口和 Runtime smoke harness |
| `NexAurRendererTests` | Renderer focused tests |

Engine 源码显式列在 [Engine CMake](../../source/Engine/CMakeLists.txt) 中；新增文件需要登记。Shader 程序通过 [manifest](../../source/Engine/Function/Renderer/Vulkan/shaders/vulkan_shader_manifest.cmake)维护。

## 运行

```powershell
./bin/msvc-vcpkg/Debug/Sandbox.exe
```

以仓库根目录作为工作目录，保留 executable 同目录的依赖 DLL 和 `shaders/Renderer/Vulkan/*.spv`。ShaderLibrary 从 executable 所在目录定位编译后的 Shader。资产边界见[资产清单](../../assets/asset_manifest.md)。

`saved/` 为本地生成数据，未纳入 Git。默认场景包含可选模型；缺少本地 DamagedHelmet 时会跳过该样例。

## 测试

`include(CTest)` 默认开启 `BUILD_TESTING`。如旧缓存关闭了测试，可重新配置：

```powershell
cmake --preset msvc-vcpkg -DBUILD_TESTING=ON
cmake --build --preset msvc-vcpkg-debug --target NexAurRendererTests Sandbox
ctest --test-dir build/msvc-vcpkg -C Debug -N
```

按改动范围选择测试，不必每次运行所有测试。例如：

```powershell
ctest --test-dir build/msvc-vcpkg -C Debug -R '^NexAur[.]Renderer[.]' --output-on-failure
ctest --test-dir build/msvc-vcpkg -C Debug -R '^NexAur[.](SceneSerializerSmoke|InputActionSmoke|GameplaySystemsSmoke)$' --output-on-failure
```

Renderer 测试覆盖 FrameContract、RenderSettings、ShadowFrameBuilder、RenderGraph image/buffer/AS 同步、FeaturePlan、History、RT reflection、retirement、FrameContext、异步上传和 BLAS/TLAS/scene table 契约。完整列表见 [Renderer 测试注册](../../source/Tests/Renderer/CMakeLists.txt)。

Sandbox 测试覆盖场景序列化、输入、音频、编辑器配置、材质与模型导入、Gameplay、Trigger、Camera、GameFlow 和 Prefab，见 [Sandbox 测试注册](../../source/Sandbox/CMakeLists.txt)。也可以直接选择一个 smoke：

```powershell
./bin/msvc-vcpkg/Debug/Sandbox.exe --scene-serializer-smoke
./bin/msvc-vcpkg/Debug/Sandbox.exe --smoke-all
```

`NexAur.SmokeAll` 会再次运行单独注册的 Sandbox cases；全量 CTest 会包含这部分重复执行。部分模型测试依赖可选本地资产，阅读测试输出中的 skip 信息。

## GPU 与视觉验证边界

CPU contract tests 不能证明实际画面或 GPU 同步正确。渲染改动还应按范围检查：

- 受影响的 Shader 编译，以及 Vulkan SDK 的 `spirv-val --target-env vulkan1.3` 验证。
- Debug 启动日志中的 validation layer 和 debug messenger 状态。
- Viewport resize、拾取、材质更新、Reflection Probe capture、功能开关与 fallback。
- 光追支持与禁用两条路径；仅启动几秒不能替代上述交互检查。

光追具体要求见 [RT 总计划](../plans/Renderer/RayTracing/ray_tracing_development_plan.md)和 [RT-10 专项](../plans/Renderer/RayTracing/ray_traced_reflection_foundation_plan.md)。其中已有的测试结果属于对应提交的历史记录。

## 常见问题

| 现象 | 检查方向 |
| --- | --- |
| 找不到 vcpkg toolchain | 检查 `VCPKG_ROOT` 和预设引用的 toolchain 路径 |
| 找不到 VS generator | 检查 VS 2022 C++ 工作负载；当前预设固定为 Visual Studio 17 2022 |
| 找不到 Vulkan / validation layer | 检查 Vulkan SDK、驱动和 Debug 启动日志 |
| 启动时报 Shader bytecode 缺失 | 重建 Shader target，确认产物已生成到 executable 旁的 Shader 目录 |
| 样例模型被跳过 | 按资产清单补充本地模型；该目录不随 Git 分发 |
| 旧文档中的测试名不存在 | 使用 `ctest -N` 和当前 CMake 注册；历史 Sandbox renderer tests 已拆到独立 target |

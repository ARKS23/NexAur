# NexAur 与 ARKRenderer 包管理及 CMake 构建方案

日期：2026-06-24

进度主文档：

```text
docs/plans/Renderer/renderer_vulkan_development_roadmap.md
```

本文是包管理与构建参考文档；后续开发进度、阶段状态和完成记录以进度主文档为准。

当前落地状态：

```text
D3 已完成。
顶层已新增 vcpkg manifest / CMakePresets。
顶层已升级到 CMake 3.25 + C++20。
vcpkg 路径与 vendored external fallback 均已验证可构建。
externalRenderer 仍只作为参考目录，不进入默认构建图。
```

## 1. 文档目标

本文档专门讨论 NexAur 接入 `externalRenderer` / ARKRenderer 时的包管理和 CMake 构建问题。

总体接入路线见：

```text
docs/plans/Renderer/ark_renderer_integration_plan.md
```

渲染接口重构方案见：

```text
docs/plans/Renderer/renderer_interface_redesign_plan.md
```

## 2. 当前状态

### 2.1 NexAur 当前依赖管理

NexAur 现在主要使用 `external/` 下的 vendored/submodule 依赖：

```text
external/glfw
external/glm
external/spdlog
external/entt
external/assimp
external/imgui
external/stb
external/glad
```

顶层 CMake：

```text
CMakeLists.txt
  add_subdirectory(external)
  add_subdirectory(source)
```

`external/CMakeLists.txt` 通过 `add_subdirectory()` 构建第三方库。

当前 NexAur 顶层 CMake 使用：

```cmake
cmake_minimum_required(VERSION 3.17)
set(CMAKE_CXX_STANDARD 17)
```

### 2.2 ARKRenderer 当前依赖管理

`externalRenderer` 是独立 CMake + vcpkg manifest 工程：

```text
externalRenderer/vcpkg.json
externalRenderer/CMakePresets.json
externalRenderer/CMakeLists.txt
```

它依赖：

```text
vulkan
vulkan-memory-allocator
volk
vk-bootstrap
spirv-headers
spirv-reflect
glfw3
glm
spdlog
fmt
imgui[docking-experimental,glfw-binding,vulkan-binding]
stb
ktx
directx-dxc
tinygltf
```

ARKRenderer 当前要求：

```cmake
cmake_minimum_required(VERSION 3.25)
target_compile_features(ark_renderer PUBLIC cxx_std_20)
```

## 3. 混合依赖模式的问题

如果直接把 `externalRenderer` 加入 NexAur，同时保留 NexAur 的 `external/` 依赖，会遇到这些问题：

- GLFW 可能被 vendored 和 vcpkg 构建两份。
- GLM / spdlog / imgui / stb 可能有两套 include 和 target。
- ImGui 后端可能一套 OpenGL，一套 Vulkan，生命周期难以统一。
- CMake target 名字可能冲突，例如 `glfw`、`glm`、`imgui`。
- vcpkg manifest 只在 externalRenderer 下生效，NexAur 顶层构建不可复现。
- ARKRenderer C++20 public headers 被 NexAur C++17 target include 时可能编译失败。
- Debug / Release runtime、triplet、编译选项可能不一致。

结论：

```text
长期不建议 NexAur 使用 vendored/submodule，而 ARKRenderer 使用 vcpkg。
```

短期可以混合，但必须非常克制，并把混合状态视为过渡。

## 4. 推荐最终方案：NexAur 顶层 vcpkg manifest

推荐最终由 NexAur 根目录统一管理 vcpkg manifest：

```text
NexAur/
  vcpkg.json
  CMakePresets.json
  CMakeLists.txt
  externalRenderer/
  source/
```

顶层 `vcpkg.json` 合并 NexAur 与 ARKRenderer 依赖：

```json
{
  "name": "nexaur",
  "version-string": "0.1.0",
  "dependencies": [
    "vulkan",
    "vulkan-memory-allocator",
    "volk",
    "vk-bootstrap",
    "spirv-headers",
    "spirv-reflect",
    "glfw3",
    "glad",
    "glm",
    "spdlog",
    "fmt",
    {
      "name": "imgui",
      "features": [
        "docking-experimental",
        "glfw-binding",
        "opengl3-binding",
        "vulkan-binding"
      ]
    },
    "stb",
    "ktx",
    "directx-dxc",
    "tinygltf",
    "entt",
    "assimp"
  ]
}
```

说明：

- `opengl3-binding` 只在 OpenGL legacy 过渡期需要。
- OpenGL 删除后可以移除 `opengl3-binding` 和 `glad`。
- 如果 vcpkg `glad` 的生成 profile / target 名称与当前 OpenGL legacy 不兼容，D3 可以暂时把 `external/glad` 作为 legacy isolated exception，但必须记录为待清理项，不能让它继续污染新 Vulkan 路径。
- `fmt` 建议保留，因为 ARKRenderer 使用 `fmt`。
- `directx-dxc` 在 Windows / Linux x64 平台用于 HLSL -> SPIR-V 编译。

## 5. CMakePresets 建议

顶层新增：

```json
{
  "version": 6,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 25,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "msvc-vcpkg",
      "displayName": "MSVC x64 + vcpkg",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build/msvc-vcpkg",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_TARGET_TRIPLET": "x64-windows",
        "VCPKG_MANIFEST_MODE": "ON",
        "NEXAUR_OUTPUT_ROOT": "${sourceDir}/bin/msvc-vcpkg"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "msvc-vcpkg-debug",
      "configurePreset": "msvc-vcpkg",
      "configuration": "Debug"
    },
    {
      "name": "msvc-vcpkg-release",
      "configurePreset": "msvc-vcpkg",
      "configuration": "Release"
    }
  ]
}
```

同时建议顶层 CMake 提升到：

```cmake
cmake_minimum_required(VERSION 3.25)
```

原因：

- ARKRenderer 已要求 CMake 3.25。
- 顶层统一版本可以减少子项目兼容分支。
- `NEXAUR_OUTPUT_ROOT` 用来隔离不同 preset 的主产物，避免 vcpkg 与 legacy fallback 同时写入同一个 `bin/Debug`。

## 6. C++ 标准建议

NexAur 当前是 C++17，ARKRenderer 是 C++20。

可选方案：

### 方案 A：NexAur 全项目升级到 C++20

优点：

- 最简单。
- ARKRenderer public headers 可直接 include。
- 后续可使用 `std::span`、`std::string_view`、concepts 等现代工具。

缺点：

- 需要确认现有依赖和 MSVC 设置都支持。

推荐程度：高。

### 方案 B：只让 ARK adapter target 使用 C++20

做法：

```text
NexAurEngine C++17
NexAurArkRendererBackend C++20
ARKRenderer C++20
```

优点：

- 对旧引擎代码影响小。

缺点：

- 如果 `NexAurEngine` 头文件需要 include ARKRenderer C++20 header，会失败。
- DLL / target 拆分更复杂。

推荐程度：中。

### 建议

如果你已经准备大幅重构渲染模块，建议直接把 NexAur 升到 C++20：

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

或者更现代：

```cmake
target_compile_features(NexAurEngine PUBLIC cxx_std_20)
```

## 7. external/ 与 vcpkg 的过渡策略

不建议一次性删除 `external/`。但既然后续主线已经确认统一到 vcpkg，D3 应该尽量把新路径做扎实，而不是只新增一个 manifest 留给后面收拾。

### 阶段 1：C++20 基线先落地

- 顶层 `cmake_minimum_required` 提升到 3.25。
- 顶层 / `NexAurEngine` 提升到 C++20。
- 仍使用当前 `external/` 构建一次，确认旧 OpenGL 路径没有被语言标准升级破坏。

### 阶段 2：新增顶层 vcpkg manifest / preset

- 新增顶层 `vcpkg.json`。
- 新增顶层 `CMakePresets.json`。
- preset 使用独立构建目录，例如 `build/msvc-vcpkg`，避免污染当前 `build/`。
- 顶层 manifest 包含当前 OpenGL legacy 和后续 RendererV2 依赖。

### 阶段 3：CMake 支持双依赖来源，并让 vcpkg 成为默认

控制方式：

- 引入 CMake option：

```cmake
option(NEXAUR_USE_VCPKG_DEPS "Use vcpkg packages for third-party dependencies." ON)
```

```cmake
if(NEXAUR_USE_VCPKG_DEPS)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(spdlog CONFIG REQUIRED)
    find_package(EnTT CONFIG REQUIRED)
    find_package(assimp CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    find_package(Stb REQUIRED)
else()
    add_subdirectory(external)
endif()
```

链接时也按来源分支：

```cmake
if(NEXAUR_USE_VCPKG_DEPS)
    target_link_libraries(NexAurEngine PUBLIC
        glfw
        glm::glm
        spdlog::spdlog
        EnTT::EnTT
        assimp::assimp
        imgui::imgui
    )
else()
    target_link_libraries(NexAurEngine PUBLIC
        glfw
        glm
        spdlog
        EnTT
        assimp
        imgui
    )
endif()
```

### 阶段 4：处理特殊依赖

#### stb

`external/stb` 现在提供了 `stb_image.cpp`，而 vcpkg `stb` 通常只提供 header。D3 不应该继续依赖 `external/stb/stb_image.cpp`。

建议新增一个引擎侧实现文件：

```text
source/Engine/ThirdParty/stb_image.cpp
```

内容只做一件事：

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
```

然后在 vcpkg 模式下把这个文件编入 `NexAurEngine`。

#### glad

OpenGL legacy 仍需要 `glad`。优先尝试 vcpkg `glad`，但这里最容易遇到 OpenGL profile / target 名称差异。

建议策略：

```text
优先：vcpkg glad
如果失败：external/glad 暂时作为 OpenGL legacy isolated exception
禁止：RendererV2 / Vulkan 路径依赖 glad
最终：OpenGL legacy 退役时删除 glad
```

#### imgui

vcpkg feature 可以同时启用：

```text
imgui[docking-experimental,glfw-binding,opengl3-binding,vulkan-binding]
```

但 D3 只解决依赖来源，不改变 UI backend 生命周期：

```text
D3：NexAur 仍初始化 OpenGL ImGui backend
D10：再处理 Vulkan viewport image / Vulkan ImGui descriptor
```

### 阶段 5：验证

- `cmake --preset msvc-vcpkg`
- `cmake --build --preset msvc-vcpkg-debug`
- 旧 OpenGL viewport 仍能构建。
- vcpkg 模式不重复构建 glfw / glm / spdlog / imgui / assimp。
- 如果保留 legacy fallback，则 `NEXAUR_USE_VCPKG_DEPS=OFF` 仍可配置构建。

### 阶段 6：OpenGL legacy 退役后删除重复 external

当 Vulkan 后端稳定：

- 删除 `external/glad` 或停止构建。
- 删除 `external/glfw` / `glm` / `spdlog` / `entt` / `assimp` 等重复 submodule。
- `.gitmodules` 同步清理。
- `external/` 只保留确实不能或不想通过 vcpkg 管理的本地库。

## 8. RendererV2 / ARKRenderer 构建边界

当前已确认 `externalRenderer/` 只是临时本地参考目录，最终渲染模块落在：

```text
source/Engine/Function/RendererV2/
```

因此 D3 不应该把 `externalRenderer` 直接加入 NexAur 顶层构建。D3 只需要保证 RendererV2 后续会用到的依赖已经由顶层 vcpkg 提供。

如果后续为了对照或临时验证需要把 ARKRenderer 当独立样例项目构建，ARKRenderer 自己的 CMake 可以保留或增加这些选项：

```cmake
option(ARK_BUILD_TESTS "Build ARKRenderer test executables." ON)
option(ARK_BUILD_APPS "Build ARKRenderer sample applications." ON)
option(ARK_COMPILE_SHADERS "Compile ARKRenderer shaders." ON)
```

但这不属于 D3 的主线任务。D3 主线应该是：

```text
NexAur 顶层 vcpkg manifest
  -> 提供 Vulkan / VMA / volk / vk-bootstrap / imgui / ktx / dxc 等依赖

RendererV2
  -> 后续在 source/Engine/Function/RendererV2 中重写/适配需要的结构

externalRenderer
  -> 继续作为参考代码，不进入默认构建图
```

说明：

- 当前 ARKRenderer 已有 `ARK_BUILD_TESTS`。
- `ark_sandbox` 目前总是构建，建议加 `ARK_BUILD_APPS` 控制。
- shader 编译通常应该保留，但未来可支持外部传入 shader output dir。

## 9. 避免重复 target 的策略

如果 `NEXAUR_USE_VCPKG_DEPS=ON`：

- 不要 `add_subdirectory(external/glfw)`。
- 不要 `add_subdirectory(external/glm)`。
- 不要 `add_subdirectory(external/spdlog)`。
- 不要 `add_subdirectory(external/assimp)`。
- 不要构建 vendored imgui。

如果 `NEXAUR_USE_VCPKG_DEPS=OFF`：

- 不建议接入 ARKRenderer。
- 或者需要让 ARKRenderer 也支持 vendored dependencies，这不推荐。

推荐原则：

```text
ARKRenderer 接入只支持 vcpkg 构建路径
旧 OpenGL legacy 可短期继续支持 vendored 构建路径
```

这样不会为了兼容旧路径把新系统拖复杂。

## 10. 顶层 CMake 集成草案

顶层：

```cmake
cmake_minimum_required(VERSION 3.25)
project(NexAur VERSION 0.1 LANGUAGES CXX)

option(NEXAUR_USE_VCPKG_DEPS "Use vcpkg dependencies." ON)
option(NEXAUR_BUILD_RENDERER_V2 "Build RendererV2 integration." OFF)
option(NEXAUR_BUILD_LEGACY_OPENGL "Build legacy OpenGL renderer." ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NEXAUR_USE_VCPKG_DEPS)
    # Do not add_subdirectory(external) for duplicated packages.
else()
    add_subdirectory(external)
endif()

add_subdirectory(source)
```

D3 暂时不链接 ARKRenderer。等 D5 创建 `RendererV2` 后，再决定是否新增独立 target，例如：

```cmake
if(NEXAUR_BUILD_RENDERER_V2)
    target_compile_definitions(NexAurEngine PRIVATE NEXAUR_ENABLE_RENDERER_V2=1)
endif()
```

注意：

- 如果 `NexAurEngine` 是 DLL，并且 ARKRenderer 是 static lib，需要确认 runtime library、导出符号和链接顺序。
- RendererV2 / ARKRenderer public include 不一定要暴露给所有消费者，尽量 `PRIVATE` 链接和 include。

## 11. ImGui 依赖策略

过渡期最容易乱的是 ImGui。

OpenGL legacy：

```text
imgui glfw backend
imgui opengl3 backend
```

ARKRenderer：

```text
imgui glfw backend
imgui vulkan backend
```

vcpkg feature 建议：

```text
imgui[docking-experimental,glfw-binding,opengl3-binding,vulkan-binding]
```

但代码层面要避免同一帧同时初始化两套 backend 管理同一个 ImGui context。

推荐过渡策略：

1. OpenGL backend 继续使用 NexAur 当前 UI。
2. ArkVulkan backend 第一版不接 NexAur Editor viewport image，也不启用 ARK sandbox UI。
3. 后续统一 NexAur UI backend 到 Vulkan。
4. 再接 Vulkan viewport image / descriptor。

## 12. Shader 编译策略

后续 RendererV2 / Vulkan 路径的新 shader 统一使用 HLSL 编写，并通过 DXC 编译到 SPIR-V。GLSL 不再作为新 Vulkan 后端的主线 shader 语言。

ARKRenderer 当前已经使用 DXC 编译 HLSL 到 SPIR-V：

```text
externalRenderer/shaders/*.hlsl
-> build/.../shaders/*.spv
```

接入 NexAur 后建议：

- 第一版保留或复刻 ARKRenderer 的 HLSL -> SPIR-V 编译流程。
- shader output dir 先由 RendererV2 / ARKRenderer 风格的 CMake 逻辑管理。
- 不要第一版就合并 NexAur 旧 OpenGL shader 与 RendererV2 Vulkan shader。
- 新增 Vulkan shader 文件优先放在 RendererV2 专属目录中，避免和 legacy OpenGL shader 语义混在一起。

长期可以考虑：

```text
assets/shaders/opengl/*
assets/shaders/vulkan/hlsl/*
assets/shaders/vulkan/generated_spv/*
assets/shaders/common/*
```

其中 `assets/shaders/vulkan/hlsl/*` 作为源文件目录，`generated_spv` 只放生成产物，不作为手写源码维护。这个目录归一可以放到 RendererV2 跑通后再执行。

## 13. externalRenderer 归属

当前已确认：

```text
externalRenderer/ 是临时本地参考目录
最终渲染模块在 source/Engine/Function/RendererV2 中重构
```

因此 D3 不处理 externalRenderer 归属，也不把它纳入默认构建图。

后续可以按代码成熟度选择：

### 方式 A：RendererV2 手动重构吸收

适合当前阶段。

做法：

```text
参考 externalRenderer 架构
  -> 在 RendererV2 中重写/适配 facade、scene、view、resource cache、Vulkan backend
  -> 不直接暴露 externalRenderer app/sandbox 层
```

### 方式 B：externalRenderer 保持独立仓库 / submodule

优点：

- 保持 ARKRenderer 独立发展。
- NexAur 记录具体版本。

缺点：

- 子模块使用复杂一点。
- 与 `RendererV2` 主线会出现双源码边界，需要明确谁是权威实现。

### 方式 C：CMake package / install 后引用

优点：

- 最干净的第三方库形态。

缺点：

- 对个人引擎开发过早复杂。

建议：

```text
短期：externalRenderer 只作为参考，不进默认构建
中期：RendererV2 在 source/Engine/Function/RendererV2 中形成稳定边界
长期：如果 ARKRenderer 不再独立发展，可以把需要的代码正式迁入 RendererV2
```

## 14. 实施步骤

### PR-C1：新增顶层 vcpkg manifest 草案

- 新增 `vcpkg.json`。
- 新增 `CMakePresets.json`。
- 不立即删除 `external/`。

### PR-C2：提升 CMake / C++ 标准

- `cmake_minimum_required` 提升到 3.25。
- `CMAKE_CXX_STANDARD` 提升到 20。
- 验证旧 OpenGL 路径是否仍能构建。

### PR-C3：CMake 增加依赖来源选项

- 新增 `NEXAUR_USE_VCPKG_DEPS`。
- vcpkg 模式使用 `find_package`。
- legacy 模式继续 `add_subdirectory(external)`。

### PR-C4：RendererV2 依赖预留，不直接接 externalRenderer

- 新增 `NEXAUR_BUILD_RENDERER_V2` 作为后续开关。
- D3 不 `add_subdirectory(externalRenderer)`。
- D3 不链接 `ARKRenderer::ark_renderer`。
- D3 只保证 RendererV2 后续需要的依赖已经可以由顶层 vcpkg 提供。

### PR-C5：特殊依赖处理

- `stb`：新增 NexAur 自己的 `stb_image.cpp` 实现文件，vcpkg 只提供 header。
- `glad`：优先 vcpkg；如不兼容，暂时隔离为 OpenGL legacy exception。
- `imgui`：统一由 vcpkg 提供 core + glfw/opengl3/vulkan binding，但 D3 不初始化 Vulkan backend。

### PR-C6：清理 external legacy

- Vulkan backend 稳定后执行。
- 移除重复 submodule。
- OpenGL 删除后移除 glad。

## 15. 完成标准

- NexAur 顶层可以通过 vcpkg preset 一键配置和构建。
- ARKRenderer 不再使用独立 manifest 与 NexAur 顶层冲突。
- 不重复构建 glfw / glm / spdlog / imgui / assimp。
- NexAur 与 ARKRenderer 使用一致的 triplet 和运行库。
- OpenGL legacy 可以在过渡期继续构建，最终可删除。

## 16. 推荐结论

推荐最终统一为：

```text
NexAur 顶层 vcpkg manifest
  -> 管理 Engine + ARKRenderer 全部第三方依赖

externalRenderer
  -> 当前只作为临时本地参考目录
  -> 不进入 NexAur 默认构建图

external/
  -> 过渡期保留
  -> Vulkan 稳定后清理重复依赖
```

这会让渲染模块重构后的构建路径更可复现，也能避免双套 GLFW / ImGui / spdlog 带来的隐性问题。

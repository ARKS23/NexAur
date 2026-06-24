# NexAur 与 ARKRenderer 包管理及 CMake 构建方案

日期：2026-06-24

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
        "VCPKG_MANIFEST_MODE": "ON"
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

不建议一次性删除 `external/`。推荐分阶段：

### 阶段 1：保留 external，新增 vcpkg 预研

- 新增顶层 `vcpkg.json`。
- 新增顶层 `CMakePresets.json`。
- 暂时保留 `external/`。
- 不立即把 NexAur 全部依赖切到 vcpkg。

风险：

- 如果同时构建 external 和 vcpkg 版本的同一库，会冲突。

控制方式：

- 引入 CMake option：

```cmake
option(NEXAUR_USE_VCPKG_DEPS "Use vcpkg packages for third-party dependencies." ON)
```

### 阶段 2：CMake 支持双依赖来源

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

### 阶段 3：迁移 NexAur 依赖到 vcpkg

按顺序迁移：

1. glm。
2. spdlog / fmt。
3. glfw。
4. entt。
5. stb。
6. imgui。
7. assimp。

每迁移一个依赖，都跑一次构建和 Sandbox。

### 阶段 4：OpenGL legacy 退役后删除重复 external

当 Vulkan 后端稳定：

- 删除 `external/glad` 或停止构建。
- 删除 `external/glfw` / `glm` / `spdlog` / `entt` / `assimp` 等重复 submodule。
- `.gitmodules` 同步清理。
- `external/` 只保留确实不能或不想通过 vcpkg 管理的本地库。

## 8. ARKRenderer CMake 调整建议

为了作为 NexAur 子项目接入，ARKRenderer CMake 建议增加几个选项：

```cmake
option(ARK_BUILD_TESTS "Build ARKRenderer test executables." ON)
option(ARK_BUILD_APPS "Build ARKRenderer sample applications." ON)
option(ARK_COMPILE_SHADERS "Compile ARKRenderer shaders." ON)
```

NexAur 接入时：

```cmake
set(ARK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ARK_BUILD_APPS OFF CACHE BOOL "" FORCE)
add_subdirectory(externalRenderer)
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
option(NEXAUR_BUILD_ARK_RENDERER "Build ARKRenderer backend." ON)
option(NEXAUR_BUILD_LEGACY_OPENGL "Build legacy OpenGL renderer." ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NEXAUR_USE_VCPKG_DEPS)
    # Do not add_subdirectory(external) for duplicated packages.
else()
    add_subdirectory(external)
endif()

if(NEXAUR_BUILD_ARK_RENDERER)
    set(ARK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ARK_BUILD_APPS OFF CACHE BOOL "" FORCE)
    add_subdirectory(externalRenderer)
endif()

add_subdirectory(source)
```

Engine target：

```cmake
if(NEXAUR_BUILD_ARK_RENDERER)
    target_link_libraries(NexAurEngine PRIVATE ARKRenderer::ark_renderer)
    target_compile_definitions(NexAurEngine PRIVATE NEXAUR_ENABLE_ARK_RENDERER=1)
endif()
```

注意：

- 如果 `NexAurEngine` 是 DLL，并且 ARKRenderer 是 static lib，需要确认 runtime library、导出符号和链接顺序。
- ARKRenderer public include 不一定要暴露给所有消费者，尽量 `PRIVATE` 链接和 include。

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

ARKRenderer 当前使用 DXC 编译 HLSL 到 SPIR-V：

```text
externalRenderer/shaders/*.hlsl
-> build/.../shaders/*.spv
```

接入 NexAur 后建议：

- 第一版保留 ARKRenderer 自己的 shader 编译流程。
- shader output dir 继续由 ARKRenderer CMake 管理。
- 不要第一版就合并 NexAur `assets/shaders` 和 ARKRenderer `shaders`。

长期可以考虑：

```text
assets/shaders/opengl/*
assets/shaders/vulkan/*
assets/shaders/common/*
```

但这不是接入第一阶段重点。

## 13. externalRenderer 仓库归属

当前 `externalRenderer/` 是未跟踪目录。

可选方式：

### 方式 A：直接纳入 NexAur 仓库

优点：

- 最简单。
- 接入时改 CMake 和代码方便。

缺点：

- ARKRenderer 独立项目历史会并入 NexAur。

### 方式 B：Git submodule

优点：

- 保持 ARKRenderer 独立发展。
- NexAur 记录具体版本。

缺点：

- 子模块使用复杂一点。

### 方式 C：CMake package / install 后引用

优点：

- 最干净的第三方库形态。

缺点：

- 对个人引擎开发过早复杂。

建议：

```text
短期：作为 externalRenderer 子目录接入验证
中期：如果 ARKRenderer 仍独立开发，改为 git submodule
长期：如果完全成为 NexAur 渲染核心，可合并进 source/Engine/Function/Renderer/Backend/ArkVulkan
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

### PR-C4：ARKRenderer 子目录接入

- 新增 `NEXAUR_BUILD_ARK_RENDERER`。
- `ARK_BUILD_TESTS=OFF`。
- 新增 `ARK_BUILD_APPS` 后关闭 sandbox。
- 链接 `ARKRenderer::ark_renderer`。

### PR-C5：逐步迁移重复依赖

- glm。
- spdlog / fmt。
- glfw。
- entt。
- stb。
- imgui。
- assimp。

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
  -> 作为子目录或 submodule
  -> 不单独决定依赖版本

external/
  -> 过渡期保留
  -> Vulkan 稳定后清理重复依赖
```

这会让渲染模块重构后的构建路径更可复现，也能避免双套 GLFW / ImGui / spdlog 带来的隐性问题。

# NexAur 资产清单

最后核对：2026-10-18。本文区分当前 Vulkan 主线、Sandbox 样例与保留的旧资产。

## Vulkan Shader

当前构建输入为 [shaders/Renderer/Vulkan](shaders/Renderer/Vulkan) 下的 HLSL/HLSLI，包括 Forward、Shadow、AO、SSR/RT Reflection、Bloom、PostProcess、SMAA 和公共 include。

[Shader manifest](../source/Engine/Function/Renderer/Vulkan/shaders/vulkan_shader_manifest.cmake)驱动 DXC 编译。运行时加载 executable 同目录 `shaders/Renderer/Vulkan/` 中的 SPIR-V；不要仅复制 HLSL 源文件作为运行产物。编译输出规则见[构建说明](../docs/development/build_and_test.md)。

## 默认环境图

[SceneFactory](../source/Engine/Function/Scene/scene_factory.cpp)优先使用：

- `assets/textures/HDR/2k.hdr`

不存在时回退到：

- `assets/textures/HDR/warm_restaurant_8k.hdr`

它们是默认场景样例使用的环境图，不是任意场景都必须依赖的固定资产。若两者均缺失，需要自行配置环境资产。

## Sandbox 材质样例

Sandbox 的材质测试会使用以下 PBR 贴图目录：

- `assets/textures/PBR/gold/`
- `assets/textures/PBR/rusted_iron/`
- `assets/textures/PBR/grass/`
- `assets/textures/PBR/plastic/`
- `assets/textures/PBR/wall/`

## 可选本地模型

`assets/models/` 被 Git 忽略，不随仓库分发。DamagedHelmet 样例的约定路径是：

```text
assets/models/DamagedHelmet/DamagedHelmet.gltf
```

同时需要模型引用的 buffer 和贴图。Sandbox 和相应测试会检查本地模型是否存在；缺失时跳过可选验证。仓库不在此清单中重新分发第三方模型或声明其许可。

## 保留的旧 Shader

以下 GLSL 目录仍存在于仓库，但不在当前 Vulkan Shader manifest 和构建主线中：

- `assets/shaders/pbr/`
- `assets/shaders/shadow/`
- `assets/shaders/skybox/`

它们作为旧实现参考保留，不应列为当前 Vulkan runtime 必需资产。本次文档整理不删除这些文件。

## 维护规则

- 新 Shader 在统一 manifest 中登记；新增资产同时说明使用者、路径和缺失行为。
- CPU 资产身份和解码由 Resource 管理，GPU 资源由 Renderer 管理。
- 默认 Runtime 不应无条件依赖 Git 忽略的本地模型。
- 额外大体积素材可放本地素材库，提交前确认分发权限。

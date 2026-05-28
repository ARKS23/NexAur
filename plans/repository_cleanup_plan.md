# NexAur 代码与文件清理方案

日期：2026-05-28

## 目标

这份计划只讨论“项目瘦身和废弃文件清理”，不做架构大改。目标是让后续开发更轻：

- 去掉空壳目标、废弃目录和不再使用的测试代码。
- 把仍有参考价值但不是主线的代码移到 legacy/实验区，避免继续污染主线编译。
- 降低 `file(GLOB_RECURSE)` 把所有历史文件都编进 Engine 的风险。
- 清理命名、目录拼写、大小写 include、资源归属等小问题。
- 为后续 Scene/Asset/Renderer 解耦创造更干净的地基。

原则：先从构建目标中隔离，再删除文件；先删无行为风险的空壳，再动旧渲染路径；每一步都要能构建 `NexAurEngine` 和 `Sandbox`。

## 清理进度

| PR | 状态 | 提交 | 说明 |
| --- | --- | --- | --- |
| PR 1：移除空壳 Editor 目标 | 已完成 | `a12d1f4` | 已移除 `source/Editor` 独立目标和空壳目录。 |
| PR 2：移出 Engine 内实验代码 | 已完成 | 本次提交 | 已删除无主线引用的 `source/Engine/TempTest`。 |

## 当前盘点结论

### 1. `source/Editor` 是空壳编辑器目标

状态：已在 PR 1 完成清理。

PR 1 前现状：

- 顶层 `source/CMakeLists.txt` 仍然 `add_subdirectory(Editor)`。
- `source/Editor/CMakeLists.txt` 会生成 `NexAurEditor` 可执行目标。
- 但 `source/Editor/main.cpp` 是空文件，`EditorLayer.*` 等大部分文件也是空壳或只有注释。
- 真正正在使用的编辑器代码在 `source/Engine/Editor`。
- 已验证：`cmake --build build --config Debug --target NexAurEditor` 会因为缺少 `main` 链接失败。

建议：

- 近期直接从 `source/CMakeLists.txt` 移除 `add_subdirectory(Editor)`。
- 删除整个 `source/Editor` 目录，或者移动到 `source/Deprecated/EditorShell` 后再删除。
- 保留并继续维护 `source/Engine/Editor`，这是当前主线编辑器代码。

风险：低。该目标目前本身就不可用。

### 2. `source/Engine/TempTest` 是旧三角形实验代码

状态：已在 PR 2 完成清理。

PR 2 前现状：

- `source/Engine/TempTest/triangle_test.*` 没有被 Sandbox 或 Engine 主流程引用。
- 因为 `source/Engine/CMakeLists.txt` 使用 `file(GLOB_RECURSE)`，它仍然会被编进 `NexAurEngine`。
- 这类实验代码会让 Engine 库暴露多余符号，也会干扰以后整理 RHI/RendererFactory。

建议：

- 如果没有继续参考价值：删除 `source/Engine/TempTest`。
- 如果还想留作学习样例：移动到 `source/Sandbox/Experiments/TriangleTest`，不要编进 Engine。

风险：低。没有主线引用。

### 3. 旧 `Scene` 与当前 `SceneV2` 并存

现状：

- 当前主线使用 `SceneV2`、`SceneManager`、ECS 组件。
- `source/Engine/Function/Scene/scene.*` 是旧渲染实体式 Scene，里面包含大量 `RendererFactory`、`RendererCommand`、IBL、Phong/PBR demo 逻辑。
- 旧 `scene.h` 仍被旧 `shadow_pass.*`、`render_pipeline.h`、`renderer_system.cpp` include。
- 旧 `Scene` 不应继续作为主线依赖，否则后续 Scene/Renderer 解耦会有两套世界模型。

建议：

- 第一阶段先把 `SceneV2` 明确标为主线，旧 `Scene` 标记为 legacy。
- 移除 `render_pipeline.h` 中不必要的 `#include "Function/Scene/scene.h"`。
- 确认旧 `shadow_pass.*` 不再参与主线后，连同旧 `Scene` 一起删除或移动到 legacy。

风险：中。旧代码仍被一些头文件 include，需要先断 include。

### 4. Pass 系统有旧版与 V2 两套

现状：

- 旧版：`RHI/render_pass.*`、`Passes/shadow_pass.*`、`Passes/skybox_pass.*`。
- V2：`Passes/interface_render_pass.*`、`Passes/shadow_pass_v2.*`、`Passes/skybox_pass_v2.*`。
- 当前 `RenderForwardPipeline` 实际使用 `ShadowPassV2` 和 `SkyboxPassV2`。
- `render_forward_pipeline.h` 仍 forward declare 旧 `SkyboxPass`、`ShadowPass`。
- `skybox_pass_v2.h` 的 include 写成了 `Function/Renderer/passes/...`，目录实际是 `Passes`，Windows 下能过，但跨平台会出问题。

建议：

- 删除或隔离旧 `shadow_pass.*`、`skybox_pass.*`。
- 删除旧 `RHI/render_pass.*`，前提是没有其他 active code 使用。
- 清理 `render_forward_pipeline.h` 里旧 pass 的 forward declaration。
- 修正 `skybox_pass_v2.h` 的路径大小写。
- 后续把 V2 后缀去掉：`IRenderPass` -> `RenderPass`，`ShadowPassV2` -> `ShadowPass`，`SkyboxPassV2` -> `SkyboxPass`。

风险：中。建议和旧 `Scene` 清理放在同一个或相邻 PR。

### 5. 空头文件和命名不规整文件

现状：

- `source/Engine/Function/Renderer/render_swap_context.h` 是空命名空间，没有引用。
- `source/Engine/Function/Renderer/RHI/render_pipeline.cpp` 是空实现文件。
- `source/Engine/Function/Renderer/RHI/material.h.cpp` 命名不规范，应为 `material.cpp`。
- `source/Engine/Editor/Panels/scene_hierachy_panel.*` 拼写应为 `scene_hierarchy_panel.*`。
- `engine.h` 中 `m_is_edtior_mode` 拼写应为 `m_is_editor_mode`。

建议：

- 直接删除 `render_swap_context.h`。
- 如果 `RenderPipeline` 保持纯接口，可删除空 `render_pipeline.cpp`。
- 重命名 `material.h.cpp` 为 `material.cpp`。
- 重命名 `scene_hierachy_panel.*`，同步修正 include。
- 修正 `m_is_edtior_mode`，这是安全的小范围重命名。

风险：低到中。文件重命名要注意 CMake glob、include 和 IDE 缓存。

### 6. 资源目录中有 legacy/demo 资产

现状：

- 当前 `SceneV2` 使用 `assets/textures/HDR/warm_restaurant_8k.hdr`。
- Sandbox 材质测试使用 `assets/textures/PBR/*`，并引用 `assets/models/DamagedHelmet/DamagedHelmet.gltf`。
- `.gitignore` 忽略了 `assets/models/`，因此 DamagedHelmet 模型不会出现在新克隆仓库里，Sandbox 可能依赖本地未跟踪文件。
- `assets/textures/HDR` 约 185MB，其中另外两个 HDR 当前没有代码引用。
- `assets/textures/skybox` 的 jpg skybox 当前没有主线引用，主要像旧 demo 资产。
- `assets/textures/container`、`assets/textures/wood`、`assets/shaders/phong`、`assets/shaders/floor`、`assets/shaders/lightCube` 多数只被旧 `Scene` 使用。

建议：

- 先建立 `assets/README.md` 或 `assets/asset_manifest.md`，列明哪些资产是 runtime 必需、Sandbox 必需、legacy demo。
- 旧 `Scene` 删除后，把 container/wood/phong/floor/lightCube/skybox jpg 放入 legacy 或删除。
- HDR 只保留一个默认环境图；另外两个移动到本地素材库或单独下载包。
- 处理 `assets/models/`：要么把 Sandbox 的 DamagedHelmet 改成可选加载，要么把模型纳入明确的 sample asset 包，不要一边引用一边 ignore。

风险：中。资源清理容易造成运行时加载失败，需要配合 Sandbox 验证。

### 7. 第三方库体积明显偏大

当前粗略体积：

- `external/assimp`：约 234.78MB，其中 `test` 目录约 208.72MB。
- `external/glm`：约 29.01MB，其中 `doc` 目录约 25.63MB。
- 其他库体积相对可接受。

建议：

- 短期不要手动乱删第三方源码，先通过 CMake 关闭无用构建：
  - `ASSIMP_BUILD_TESTS OFF`
  - `ASSIMP_BUILD_ASSIMP_TOOLS OFF`
  - `ASSIMP_INSTALL OFF`
  - `GLM_TEST_ENABLE OFF` 或对应选项
  - `SPDLOG_BUILD_TESTS OFF`
  - `SPDLOG_BUILD_EXAMPLE OFF`
- 如果确定采用 vendored 精简副本，再删除 `assimp/test`、`glm/doc` 等目录。
- 更长期可以改成 git submodule、FetchContent、vcpkg 或手动 vendor-minimal，但个人项目不必现在引入太重的包管理流程。

风险：中到高。第三方目录清理需要先确认 CMake 不依赖被删文件。

### 8. 文档目录拼写和计划文件归属

现状：

- `docs/architucture` 拼写错误，应为 `docs/architecture`。
- README 已引用 `docs/architucture/...`，因此不能只改目录不改链接。
- `plans/` 当前被 `.gitignore` 忽略。计划文档写得进去，但不会出现在 `git status` 中。

建议：

- 单独做文档路径 PR：`docs/architucture` -> `docs/architecture`，同步修 README。
- 如果希望 `plans` 文档入库，需要调整 `.gitignore`，例如只忽略 `plans/*.pdf` 或临时生成物。

风险：低。主要是链接修正。

## 建议执行顺序

### PR 1：移除空壳 Editor 目标（已完成）

内容：

- 从 `source/CMakeLists.txt` 移除 `add_subdirectory(Editor)`。
- 删除 `source/Editor` 目录。
- 确认 `source/Engine/Editor` 是唯一编辑器实现。

验收：

- `cmake --build build --config Debug --target NexAurEngine`
- `cmake --build build --config Debug --target Sandbox`
- 不再尝试构建空壳 `NexAurEditor`。

风险：低。

### PR 2：移出 Engine 内实验代码（已完成）

内容：

- 删除 `source/Engine/TempTest`，或移动到 `source/Sandbox/Experiments`。
- 如果保留实验代码，确保不被 `NexAurEngine` 编译。

验收：

- 全项目搜索 `TriangleTest` 无主线引用。
- Engine/Sandbox 构建通过。

风险：低。

### PR 3：清理低风险空文件和拼写

内容：

- 删除 `render_swap_context.h`。
- 删除空 `render_pipeline.cpp`，或保留到显式 source list 改造时再删。
- `material.h.cpp` -> `material.cpp`。
- `m_is_edtior_mode` -> `m_is_editor_mode`。
- 修正 `skybox_pass_v2.h` include 路径大小写。

验收：

- Engine/Sandbox 构建通过。
- `rg "edtior|hierachy|Function/Renderer/passes"` 不再命中主线拼写问题。

风险：低。

### PR 4：隔离旧 Scene 和旧 Pass

内容：

- 移除 `render_pipeline.h` 对 `scene.h` 的 include。
- 删除或 legacy 化 `scene.*`。
- 删除或 legacy 化旧 `shadow_pass.*`、`skybox_pass.*`、`RHI/render_pass.*`。
- 清理 `render_forward_pipeline.h` 中旧 Pass forward declaration。

验收：

- `rg "Function/Scene/scene.h|RenderEntity|std::shared_ptr<Scene>|ShadowPass\\b|SkyboxPass\\b"` 不再命中主线代码。
- `SceneV2` 是唯一 active scene。
- Sandbox 仍能展示当前 PBR/IBL 场景。

风险：中。

### PR 5：资源目录分级

内容：

- 新增 `assets/asset_manifest.md`。
- 标记 runtime、sandbox、legacy 资产。
- 删除或移动只服务旧 Scene 的 demo assets。
- 对 HDR 资产做保留策略：默认保留一个，其他改为外部下载或本地忽略素材。
- 处理 `assets/models/` 被 ignore 但 Sandbox 引用的问题。

验收：

- Sandbox 启动时没有缺失资产错误。
- 新克隆仓库能明确知道 sample model 是否需要额外下载。

风险：中。

### PR 6：第三方库构建瘦身

内容：

- 在 `external/CMakeLists.txt` 中为 assimp/glm/spdlog 等设置关闭 tests/examples/tools/install 的选项。
- 重新配置 CMake。
- 如果确认无影响，再考虑删除第三方库的 tests/docs 大目录。

验收：

- `cmake -S . -B build` 重新配置成功。
- Engine/Sandbox 构建通过。
- Visual Studio 解决方案中不再出现大量第三方测试/工具目标。

风险：中。

### PR 7：CMake 源文件控制

内容：

- 逐步替换 `file(GLOB_RECURSE engine_sources ...)`。
- 用模块化 `target_sources` 或每个目录一个 source list 管理主线文件。
- legacy/实验文件不再因为放在 `source/Engine` 下就自动进入 Engine。

验收：

- 新增一个实验 cpp 不会自动编进 Engine。
- Engine/Sandbox 构建通过。

风险：中。改动面较大，但长期收益高。

### PR 8：文档路径整理

内容：

- `docs/architucture` -> `docs/architecture`。
- 修正 README 链接。
- 决定 `plans/` 是否入库；如果入库，调整 `.gitignore`。

验收：

- README 链接有效。
- `rg "architucture"` 不再命中。

风险：低。

## 建议保留的内容

这些暂时不要因为“看着不够优雅”就删：

- `SceneV2`、`SceneManager`、`component.h`、`entity.*`：当前主线 scene。
- `RenderContext`：当前用于双缓冲 `RenderDataPacket`，仍在 Engine 主循环里用。
- `RenderResourceUploader`：虽然位置应迁移到 Renderer/Resources，但当前 AssetManager 仍依赖它。
- `IBLBuilder`：位置应从 Resource 移到 Renderer，但功能仍被 EnvironmentMap 加载使用。
- PBR shader、shadow shader、skybox shader：当前渲染路径仍需要。
- Sandbox：它是当前最重要的回归验证入口。

## 推荐清理清单

| 项目 | 建议动作 | 优先级 | 风险 |
| --- | --- | --- | --- |
| `source/Editor` | 已删除，并从 CMake 移除 | 已完成 | 低 |
| `source/Engine/TempTest` | 已删除 | 已完成 | 低 |
| `render_swap_context.h` | 删除 | 高 | 低 |
| `material.h.cpp` | 重命名为 `material.cpp` | 高 | 低 |
| `m_is_edtior_mode` | 重命名为 `m_is_editor_mode` | 高 | 低 |
| `scene_hierachy_panel.*` | 重命名为 `scene_hierarchy_panel.*` | 中 | 中 |
| 旧 `scene.*` | 先断 include，再删除/legacy | 高 | 中 |
| 旧 `shadow_pass.*` / `skybox_pass.*` | 随旧 Scene 清理 | 高 | 中 |
| 旧 `RHI/render_pass.*` | 确认无引用后删除 | 中 | 中 |
| `assets/textures/HDR` 未用 HDR | 移出仓库或外部下载 | 中 | 中 |
| `assets/textures/skybox` | 若旧 Scene 删除则 legacy/delete | 中 | 中 |
| `assets/models` ignore 与 Sandbox 引用冲突 | 明确 sample asset 策略 | 高 | 中 |
| `external/assimp/test` | 先关闭构建，再考虑删除 | 中 | 中高 |
| `external/glm/doc` | 可考虑 vendor 瘦身 | 低 | 中 |
| `docs/architucture` | 重命名并修 README | 低 | 低 |
| `plans/` 被 ignore | 决定是否纳入版本管理 | 中 | 低 |

## 每个清理 PR 的固定检查

建议每次清理后至少跑：

```powershell
cmake --build build --config Debug --target NexAurEngine
cmake --build build --config Debug --target Sandbox
```

如果改了 CMake 或 third-party 选项，再跑：

```powershell
cmake -S . -B build
```

如果改了资源目录，再启动 Sandbox 做一次运行验证，重点看：

- 默认环境贴图是否能加载。
- PBR 材质测试是否正常。
- DamagedHelmet 是否缺失。
- Editor viewport 是否仍能显示 framebuffer。
- picking 和 properties 面板是否仍可用。

## 最小推荐起手

如果只想先做一轮最稳的清理，我建议顺序是：

1. 移除 `source/Editor` 空壳目标。（已完成）
2. 删除或移走 `source/Engine/TempTest`。（已完成）
3. 删除 `render_swap_context.h`。
4. 重命名 `material.h.cpp`。
5. 修正 `m_is_edtior_mode` 和 `skybox_pass_v2.h` 的 include 大小写。

这五步基本不会改变引擎行为，但能立刻减少噪音。之后再动旧 `Scene` 和旧 Pass，那一步才是真正清理主线架构分叉。

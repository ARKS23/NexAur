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
| PR 2：移出 Engine 内实验代码 | 已完成 | `45573a5` | 已删除无主线引用的 `source/Engine/TempTest`。 |
| PR 3：清理低风险空文件和拼写 | 已完成 | `3f7746a` | 已删除空文件，并修正文件名、变量名和 include 大小写。 |
| PR 4：隔离旧 Scene 和旧 Pass | 已完成 | `801742c` | 已删除旧 `Scene`、旧 Pass，并清理残留 include/声明。 |
| PR 5：资源目录分级 | 已完成 | `a07565a` | 已新增资产清单，删除旧 demo 资产，并把本地 sample model 改为可选加载。 |
| PR 6：第三方库构建瘦身 | 已完成 | `73743fc` | 已统一关闭第三方测试/样例/安装目标，并把 GLM 改为 header-only 目标。 |
| PR 7：CMake 源文件控制 | 已完成 | `59c18aa` | 已把 Engine 源文件从递归 glob 改为按模块维护的显式清单。 |
| PR 8：文档路径整理 | 已完成 | 本次提交 | 已修正 `docs/architecture` 路径和 README 链接，并让 `plans/*.md` 入库。 |

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

状态：已在 PR 4 完成清理，当前主线只保留 `SceneV2`。

PR 4 前现状：

- 当前主线使用 `SceneV2`、`SceneManager`、ECS 组件。
- `source/Engine/Function/Scene/scene.*` 是旧渲染实体式 Scene，里面包含大量 `RendererFactory`、`RendererCommand`、IBL、Phong/PBR demo 逻辑。
- 旧 `scene.h` 仍被旧 `shadow_pass.*`、`render_pipeline.h`、`renderer_system.cpp` include。
- 旧 `Scene` 不应继续作为主线依赖，否则后续 Scene/Renderer 解耦会有两套世界模型。

建议：

- 已把 `SceneV2` 作为当前唯一 active scene。
- 已移除 `render_pipeline.h` 中不必要的 `#include "Function/Scene/scene.h"`。
- 已删除旧 `scene.*`，旧 demo 逻辑不再进入 Engine 编译图。

风险：已处理。本次清理只删除无主线引用代码，未触碰资源目录。

### 4. Pass 系统有旧版与 V2 两套

状态：已在 PR 4 完成清理，当前主线只保留 `IRenderPass`、`ShadowPassV2`、`SkyboxPassV2`。

PR 4 前现状：

- 旧版：`RHI/render_pass.*`、`Passes/shadow_pass.*`、`Passes/skybox_pass.*`。
- V2：`Passes/interface_render_pass.*`、`Passes/shadow_pass_v2.*`、`Passes/skybox_pass_v2.*`。
- 当前 `RenderForwardPipeline` 实际使用 `ShadowPassV2` 和 `SkyboxPassV2`。
- `render_forward_pipeline.h` 仍 forward declare 旧 `SkyboxPass`、`ShadowPass`。
- `skybox_pass_v2.h` 的 include 写成了 `Function/Renderer/passes/...`，目录实际是 `Passes`，Windows 下能过，但跨平台会出问题。

建议：

- 已删除旧 `shadow_pass.*`、`skybox_pass.*`。
- 已删除旧 `RHI/render_pass.*`。
- 已清理 `render_forward_pipeline.h` 里旧 pass 的 forward declaration。
- 修正 `skybox_pass_v2.h` 的路径大小写。
- 后续把 V2 后缀去掉：`IRenderPass` -> `RenderPass`，`ShadowPassV2` -> `ShadowPass`，`SkyboxPassV2` -> `SkyboxPass`。

风险：已处理。V2 后缀统一命名留到后续 renderer 内部边界整理时再做。

### 5. 空头文件和命名不规整文件

状态：已在 PR 3 完成清理。

PR 3 前现状：

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

状态：已在 PR 5 完成第一轮分级，当前仓库只保留主线和 Sandbox 必需资产。

PR 5 前现状：

- 当前 `SceneV2` 使用 `assets/textures/HDR/warm_restaurant_8k.hdr`。
- Sandbox 材质测试使用 `assets/textures/PBR/*`，并引用 `assets/models/DamagedHelmet/DamagedHelmet.gltf`。
- `.gitignore` 忽略了 `assets/models/`，因此 DamagedHelmet 模型不会出现在新克隆仓库里，Sandbox 可能依赖本地未跟踪文件。
- `assets/textures/HDR` 约 185MB，其中另外两个 HDR 当前没有代码引用。
- `assets/textures/skybox` 的 jpg skybox 当前没有主线引用，主要像旧 demo 资产。
- `assets/textures/container`、`assets/textures/wood`、`assets/shaders/phong`、`assets/shaders/floor`、`assets/shaders/lightCube` 多数只被旧 `Scene` 使用。

建议：

- 已建立 `assets/asset_manifest.md`，明确 runtime、Sandbox、可选 sample model 和 legacy/demo 资产边界。
- 已删除只服务旧 `Scene` 的 `container`、`wood`、jpg skybox、Phong/Floor/LightCube/Container shader。
- HDR 当前只保留默认环境图 `warm_restaurant_8k.hdr`；另外两个未引用 HDR 已移出仓库。
- `assets/models/` 继续保持本地忽略，但 Sandbox 会在 `DamagedHelmet` 缺失时跳过加载，并在资产清单里说明可选样例策略。

风险：已降低。后续如果把 sample model 纳入仓库，建议单独做小型样例资产包，并明确许可证。

### 7. 第三方库体积明显偏大

状态：已在 PR 6 完成构建目标瘦身；第三方源码目录暂不物理删除。

PR 6 前粗略体积：

- `external/assimp`：约 234.78MB，其中 `test` 目录约 208.72MB。
- `external/glm`：约 29.01MB，其中 `doc` 目录约 25.63MB。
- 其他库体积相对可接受。

建议：

- 已在 `external/CMakeLists.txt` 里集中设置第三方库选项，关闭 tests/examples/bench/tools/install。
- 已关闭 `ASSIMP_BUILD_TESTS`、`ASSIMP_BUILD_ASSIMP_TOOLS`、`ASSIMP_BUILD_SAMPLES`、`ASSIMP_INSTALL`、`ASSIMP_INSTALL_PDB`，并关闭当前不需要的 exporter 代码。
- 已关闭 `SPDLOG_BUILD_EXAMPLE*`、`SPDLOG_BUILD_TESTS*`、`SPDLOG_BUILD_BENCH`、`SPDLOG_INSTALL`。
- 已关闭 `GLM_BUILD_TESTS`、`GLM_BUILD_INSTALL`，并将 GLM 改为 header-only interface 目标，避免生成独立 `glm` 库目标。
- 已确认重新配置后解决方案不再出现 `INSTALL`、`uninstall`、assimp `unit` 和独立 `glm` 库目标。
- Assimp 仍会由 vendored CMake 生成一个 MSVC 拷贝辅助目标 `UpdateAssimpLibsDebugSymbolsAndDLLs`，本 PR 不修改第三方源码，只在总控 CMake 中将其排除出默认构建。
- 暂不删除 `assimp/test`、`glm/doc` 等 vendored 源码目录；如果后续决定采用 vendor-minimal，再单独做物理瘦身 PR。
- 更长期可以改成 git submodule、FetchContent、vcpkg 或手动 vendor-minimal，但个人项目不必现在引入太重的包管理流程。

风险：已降低。PR 6 只调整 CMake 配置，不删除第三方源码目录。

### 8. 文档目录拼写和计划文件归属

状态：已在 PR 8 完成整理。

PR 8 前现状：

- 原架构文档目录拼写错误，应统一为 `docs/architecture`。
- README 已引用旧目录路径，因此不能只改目录不改链接。
- `plans/` 当前被 `.gitignore` 忽略。计划文档写得进去，但不会出现在 `git status` 中。

建议：

- 已将原架构文档目录重命名为 `docs/architecture`，并同步修正 README。
- 已调整 `.gitignore`：`plans/*.md` 作为可追踪设计文档入库，`plans/*.pdf` 继续作为导出生成物忽略。
- 已清理 `docs/architecture/Architecture.md` 中指向不存在图片的占位链接。

风险：已处理。后续新增计划文档默认使用 Markdown 入库，导出物单独忽略。

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

### PR 3：清理低风险空文件和拼写（已完成）

内容：

- 删除 `render_swap_context.h`。
- 删除空 `render_pipeline.cpp`。
- `material.h.cpp` -> `material.cpp`。
- `m_is_edtior_mode` -> `m_is_editor_mode`。
- 修正 `skybox_pass_v2.h` include 路径大小写。
- `scene_hierachy_panel.*` -> `scene_hierarchy_panel.*`。

验收：

- Engine/Sandbox 构建通过。
- `rg "edtior|hierachy|Function/Renderer/passes" source` 不再命中主线拼写问题。

风险：低。

### PR 4：隔离旧 Scene 和旧 Pass（已完成）

内容：

- 移除 `render_pipeline.h` 对 `scene.h` 的 include。
- 删除 `scene.*`。
- 删除旧 `shadow_pass.*`、`skybox_pass.*`、`RHI/render_pass.*`。
- 清理 `render_forward_pipeline.h` 中旧 Pass forward declaration。

验收：

- `rg "Function/Scene/scene.h|RenderEntity|std::shared_ptr<Scene>|ShadowPass\\b|SkyboxPass\\b" source` 不再命中主线代码。
- `SceneV2` 是唯一 active scene。
- Sandbox 仍能展示当前 PBR/IBL 场景。

风险：中。

### PR 5：资源目录分级（已完成）

内容：

- 新增 `assets/asset_manifest.md`。
- 标记 runtime、Sandbox、legacy 资产。
- 删除只服务旧 Scene 的 demo assets。
- 对 HDR 资产做保留策略：默认保留 `warm_restaurant_8k.hdr`，删除未引用 HDR。
- 处理 `assets/models/` 被 ignore 但 Sandbox 引用的问题：改为本地可选加载。

验收：

- Sandbox 启动时不会因为缺失 `DamagedHelmet` 报错。
- 新克隆仓库能通过 `assets/asset_manifest.md` 明确知道 sample model 是本地可选资产。

风险：中。

### PR 6：第三方库构建瘦身（已完成）

内容：

- 在 `external/CMakeLists.txt` 中为 assimp/glm/spdlog/glfw/entt 等设置关闭 tests/examples/tools/install 的选项。
- 重新配置 CMake。
- GLM 使用 header-only 目标，不再生成独立库。
- 不在本 PR 删除第三方库的 tests/docs 大目录，避免误删 vendored 源码。

验收：

- `cmake -S . -B build` 重新配置成功。
- Engine/Sandbox 构建通过。
- Visual Studio 解决方案中不再出现 `INSTALL`、`uninstall`、assimp `unit` 和独立 `glm` 库目标。
- `UpdateAssimpLibsDebugSymbolsAndDLLs` 若仍由 assimp 生成，应被排除出默认构建。

风险：已降低。

### PR 7：CMake 源文件控制（已完成）

内容：

- 已替换 `source/Engine/CMakeLists.txt` 中的 `file(GLOB_RECURSE engine_sources ...)`。
- 已按 Root/Core/Editor/Function/Renderer 模块显式维护 Engine 文件清单，并用 `source_group(TREE ...)` 保持 IDE 分组。
- legacy/实验文件不再因为放在 `source/Engine` 下就自动进入 Engine。

验收：

- 已临时新增带 `#error` 的 `source/Engine/pr7_cmake_guard.cpp`，重新配置并构建 Engine 通过，确认实验 cpp 不会自动编进 Engine；测试后已删除临时文件。
- Engine/Sandbox 构建通过。

风险：已降低。后续新增 Engine 主线文件时需要同步维护 `source/Engine/CMakeLists.txt`。

### PR 8：文档路径整理（已完成）

内容：

- 原架构文档目录 -> `docs/architecture`。
- 修正 README 链接。
- 调整 `.gitignore`：`plans/*.md` 入库，`plans/*.pdf` 忽略。

验收：

- README 链接有效。
- 旧目录拼写不再命中。
- `plans/architecture_decoupling_plan.md`、`plans/open_source_engine_architecture_study.md`、`plans/repository_cleanup_plan.md` 均纳入版本管理。

风险：已降低。

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
| `render_swap_context.h` | 已删除 | 已完成 | 低 |
| `render_pipeline.cpp` | 已删除 | 已完成 | 低 |
| `material.h.cpp` | 已重命名为 `material.cpp` | 已完成 | 低 |
| `m_is_edtior_mode` | 已重命名为 `m_is_editor_mode` | 已完成 | 低 |
| `scene_hierachy_panel.*` | 已重命名为 `scene_hierarchy_panel.*` | 已完成 | 中 |
| 旧 `scene.*` | 已删除 | 已完成 | 中 |
| 旧 `shadow_pass.*` / `skybox_pass.*` | 已删除 | 已完成 | 中 |
| 旧 `RHI/render_pass.*` | 已删除 | 已完成 | 中 |
| `assets/textures/HDR` 未用 HDR | 已删除未引用 HDR，仅保留默认环境图 | 已完成 | 中 |
| `assets/textures/skybox` | 已删除旧 jpg skybox | 已完成 | 中 |
| `assets/models` ignore 与 Sandbox 引用冲突 | 已改为可选加载，并写入资产清单 | 已完成 | 中 |
| `external/assimp/test` | 已关闭构建，源码目录暂保留 | 已完成 | 中 |
| `external/glm/doc` | 已关闭安装/测试，物理瘦身留到后续 | 低 | 中 |
| Engine `file(GLOB_RECURSE)` | 已替换为按模块维护的显式源文件清单 | 已完成 | 中 |
| 原架构文档目录拼写 | 已重命名为 `docs/architecture` 并修 README | 已完成 | 低 |
| `plans/` 被 ignore | 已改为 Markdown 入库、PDF 忽略 | 已完成 | 低 |

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
3. 删除 `render_swap_context.h`。（已完成）
4. 重命名 `material.h.cpp`。（已完成）
5. 修正 `m_is_edtior_mode` 和 `skybox_pass_v2.h` 的 include 大小写。（已完成）

这五步基本不会改变引擎行为，但能立刻减少噪音。之后再动旧 `Scene` 和旧 Pass，那一步才是真正清理主线架构分叉。

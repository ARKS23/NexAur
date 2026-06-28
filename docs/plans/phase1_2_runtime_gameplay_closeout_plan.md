# NexAur Phase1.2 Runtime / Gameplay Foundation 收口计划

日期：2026-06-28

## 1. 文档目标

Phase1.2 是 Runtime / Scene / Gameplay foundation 主线完成后的收口阶段。

`docs/plans/runtime_scene_gameplay_foundation_plan.md` 已经完成 PR-G1 到 PR-G10 的主要功能路径：

- Scene serialization。
- Editor save / load。
- Runtime scene tick。
- GameModule。
- InputActionSystem。
- Gameplay components and basic systems。
- Simple collider / trigger。
- Runtime camera、HUD 和 GameState。
- PrefabFactory / spawn templates。
- Audio v1。

下一步不建议立刻继续叠新的 gameplay 功能，而是先把已经完成的 foundation 固化成一条清爽、可验证、边界明确的主线。

一句话目标：

```text
让 Runtime / Gameplay 这条线易读、易测，并且不容易在后续 PR 中重新混淆模块边界。
```

本文档用于跟踪后续工作：

- 文档收口。
- Smoke 测试整理。
- 命名整理。
- 职责边界清理。
- 后续每个收口 PR 的状态同步。

## 2. 当前状态

当前 foundation 已经具备功能可用性：

- `RuntimeModule` 拥有 active scene tick 和 render data extraction。
- `GameModule` 拥有 gameplay update、runtime HUD 和 game state flow。
- `InputActionModule` 向玩法层暴露语义输入。
- `Function/Physics` 包含简单 collision query 和 trigger overlap。
- `AudioModule` 暴露 `AudioService`，miniaudio 被隐藏在 backend 实现层。
- `PrefabFactory` 可以创建 demo 需要的 player、enemy、collectible 和 projectile。
- Sandbox 已经提供 runtime / gameplay 相关 smoke 入口。

当前主要问题不是功能缺失，而是工程卫生：

- `runtime_scene_gameplay_foundation_plan.md` 仍有一部分内容像执行计划，而不是完成后的 baseline 记录。
- `Sandbox.cpp` 已经堆积了较多 smoke 实现和测试 helper。
- 部分命名是合理的 v1 命名，但长期含义需要写清楚。
- Runtime、Game、Scene、Physics、Audio、Editor、Sandbox 的边界需要文档化并做轻量审计。

## 3. 设计原则

- 这一阶段要朴素、克制、可回滚。
- 不增加新的 gameplay 功能。
- 如果 CTest 加现有 Sandbox smoke 足够，就不引入完整测试框架。
- 不做大规模 rename，除非现有名字真的会误导架构理解。
- 优先补文档和测试，让后续 PR 更容易 review。
- 每个 PR 都必须保持构建和 Sandbox 可运行。
- 继续保持当前模块方向：

```text
Scene     -> 数据和序列化
Runtime   -> scene tick 和 render extraction
Game      -> gameplay systems、game state 和 runtime HUD
Input     -> 语义输入
Physics   -> collision query / trigger overlap
Audio     -> audio service 和 backend
Editor    -> authoring 和 debug tools
Sandbox   -> demo 和 smoke 入口
```

## 4. 职责边界

### 4.1 Scene

Scene 应该负责：

- Entity。
- Component。
- Scene serialization。
- Scene 内部查询和遍历 helper。

Scene 不应该负责：

- Gameplay 规则。
- Editor UI。
- Renderer backend 对象。
- Audio backend 对象。
- 直接读取输入。

期望依赖方向：

```text
Game / Runtime / Editor / Renderer frontend
  -> Scene
```

Scene 不应该反向依赖这些更高层系统。

### 4.2 RuntimeModule

RuntimeModule 应该负责：

- Active scene tick。
- 如果后续需要，负责 scene transition application。
- 把 active scene 提取到 `RenderContext`。

RuntimeModule 不应该负责：

- Player movement 规则。
- Score / win / lose 逻辑。
- Runtime HUD 绘制。
- Audio 播放策略。
- Editor camera 行为。

RuntimeModule 是运行时 scene 协调层，不是玩法逻辑层。

### 4.3 GameModule

GameModule 应该负责：

- Gameplay systems。
- GameState update。
- Runtime camera controller。
- Runtime HUD v1。
- 调用轻量 gameplay-adjacent 系统，例如 trigger overlap。

GameModule 可以依赖：

- `SceneService`。
- `InputActionService`。
- `AudioService`。
- 简单 Physics systems。

GameModule 不应该依赖：

- Renderer backend class。
- Editor panel。
- Vulkan class。
- 把 global context 当成 gameplay 数据来源。

当前可接受的例外：

- `RuntimeHud` v1 可以使用 ImGui 绘制，但 ImGui 依赖必须限制在 HUD 实现内部，不要泄漏到 gameplay component 或 gameplay system。

### 4.4 Physics

`Function/Physics` 当前表示简单 gameplay collision 支持，不表示完整物理引擎。

它可以负责：

- Collider math。
- Trigger overlap query。
- Collision shape type。

它暂时不应该宣称负责：

- Rigid body simulation。
- Constraint solving。
- Broadphase acceleration。
- Fixed-step scheduling。
- Physics scene ownership。

当前模型可以保持：

```text
GameModule
  -> TriggerOverlapSystem
  -> Scene components
```

如果后续引入刚体模拟，再把它升级成真正的 `PhysicsModule`。

### 4.5 Audio

Audio 对外只应该暴露 NexAur 自己的接口。

允许对外出现：

- `AudioService`。
- `AudioModule`。
- `AudioSystem`。

只允许出现在 backend 实现层：

- miniaudio include。
- miniaudio implementation。

建议审计：

```powershell
rg "miniaudio" source/Engine
```

长期期望是 miniaudio 只出现在 audio backend 实现文件中。

### 4.6 Sandbox

Sandbox 可以负责：

- 手写 demo scene。
- Smoke test 命令行入口。
- 临时验证 helper。

Sandbox 不应该变成：

- 隐藏的 gameplay framework。
- 长期 gameplay system 调度中心。
- 唯一保存 game flow 规则的地方。

Phase1.2 完成后，Sandbox 应该更像一个薄 application 和 smoke launcher。

## 5. PR-G11：文档收口

执行状态：已完成。

目标：

- 把 Runtime / Scene / Gameplay foundation 计划从执行计划整理成完成后的 baseline 记录。
- 让本文档成为 Phase1.2 后续 cleanup 的主动跟踪文档。

任务：

1. 更新 `runtime_scene_gameplay_foundation_plan.md`。
   - 标记 PR-G1 到 PR-G10 主线已完成。
   - 移除类似“下一步执行 PR-G10”的过期文字。
   - 增加到本文档的链接。
2. 增加当前架构快照。
   - Runtime 拥有 scene tick 和 extraction。
   - Game 拥有 gameplay 和 HUD。
   - Physics 是 simple trigger support。
   - Audio 是 service-based，并且 backend-hidden。
3. 增加 smoke 测试摘要。
   - 列出每个 smoke command。
   - 说明每个 smoke 覆盖的内容。
4. 增加下一阶段建议。
   - Phase1.2 cleanup。
   - Playable demo integration。
   - demo loop 稳定后再推进 renderer polish。

验收：

- 旧 foundation plan 不再表现得像 PR-G10 仍未开始。
- 旧 foundation plan 指向本文档作为 cleanup 跟踪入口。
- 新读者不用翻所有 commit，也能理解 G1-G10 的最终结果。

建议验证：

```powershell
rg "PR-G10|下一步|计划中" docs/plans/runtime_scene_gameplay_foundation_plan.md
```

该命令不要求零结果，但每一个结果都应该是有意保留的，而不是过期内容。

## 6. PR-G12：Smoke Harness 整理

执行状态：计划中。

目标：

- 保留现有 smoke 覆盖。
- 避免 `Sandbox.cpp` 继续变成测试大杂烩。
- 让 smoke 更容易本地运行，也更容易接入 CI。

当前 smoke 入口包括：

```text
--scene-serializer-smoke
--audio-smoke
--input-action-smoke
--gameplay-systems-smoke
--physics-trigger-smoke
--runtime-camera-smoke
--runtime-game-flow-smoke
--prefab-factory-smoke
```

建议第一步：

- 增加 `--smoke-all`。
- 从 `Sandbox.cpp` 抽出通用 helper。
- 让每个 smoke 实现都保持聚焦和可读。

建议文件布局：

```text
source/Sandbox/Smoke/
  smoke_test_registry.h
  smoke_test_registry.cpp
  smoke_test_context.h
  scene_serializer_smoke.cpp
  audio_smoke.cpp
  input_action_smoke.cpp
  gameplay_systems_smoke.cpp
  physics_trigger_smoke.cpp
  runtime_camera_smoke.cpp
  runtime_game_flow_smoke.cpp
  prefab_factory_smoke.cpp
```

如果一次拆这么多感觉太重，第一版也可以用更小布局：

```text
source/Sandbox/
  smoke_tests.h
  smoke_tests.cpp
```

这个较小布局也可以接受。关键是让 `Sandbox.cpp` 变成 dispatcher，而不是长测试文件。

建议 CTest 接入：

```cmake
enable_testing()

add_test(NAME NexAur.SceneSerializerSmoke COMMAND Sandbox --scene-serializer-smoke)
add_test(NAME NexAur.AudioSmoke COMMAND Sandbox --audio-smoke)
add_test(NAME NexAur.InputActionSmoke COMMAND Sandbox --input-action-smoke)
add_test(NAME NexAur.GameplaySystemsSmoke COMMAND Sandbox --gameplay-systems-smoke)
add_test(NAME NexAur.PhysicsTriggerSmoke COMMAND Sandbox --physics-trigger-smoke)
add_test(NAME NexAur.RuntimeCameraSmoke COMMAND Sandbox --runtime-camera-smoke)
add_test(NAME NexAur.RuntimeGameFlowSmoke COMMAND Sandbox --runtime-game-flow-smoke)
add_test(NAME NexAur.PrefabFactorySmoke COMMAND Sandbox --prefab-factory-smoke)
```

完成后推荐命令：

```powershell
cmake --build --preset msvc-vcpkg-debug
ctest --test-dir build/msvc-vcpkg -C Debug --output-on-failure
```

验收：

- 现有 smoke 测试继续通过。
- `Sandbox.exe --smoke-all` 通过。
- CTest 可以运行 smoke suite。
- `Sandbox.cpp` 变小，主要负责命令分发和 demo 启动。
- Smoke helper 不再在多个测试中重复。

## 7. PR-G13：命名和职责边界清理

执行状态：计划中。

目标：

- 清理会影响架构理解的命名。
- 审计 G1-G10 后的模块边界。
- 避免无收益的大规模机械 rename。

建议检查：

```powershell
rg "global_context" source/Engine/Function/Game source/Engine/Function/Physics source/Engine/Function/Audio
rg "RendererSystem|Vulkan|Editor" source/Engine/Function/Game source/Engine/Function/Scene
rg "ImGui" source/Engine/Function/Scene source/Engine/Function/Physics source/Engine/Function/Audio
rg "miniaudio" source/Engine
```

期望方向：

- Game / Physics / Audio 不从 global context 获取 runtime 状态。
- Scene 不知道 Editor、ImGui 或 renderer backend type。
- miniaudio 只留在 Audio backend 实现层。
- Runtime HUD 的 ImGui 依赖限制在 HUD 绘制代码中。

命名候选：

1. `SceneV2`。
   - 如果旧 Scene 路径已经不存在，可以考虑改回 `Scene`。
   - 这是较大的 rename，如果要做，建议单独开一个聚焦 PR。
   - 如果现在不改名，就在文档里明确 `SceneV2` 是当前正式 scene type。
2. `gameplay_component.h`。
   - 如果继续膨胀，可以把 runtime camera 数据拆到 `runtime_camera_component.h`。
   - 小型 gameplay component 可以暂时放一起，等真的形成摩擦再拆。
3. `Function/Physics`。
   - 文件夹名可以保持。
   - 但文档里要说明 v1 只覆盖 trigger / collision-query。
   - 不要在没有 simulation owner 前引入 `PhysicsModule`。
4. `PrefabFactory`。
   - 名字可以保持。
   - 但要说明它是 spawn-template helper，不是 serialized prefab asset system。
5. `RuntimeHud`。
   - v1 可以保持该名字。
   - 但要说明它是 ImGui-backed temporary runtime HUD，不是最终 runtime UI framework。

验收：

- 不引入新的功能行为。
- Include 和依赖关系更清楚。
- 每个 rename 或文件拆分都有明确理由。
- 相关边界在代码附近或本文档中写清楚。
- 现有 smoke suite 通过。

## 8. 推荐执行顺序

推荐顺序：

```text
PR-G11 文档收口
PR-G12 Smoke harness 整理
PR-G13 命名和职责边界清理
```

原因：

- 文档收口先给这个阶段一个稳定目标。
- Smoke 整理会让后续 PR 的验证更轻松。
- 命名和边界清理最好在 smoke 更容易执行之后做。

## 9. Phase1.2 完成标准

满足以下条件后，可以认为 Phase1.2 完成：

- 旧 Runtime / Scene / Gameplay foundation plan 明确记录 PR-G1 到 PR-G10 已完成。
- 本文档成为后续 cleanup 的主动跟踪文档。
- Smoke 测试被分组、记录，并可以统一运行。
- CTest 可以运行 smoke suite。
- `Sandbox.cpp` 不再是 smoke 实现的主要承载文件。
- Runtime / Game / Scene / Physics / Audio 的职责边界完成文档化和轻量审计。
- 后端实现细节不泄漏到 gameplay public API。
- 构建和 smoke 验证通过。

推荐最终验证：

```powershell
cmake --preset msvc-vcpkg
cmake --build --preset msvc-vcpkg-debug
ctest --test-dir build/msvc-vcpkg -C Debug --output-on-failure
bin\msvc-vcpkg\Debug\Sandbox.exe --smoke-all
```

## 10. 暂缓工作

Phase1.2 不做：

- Playable demo 内容制作。
- Renderer 画面效果开发。
- 完整 PhysicsModule。
- Script system。
- Runtime UI framework。
- Asset browser。
- Serialized prefab asset pipeline。
- AudioSourceComponent / 3D audio。
- Input rebinding UI。

这些都是合理的后续任务，但不应该挤进这个收口阶段。

## 11. 执行日志

Phase1.2 后续每个 PR 落地时，都应该更新本节。

| PR | 状态 | 摘要 |
| --- | --- | --- |
| PR-G11 文档收口 | 已完成 | 更新已完成的 foundation plan，补充 baseline、架构快照、smoke 清单，并让它指向 Phase1.2。 |
| PR-G12 Smoke harness 整理 | 计划中 | 抽出 smoke helper，增加 `--smoke-all`，注册 CTest smoke cases。 |
| PR-G13 命名和职责边界清理 | 计划中 | 审计依赖关系，并做小范围命名 / header 清理。 |

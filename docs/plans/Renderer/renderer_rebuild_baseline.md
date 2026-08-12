# NexAur Renderer Rebuild Baseline

日期：2026-08-11

状态：RR-00 Baseline Established

基线起点：`4728370 docs(renderer): add ray tracing development plan`

## 1. 文档目的

本文档固定 Renderer Rebuild 开始前的必要 build、focused smoke、GPU startup、Vulkan validation 和 Editor 手工验证契约。

RR-00 不修改渲染算法、视觉参数、pass 顺序或资源同步语义。后续 RR 工作包必须以本基线判断改动是否属于预期行为变化。

本基线记录三类结果：

```text
Automated
  Debug build
  CPU focused smoke

Observed on current development machine
  Vulkan device / swapchain startup
  validation layer and debug messenger
  captured validation output

Manual contract
  Editor visual feature matrix
  picking / resize / Reflection Probe workflows
```

没有截图或人工观察证据的视觉项不得仅凭进程未崩溃标记为通过。

## 2. 固定环境

RR-00 实测环境：

| 项目 | 值 |
|---|---|
| OS | Windows 11 10.0.22631 |
| Generator | Visual Studio 17 2022, x64 |
| Configuration | Debug |
| Build preset | `msvc-vcpkg-debug` |
| Vulkan instance | 1.4.335 |
| Selected GPU | NVIDIA GeForce RTX 4080 Laptop GPU |
| Selected device API | 1.4.325 |
| NVIDIA driver | 591.86 |
| Secondary GPU | Intel RaptorLake-S Mobile Graphics Controller, Vulkan 1.3.295 |
| Validation layer | `VK_LAYER_KHRONOS_validation` 1.4.335 |
| Swapchain baseline | 1920x1080, 3 images |

设备列表不是 Renderer 的硬编码要求。后续 device capability 工作必须继续保留 Vulkan 1.3 baseline 和多 GPU 环境下的正确选择行为。

## 3. 必要自动验证

### 3.1 Debug build

命令：

```powershell
cmake --build --preset msvc-vcpkg-debug --target Sandbox -- /m
```

RR-00 结果：Pass。

覆盖：

- `NexAurEngine.dll`。
- `Sandbox.exe`。
- Vulkan renderer HLSL -> SPIR-V 编译。
- 新增 graph state planner 被 Engine 和 Sandbox 同时编译。

### 3.2 RenderGraph state planner focused smoke

命令：

```powershell
ctest --test-dir build/msvc-vcpkg -C Debug `
  -R "^NexAur.RenderGraphStatePlannerSmoke$" `
  --output-on-failure
```

也可以直接运行：

```powershell
bin/msvc-vcpkg/Debug/Sandbox.exe --render-graph-state-planner-smoke
```

RR-00 结果：Pass。

该测试不启动 Engine，不创建 window、Vulkan instance、device 或 command buffer。它只固定 graph planner 的纯 CPU 输入输出。

覆盖：

- Color attachment usage state。
- Depth/stencil attachment usage state。
- Fragment shader read usage state。
- Transfer source usage state。
- Present usage state。
- 未识别 layout 的保守 access / stage。
- Color attachment -> shader read transition。
- 当前 layout-only planner 对相同 layout 跳过 barrier 的行为。

### 3.3 Render settings focused smoke

命令：

```powershell
ctest --test-dir build/msvc-vcpkg -C Debug `
  -R "^NexAur.RenderSettingsSmoke$" `
  --output-on-failure
```

RR-00 结果：Pass。

该测试固定 RenderSettings、frame extraction、shadow budget 和 Reflection Probe 数据契约，不替代 GPU 视觉验证。

### 3.4 默认验证范围

每个 Renderer PR 默认只运行：

1. `Sandbox` Debug target build。
2. 与修改范围直接相关的 focused smoke。
3. `git diff --check`。
4. 从第 7 节选择必要的 GPU / Editor 手工项。

不默认运行 `SmokeAll` 或全量 CTest。

## 4. RenderGraph Baseline

### 4.1 当前 usage state

| Usage | Layout | Access | Stage |
|---|---|---|---|
| ColorAttachment | `COLOR_ATTACHMENT_OPTIMAL` | `COLOR_ATTACHMENT_WRITE` | `COLOR_ATTACHMENT_OUTPUT` |
| DepthStencilAttachment | `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` | depth read + write | early + late fragment tests |
| ShaderRead | `SHADER_READ_ONLY_OPTIMAL` | `SHADER_READ` | fragment shader |
| TransferSource | `TRANSFER_SRC_OPTIMAL` | `TRANSFER_READ` | transfer |
| Present | `PRESENT_SRC_KHR` | none | bottom of pipe |

### 4.2 当前 transition 规则

```text
current VkImageLayout
  -> source state inferred from layout
requested VulkanGraphImageUsage
  -> destination state inferred from usage

source layout != destination layout
  -> emit VkImageMemoryBarrier

source layout == destination layout
  -> skip barrier
```

`VulkanGraphStatePlanner` 是 header-only 纯逻辑入口；`VulkanGraphExecutor` 消费 planner 结果并录制原有 `vkCmdPipelineBarrier`。

### 4.3 已知正确性缺口

当前 `readImage()` 和 `writeImage()` 最终写入同一种 `VulkanGraphImageAccess`，planner 不知道真实访问类型。相同 layout 时会直接跳过 barrier，因此不能正确表达：

- Write -> Read，layout 不变。
- Write -> Write，layout 不变。
- Read -> Write，layout 不变。
- mip / layer / aspect 独立访问。
- 精确 producer / consumer stage。

RR-00 的 same-layout focused assertion 是对当前行为的记录，不表示该行为正确。RR-01 必须有意识地更新该 assertion，并增加 access / subresource hazard tests。

## 5. 当前视觉参数基线

### 5.1 Final Lit

| 设置 | 默认值 |
|---|---|
| Lighting preset | Outdoor |
| Tone mapping | ACES |
| Exposure | 0.85 |
| AO | Enabled |
| SSR | Disabled |
| Bloom | Enabled |
| Color grading | Enabled |
| Anti-aliasing | SMAA |
| Effect debug view | Final Lit |
| IBL debug mode | Final Lit |

Final Lit baseline 必须显示完整 forward lighting、shadow、IBL、AO、bloom、tone mapping、color grading 和 SMAA 组合结果，不得残留单项 debug output。

### 5.2 SSR

默认状态：Disabled。

启用参数：

| 参数 | 默认值 |
|---|---|
| Max distance | 18.0 |
| Max steps | 32 |
| Thickness | 0.18 |
| Stride | 1.0 |
| Roughness fade | 0.65 |
| Edge fade | 0.12 |
| Intensity | 1.0 |

需要固定的 debug views：

- SSR Hit Mask。
- SSR Ray Steps。
- SSR Raw Reflection。
- SSR Surface Mask。

SSR debug isolation 不得被 local Reflection Probe 或 global IBL 结果污染；Final Lit 中 SSR miss 继续回退 Probe / IBL。

### 5.3 AO

| 参数 | 默认值 |
|---|---|
| Enabled | true |
| Radius | 1.2 |
| Intensity | 0.6 |
| Bias | 0.025 |
| Power | 1.2 |
| Blur | Enabled |
| Resolution | Half resolution |

需要固定 `AO Raw` 和 `AO Blurred`。Raw 应保留采样噪声，Blurred 应保持几何边界且无全屏明暗跳变。

### 5.4 Bloom

| 参数 | 默认值 |
|---|---|
| Enabled | true |
| Intensity | 0.05 |
| Scatter | 0.7 |
| Radius | 1.0 |

需要固定 Bloom Composite、Downsample Mip 和 Upsample Mip。切换 mip 不得引用越界 image view 或改变 viewport extent。

### 5.5 SMAA

| 参数 | 默认值 |
|---|---|
| Mode | SMAA |
| Edge threshold | 0.08 |
| Contrast factor | 2.0 |
| Max search steps | 8 |
| Blend strength | 0.85 |

需要固定 SMAA Edge Mask、Blend Weight 和 Output。Final Lit 默认消费 SMAA output。

### 5.6 Shadow

Directional shadow：

| 参数 | 默认值 |
|---|---|
| Enabled | true |
| Filter | PCF 3x3 |
| Strength | 0.7 |
| Distance | 35.0 |
| Resolution | 2048 |
| Cascades | 4 |
| Split lambda | 0.65 |
| Stabilize | true |

- Point shadow：Enabled，budget 1，resolution 512。
- Rect shadow：Enabled，budget 1，resolution 1024，soft shadow enabled。

需要固定：

- Directional Shadow Map / Cascades。
- Point Shadow Map layer。
- Rect Shadow Map layer。
- 近接触面、cascade 边界和 camera movement 下无明显跳动。

### 5.7 Picking

Picking baseline：

- Picking target 与 viewport extent 一致。
- 点击 opaque entity 返回正确 entity ID。
- 点击背景返回无命中。
- Resize 后第一张 ready picking frame 不读取旧 extent。

### 5.8 Reflection Probe

当前契约：

| 项目 | 值 |
|---|---|
| Scene extraction limit | 16 probes |
| Runtime capture budget | 1 per frame |
| Runtime resident limit | 8 captures |
| Lighting | Local specular + local diffuse IBL |
| Projection | Box projection supported |
| Runtime state | Pending / Capturing / Ready / Failed |

需要固定：

- Capture 六个 cubemap face。
- Bake 后资源保持 resident，并受 freshness / generation 约束。
- Probe transform 或参数变化触发 dirty rebake。
- Clear 后不再绑定旧 runtime capture。
- Resident limit 下不淘汰 pinned、active 或 pending capture。
- Reflection Probe Specular、Diffuse 和 Influence debug isolation。

## 6. Vulkan Validation Baseline

### 6.1 启用规则

```text
Debug build
  -> request standard validation layer when available
  -> create default debug messenger
  -> log availability / active state

Release build
  -> do not request validation layer
  -> do not create debug messenger
```

Validation 在 Debug 下仍是 optional development capability。目标机器没有 Vulkan SDK validation layer 时，Renderer 记录 warning 并继续尝试 Raster Vulkan 初始化，不把开发层变成发布依赖。

### 6.2 RR-00 实测

运行方式：Debug Sandbox，正常 Editor 启动，捕获 stdout / stderr 15 秒。

观察结果：

```text
Vulkan validation enabled for Debug build; debug messenger active.
VulkanRenderResourceCache initialized.
VulkanRendererSystem initialized: surface 1920x1080,
  swapchain 3 images, API 1.4.325.
NexAur Engine started.
```

Validation 输出：

| Severity | 已知基线 |
|---|---|
| Error / VUID | 0 |
| Warning | 0 |
| stderr output | Empty |

该结果只覆盖当前启动与短时 Editor smoke，不代表所有交互路径天然无 validation 问题。每个后续工作包仍需运行它触及的手工矩阵。

Synchronization Validation 尚未在 RR-00 中作为单独 preset 强制开启。RR-02 迁移到 synchronization2 时必须增加对应验证记录。

### 6.3 RR-02 Synchronization Validation 实测

日期：2026-08-12。

代码基线：`deed5da refactor(renderer): add RenderGraph access model` 加 RR-02 工作区改动。

环境：沿用第 2 节固定环境，Debug Sandbox，`VK_LAYER_KHRONOS_validation` 1.4.335。

运行方式：

```powershell
$env:VK_LAYER_VALIDATE_SYNC = "1"
bin/msvc-vcpkg/Debug/Sandbox.exe
```

覆盖：

- 正常 Editor 启动并连续渲染 25 秒。
- Swapchain 三张 image 的首次 acquire、present 和后续 reacquire。
- 窗口 resize、minimize 到 `0x0`、restore，再次 resize。
- RenderGraph image barrier 的 `vkCmdPipelineBarrier2` 提交路径。

结果：

| 检查项 | 结果 |
|---|---|
| `SYNC-HAZARD` | 0 |
| VUID | 0 |
| Validation error / warning | 0 / 0 |
| stderr output | Empty |
| resize / minimize / restore | Pass；恢复后继续 present，进程未提前退出 |

本记录只证明本次涉及的常规帧与 swapchain 生命周期在 Synchronization Validation 下通过，不替代 SSR debug isolation、Reflection Probe bake、picking 等独立人工视觉验证。

## 7. GPU / Editor 手工验证矩阵

以下矩阵是后续 Renderer PR 的选择性验证入口。只有受影响项需要执行，但所有已执行项都应记录 GPU、结果和异常。

| ID | 场景 / 操作 | 通过条件 |
|---|---|---|
| RRB-01 | 启动 Sandbox / Final Lit | Renderer、viewport、ImGui 正常；无全黑或 validation error |
| RRB-02 | SSR on/off + 4 个 SSR debug view | Debug isolation 正确；Final Lit fallback 连续 |
| RRB-03 | AO Raw / Blurred | 尺寸正确；blur 无明显跨边界泄漏 |
| RRB-04 | Bloom Composite / downsample / upsample mip | mip 有效；无 stale view / resize artifact |
| RRB-05 | SMAA Edge / Blend / Output | 三阶段输出可区分；Final Lit 使用 SMAA output |
| RRB-06 | Directional shadow map / cascades | 4 cascades、方向、稳定化和边界正确 |
| RRB-07 | Point / rect shadow layer | budget、layer 和对应 light 正确 |
| RRB-08 | Viewport resize | 所有 target 重建；画面、picking 和 debug view extent 一致 |
| RRB-09 | Swapchain resize / minimize / restore | 不崩溃；恢复后正常 present |
| RRB-10 | Entity picking | opaque hit、background miss、resize 后结果正确 |
| RRB-11 | Reflection Probe Capture / Bake | 六面 capture 完成；Ready 状态与 lighting 生效 |
| RRB-12 | Probe dirty rebake / clear | generation 更新；clear 不绑定旧资源 |
| RRB-13 | Probe resident limit | pinned / active / pending 不被错误淘汰 |
| RRB-14 | Runtime material edit / texture replacement | 当前帧或约定下一帧更新；无 descriptor lifetime error |
| RRB-15 | Validation log review | 本次操作未新增 warning、error 或 VUID |

建议记录格式：

```text
Date:
Commit:
GPU / driver:
Items:
Result:
Validation warnings/errors:
Notes / screenshots:
```

## 8. 后续工作约束

- RR-01 修改 access model 时，先更新 planner focused smoke，再修改 executor。
- RR-02 修改 barrier API 时，保持 pass 顺序和本视觉基线不变。
- Structural PR 不调整 AO、SSR、Bloom、SMAA 或 shadow 参数。
- 任何 intentional visual change 必须在对应计划中建立新 baseline，不能静默覆盖本文档。
- 当前短时 startup smoke 不能替代 Reflection Probe、picking、resize 和 debug isolation 的人工检查。
- 后续测试失败时先判断是预期契约变化还是 regression；不得为了让测试通过而削弱 assertion。

## 9. RR-00 完成标准

RR-00 满足以下条件：

1. `Sandbox` Debug target 可构建，Renderer shader 同步编译。
2. RenderGraph state planning 有不依赖 Vulkan device 的纯逻辑入口。
3. Planner focused smoke 已注册到 CTest 并通过。
4. 当前 layout-only barrier 行为和已知缺口已记录。
5. Final Lit、SSR、AO、Bloom、SMAA、shadow、picking 和 Reflection Probe 基线已固定。
6. Debug validation layer 的请求和实际 debug messenger 状态可从日志确认。
7. 当前开发机 startup smoke 没有捕获到 validation warning / error。
8. 后续 Renderer PR 有可选择、可记录的 GPU / Editor 手工验证矩阵。

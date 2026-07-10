# SSR / Reflection Probe Bug Fix Plan

状态：4.1 - 4.2 已审核并完成；4.3 - 4.4 待审核。

## 1. 背景与结论

当前 SSR 与 Reflection Probe 的基础链路已经完整，但存在若干会直接影响画面正确性、调试可信度和 baked probe 生命周期的问题。

已确认的结论：

- Reflection Probe 当前确实会从 probe 世界坐标渲染六个 90 度 cubemap face，不是简单复用天空盒或 environment asset。
- 六面方向、up vector 和 cubemap 方向约定一致，六面 capture 本身不是本轮主要问题。
- SSR 的主要问题集中在 depth sampling、confidence / material mask 重复衰减，以及 debug 输出仍被后处理合成污染。
- Reflection Probe 的主要问题集中在 Transform 改动未触发重新 capture、dirty 状态过早清除，以及 runtime baked resource 被淘汰后无法恢复。
- 当前只绑定一个 active probe，并按相机位置选择；这是现阶段架构限制，不作为本轮 bug 修复项。

## 2. 修复目标

- 消除 SSR 在几何边缘由插值 depth 引起的错误命中和噪点。
- 让 SSR hit mask 只表达几何置信度，材质反射 mask 只应用一次。
- 保证 SSR、Reflection Probe 和 IBL debug view 输出纯净、可解释。
- 保证 probe 移动后一定进入 dirty 状态，并且只在 capture 真正成功后恢复 fresh。
- 保证组件仍引用的 baked probe 不会被 renderer 静默淘汰并退回全局 IBL。
- 保持现有六面 scene capture、local diffuse IBL 和 probe specular 行为不回退。

## 3. 已确认问题

| ID | 优先级 | 子系统 | 问题 | 根因 | 目标修复 |
| --- | --- | --- | --- | --- | --- |
| BF-SSR-01 | P0 | SSR Trace | 几何边缘可能出现虚假 hit、闪烁或噪点 | scene depth 与 SSR color 共用 linear sampler，depth 查询发生双线性插值 | 为 depth 使用 nearest sampler 或整数 texel `Load`；scene color 保持 linear sampling |
| BF-SSR-02 | P1 | SSR Composite | SSR contribution 被额外压低，参数响应不直观 | surface reflection mask 已进入 trace confidence，post composite 又乘一次 | hit mask 只保留几何 confidence；材质 surface mask 仅在 composite 应用一次 |
| BF-SSR-03 | P1 | Debug Output | Reflection Probe / IBL debug view 会混入 SSR 或后处理结果 | forward 已输出 debug color，但 `shouldRenderSsr()` 未考虑 IBL / material debug mode | debug mode 走明确的输出隔离路径，跳过 SSR 及会改变贡献判断的后处理阶段 |
| BF-PROBE-01 | P0 | Probe Capture | 移动 probe 后仍显示 Fresh，cubemap 内容与新位置不一致 | Gizmo 和 Properties 直接写 Transform，没有使 capture dirty | probe translation 改变时统一标记 `capture_dirty = true`，覆盖 Inspector 与 Gizmo 路径 |
| BF-PROBE-02 | P0 | Probe State | capture / bake 进入队列后过早显示 Fresh | 请求被接受时就清除 dirty，而不是等待 GPU capture 成功 | pending / capturing 阶段保持 dirty；仅在 `Ready && runtime_resource_ready` 后清除；失败保持 dirty |
| BF-PROBE-03 | P1 | Probe Transform | rotation / scale 可编辑但对 capture 和 influence box 无效 | 当前只提取 translation，shader 使用 axis-aligned center / extents | 本轮明确 AABB baseline：只支持 translation + component extents，并在 Editor 中避免 rotation / scale 造成错误预期 |
| BF-PROBE-04 | P0 | Probe Lifetime | baked probe 可能被 LRU 淘汰，组件仍显示 baked asset，但渲染已静默 fallback | resident limit 淘汰 runtime capture；runtime-generated asset 没有持久化 cubemap，无法重新加载 | 组件仍引用的 baked resource 不得静默淘汰；优先 pin 引用中的 baked entry，超预算时返回明确失败状态 |

### 3.1 代码证据

- SSR linear sampler 创建：`source/Engine/Function/Renderer/Vulkan/targets/vulkan_scene_color_target.cpp:164`。
- SSR color / depth descriptor 绑定：`source/Engine/Function/Renderer/Vulkan/vulkan_renderer_system.cpp:3970`。
- SSR depth 采样：`assets/shaders/Renderer/Vulkan/reflection/vulkan_ssr_trace.hlsl:58`、`:150`、`:236`。
- surface mask 进入 trace confidence：`vulkan_ssr_trace.hlsl:345`。
- surface mask 在 post composite 再次相乘：`assets/shaders/Renderer/Vulkan/post_process/vulkan_post_process.hlsl:240`。
- forward debug color 输出：`assets/shaders/Renderer/Vulkan/forward/vulkan_forward.hlsl:345`。
- SSR 调度判断未隔离 IBL debug：`source/Engine/Function/Renderer/Vulkan/vulkan_renderer_system.cpp:2854`。
- Gizmo 直接写 Transform：`source/Engine/Editor/Panels/viewport_panel.cpp:616`、`:680`。
- Properties Transform 编辑：`source/Engine/Editor/Panels/properties_panel.cpp:255`。
- Probe render position 仅取 translation：`source/Engine/Function/Scene/scene_v2.cpp:73`。
- Probe shader 使用 AABB center / extents：`assets/shaders/Renderer/Vulkan/common/pbr_reflection_probe.hlsli:11`、`:26`。
- Properties 请求接受后提前清 dirty：`source/Engine/Editor/Panels/properties_panel.cpp:757`。
- Bake All 请求接受后提前清 dirty：`source/Engine/Editor/editor_layer.cpp:1127`。
- runtime resident limit 与 LRU 淘汰：`source/Engine/Function/Renderer/Vulkan/vulkan_renderer_system.cpp:77`、`:1837`。
- runtime-generated baked asset 无法按普通资产恢复：`source/Engine/Function/Renderer/Vulkan/vulkan_draw_list_builder.cpp:146`。

行号用于本次审查定位，实施时以对应符号和实际调用链为准。

## 4. 工作包

### 4.1 BF-R40.6.2：SSR Depth Sampling and Confidence Fix（已完成）

主要工作：

- 拆分 SSR color sampler 与 depth sampler。
- depth 采用 nearest sampling，或改为基于像素坐标的 `Texture2D.Load`；binary refinement 和 coarse trace 使用相同规则。
- 保留 scene color 的 linear sampling，避免 raw reflection 颜色出现明显块状采样。
- 将 hit mask 收敛为 ray hit、edge、distance、thickness、facing 等几何置信度。
- material surface reflection mask 仅在最终 SSR composite 中应用一次。
- 不通过放大 intensity 或 hit mask 掩盖 trace 错误。

实现结果：

- SSR depth 查询统一改为基于像素坐标的 `Texture2D.Load`，不再经过 linear sampler。
- scene color 继续使用 linear sampling，descriptor layout 和 Vulkan pass 接口保持不变。
- trace 阶段移除 surface reflection mask 的采样、早退和 confidence 乘法。
- post composite 明确区分 geometric confidence 与 material surface mask，并只在最终 composite 相乘一次。

验收：

- `SSR Mirror Wall` 的物体边缘不再因插值 depth 形成明显虚假 hit。
- `SSR Hit Mask` 能单独解释几何命中，不随材质 mask 被重复平方式衰减。
- SSR disabled / miss 时继续保留 Reflection Probe / IBL fallback。

验证：

- Debug Sandbox 构建通过，SSR HLSL 已成功编译为 SPIR-V。
- `NexAur.RenderSettingsSmoke` 通过。
- Sandbox 隐藏窗口短启动通过。
- `SSR Mirror Wall` 的 depth discontinuity 视觉检查仍需在编辑器视角下人工确认。

### 4.2 BF-R40.6.3：SSR / IBL Debug Isolation（已完成）

主要工作：

- IBL debug、material debug 或其他直接输出贡献色的模式激活时，不执行 SSR final composite。
- 对会改变 debug 数值判断的 AO、Bloom、tone / color grading 阶段建立明确的 bypass 规则。
- 保持普通 Final Lit 渲染顺序不变。

实现结果：

- 当 IBL / material debug 非 `FinalLit` 且 Effects Debug 为 `FinalLit` 时，renderer 启用 forward debug isolation。
- viewport 与直接 swapchain 两条 render graph 路径都会跳过 AO、SSR、Bloom 和 SMAA pass。
- post-process push constants 新增内部隔离标志；shader 只读取 forward debug color 并完成输出颜色编码，不执行 exposure、tone mapping、color grading、vignette 或 sharpen。
- 显式 Effects Debug 保持更高优先级，SSR / AO / Bloom / SMAA debug 仍能按需调度对应 pass。

验收：

- `Reflection Probe Influence` 只显示 influence。
- `Reflection Probe Specular` 只显示 local probe specular contribution。
- `Reflection Probe Diffuse` 只显示 local probe diffuse contribution。
- SSR debug view 不被 final SSR composite 再次叠加。

验证：

- Debug Sandbox 增量构建通过，post-process HLSL 已成功编译为 SPIR-V。
- `NexAur.RenderSettingsSmoke` 通过。
- Sandbox 隐藏窗口短启动通过。
- Reflection Probe Influence / Specular / Diffuse 的纯输出仍需在编辑器视角下人工确认。

### 4.3 BF-R40.9.1：Reflection Probe Transform / Dirty State Fix

主要工作：

- 为 probe capture 建立统一的 Transform revision 或 capture input hash，至少覆盖 world translation、capture 参数和会影响 capture 的组件参数。
- Inspector 与 Gizmo 不再分别维护脏状态；二者都通过统一判定触发 dirty。
- 请求入队只改变状态为 Pending，不清除 dirty。
- Capturing 保持 dirty；成功生成且绑定 runtime resource 后才转为 Fresh。
- capture 失败、资源创建失败或请求被拒绝时保持 dirty，并保留上一版可用 resource。
- 本轮维持 axis-aligned probe box：rotation / Transform scale 不参与 capture 或 influence；Editor 应明确该约束，box 尺寸继续由 component extents 控制。

验收：

- 用 Gizmo 或 Properties 移动 probe 后立即显示 Dirty。
- Pending / Capturing 期间不会短暂显示 Fresh。
- capture 成功后显示 Fresh，失败后仍显示 Dirty 且错误信息可见。
- 只修改不影响 capture 内容的 diffuse / specular intensity 时，不触发不必要的六面 recapture。

### 4.4 BF-R40.9.2：Runtime Bake Lifetime Fix

主要工作：

- 区分 transient capture 与组件正在引用的 baked capture。
- 组件引用中的 baked resource 作为 pinned entry，不进入普通 transient LRU 淘汰集合。
- resident budget 不足时，不允许通过静默淘汰另一个仍被引用的 baked probe 来完成新 bake。
- 超预算请求返回明确 Failed / OverBudget 状态，旧 probe 保持可用。
- 清除 baked data、删除 probe 或卸载 scene 时解除 pin 并正常释放资源。

验收：

- renderer debug 中显示 baked asset 时，对应 runtime descriptor 必须真实可用。
- 多 probe bake 达到 resident limit 后，不会出现 Inspector 仍为 Fresh 但画面已 fallback 的状态。
- scene reload、删除 probe、Clear Baked Data 后不残留无主 pinned resource。

## 5. 范围约束

本轮不包含：

- SSR temporal accumulation、denoise、Hi-Z tracing、stochastic SSR 或透明物体 SSR。
- material normal / roughness auxiliary target；当前 `roughness_fade` 仍是全局近似控制。
- oriented box reflection probe；rotation 支持留给独立 OBB probe 工作项。
- 多 probe per-object selection、priority stack 或 probe blending。
- baked cubemap 的 DDS / KTX / HDR 持久化导出与跨进程恢复。
- light probe volume、DDGI 或完整 diffuse GI。

其中 `roughness_fade` 当前在 post 中仅形成全局衰减，语义不够精确，但在缺少 per-pixel roughness target 的前提下不作为本轮阻塞缺陷；后续应随 material meta target 一并修正。

## 6. 实施顺序

1. 修复 SSR depth sampling 与 surface mask 单次应用。
2. 建立 SSR / IBL / material debug 输出隔离。
3. 修复 probe Transform invalidation 和 dirty / capture 状态机。
4. 修复 runtime baked resource pinning 与超预算反馈。
5. 完成必要自动测试和针对性视觉验证。

## 7. 必要测试

只执行与本轮改动直接相关的验证：

```text
cmake --build build\msvc-vcpkg --config Debug --target Sandbox
ctest --test-dir build\msvc-vcpkg -C Debug -R "NexAur\.(RenderSettingsSmoke|SceneSerializerSmoke)" --output-on-failure
bin\msvc-vcpkg\Debug\Sandbox.exe short startup smoke
```

建议新增或扩展一个 focused smoke，覆盖：

- probe translation revision 变化后从 Fresh 转为 Dirty。
- Pending / Capturing 不清 dirty。
- Ready 且 runtime resource ready 后才转为 Fresh。
- Failed / OverBudget 保持 Dirty。

必要人工视觉检查：

- `SSR Mirror Wall`：观察 Hit Mask、Raw Reflection 和 Final Lit，重点检查物体轮廓与 depth discontinuity。
- Reflection Probe debug：Influence / Specular / Diffuse 三种模式分别确认无 SSR 和后处理污染。
- 移动已 bake probe：确认 Dirty -> Capture -> Fresh 状态与画面位置一致。
- 构造超过 resident limit 的 probe bake：确认不会静默丢失仍被组件引用的 baked resource。

## 8. 完成定义

- 上述 P0 / P1 问题均有对应代码修复或明确状态反馈，不再静默 fallback。
- SSR debug 数据、probe debug 数据和 Final Lit 的职责边界清楚。
- 六面 true scene capture、local diffuse IBL 和现有 SceneSerializer 行为没有回退。
- 必要构建、focused smoke 和短启动通过。
- 视觉验收能稳定复现修复结果，而不是依赖偶然视角或参数放大。

## 9. 后续工作

- Forward material normal / roughness auxiliary target，替代 depth-derived normal 和全局 roughness fade。
- Oriented box reflection probe，上传 world-to-probe transform 并支持旋转 box projection。
- 基于 object / shading position 的多 probe 选择与 blend。
- 持久化 generated cubemap asset，可在资源淘汰或进程重启后重新加载。
- Light Probe / irradiance volume / DDGI，用于带 visibility 的空间 diffuse lighting。

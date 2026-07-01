# NexAur 渲染质感提升计划

日期：2026-07-01

## 1. 文档定位

本文档用于整理 NexAur 在完成 Vulkan 渲染主链路、HDR / ACES / Bloom、PBR / IBL、CSM / PCF / PCSS 等基础能力之后，继续提升画面质感所需的开发路线。

本文档不是替代 `renderer_visual_effects_development_plan.md`，而是作为后续“画面从能跑走向好看”的质量提升计划。后续每个 PR 完成后，建议同步更新本文件中的完成记录、验收结果和遗留问题。

## 2. 当前画面问题判断

以当前 Sandbox 和 Cornell Box 场景为例，画面显得“假”的主要原因不是单个模型或颜色摆放问题，而是渲染闭环还缺少几类关键能力：

- 点光源当前没有阴影，Cornell Box 顶部点光不能产生物体投影和接触阴影。
- Emissive 面板只是可见自发光材质，暂时不会真正作为面积光照亮场景。
- IBL、天空盒、方向光仍在参与 Cornell 场景，暗部被环境光填满，缺少封闭室内光照基准。
- 缺少 AO / contact shadow，物体和地面、墙体之间没有足够的接触遮蔽。
- 当前 IBL 更偏全局环境反射，缺少本地 reflection probe 或 SSR，室内/局部空间的反射一致性不足。
- 后期已有 ACES / Bloom 基础，但还缺少完整的 color grading、白点、对比度、饱和度、抗锯齿和稳定性收口。

结论：继续提升质感时，不应该只堆更多效果，而要先建立稳定的光照基线、调试视图和场景预设，再逐步补齐局部阴影、AO、面积光、反射和后期调色。

## 3. 总体原则

### 3.1 先校准，再加效果

画面偏白、偏灰、偏平时，优先检查 exposure、tone mapping、IBL intensity、skybox intensity、directional light intensity 和材质反照率范围。

如果没有固定参考基线，AO、Bloom、阴影和色彩调节会互相掩盖问题，后续很难判断某个 PR 是否真的提升了质量。

### 3.2 光照测试场景需要隔离

建议至少维护以下场景或 render preset：

- `Outdoor`：天空盒、IBL、方向光为主，用于开放场景。
- `Studio`：中性 HDR + 控制光源，用于模型和材质检查。
- `Cornell`：关闭或大幅压低 skybox / IBL / directional light，只使用顶部局部光源。
- `AssetPreview`：固定相机、固定 HDR、固定曝光，用于 DamagedHelmet / glTF PBR 回归。

### 3.3 Debug view 必须和效果一起交付

每个视觉效果都应该带最小 debug view：

- AO：AO raw / blurred AO / final composite。
- Shadow：directional shadow / point cubemap face / local shadow factor / contact shadow mask。
- Reflection：probe cubemap / SSR hit / reflection contribution。
- Color：pre-tonemap HDR / post-tonemap LDR / bloom contribution。
- Lighting：direct / indirect / emissive / final lit。

### 3.4 避免把所有功能塞进 Forward shader

继续新增效果时，需要保持 shader 组织清爽：

```text
assets/shaders/Renderer/Vulkan/
  common/
    color.hlsli
    pbr_brdf.hlsli
    pbr_ibl.hlsli
    shadow_sampling.hlsli
  ao/
    vulkan_ssao.hlsl
    vulkan_ao_blur.hlsl
  post/
    vulkan_color_grading.hlsl
    vulkan_fxaa.hlsl
  reflection/
    vulkan_ssr.hlsl
  shadow/
    vulkan_point_shadow_depth.hlsl
    vulkan_contact_shadow.hlsl
```

Renderer C++ 侧也应继续按 target / pass / resource / settings 拆分，避免把 effect state 全塞进 `VulkanRendererSystem`。

### 3.5 不埋坑约束

本阶段目标是稳定提高 NexAur 画面质感，不追求一次性补齐现代渲染器的全部高级能力。每个 PR 都必须有清晰边界、默认关闭或安全默认值、debug view 和回退路径。

必须坚持：

- 每个效果都要能独立开关。
- 每个效果都要有最小 debug view。
- 每个效果都要能在 `RenderSettings` 中序列化或保持稳定默认值。
- 每个 pass 的输入/输出 target 要明确，避免隐式依赖上一帧状态。
- 每个 PR 只解决一个主问题，不同时引入多个互相影响的效果。
- 第一版优先做 CPU / GPU 资源生命周期清楚的实现，再考虑更复杂的性能优化。

本阶段明确不做：

- 不做完整 deferred renderer 重写。
- 不做全量 clustered / forward+ light culling。
- 不做无限制 point light cubemap shadow 覆盖所有点光；R37 第一版只允许显式开启且受预算限制的少量点光投影。
- 不做 DDGI / Lumen 类动态全局光照。
- 不做 ray tracing 依赖链路。
- 不做完整 TAA，除非 motion vector、history target 和 jitter 管线已经单独规划。
- 不做自动曝光 histogram，先使用手动 exposure 和固定 preset。
- 不做复杂 probe blending，第一版 reflection probe 只允许简单、可解释的影响范围。

如果某项能力会迫使 renderer 架构大改，应该单独开设计文档，不塞进当前质感提升 PR。

## 4. 推荐 PR 路线

本节按“先补最缺的质感基础，再补更高阶效果”的顺序排列。实际开发时建议先完成 4.1 到 4.4，后续效果根据画面问题再继续推进。

### 4.1 PR-R35.5：Lighting Calibration / Render Presets （已完成）

目标：

- 建立可复用的光照预设，先把“画面偏白、Cornell 不像 Cornell、模型参考不稳定”的问题收口。
- 为后续 AO、局部阴影、面积光提供稳定基线。

主要工作：

- 在 `RenderSettings` 中增加 lighting preset 或 scene lighting mode。
- 增加 `Outdoor / Studio / Cornell / AssetPreview` 基础预设。
- Cornell preset 中降低或关闭 IBL、skybox、directional light。
- Renderer Settings / Debug Panel 中提供 preset 切换入口。
- 固定 DamagedHelmet / Cornell 的推荐曝光、Bloom 和环境参数。

范围约束：

- 本 PR 只做参数预设和场景光照隔离，不新增新光源类型。
- 不改变现有 PBR / IBL shader 公式。
- 不做自动曝光。

验收标准：

- Cornell 场景暗部不再被天空盒和 IBL 大面积填白。
- DamagedHelmet 在 AssetPreview preset 下表现稳定，不依赖手动乱调 exposure。
- 切换 preset 不破坏现有 Outdoor Sandbox 画面。

### 4.2 PR-R36：SSAO / GTAO Baseline （已完成）

目标：

- 补足角落、缝隙和物体接触处的环境遮蔽。
- 让场景从“平”变得更有空间层次。
- 建立后续 GTAO / SSGI / contact shadow 可复用的 depth sampling 和 AO target 管线。

阶段定位：

- 本 PR 名称保留 `SSAO / GTAO Baseline`，但第一版实际只交付 SSAO v1 和 AO 管线骨架。
- GTAO 不进入第一版实现，只保留数据结构和 shader 目录上的扩展空间。
- 本 PR 可以先于 Lighting Calibration 推进，但必须保证 AO 默认值保守，避免用 AO 掩盖曝光、IBL 或 direct light 问题。

推荐数据流：

```text
DirectionalShadow
  -> SceneDepth / DepthPrepass
  -> SSAO Raw
  -> SSAO Blur
  -> Skybox
  -> ForwardScene(read Shadow, read AO)
  -> Bloom
  -> PostProcess
  -> ImGui / Present
```

设计判断：

- 当前 viewport depth 主要作为 depth attachment 使用，不应隐式假设它可以被 SSAO 直接采样。
- R36 应明确新增 sampled scene depth 能力，或者引入独立 `VulkanSceneDepthTarget`，让 pass graph 中的 depth write / shader read 关系显式化。
- 第一版 normal 可以从 depth reconstruct，避免一开始引入 normal target / GBuffer / deferred 重构。
- AO 最终应该尽量作用于 indirect lighting / IBL，不建议长期使用“最终画面整体乘 AO”的方案。

建议拆分：

1. R36-A：SceneDepth sampled target。
   - 新增或扩展 scene depth target。
   - depth image usage 至少包含 `DEPTH_STENCIL_ATTACHMENT` 和 `SAMPLED`。
   - pass graph 明确 depth attachment 到 shader read 的 layout transition。
   - 第一版只覆盖 opaque geometry，透明物体不参与 AO。
2. R36-B：SSAO target + pass。
   - 新增 `VulkanAoTarget`，管理 raw AO 和 blurred AO。
   - AO target 格式优先 `R8_UNORM` 或 `R16_SFLOAT`，先以简单稳定为主。
   - 新增 `VulkanAoPass`，包含 SSAO 和 blur 两个阶段，或拆成 `VulkanSsaoPass` / `VulkanAoBlurPass`。
3. R36-C：Render settings + debug view。
   - 新增 `RenderAoSettings`。
   - `RenderSettings` 持有 AO 设置。
   - Render Settings 面板增加 AO 参数。
   - Effects debug view 增加 SceneDepth / AO Raw / AO Blurred。
4. R36-D：Forward 接入 AO。
   - Forward shader 读取 AO texture。
   - AO 优先压 indirect / IBL contribution。
   - 如果第一版必须采用 post composite，应标记为临时路径，默认强度保守，并在完成记录中说明后续迁移点。

建议参数：

```cpp
struct RenderAoSettings {
    bool enabled = false;
    float radius = 1.2f;
    float intensity = 0.6f;
    float bias = 0.025f;
    float power = 1.2f;
    bool blur_enabled = true;
    bool half_resolution = true;
};
```

建议 shader / C++ 组织：

```text
source/Engine/Function/Renderer/Vulkan/targets/
  vulkan_ao_target.h
  vulkan_ao_target.cpp

source/Engine/Function/Renderer/Vulkan/passes/
  vulkan_ao_pass.h
  vulkan_ao_pass.cpp

assets/shaders/Renderer/Vulkan/ao/
  vulkan_ssao.hlsl
  vulkan_ao_blur.hlsl
```

shader 输入建议：

- sampled scene depth。
- camera projection / inverse projection。
- viewport size / inverse viewport size。
- AO 参数 push constants。
- 可选 noise texture；第一版可以用固定 hash / poisson pattern，避免先引入新 texture resource。

范围约束：

- 第一版只做 screen-space AO，不做 bent normal。
- 第一版不要求 temporal accumulation。
- 第一版不和 SSGI 合并。
- 如果 normal target 成本过大，可以先从 depth reconstruct normal，但需要在 debug view 中暴露质量问题。
- 第一版不做 GTAO。
- 第一版不做 normal prepass / GBuffer。
- 第一版不做 TAA denoise。
- 第一版不让 AO 影响 skybox、emissive、debug draw、UI。

代码组织：

- `VulkanAoTarget`：AO raw / AO blurred target。
- `VulkanAoPass`：SSAO + blur。
- `assets/shaders/Renderer/Vulkan/ao/`：AO shader 独立目录。
- `RenderPostProcessSettings` 或独立 `RenderAoSettings`：保存 AO 参数。

验收标准：

- Cornell block 与地面、墙角有可见但不过脏的接触遮蔽。
- AO debug view 可查看 raw / blurred。
- AO 关闭时画面回到原始 indirect lighting。
- Skybox 不被 AO 污染。
- Bloom / ACES 不因为 AO 开关产生异常曝光跳变。
- viewport resize 后 AO target 正确重建。
- Debug / Release 构建通过，HLSL 编译通过。

### 4.3 PR-R37：Point Light Cube Shadow Map + Contact Shadow （已完成）

目标：

- 建立正式的点光源 cube shadow map 管线，解决局部点光不投影导致的“假”。
- 让 Cornell 顶部 point light 能给 block、地面和墙体提供可解释的局部阴影基线。
- Contact shadow 只作为短程补强，用于修补 shadow bias 带来的接触漂浮，不替代 cube shadow map。

建议拆分为保守 MVP：

- R37-A：数据契约和预算开关。为点光增加 `casts_shadow`、shadow range、bias、normal bias、shadow resolution / budget 配置。
- R37-B：`VulkanPointShadowTarget`。创建 depth cube array 或 2D array shadow target，每个 shadowed point light 占 6 个 face。
- R37-C：`PointShadowMap` graph pass。第一版复用现有 depth-only `VulkanShadowPass`，对每个 shadowed point light 渲染 6 个 per-face depth layer，暂不依赖 geometry shader / layered rendering。
- R37-D：Forward shader point shadow sampling。按世界空间 light-to-fragment vector 选择 face，再用对应 view-projection 采样 2D array depth layer，加入 bias、PCF 和 shadow strength。
- R37-E：Debug view 和性能开关。Render Settings 暴露 point shadow enable、max shadowed point lights、resolution、bias、normal bias、PCF radius；Debug Panel 显示 shadow request / shadowed count、point shadow target 和 debug layer。
- R37-F：Contact shadow v1 可选补强。默认关闭，作为 point shadow 已遮挡区域的近接触强度增强。

实现建议：

- 第一版只给显式开启 `casts_shadow` 的点光分配 shadow slot，默认 `max_shadowed_point_lights = 1` 或 `2`。
- 不做“所有 point lights 自动投影”。超过预算的光源保持无阴影，并在 debug stats 中显示被裁剪数量。
- 每个 shadowed point light 需要 6 个 view-projection 矩阵，near plane 固定小值，far plane 取 light radius 或显式 shadow range。
- depth target 推荐从 512 起步，Cornell 可允许 1024；默认使用硬件 depth compare 或 shader 手动 compare，第一版 PCF tap 数保持很小。
- shadow pass 先复用现有 mesh draw / depth-only pipeline 思路，材质只需要 alpha-mask 预留，不引入复杂 material variant。
- `assets/shaders/Renderer/Vulkan/common/shadow_sampling.hlsli` 放通用 point shadow sampling，避免把采样逻辑塞进主 forward shader。
- Contact shadow 第一版不另开 screen-space pass；先作为 point shadow visibility 的近接触增强，避免在没有 depth prepass / normal buffer 的前提下引入不稳定屏幕空间假象。

范围约束：

- 第一版只做 point light cube shadow map，不同时交付 spot shadow、rect shadow、area light shadow。
- 第一版不支持无限数量局部阴影光源，不做 clustered / forward+ light culling。
- 第一版不做 VSM / EVSM / PCSS / soft shadow，先交付稳定 depth compare + 小核 PCF。
- 第一版不要求 alpha-tested foliage 完整正确，但要保留 alpha mask depth pass 的扩展点。
- 局部阴影默认关闭，只在 Cornell / 明确测试场景或显式 light flag 中启用。
- Contact shadow 作为短程补充，不替代 cube shadow map，不用于制造大范围阴影。

验收标准：

- Cornell 顶部 point light 开启 `casts_shadow` 后，内部 block 能在地面和墙面产生稳定阴影。
- Debug view 可查看 point shadow 2D array 的当前选中 face layer；Debug Panel 可查看 point shadow target、shadow request 和实际 shadowed point light 数。
- 关闭 point shadow 后画面回到 R35.5 / R36 baseline；关闭 contact shadow 不影响 cube shadow map。
- 多点光场景不会默认开启所有局部阴影，超过 shadow budget 时行为可预测且 debug stats 可见。
- viewport resize / scene reload 后 point shadow target 正确重建，不泄漏旧 descriptor / image view。
- Debug 构建通过，HLSL 编译通过，`RenderSettingsSmoke` 覆盖 point shadow / contact shadow settings 和 shadow slot 预算分配。

### 4.4 PR-R38：Rect Area Light / Cornell Lighting

目标：

- 让矩形面光源成为正式光源类型，而不是靠 emissive 材质和点光源假装。
- 为 Cornell Box、室内光、灯板、窗光提供更真实的光照基础。

建议第一版：

- 增加 `RectLightComponent` 或通用 `AreaLightComponent`。
- CPU / RenderData / GPU buffer 打通 rect light 数据。
- 直接光照先做简化矩形灯近似。
- 后续再升级 LTC area light。
- Emissive mesh 仍作为可见几何，不直接等于 GI 光源。

范围约束：

- 第一版只做 direct lighting，不做 emissive GI。
- 第一版可以先不做真实软阴影，只保证能量和高光形态比 point light 更合理。
- LTC lookup table 可以后置，先保留接口。
- Rect light 与可见 mesh 不强绑定，避免材质系统和 light system 过早耦合。

验收标准：

- Cornell 顶部灯以矩形面积参与光照。
- 高 roughness 表面能看到更自然的宽高光。
- 面光源参数可在 Inspector / Render Settings 中调节。

### 4.5 PR-R39：Color Grading + Anti-Aliasing Polish

目标：

- 在已有 HDR / ACES / Bloom 基础上增加最终画面调色和抗锯齿收口。

主要工作：

- Color grading：exposure offset、contrast、saturation、temperature / tint、vignette、LUT 预留。
- FXAA 或 SMAA v1：先解决明显几何边缘锯齿。
- Sharpen：可选，配合 TAA / FXAA 后恢复细节。
- Debug view：pre-tonemap、post-tonemap、bloom contribution。

后续升级：

- TAA：需要 motion vector、jitter、history buffer。
- 自动曝光：需要 luminance histogram 或 mip reduction，建议后置，不要和第一版 grading 混在一起。

范围约束：

- 第一版只做手动 color grading 参数和 FXAA / SMAA 之一。
- 不做 TAA。
- 不做自动曝光。
- 不引入复杂 LUT 资产管理；LUT 可先预留接口。

验收标准：

- 默认画面不再泛白，黑位、白位和中间调更稳定。
- FXAA / SMAA 开关可控，关闭后回到原始输出。
- Bloom 不再靠过曝撑画面，强度有健康默认值。

### 4.6 PR-R40：Reflection Probe / SSR v1

目标：

- 解决当前 IBL 只提供全局反射，室内或局部场景反射不匹配的问题。

建议路线：

- 先做 Reflection Probe 数据结构和调试显示。
- 支持手动放置 probe，绑定 cubemap 或后续 runtime capture。
- Forward shader 中增加 local reflection contribution 混合策略。
- SSR 作为屏幕空间补充，不替代 probe。

范围约束：

- 第一版只支持手动 probe，不做 runtime cubemap capture。
- 第一版只支持一个主 probe 或简单最近 probe，不做复杂 probe blending。
- SSR 后置，不和 Reflection Probe v1 同一个 PR。
- Probe 只解决 specular reflection，不承担 diffuse GI。

验收标准：

- 室内/局部场景的金属和光滑材质不再只反射全局天空盒。
- Probe debug view 可显示影响范围和 cubemap。
- SSR 关闭时 fallback 到 probe / IBL。

### 4.7 PR-R41：Material / Texture Sampling Polish

目标：

- 提升 PBR 材质稳定性和细节表现，避免“材质数据正确但看起来廉价”。

主要工作：

- 全面检查 texture mipmap、anisotropic filtering、sampler address mode。
- 检查 normal map 强度、TBN handedness、roughness / metallic channel 约定。
- 增加 glTF material extension 预留：clearcoat、emissive strength、transmission 后续可选。
- 增加 material debug view：base color、normal、roughness、metallic、AO、emissive。

验收标准：

- 斜视角地面和墙面纹理不明显糊成一片。
- DamagedHelmet 等 glTF PBR 样例在固定 preset 下稳定接近参考。
- 材质 debug view 能快速定位贴图通道问题。

### 4.8 PR-R42：Atmosphere / Fog / Depth Polish

目标：

- 为室外、远景和大场景增加空间空气感。

建议第一版：

- Height fog / exponential fog。
- Skybox 与 fog 颜色/曝光一致。
- Fog debug 和 RenderSettings 控制。

非目标：

- 第一版不做复杂体积云。
- 第一版不做重型 volumetric lighting。

验收标准：

- Outdoor preset 下远景层次更自然。
- 室内 / Cornell preset 默认不被 fog 污染。

## 5. Cornell Box 专项建议

如果目标是让 Cornell Box 成为可靠光照测试场景，建议按以下顺序处理：

1. 给 Cornell 增加专用 render preset，关闭或压低 IBL、skybox 和方向光。
2. 顶部 point light 先开启 cube shadow map，让 block 能投影到地面和墙体。
3. 后续再将顶部光源从 point light 临时方案升级为 rect light。
4. 增加 contact shadow 或 SSAO，让 block 落地。
5. 锁定 exposure / bloom 默认值，避免靠过曝制造亮度。
6. 保留 Cornell debug view：direct、indirect、AO、shadow、final lit。

## 6. 推荐优先级

最推荐的实际开发顺序分为两个阶段。

阶段一：必须收口，优先做小而稳的能力。

```text
R35.5 Lighting Calibration / Render Presets
  -> R36 SSAO / GTAO Baseline
  -> R37 Point Light Cube Shadow Map + Contact Shadow
  -> R38 Rect Area Light / Cornell Lighting
```

阶段二：根据阶段一后的画面问题再决定是否推进。

```text
R39 Color Grading + FXAA / SMAA
  -> R40 Reflection Probe v1
  -> R41 Material / Texture Sampling Polish
  -> R42 Fog / Atmosphere Polish
```

其中最能立刻改善当前观感的是：

1. Lighting Calibration。
2. SSAO。
3. Point Light Cube Shadow Map。
4. Color Grading。

Reflection、Area Light、TAA、SSR 会进一步提高上限，但不建议在没有光照基线和 debug view 的情况下提前堆叠。

## 7. 第一阶段完成定义

第一阶段不是追求所有效果都高级，而是让 NexAur 的画面具备稳定、可解释、可继续扩展的基础质感。

第一阶段完成时应满足：

- Outdoor / Studio / Cornell / AssetPreview preset 可切换。
- Cornell 场景不会被全局 IBL 和方向光误照亮。
- 物体和地面/墙体之间有 AO 或 contact shadow。
- Point light cube shadow map 能服务 Cornell 或室内测试场景。
- Rect light 能作为正式 light 数据流进入 renderer。
- Renderer Debug / Render Settings 能观察和调节上述效果。
- 默认 Sandbox 不因为新增效果明显变慢或变暗。
- 所有效果关闭后能回到当前 baseline。

## 8. 后置能力池

以下能力有价值，但不进入第一阶段：

- 多局部光源 clustered / forward+ shadow culling。
- Spot / rect light shadow。
- Full clustered / forward+ lighting。
- TAA。
- Auto exposure。
- SSR。
- Runtime reflection capture。
- Irradiance probe volume / DDGI。
- Volumetric lighting。
- Ray traced shadow / reflection。

这些能力应在第一阶段稳定后，再结合具体 demo 场景和性能预算单独规划。

## 9. 完成记录

### 2026-07-01：PR-R35.5 Lighting Calibration / Render Presets

完成内容：
- 新增 backend-neutral `RenderLightingPreset` / `RenderLightingCalibrationSettings`，支持 `Outdoor`、`Studio`、`Cornell`、`AssetPreview`、`Custom`。
- 新增 `applyRenderLightingPreset()`，统一写入 preset 对应的 exposure、Bloom、directional / point / skybox / IBL intensity scale。
- `RenderSceneFrameBuilder` 在 Scene -> Renderer frontend 阶段应用 lighting scale；Cornell preset 会关闭方向光阴影、关闭 skybox，并将 IBL 压低到 3%，保留局部点光强度。
- Render Settings 面板新增 Lighting Preset 区域，preset combo 可切换基础预设，手动调倍率会转为 `Custom`。
- Renderer Debug Effects 区只读显示当前 lighting preset。
- `RenderSettingsSmoke` 覆盖 preset 双缓冲传递和 Cornell frame builder 校准。

范围说明：
- 本 PR 只交付 renderer settings / frontend calibration，不新增新光源类型、不改 PBR / IBL shader 公式、不做自动曝光。
- Cornell 仍使用当前 point light + emissive panel；局部阴影和 rect area light 留给 R37 / R38。

验证：
- `cmake --build build\msvc-vcpkg --config Debug --target Sandbox`
- `ctest --test-dir build\msvc-vcpkg -C Debug -R NexAur.RenderSettingsSmoke --output-on-failure`

### 2026-07-01：PR-R36 SSAO / GTAO Baseline

完成内容：
- 新增 SSAO v1 管线：scene depth sampled input -> AO raw -> AO blurred -> post-process composite。
- 新增 `RenderAoSettings`，支持 enabled、radius、intensity、bias、power、blur、half resolution 参数。
- 新增 `VulkanAoTarget` 与 `VulkanAoPass`，AO shader 独立放入 `assets/shaders/Renderer/Vulkan/ao/`。
- PostProcess 支持 Scene Depth / AO Raw / AO Blurred debug view，并在 Render Settings / Renderer Debug 面板暴露状态和参数。
- viewport / swapchain depth 明确增加 sampled usage，并要求 depth format 同时支持 attachment + sampled。

范围说明：
- 本 PR 只交付 SSAO v1 和 AO 管线骨架，GTAO、normal target / GBuffer、temporal denoise 暂不进入实现。
- AO 第一版在 post-process 阶段乘到最终 lit color，默认强度保持保守；后续如果引入更完整 lighting 分解，应迁移到 indirect / IBL contribution。

验证：
- `cmake --build build\msvc-vcpkg --config Debug --target Sandbox`
- `ctest --test-dir build\msvc-vcpkg -C Debug -R NexAur.RenderSettingsSmoke --output-on-failure`
- `bin\msvc-vcpkg\Debug\Sandbox.exe` 短启动 smoke

### 2026-07-01：PR-R37 Point Light Cube Shadow Map + Contact Shadow

完成内容：
- `PointLightComponent` / serializer / Properties Panel 新增 point light shadow intent：`cast_shadow`、`shadow_range`、`shadow_strength`。
- `RenderSettings` 新增 `RenderPointShadowSettings` 与 `RenderContactShadowSettings`；Render Settings 面板暴露 point shadow budget、resolution、bias、normal bias、filter radius 和 contact shadow 参数。
- `RenderSceneFrameBuilder` 按 `max_shadowed_lights` 给点光分配 `shadow_slot`，保留 `shadow_requested` 供 debug 显示预算裁剪。
- 新增 `VulkanPointShadowTarget`，以 2D array 管理 point shadow cube faces，每个 shadowed point light 占 6 个 depth layer。
- 渲染图新增 `PointShadowMap` pass，复用现有 depth-only `VulkanShadowPass` 渲染 per-face depth，再在 ForwardScene 中读取 directional shadow 与 point shadow。
- Frame lighting UBO / point light SSBO 扩展 point shadow face matrices、shadow slot、range、strength、bias 和 contact shadow 参数。
- Forward shader 新增 point shadow face 选择、2D array depth compare、小核 PCF 和 contact shadow 近接触增强。
- PostProcess / Render Settings / Renderer Debug 支持 `Point Shadow Map` debug view 和 point shadow target / shadow request / shadowed count 观察。
- Cornell Box 顶部 point light 默认开启 shadow，作为 R38 rect light 前的稳定投影基线。

范围说明：
- 第一版不做 spot / rect shadow、不做 clustered / forward+ shadow culling、不做 VSM / EVSM / PCSS / soft shadow。
- 第一版 contact shadow 不另开 screen-space pass，只作为 point shadow 遮挡结果的近接触增强；后续如果引入 depth prepass / normal buffer，可升级为真正 screen-space contact shadow。
- 第一版复用 depth-only pass 和 per-face 2D array layer，不引入 geometry shader / layered rendering。

验证：
- `cmake --build build\msvc-vcpkg --config Debug --target Sandbox`
- `ctest --test-dir build\msvc-vcpkg -C Debug -R "NexAur\.(RenderSettingsSmoke|SceneSerializerSmoke)" --output-on-failure`
- `bin\msvc-vcpkg\Debug\Sandbox.exe` 短启动 smoke

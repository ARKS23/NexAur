# NexAur 渲染画面效果开发计划

日期：2026-06-28
分支：`vulkanRenderer`

## 1. 文档定位

本文档记录 NexAur Vulkan renderer 在接入完成、Runtime / Gameplay foundation 收口之后的画面效果开发路线。

关联文档：

- `renderer_vulkan_development_roadmap.md`：Vulkan renderer 接入和迁移历史主线。
- `renderer_post_vulkan_integration_plan.md`：Vulkan 后端接入完成后的基础 renderer 能力补齐。
- `phase1_2_runtime_gameplay_closeout_plan.md`：Runtime / Gameplay foundation 收口计划。

当前 renderer 已经具备：

- Vulkan 主线后端。
- Forward pass。
- Material / texture 最小链路。
- Direct-light PBR baseline。
- Procedural skybox baseline。
- Directional shadow map baseline。
- ObjectId picking。
- Debug draw。
- Renderer debug panel。
- Editor debug visualization controls。

下一阶段目标不是继续补“能跑”的基础能力，而是把画面从 baseline 推进到更接近现代游戏引擎的效果层级。

目标效果包括：

- PCF / PCSS / CSM 阴影质量提升。
- Physically Based Bloom。
- PBR + IBL。
- ACES tone mapping。
- HDR color pipeline。
- 后续可选的 AO、Color Grading、Fog、TAA / FXAA、debug views 和 GPU profiler。

说明：

用户提到的 `PBR + BIL` 本文档按 `PBR + IBL` 理解。

本文档是后续画面效果阶段的主计划文档。每个 PR 完成后，应同步更新对应章节的执行状态、完成记录和验证结果。

## 2. 总体原则

### 2.1 先补 HDR / PostProcess 骨架

Bloom、ACES、PBR 高光和 IBL 都依赖 HDR color pipeline。

如果不先把 scene color 从 LDR 推到 HDR，后续 Bloom 和 Tone Mapping 会反复返工。

推荐主线：

```text
Shadow
  -> Skybox
  -> Forward HDR
  -> DebugDraw
  -> PostProcess
  -> ImGui / Present
```

### 2.2 后处理先简单、骨架先正确

第一版 PostProcess 不追求效果复杂，而是要建立：

- fullscreen triangle pipeline。
- HDR scene color input。
- LDR output。
- ping-pong / intermediate target 管理。
- PassGraph image usage 和 layout transition。
- RenderSettings / effects UI 控制入口。

### 2.3 PBR / IBL 不和 Bloom 抢同一个 PR

PBR / IBL 会牵涉：

- 材质贴图种类。
- HDR environment import。
- cubemap conversion。
- irradiance map。
- prefiltered environment map。
- BRDF LUT。

Bloom 会牵涉：

- downsample / upsample mip chain。
- HDR threshold / energy。
- tone mapping 前合成。

两者都重要，但不要放进同一个 PR。

### 2.4 阴影先稳定，再高级

当前已有 directional shadow baseline。下一阶段建议：

```text
PCF / bias / stability
  -> CSM
  -> PCSS
```

原因：

- PCF / bias / stability 是所有阴影质量的基础。
- CSM 对大场景和第三人称 demo 的收益通常比单张 shadow map PCSS 更明显。
- PCSS 在 CSM / bias / debug view 稳定后更容易调。

### 2.5 每个效果都要有 debug view

画面效果如果没有 debug view，后续调参会很痛苦。

建议逐步支持：

- Shadow map viewer。
- Cascade color overlay。
- Bloom mip viewer。
- HDR scene color viewer。
- Exposure / tone mapping debug。
- IBL irradiance / prefilter debug。

第一版不需要都做，但每个大效果至少预留 debug hook。

### 2.6 RenderSettings 和 Debug Snapshot 分离

PR-R24 的 `RendererDebugService` 定位是观察 renderer 状态，后续不建议把 Bloom、ACES、Shadow、IBL 的可调参数直接塞进 debug snapshot。

建议拆成两类数据：

```text
RenderSettings / RenderContext options
  - 用户或 Editor 可以修改的渲染效果参数
  - 例如 exposure、tone mapping mode、bloom intensity、shadow mode

RendererDebugSnapshot
  - renderer 上一帧只读状态
  - 例如 target format、draw count、GPU resource count、debug view availability
```

Editor UI 可以临时把效果调参放在 `Renderer Debug` 面板下的 `Effects` 分组，也可以后续拆成独立 `Renderer Effects` 面板。但底层数据契约应保持清楚：调参写入 backend-neutral settings，状态显示读取 backend-neutral snapshot。

## 3. 推荐执行顺序

建议从 PR-R26 开始：

```text
PR-R26 HDR Scene Color + PostProcess Framework
PR-R27 ACES Tone Mapping + Exposure
PR-R28 Physically Based Bloom
PR-R29 PBR Material 完整化
PR-R30 IBL 基础链路
PR-R30.5 IBL Quality / Debug View / Shader Cleanup
PR-R30.6 DamagedHelmet PBR / IBL Reference Alignment
PR-R31 Shadow Quality Pass 1：PCF / Bias / Stability
PR-R32 Cascaded Shadow Maps
PR-R33 PCSS / Contact Hardening Shadow
PR-R34 Renderer Effects Debug Views
PR-R35 Color Grading / FXAA / Polish Pass
```

如果只想先快速提升观感，最短路径是：

```text
R26 HDR + PostProcess
R27 ACES
R28 Bloom
R31 Shadow Quality
```

如果目标是材质质感，优先：

```text
R26 HDR + PostProcess
R27 ACES
R29 PBR Material
R30 IBL
R30.5 IBL Quality / Debug
R30.6 DamagedHelmet PBR / IBL 对齐
```

## 4. PR-R26：HDR Scene Color + PostProcess Framework

执行状态：已完成。

目标：

- 建立 HDR scene color。
- 建立 Vulkan post process pass 骨架。
- 为 ACES、Bloom、Color Grading、FXAA 等效果准备稳定入口。

完成记录：

- 新增 `VulkanSceneColorTarget`，使用 HDR scene color intermediate target，优先格式为 `VK_FORMAT_R16G16B16A16_SFLOAT`，并提供 sampled image / sampler 给 post process。
- 新增 `VulkanPostProcessPass`，使用 fullscreen triangle 从 HDR scene color 输出到 LDR viewport / swapchain target。
- 新增 `vulkan_post_process.hlsl`，第一版只做 HDR color sample + clamp 输出，不实现 ACES / Bloom。
- `VulkanForwardPass` 拆分 forward render color format 和 swapchain image view format，避免 HDR pipeline 与 swapchain format 混用。
- PassGraph 已改为 `DirectionalShadowMap -> Skybox(HDR) -> ForwardScene(HDR) -> DebugDraw(HDR) -> ObjectIdPicking -> PostProcess -> ImGuiComposite/Present`。
- Viewport 路径输出到 existing `VulkanViewportTarget`，fallback swapchain 路径直接输出到 swapchain image。
- `RendererDebugSnapshot` 和 `RendererDebugPanel` 增加 HDR scene target 与 PostProcess 只读状态。
- ObjectId picking 继续使用独立 picking target，不受 HDR / PostProcess 影响。

建议工作：

1. HDR scene target。
   - Viewport / swapchain scene color 改为 HDR intermediate target。
   - 推荐第一版使用 `VK_FORMAT_R16G16B16A16_SFLOAT`。
   - 如果后续追求带宽优化，再评估 `R11G11B10_FLOAT`。
2. PostProcess target。
   - 新增 post process output target。
   - viewport 路径输出给 ImGui viewport texture。
   - swapchain fallback 路径输出到 swapchain color。
3. Fullscreen triangle pipeline。
   - 新增通用 fullscreen pass 基础能力。
   - shader 使用 HLSL。
   - 不引入 fullscreen quad vertex buffer。
4. PassGraph 接入。
   - Forward pass 输出 HDR scene color。
   - PostProcess pass 读取 HDR scene color。
   - PostProcess pass 输出 LDR color。
5. Renderer debug panel。
   - debug snapshot 显示 HDR scene target ready / format / size。
   - RenderSettings 预留 post process enabled / tone mapping enabled 等效果开关。

建议新增文件：

```text
Renderer/Vulkan/targets/
  vulkan_scene_color_target.h
  vulkan_scene_color_target.cpp

Renderer/Vulkan/passes/
  vulkan_post_process_pass.h
  vulkan_post_process_pass.cpp

assets/shaders/Renderer/Vulkan/
  vulkan_post_process.hlsl
```

暂时不做：

- Bloom。
- ACES。
- Color grading。
- TAA / FXAA。
- 自动曝光。

验收：

- 画面经过 PostProcess pass 输出。
- 未开启 tone mapping 时视觉与当前接近。
- viewport / swapchain fallback 路径都能正常显示。
- ObjectId picking 不受影响。
- Debug draw 仍在正确位置显示。
- 构建和 smoke / CTest 通过。

验证：

```powershell
cmake --preset msvc-vcpkg
cmake --build --preset msvc-vcpkg-debug
ctest --test-dir build/msvc-vcpkg -C Debug --output-on-failure
bin\msvc-vcpkg\Debug\Sandbox.exe  # hidden 3 秒图形 smoke
```

## 5. PR-R27：ACES Tone Mapping + Exposure

执行状态：已完成。

目标：

- 在 HDR pipeline 上接入 ACES tone mapping。
- 提供 exposure 控制。
- 建立颜色空间和 gamma 输出约定。

建议工作：

1. Tone mapping shader。
   - 在 `vulkan_post_process.hlsl` 中实现 ACES fitted tone mapping。
   - 明确 linear HDR input -> tone mapped linear / sRGB output。
2. Exposure。
   - 第一版使用手动 exposure。
   - Editor effects UI 通过 RenderSettings 修改 exposure。
   - 后续自动曝光单独做。
3. Gamma。
   - 明确 swapchain / viewport 输出是否为 sRGB format。
   - 避免重复 gamma correction。
4. Render settings。
   - 第一版可以放在 `RenderContext` 或 backend-neutral `RenderSettings` 中。
   - 不把 exposure 等可写参数放进 `RendererDebugSnapshot`。
   - 后续再抽成正式 `RenderSettings` asset。

完成记录：

- 新增 backend-neutral `RenderSettings` / `RenderPostProcessSettings` / `RenderToneMappingMode`，由 `RenderContext` 持有并通过 `RenderDataPacket` 双缓冲传递给 renderer。
- `VulkanPostProcessPass` 增加 fragment push constants，向 HLSL 传递 exposure、tone mapping mode、output gamma 和 manual gamma 标志。
- `vulkan_post_process.hlsl` 实现手动 exposure、ACES fitted tone mapping、None fallback 和按输出格式控制的手动 gamma。
- Renderer Debug 面板新增 `Effects` 分组，可切换 `None / ACES` 并调节 exposure；可写效果参数不进入 `RendererDebugSnapshot`。
- 明确 gamma 约定：sRGB 输出格式由 Vulkan color attachment 自动 encode；非 sRGB 输出格式才由 shader 手动做 `linear -> sRGB`。
- 新增 `RenderSettingsSmoke`，覆盖 RenderSettings 从 `RenderContext` 写入 read packet 的双缓冲链路。

暂时不做：

- 自动曝光。
- 色调曲线编辑器。
- Color grading LUT。
- Film grain / vignette。

验收：

- 高亮不再直接白切。
- exposure 可调。
- ACES 开关可控。
- UI / ImGui 不受 tone mapping 影响。
- 构建和 smoke / CTest 通过。

验证：

```powershell
cmake --preset msvc-vcpkg
cmake --build --preset msvc-vcpkg-debug
ctest --test-dir build/msvc-vcpkg -C Debug --output-on-failure
bin\msvc-vcpkg\Debug\Sandbox.exe  # hidden 3 秒图形 smoke
```

结果：

- CMake configure 通过。
- Debug 构建通过，HLSL shader 编译通过。
- CTest 10/10 通过，其中包含 `NexAur.RenderSettingsSmoke`。
- Sandbox 隐藏启动 3 秒 smoke 通过，未提前退出。

## 6. PR-R28：Physically Based Bloom

执行状态：已完成。

目标：

- 基于 HDR scene color 做 Bloom。
- Bloom 在 tone mapping 前合成。
- 效果倾向物理能量感，而不是简单 LDR 发光描边。

建议工作：

1. Bloom mip chain。
   - 从 HDR scene color downsample。
   - 多级 mip 逐层降采样。
   - 再 upsample 合成。
2. Threshold 策略。
   - 不建议第一版做硬 threshold。
   - 可以提供 soft threshold / knee。
   - 也可以采用近似 thresholdless 的 energy-preserving bloom。
3. 合成。
   - Bloom 与 HDR scene color 在 tone mapping 前合成。
   - 参数包括 intensity、radius、scatter。
4. Debug。
   - Editor effects UI 增加 bloom enabled / intensity / radius。
   - Renderer debug snapshot 只显示 bloom target ready / mip count / format 等只读状态。
   - 后续 PR-R34 增加 bloom mip viewer。

完成记录：

- 新增 `VulkanBloomTarget`，为 Bloom downsample chain、upsample chain 和 HDR composite target 管理独立 render target，格式跟随 HDR scene color。
- 新增 `VulkanBloomPass`，使用 fullscreen triangle 执行 downsample、upsample 和 tone mapping 前的 HDR composite。
- PassGraph 已接入 `HDRSceneColor -> BloomDownsample -> BloomUpsample -> BloomComposite -> PostProcess/ACES`。
- Bloom 关闭或 intensity 为 0 时跳过 Bloom graph，PostProcess 直接读取 HDR scene color，画面回到 ACES-only。
- `RenderPostProcessSettings` 增加 `bloom_enabled`、`bloom_intensity`、`bloom_scatter`、`bloom_radius`。
- Renderer Debug 面板 `Effects` 分组增加 Bloom 参数，`Targets` 分组增加 Bloom target 只读状态。
- `RendererDebugSnapshot` 增加 Bloom target ready / size / mip count / format，只记录 renderer 状态，不承载可写调参。
- HLSL 组织调整为：
  - `assets/shaders/Renderer/Vulkan/common/vulkan_fullscreen_triangle.hlsli`
  - `assets/shaders/Renderer/Vulkan/bloom/vulkan_bloom_downsample.hlsl`
  - `assets/shaders/Renderer/Vulkan/bloom/vulkan_bloom_upsample.hlsl`
  - `assets/shaders/Renderer/Vulkan/bloom/vulkan_bloom_composite.hlsl`
- `vulkan_post_process.hlsl` 复用 fullscreen triangle 公共 include，避免 fullscreen pass 重复手写 VS。

建议新增文件：

```text
Renderer/Vulkan/passes/
  vulkan_bloom_pass.h
  vulkan_bloom_pass.cpp

assets/shaders/Renderer/Vulkan/
  common/vulkan_fullscreen_triangle.hlsli
  bloom/vulkan_bloom_downsample.hlsl
  bloom/vulkan_bloom_upsample.hlsl
  bloom/vulkan_bloom_composite.hlsl
```

暂时不做：

- Lens dirt。
- Star streak。
- Anamorphic bloom。
- Auto exposure coupling。

验收：

- 高亮材质或天空能产生柔和 bloom。
- Bloom 关闭后画面回到 ACES-only。
- 不明显闪烁。
- viewport resize 后 bloom targets 正确重建。
- 构建和 smoke / CTest 通过。

验证：

```powershell
cmake --build --preset msvc-vcpkg-debug
ctest --test-dir build/msvc-vcpkg -C Debug --output-on-failure
bin\msvc-vcpkg\Debug\Sandbox.exe  # hidden 3 秒启动 smoke
```

结果：

- Debug 构建通过，Bloom / PostProcess HLSL 编译通过。
- CTest 10/10 通过，其中包含 `NexAur.RenderSettingsSmoke`。
- Sandbox 隐藏启动 3 秒 smoke 通过，未提前退出。

## 7. PR-R29：PBR Material 完整化

执行状态：已完成。

目标：

- 将当前 direct-light PBR baseline 扩展成更完整的 metallic-roughness workflow。
- 为 IBL 做准备。

建议工作：

1. 材质贴图。
   - normal map。
   - metallic-roughness texture。
   - AO texture。
   - emissive texture / emissive factor。
2. Tangent space。
   - Mesh import 保存 tangent。
   - 没有 tangent 时可 CPU 生成或 fallback。
3. Shader。
   - GGX / Smith / Schlick Fresnel。
   - normal mapping。
   - AO 影响 indirect lighting，direct baseline 可先保守处理。
   - emissive 进入 HDR scene color。
4. Resource / MaterialAsset。
   - `MaterialAsset` 扩展 texture slots。
   - 明确 sRGB / linear：base color 和 emissive 通常 sRGB，normal / roughness / metallic / AO 为 linear。

完成记录：

- `MaterialImportData` / `MaterialAsset` 扩展为完整 metallic-roughness v1：base color、normal、metallic、roughness、packed glTF metallic-roughness、AO、emissive texture / factor、normal scale、AO strength。
- `AssetManager::createMaterialFromImportData()` 按材质语义注册贴图颜色空间：base color / emissive 为 sRGB，其余 PBR 数据为 linear。
- Assimp 模型导入补充 base color factor、metallic / roughness factor、emissive factor、AO / emissive texture、packed metallic-roughness 识别和更稳的相对路径解析。
- Vulkan material descriptor layout 扩展为固定 material set：material constants + 7 个 sampled image slot + 1 个 sampler；缺失贴图统一绑定 fallback texture，由 shader flags 决定是否采样。
- `VulkanMaterialResource` 常量 buffer 增加 texture flags、emissive factor、normal scale、AO strength，并一次性写入完整 descriptor set。
- `vulkan_forward.hlsl` 增加 normal mapping、separate / packed metallic-roughness 采样、AO 对 ambient fallback 的影响、emissive 输出到 HDR scene color。
- Mesh vertex input 增加 tangent / bitangent attribute；程序化 cube / sphere 生成稳定 tangent space，缺失 tangent 时 shader 有 fallback basis。
- 新增 `MaterialAssetSmoke`，覆盖 PBR texture slots、packed metallic-roughness 和 sRGB / linear 注册约定。

暂时不做：

- Clearcoat。
- Sheen。
- Subsurface。
- Transmission。
- Anisotropy。
- 全套 glTF material extension。

验收：

- DamagedHelmet 等标准 glTF PBR 样例观感接近预期。
- base color / normal / metallic-roughness 可同时工作。
- emissive 能进入 HDR / Bloom。
- 材质贴图缺失时 fallback 稳定。
- 构建和 smoke / CTest 通过。

验证：

```powershell
cmake --build --preset msvc-vcpkg-debug
ctest --test-dir build/msvc-vcpkg -C Debug --output-on-failure
bin\msvc-vcpkg\Debug\Sandbox.exe  # hidden 3 秒启动 smoke
```

结果：

- Debug 构建通过，Forward HLSL 编译通过。
- CTest 11/11 通过，其中包含 `NexAur.MaterialAssetSmoke`。
- Sandbox 隐藏启动 3 秒 smoke 通过，未提前退出。

## 8. PR-R30：IBL 基础链路

执行状态：已完成。

目标：

- 接入 image based lighting。
- 让 PBR 材质拥有环境漫反射和环境高光。
- 从 procedural skybox 过渡到真实 HDR environment。

当前状态：

- `SceneFactory` 已经注册 `assets/textures/HDR/warm_restaurant_8k.hdr` 为 `EnvironmentMap`。
- `RenderSceneFrame` 已经携带 `environment_asset`、`environment_color` 和 `environment_intensity`。
- `VulkanDrawListBuilder` 当前只传递 environment color / intensity，尚未把 `environment_asset` 传入 Vulkan draw list。
- `VulkanSkyboxPass` 当前是 procedural fullscreen gradient，不采样 cubemap。
- `TextureAsset` / `VulkanTextureResource` 当前主要覆盖 2D RGBA8 贴图，不覆盖 HDR float image、cubemap、mip chain 和 IBL bake result。
- R29 已完成 PBR material texture slots，Forward shader 已具备接入 IBL 所需的 base color、normal、metallic、roughness、AO、emissive 输入。

范围判断：

- R30 建议把真实 environment skybox 一起做掉，因为 Skybox 和 IBL 应该来自同一个 environment source。
- R30 只做单个全局 environment，不做多 probe、probe blending 或 local reflection。
- 如果资源是 HDR equirectangular，优先作为 IBL source。
- 如果资源是 6 张 LDR cubemap，可以作为 skybox-only fallback，但不建议作为第一版 IBL 主输入。

建议工作：

1. Resource / asset。
   - 新增 `EnvironmentMapAsset` 或等价 CPU asset contract。
   - 支持 HDR equirectangular 读取，建议使用 float RGBA 数据。
   - `AssetManager` 增加 `loadEnvironmentMapCPU()`，`importEnvironmentMapAsset()` 继续只负责 asset identity。
   - `TextureLoader` 可增加 `loadHDR()`，但不要把 EnvironmentMap 和普通 2D material texture 语义混在一起。
2. Vulkan environment resource。
   - 新增 `VulkanEnvironmentResource`，由 `VulkanRenderResourceCache` 按 `AssetHandle` 缓存。
   - 内部持有 environment cubemap、irradiance cubemap、prefiltered cubemap、BRDF LUT 和 environment descriptor set。
   - 提供 fallback environment：没有 environment asset 时仍可稳定渲染。
3. IBL bake。
   - HDR equirectangular -> environment cubemap。
   - environment cubemap -> irradiance cubemap。
   - environment cubemap -> prefiltered environment cubemap。
   - 生成 BRDF LUT。
   - 第一版建议在 environment resource 创建时同步 bake，不每帧重算。
   - 推荐初始尺寸：environment 512 或 1024；irradiance 32 或 64；prefilter 128 或 256 with mip chain；BRDF LUT 256。
4. Descriptor / pipeline。
   - 新增 `VulkanDescriptorSetLayoutId::Environment`。
   - Forward pipeline 使用：

```text
set 0: FrameGlobal
set 1: Material
set 2: Environment
```

   - Environment descriptor 建议：

```text
binding 0: environment cubemap sampled image
binding 1: irradiance cubemap sampled image
binding 2: prefiltered environment cubemap sampled image
binding 3: BRDF LUT sampled image
binding 4: sampler
```

5. Forward shader。
   - 将 R29 中的 GGX / Fresnel / material sampling 逐步整理到 `common/pbr_lighting.hlsli`。
   - 增加 diffuse IBL：`irradiance * base_color * (1 - metallic)`。
   - 增加 specular IBL：prefiltered environment + BRDF LUT。
   - roughness 控制 prefiltered cubemap mip selection。
   - AO 第一版继续只影响 indirect / ambient 部分。
6. Skybox。
   - `VulkanSkyboxPass` 增加 environment descriptor set。
   - 有 environment cubemap 时采样真实 skybox。
   - 没有 environment asset 或 resource 不 ready 时保留 procedural skybox fallback。
   - Skybox intensity 继续使用 `environment_intensity`，避免新增一套调参入口。
7. Debug / settings。
   - Renderer debug snapshot 增加 environment resource ready、cubemap size、irradiance size、prefilter mip count、BRDF LUT ready。
   - R30 不做完整 IBL debug viewer；预留给 PR-R34。

实现记录：

- 新增 `EnvironmentMapAsset`，作为 HDR equirectangular CPU asset contract，保存 float RGBA 像素、尺寸和 source path。
- `TextureLoader::loadHDREnvironment()` 支持通过 `stbi_loadf()` 读取 HDR environment，并在 CPU 侧压到默认最大宽度，避免第一版直接把 8K float texture 全量送入后端。
- `AssetManager::loadEnvironmentMapCPU()` 增加 EnvironmentMap CPU 缓存，`importEnvironmentMapAsset()` 继续只负责 asset identity。
- 新增 `VulkanEnvironmentResource`，由 `VulkanRenderResourceCache` 按 `AssetHandle` 缓存，并创建 fallback environment。
- R30 实现采用 CPU bootstrap bake：
  - HDR equirectangular -> environment cubemap。
  - environment cubemap -> 近似 irradiance cubemap。
  - environment cubemap -> 近似 prefiltered cubemap mip chain。
  - CPU 生成 BRDF LUT。
- 这版先打通 resource / descriptor / shader / pass 链路，后续可把 CPU bootstrap bake 替换为 `vulkan_ibl_baker` GPU 离屏 bake。
- 新增 `VulkanDescriptorSetLayoutId::Environment`，Forward pipeline 绑定 `set 2: Environment`。
- `VulkanSkyboxPass` 绑定同一个 Environment descriptor layout，有真实 environment asset 时采样 cubemap，没有 asset 时保留 procedural fallback。
- `vulkan_forward.hlsl` 增加 diffuse IBL、specular IBL、roughness mip selection 和 BRDF LUT 采样，AO 只作用于 indirect。
- Renderer Debug 面板 `Resources` 分组展示 environment count、fallback environment、active environment、cubemap / irradiance / prefilter / BRDF LUT 状态。

建议新增 / 调整文件：

```text
source/Engine/Function/Resource/
  environment_map_asset.h
  environment_map_asset.cpp
  texture_loader.h/.cpp                 # 增加 HDR load helper
  asset_manager.h/.cpp                  # loadEnvironmentMapCPU()

source/Engine/Function/Renderer/Vulkan/resources/
  vulkan_environment_resource.h
  vulkan_environment_resource.cpp
  vulkan_cubemap_resource.h             # 可选：作为内部 helper
  vulkan_cubemap_resource.cpp

source/Engine/Function/Renderer/Vulkan/ibl/
  vulkan_ibl_baker.h
  vulkan_ibl_baker.cpp

source/Engine/Function/Renderer/Vulkan/passes/
  vulkan_skybox_pass.h/.cpp             # 支持真实 environment cubemap
  vulkan_forward_pass.h/.cpp            # 绑定 Environment descriptor set

assets/shaders/Renderer/Vulkan/common/
  pbr_lighting.hlsli

assets/shaders/Renderer/Vulkan/ibl/
  vulkan_equirect_to_cubemap.hlsl
  vulkan_irradiance_convolution.hlsl
  vulkan_prefilter_environment.hlsl
  vulkan_brdf_lut.hlsl
```

暂时不做：

- 多 reflection probe。
- Probe blending。
- Runtime dynamic reflection capture。
- Local parallax corrected cubemap。
- Skybox 编辑器 UI / 资源浏览器。
- IBL irradiance / prefilter debug viewer。
- LDR cubemap 完整导入链路。

验收：

- 金属材质能反射 environment。
- 粗糙度影响 specular lobe。
- 没有 environment asset 时 fallback 稳定。
- Skybox 使用真实 environment。
- Skybox 背景和 PBR 金属反射来自同一个 environment source。
- AO 只影响 indirect / ambient，不错误压暗 direct lighting。
- viewport resize 后 environment resource 不重复 bake，目标重建不破坏 IBL descriptor。
- 构建和 smoke / CTest 通过。

## 9. PR-R30.5：IBL Quality / Debug View / Shader Cleanup

执行状态：已完成。

目标：

- 在进入 shadow quality 之前，先把 R30 的 IBL v1 从“链路可用”收口到“可调、可观察、可对齐参考效果”。
- 以 externalRenderer / 成熟 IBL 管线作为参考，校准 DamagedHelmet、材质球矩阵和默认 HDR environment 的观感。
- 增加 IBL debug view，避免后续调 PBR / IBL / exposure 时只能凭肉眼猜。
- 整理 Forward HLSL 组织，让 PBR / IBL / material sampling 不继续堆在单个 `vulkan_forward.hlsl` 里。

当前问题：

- R30 已经打通 environment cubemap、irradiance、prefilter、BRDF LUT 和 Forward IBL 采样，但 bake 质量、mip 选择、环境强度和 debug 可视化还停留在 v1。
- DamagedHelmet 等高金属模型的观感仍需要和参考渲染器对齐，尤其是环境反射亮度、粗糙度响应、normal map 方向和 exposure / bloom 组合。
- 现在 Renderer Debug 只能看到 IBL resource ready / size / mip count，不能直接查看 irradiance、prefilter mip、BRDF LUT 或 IBL contribution。
- `vulkan_forward.hlsl` 同时承担 pass IO、material resolve、BRDF、IBL、shadow 和 direct light，后续接 PCF / CSM / PCSS 前需要先拆干净。

建议工作：

1. IBL 参考对齐。
   - 固定一组参考场景：DamagedHelmet、PBR 材质球矩阵、单一 HDR environment、固定 camera / exposure / bloom 参数。
   - 明确参考截图和参数：tone mapping、exposure、gamma、bloom enabled、directional light intensity、environment intensity。
   - 校准 skybox 和 specular reflection 是否来自同一个 environment source。
   - 检查 normal / tangent handedness、glTF packed metallic-roughness、AO 只影响 indirect 的约定。
   - prefiltered cubemap 采样 LOD 不再硬编码常量，优先由 actual prefilter mip count 推导。
   - 评估 irradiance / prefilter bake resolution 和 sample count 的最低可接受默认值，避免为了快启动牺牲到明显错误。
2. IBL debug view。
   - 增加 backend-neutral debug view enum，避免 Editor 直接依赖 Vulkan 类型。
   - 建议第一版支持：
     - Final Lit。
     - Diffuse IBL contribution。
     - Specular IBL contribution。
     - Normal / Roughness / Metallic / AO material channels。
     - Irradiance cubemap preview。
     - Prefilter cubemap mip preview。
     - BRDF LUT preview。
   - Debug view 默认关闭，不影响正常 render path。
   - Renderer Debug 面板增加 IBL debug view selector、prefilter mip selector、cubemap face / direction preview 控制。
3. Shader 组织优化。
   - 把 PBR 公共逻辑拆到 HLSL include：

```text
assets/shaders/Renderer/Vulkan/common/
  pbr_material.hlsli      # material texture flags / resolve helpers
  pbr_brdf.hlsli          # GGX / Smith / Fresnel / direct lighting
  pbr_ibl.hlsli           # irradiance / prefilter / BRDF LUT sampling
```

   - `vulkan_forward.hlsl` 保留 pass 输入输出、descriptor 声明和 PS orchestration。
   - 后续 shadow sampling 可继续拆到 `common/shadow_sampling.hlsli`，但本 PR 只做 PBR / IBL 相关拆分。
   - 保持 descriptor set contract 不变：`set 0: FrameGlobal`、`set 1: Material`、`set 2: Environment`。
4. IBL 性能和缓存收口。
   - 保留 R30 CPU bootstrap bake，但避免每次 resize 或普通 target rebuild 触发重复 bake。
   - BRDF LUT 应作为共享资源或缓存结果，避免每个 environment 重复计算。
   - 如时间允许，增加 baked IBL disk cache；如果超范围，至少在文档和代码 TODO 中把 GPU/offline baker 的替换点标清。

建议新增 / 调整文件：

```text
source/Engine/Function/Renderer/data/
  render_debug_view.h                 # 可选：backend-neutral debug view enum

source/Engine/Function/Renderer/Vulkan/passes/
  vulkan_ibl_debug_pass.h
  vulkan_ibl_debug_pass.cpp

assets/shaders/Renderer/Vulkan/common/
  pbr_material.hlsli
  pbr_brdf.hlsli
  pbr_ibl.hlsli

assets/shaders/Renderer/Vulkan/ibl/
  vulkan_ibl_debug.hlsl               # 可选：IBL debug fullscreen preview
```

实现记录：

- 新增 backend-neutral `RenderIblDebugMode` / `RenderIblDebugSettings`，随 `RenderSettings` 通过现有双缓冲 render data 链路传递。
- Renderer Debug 面板 `Effects` 分组增加 `IBL Debug` 下拉和 `Prefilter Mip` 控制，默认仍为 `Final Lit`，不影响正常渲染路径。
- `VulkanFrameLightingResource` 的 frame global uniform 增加 `ibl_debug_params`，向 shader 传递 debug mode、debug prefilter mip 和真实 prefilter max LOD。
- Forward shader 不再硬编码 `MAX_REFLECTION_LOD = 7`，而是由当前 active environment 的 prefilter mip count 推导。
- `vulkan_forward.hlsl` 拆出 PBR / IBL include：
  - `common/pbr_material.hlsli`：材质贴图 flags、base color、normal、metallic、roughness、AO、emissive resolve。
  - `common/pbr_brdf.hlsli`：GGX、Smith、Schlick Fresnel 和 direct-light BRDF。
  - `common/pbr_ibl.hlsli`：irradiance、prefiltered environment、BRDF LUT 和 IBL contribution。
- Forward shader 增加 IBL debug early return，支持查看 Final Lit、Diffuse IBL、Specular IBL、Combined IBL、Normal、Metallic、Roughness、AO、Emissive、Irradiance、Prefiltered Environment 和 BRDF LUT。
- `VulkanEnvironmentResource` 的 CPU BRDF LUT 生成改为返回静态缓存引用，避免每个 environment 创建时重复拷贝 LUT 像素。
- `RenderSettingsSmoke` 覆盖 IBL debug settings 的双缓冲传递。

暂时不做：

- 多 reflection probe。
- Probe blending。
- Runtime dynamic reflection capture。
- Local parallax corrected cubemap。
- 完整 GPU IBL baker。
- 完整 RenderDoc 自动对比或截图 diff 测试。
- 把所有 renderer debug view 一次性做完；Shadow / Bloom 综合 debug viewer 仍放到 PR-R34。

验收：

- DamagedHelmet 在默认 HDR environment 下不再明显偏黑，金属反射和粗糙度响应接近 externalRenderer 参考观感。
- Bloom 关闭 / 开启时，IBL 本身的亮度判断仍然清楚，不依赖 bloom 掩盖问题。
- Renderer Debug 可以切换查看 diffuse IBL、specular IBL、material channel、irradiance、prefilter mip 和 BRDF LUT。
- `vulkan_forward.hlsl` 明显瘦身，PBR / IBL helper 有独立 include，后续 PR-R31 接 shadow sampling 时不会继续把 forward shader 堆大。
- IBL bake 不因 viewport resize 重复触发；启动耗时有明确记录，若没有做 baked cache，需要在完成记录中说明原因和后续入口。
- 构建、shader compile、smoke / CTest 通过。

验证：

```powershell
cmake --build --preset msvc-vcpkg-debug
ctest --test-dir build/msvc-vcpkg -C Debug --output-on-failure
bin\msvc-vcpkg\Debug\Sandbox.exe  # hidden 3 秒启动 smoke
```

结果：

- Debug 构建通过，Forward HLSL include 拆分后 shader 编译通过。
- CTest 12/12 通过，其中 `NexAur.RenderSettingsSmoke` 覆盖 IBL debug settings。
- Sandbox 隐藏启动 3 秒 smoke 通过，未提前退出。

## 9.5. PR-R30.6：DamagedHelmet PBR / IBL Reference Alignment

执行状态：计划中。

目标：

- 在进入 Shadow Quality 之前，先把 DamagedHelmet 这类标准 glTF PBR 样例的材质输入、IBL 贡献、曝光和参考对比基线收口干净。
- 确认当前问题到底是贴图导入、tangent space、IBL 能量、AO 约定、tone mapping / bloom，还是参考场景不一致。
- 为后续 PR-R31 的 shadow 质量判断提供稳定画面基线，避免把材质 / IBL 问题误判成 shadow 问题。

背景：

- externalRenderer 中 DamagedHelmet 的观感更接近标准参考：金属反射清楚，暗部有足够压暗，蓝色舱盖、白色外壳和黄色条纹都比较稳定。
- Sandbox 当前画面中 DamagedHelmet 并不像“贴图文件串了”，更像 PBR / IBL / post process 的组合没有对齐：环境反射偏亮，暗部不够压，红色测试地面和高亮 HDR 背景也会影响肉眼判断。
- 本地 DamagedHelmet 资源结构正常：`Default_albedo.jpg`、`Default_metalRoughness.jpg`、`Default_emissive.jpg`、`Default_AO.jpg`、`Default_normal.jpg` 齐全，glTF material 使用标准 metallic-roughness workflow。
- 现有 smoke 已覆盖 DamagedHelmet 的 base color、packed metallic-roughness、emissive 路径和 sRGB / linear 注册约定，说明“贴图路径整体错乱”的概率较低。

需要解决的问题：

1. 参考场景不一致。
   - externalRenderer 参考图中背景、地面、窗帘和 HDR 反射是一组统一测试环境。
   - Sandbox 当前是红色测试地面 + 默认 HDR environment，不能直接拿最终画面对比贴图是否正确。
2. Tangent space 可能被二次生成扰动。
   - Assimp 已启用 `aiProcess_CalcTangentSpace`。
   - 当前模型导入后仍会对所有带 UV 的 mesh 再跑自定义 `generateTangents()`。
   - DamagedHelmet 这类复杂 UV atlas 对 tangent / bitangent handedness 很敏感，normal map 误差会让高光和凹槽看起来像贴图错位。
3. IBL AO 约定需要校准。
   - 当前 IBL shader 只让 AO 影响 diffuse IBL。
   - externalRenderer / 常见 PBR baseline 通常会让 AO 影响整个 indirect ambient，至少要评估 specular occlusion。
   - DamagedHelmet 凹槽多、金属占比高，不压 specular IBL 会显得暗部发白、塑料感偏重。
4. Tone mapping / Bloom / exposure 需要固定对比基线。
   - 当前默认 `ACES + exposure 1.0 + Bloom enabled` 容易放大 HDR 背景差异。
   - 对齐 reference 时应先提供一组固定参数，避免靠反复拖 slider 猜问题。
5. IBL 启动耗时需要顺手确认。
   - PR-R30 / R30.5 后启动变慢主要可能来自 HDR load、CPU environment cubemap、irradiance、prefilter 和 BRDF LUT bootstrap bake。
   - 本 PR 不要求完整 GPU IBL baker，但要确认没有 viewport resize / target rebuild 导致重复 bake。

建议工作：

1. 建立 DamagedHelmet 对齐 preset。
   - 固定 model、camera、HDR environment、environment intensity、directional light、exposure、bloom enabled / intensity。
   - 建议第一轮对比关闭 Bloom，使用 neutral floor 或隐藏高饱和红色测试地面。
   - 记录 externalRenderer 参考截图的关键参数，作为后续视觉验收基线。
2. 材质输入 debug view 补强。
   - 在现有 IBL Debug 基础上补齐或确认以下视图：
     - BaseColor。
     - Normal。
     - Metallic。
     - Roughness。
     - AO。
     - Emissive。
   - 这些视图用于判断“贴图输入是否正确”，不用于最终画面调色。
3. Tangent space 修正。
   - 优先保留 Assimp 生成的 tangent / bitangent。
   - 仅在 mesh 缺失 tangent / bitangent 时调用自定义 `generateTangents()`。
   - 保持 shader fallback tangent 作为最后兜底，不把它当主路径。
4. IBL AO / specular occlusion 校准。
   - 先按成熟 baseline 尝试让 AO 影响 indirect diffuse + specular。
   - 如效果过重，再引入轻量 specular occlusion 近似，而不是简单无条件压暗 direct lighting。
   - 明确文档约定：AO 不影响 direct lighting。
5. Tone mapping / Bloom 基线参数。
   - Renderer Debug 提供一键或固定说明的 reference 参数。
   - 对齐阶段先比较 `Bloom off` 和 `Bloom on` 两组截图。
   - 确认 ACES / gamma 输出没有重复 encode 或漏 encode。
6. IBL bake 耗时记录。
   - 打印或记录 environment bake 的关键阶段耗时。
   - 确认 BRDF LUT 已共享缓存。
   - 确认 viewport resize 不触发 environment 重新 bake。

建议调整文件：

```text
source/Engine/Function/Resource/
  model.cpp                              # tangent 保留 / fallback 生成策略

source/Engine/Function/Renderer/data/
  render_settings.h                      # 可选：reference preset 参数入口

source/Engine/Editor/Panels/
  renderer_debug_panel.cpp               # 材质输入 debug view / reference 参数入口

assets/shaders/Renderer/Vulkan/common/
  pbr_material.hlsli                     # BaseColor 等 material debug 输出支持
  pbr_ibl.hlsli                          # AO / specular occlusion 校准

docs/plans/Renderer/
  renderer_visual_effects_development_plan.md
```

暂时不做：

- 完整 GPU IBL baker。
- reflection probe / probe blending。
- RenderDoc 自动截图 diff。
- 新的 glTF importer 重写。
- Shadow PCF / CSM / PCSS。
- 改 Bloom 算法本身。

验收：

- DamagedHelmet 的 BaseColor / Normal / Metallic / Roughness / AO / Emissive debug view 与资源预期一致。
- 默认 reference preset 下，DamagedHelmet 不再明显发白、偏塑料或暗部漂浮。
- Bloom 关闭时仍能判断 IBL 本身是否合理；Bloom 开启只增强高亮，不掩盖材质问题。
- Tangent 修正后 normal map 高光方向稳定，没有明显 seam / handedness 错误。
- IBL bake 不因 viewport resize 重复触发；启动耗时来源有明确记录。
- 构建、shader compile、smoke / CTest 通过。

验证：

```powershell
cmake --build --preset msvc-vcpkg-debug
ctest --test-dir build/msvc-vcpkg -C Debug --output-on-failure
bin\msvc-vcpkg\Debug\Sandbox.exe  # hidden 3 秒启动 smoke
```

## 10. PR-R31：Shadow Quality Pass 1：PCF / Bias / Stability

执行状态：已完成。

目标：

- 把当前 directional shadow baseline 做稳。
- 提供可调 PCF 和 bias。
- 为 CSM / PCSS 打基础。

当前状态：

- Directional shadow map baseline 已存在。
- Forward shader 已有基础 shadow sampling / 3x3 PCF 方向。

建议工作：

1. PCF 参数化。
   - 支持 1x1 / 3x3 / 5x5。
   - 支持 kernel radius。
   - 后续再考虑 poisson disk。
2. Bias。
   - constant bias。
   - normal bias。
   - slope-scale bias。
3. Stability。
   - light view-projection texel snapping。
   - shadow distance 控制。
   - shadow map size 控制。
4. Debug。
   - RenderSettings 暴露 shadow enabled / strength / bias / distance / kernel size。
   - Renderer debug snapshot 显示 shadow target ready / size / format。
   - 后续 shadow map viewer 放到 PR-R34。

实现记录：

- 新增 backend-neutral `RenderShadowFilterMode` / `RenderShadowSettings`，随 `RenderSettings` 通过现有双缓冲 render data 链路传递。
- Renderer Debug 面板 `Effects` 分组增加 Shadow 调参：
  - enabled。
  - filter：Hard / PCF 3x3 / PCF 5x5。
  - strength。
  - constant bias / normal bias / slope bias。
  - filter radius。
  - distance。
  - shadow map：1024 / 2048 / 4096。
  - stabilize。
- `VulkanFrameLightingResource` 的 frame global uniform 增加 `shadow_quality_params`，向 shader 传递 filter mode、filter radius、normal bias 和 slope bias。
- 新增 `assets/shaders/Renderer/Vulkan/common/shadow_sampling.hlsli`：
  - 统一 shadow projection / depth compare / PCF sampling。
  - 支持 hard shadow、3x3 PCF 和 5x5 PCF。
  - 支持 receiver-side constant bias、normal bias 和 slope-scale bias。
- `vulkan_forward.hlsl` 去掉内联 shadow sampling，forward shader 只保留 pass IO、材质/光照 orchestration。
- Directional shadow light-space center 增加 texel snapping，减少 camera 小幅移动时的 shadow shimmer。
- `VulkanShadowMapTarget` 保持 Vulkan backend 内部资源，运行时根据 `RenderSettings.shadow.map_resolution` 在 1024 / 2048 / 4096 间重建并刷新 frame-global shadow descriptor。
- `RenderSettingsSmoke` 覆盖 shadow settings 的双缓冲传递。

暂时不做：

- CSM。
- PCSS。
- VSM / EVSM。
- point light shadow。
- transparent shadow。

验收：

- 默认场景阴影稳定。
- camera 移动时不明显游泳。
- bias 可调，能平衡 acne / peter-panning。
- PCF kernel 改变能影响边缘软硬。
- 构建和 smoke / CTest 通过。

验证：

```powershell
cmake --build --preset msvc-vcpkg-debug
ctest --test-dir build/msvc-vcpkg -C Debug --output-on-failure
bin\msvc-vcpkg\Debug\Sandbox.exe  # hidden 3 秒启动 smoke
```

结果：

- Debug 构建通过，Forward / Shadow HLSL 编译通过。
- CTest 18/18 通过，其中 `NexAur.RenderSettingsSmoke` 覆盖 Shadow settings。
- Sandbox 隐藏启动 3 秒 smoke 通过，未提前退出。

## 11. PR-R32：Cascaded Shadow Maps

执行状态：计划中。

目标：

- 支持大范围 directional light shadow。
- 提升第三人称 / 户外 demo 阴影质量。

建议工作：

1. Cascades。
   - 第一版 3 或 4 cascades。
   - 支持 split lambda。
   - 支持 max shadow distance。
2. Stable CSM。
   - 每个 cascade 做 texel snapping。
   - 减少 shimmer。
3. Shader。
   - 根据 view-space depth 选择 cascade。
   - 每个 cascade 单独 light VP。
   - 支持 cascade blending 可后续再做。
4. Resource。
   - shadow map array 或多个 shadow targets。
   - 第一版建议 array texture，更利于 shader 选择。
5. Debug。
   - cascade color overlay。
   - cascade split 显示。

暂时不做：

- PCSS per cascade。
- cascade blending 高级优化。
- shadow atlas。

验收：

- 近处阴影清晰，远处阴影覆盖范围更大。
- cascade 切换不明显跳变。
- Debug overlay 能显示 cascade 分区。
- 构建和 smoke / CTest 通过。

## 12. PR-R33：PCSS / Contact Hardening Shadow

执行状态：计划中。

目标：

- 支持接触硬、远处软的阴影观感。
- 为 directional light 提供 light angular size / source radius 参数。

建议工作：

1. Blocker search。
   - 在 shadow map 中搜索 blocker depth。
   - 控制 search radius。
2. Penumbra estimation。
   - 根据 receiver / blocker depth 估算 penumbra。
   - 与 light size 参数关联。
3. Variable PCF。
   - 根据 penumbra 调整 filter radius。
   - 注意采样数量和性能。
4. Debug / fallback。
   - 支持 PCF / PCSS 切换。
   - 性能较差时能退回 PCF。

建议顺序：

- 可以先在单张 shadow map 上做 PCSS prototype。
- 但正式整合建议在 CSM 稳定后做。

暂时不做：

- Area light 全套物理阴影。
- Ray traced shadow。
- Moment shadow map。

验收：

- 接触处阴影较硬。
- 离接触点越远越软。
- PCSS 关闭后回到 PCF。
- 性能有基础统计。
- 构建和 smoke / CTest 通过。

## 13. PR-R34：Renderer Effects Debug Views

执行状态：计划中。

目标：

- 给后续效果调试提供统一入口。
- 避免调 Bloom / Shadow / IBL 时只能靠猜。

建议支持：

- HDR scene color viewer。
- PostProcess output viewer。
- Shadow map viewer。
- Cascade debug colors。
- Bloom mip viewer。
- IBL irradiance / prefilter viewer。
- Exposure / tone mapping stats。

建议接入：

- 可以放入 `Renderer Debug` 面板，也可以后续拆成独立 `Renderer Effects` 面板。
- debug view 显示只读为主，调参走 RenderSettings。
- 不暴露 Vulkan handle。
- Debug view 数据通过后端无关 snapshot 或 renderer debug service 暴露。

验收：

- Debug view 不影响正常 render path。
- Editor 不 include `Renderer/Vulkan/*`。
- 构建和 smoke / CTest 通过。

## 14. PR-R35：Color Grading / FXAA / Polish Pass

执行状态：计划中。

目标：

- 在 HDR / ACES / Bloom / PBR / IBL 稳定后做轻量画面 polish。

建议内容：

- Color grading LUT。
- FXAA。
- Vignette。
- Film grain。
- Chromatic aberration 可选，默认关闭。

暂时不做：

- TAA。
- DLSS / FSR。
- Motion blur。
- Depth of field。

说明：

- FXAA 成本低，适合先接。
- TAA 对后续 SSR、PCSS 噪点和 temporal stability 很有价值，但需要 camera jitter、history buffer、motion vector，建议单独规划。

## 15. 其他可选画面效果建议

后续可以考虑：

1. SSAO / GTAO。
   - 对空间接触感提升明显。
   - 需要稳定 depth / normal 输入。
2. Height Fog / Volumetric Fog。
   - 对 demo 氛围提升大。
   - 可以先做 height fog，再做 volumetric。
3. Contact Shadow。
   - 可补充 CSM 近处接触细节。
4. Reflection Probe。
   - IBL 后自然延伸。
   - 第一版做 static probe。
5. GPU timestamp profiler。
   - Bloom、IBL、CSM、PCSS 后非常需要。
   - 可以接入 `Renderer Debug` 面板。
6. Transparent sorting / Weighted Blended OIT。
   - 当前透明如果只是基础链路，后续需要专项。
7. TAA。
   - 长期很重要。
   - 但需要 history、jitter、motion vectors，不建议过早做。

## 16. 完成标准

这一轮 Visual Effects 阶段达到以下状态即可认为完成：

- Renderer 拥有稳定 HDR scene color。
- ACES + exposure 可用。
- Bloom 基于 HDR，在 tone mapping 前合成。
- PBR 材质支持基础贴图全链路。
- IBL 可用，金属 / 粗糙材质能正确响应环境。
- Directional shadow 支持稳定 PCF。
- CSM 可覆盖较大场景。
- PCSS 可作为高质量 shadow mode。
- 关键效果有 debug view。
- Editor / Runtime / Scene 不直接依赖 Vulkan backend。
- 构建、shader compile、Sandbox smoke 和 CTest 稳定通过。

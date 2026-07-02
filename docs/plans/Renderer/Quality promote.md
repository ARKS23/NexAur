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

### 4.4 PR-R38：Rect Area Light / Cornell Lighting （已完成）

目标：

- 让矩形面光源成为正式光源类型，而不是靠 emissive 材质和点光源假装。
- 为 Cornell Box、室内光、灯板、窗光提供更真实的光照基础。
- 建立 area light 从 Scene 到 Renderer frontend 再到 Vulkan shader 的完整数据链路，为后续 LTC、rect shadow 和 emissive lighting 留出清晰扩展点。

推荐第一版定位：

- 新增正式 `RectLightComponent`，暂不做通用 `AreaLightComponent` 抽象，避免 sphere / disk / tube light 的参数提前污染第一版接口。
- CPU / RenderData / RenderFrame / Vulkan GPU buffer 打通 rect light 数据。
- Forward shader 先实现稳定、低成本的矩形直射光近似：diffuse 使用 solid-angle 近似，specular 使用 representative point + roughness widening。
- Cornell 顶部灯改为 rect light 参与直接光照；可见发光面仍使用 emissive mesh 表达灯板外观。
- R37 point light shadow 可在 Cornell 中保留为过渡 shadow proxy，但不应把 shadow proxy 逻辑塞进 `RectLightComponent`。
- 后续再升级 LTC area light、rect light soft shadow 或 emissive GI。

建议数据流：

```text
Scene Entity
  RectLightComponent
    -> Scene extraction / Renderer scene data
    -> RenderFrameRectLight
    -> VulkanFrameLightingResource rect light SSBO
    -> Forward shader rect light loop
```

建议组件数据：

```cpp
struct RectLightComponent {
    glm::vec3 color{1.0f};
    float intensity = 8.0f;
    glm::vec2 size{1.0f, 1.0f};
    float range = 8.0f;
    bool two_sided = false;
};
```

Renderer frame 中不要只传 transform matrix。建议在 CPU 侧直接整理出 shader 需要的世界空间基向量，减少 shader 每像素重复计算：

```cpp
struct RenderFrameRectLight {
    glm::vec3 position;
    glm::vec3 right;
    glm::vec3 up;
    glm::vec3 normal;
    glm::vec2 size;
    glm::vec3 color;
    float intensity;
    float range;
    bool two_sided;
};
```

Vulkan GPU buffer 建议使用 vec4 对齐结构，作为独立 SSBO，而不是塞进 point light buffer：

```cpp
struct VulkanGpuRectLight {
    glm::vec4 position_intensity;
    glm::vec4 right_width;
    glm::vec4 up_height;
    glm::vec4 normal_range;
    glm::vec4 color_flags;
};
```

建议代码组织：

- `scene_component.h` / serializer / Properties Panel：新增 `RectLightComponent`、Inspector 参数和场景序列化。
- `scene_v2.cpp` 或 renderer extraction：从 ECS 提取 rect light，应用 transform，生成 backend-neutral renderer 数据。
- `render_scene_frame.h` / `render_scene_frame_builder.cpp`：新增 `RenderFrameRectLight` 数组、数量上限和强度校准。
- `vulkan_frame_lighting_resource.h/.cpp`：新增 rect light SSBO、descriptor binding 和每帧上传。
- `vulkan_forward.hlsl`：新增 rect light buffer、rect light loop 和独立 helper 函数。
- `scene_test.cpp`：Cornell 顶部发光面旁新增正式 rect light；原 point light shadow 只作为 R37 过渡验证路径保留或降权。

核心算法第一版：

1. CPU 根据 entity transform 计算 rect light 的 `position / right / up / normal`，`right` 和 `up` 只表示方向，实际半宽半高来自 `size`。
2. shader 中把 shading point 投影到 rect light 局部平面，clamp 到矩形范围，得到最近代表点。
3. diffuse 使用矩形面积、距离平方和灯面朝向估算 solid angle，避免像 point light 一样无限小。
4. specular 使用视线反射方向与灯面求交，交点 clamp 到矩形范围；无有效交点时回退最近代表点。
5. 用 light size / distance 对 roughness 做保守扩宽，再复用现有 direct PBR BRDF。
6. 使用 range 做平滑衰减；不在 R38 中引入 rect shadow。

参考伪代码：

```hlsl
float3 to_surface = surface_position - light.position;
float x = clamp(dot(to_surface, light.right), -light.width * 0.5, light.width * 0.5);
float y = clamp(dot(to_surface, light.up), -light.height * 0.5, light.height * 0.5);
float3 closest = light.position + light.right * x + light.up * y;

float3 Lvec = closest - surface_position;
float distance2 = max(dot(Lvec, Lvec), 0.0001);
float3 L = normalize(Lvec);

float NoL = saturate(dot(normal, L));
float light_facing = light.two_sided
    ? abs(dot(light.normal, -L))
    : saturate(dot(light.normal, -L));

float area = light.width * light.height;
float solid_angle = min(area * light_facing / distance2, 6.2831853);
float distance = sqrt(distance2);
float attenuation = saturate(1.0 - distance / light.range);
attenuation *= attenuation;

float apparent_radius = 0.5 * sqrt(area / 3.1415926);
float roughness_widen = apparent_radius / max(distance, 0.001);
float rect_roughness = saturate(sqrt(roughness * roughness + roughness_widen * roughness_widen));

float3 radiance = light.color * light.intensity * solid_angle * attenuation;
lighting += EvaluateDirectPbr(material, normal, view, L, radiance, rect_roughness);
```

拆分建议：

- R38-A：`RectLightComponent`、序列化、Inspector、Cornell scene 数据。
- R38-B：Renderer frontend 数据链路和 frame builder 预算，例如 `max_rect_lights = 16`。
- R38-C：Vulkan rect light SSBO、descriptor layout、per-frame upload。
- R38-D：Forward shader rect light direct lighting helper，先不做 LTC。
- R38-E：Debug / Settings 可观测性，至少显示 rect light count、预算裁剪数量和 Cornell rect light 参数。
- R38-F：文档、smoke 测试和 Cornell 视觉检查。

范围约束：

- 第一版只做 direct lighting，不做 emissive GI。
- 第一版可以先不做真实软阴影，只保证能量和高光形态比 point light 更合理。
- LTC lookup table 后置到 R38.5，R38 第一版只保留数据和 shader helper 的升级接口。
- Rect light 与可见 mesh 不强绑定，避免材质系统和 light system 过早耦合。
- 不做 rect light shadow map / ray traced shadow；阴影仍由 R37 point shadow 或后续专门 PR 处理。
- 不做 clustered / forward+ light culling；rect light 数量第一版使用小预算和 frame builder 裁剪。
- 不改变 emissive material 的语义；emissive 仍是可见自发光，不自动成为 GI 光源。

验收标准：

- Cornell 顶部灯以矩形面积参与光照。
- 高 roughness 表面能看到更自然的宽高光。
- 面光源参数可在 Inspector / Render Settings 中调节。
- Scene serializer 能保存 / 读取 rect light 参数。
- Renderer Debug 能看到 rect light count、GPU buffer 是否有效和预算裁剪情况。
- 关闭 rect light 后画面回到 R37 baseline；保留的 point shadow proxy 不应成为唯一直接照明来源。
- Debug 构建通过，HLSL 编译通过；必要 smoke 覆盖 `SceneSerializerSmoke` 和 renderer frame builder / settings 传递。

### 4.5 PR-R38.5：LTC Rect Light Specular （已完成）

目标：

- 将 R38 中的 representative point specular 升级为更接近物理积分结果的矩形面光高光。
- 让低 roughness / metallic / grazing angle 下的 rect light highlight 呈现矩形面光形状，而不是退化成点光高光。
- 为后续高质量 area light diffuse、IES / textured area light、rect light shadow 提供稳定 shader 组织。

阶段定位：

- 这是 R38 的质量升级 PR，而不是新光源类型 PR；不新增组件语义，不改变 `RectLightComponent` 的基础数据契约。
- 如果项目节奏希望把它升格为 `PR-R39`，则原 `PR-R39：Color Grading + Anti-Aliasing Polish` 顺延到后续编号；从依赖关系看，LTC 应当排在 R38 之后、Color Grading 之前或之后都可独立推进。
- 第一版只做 rect light specular LTC；diffuse 可以继续沿用 R38 的 solid-angle 近似，避免一次性替换所有 area light 公式。

核心思路：

LTC（Linearly Transformed Cosines）的关键是：用一张按 `roughness / NoV` 查询的预积分 LUT，把 GGX specular lobe 近似成被线性变换后的 cosine lobe。shader 每像素不再对矩形面积采样多次，而是把矩形四个角变换到局部空间，然后对变换后的多边形做解析积分。

推荐数据和资源：

```text
RectLightComponent
  -> R38 已有 rect light GPU data
  -> Forward shader
      -> LTC LUT textures
      -> EvaluateRectLightDiffuseApprox()
      -> EvaluateRectLightSpecularLtc()
```

第一版可以先使用 shader 内置解析系数和 polygon integral，避免新增外部 LUT 资产依赖；如果后续需要更接近参考实现，再将系数采样层替换为离线 LUT 资源。

建议新增资源（外部 LUT 版本）：

- `assets/textures/Renderer/LTC/ltc_mat.dds`：`RGBA16F` 或 `RGBA32F`，保存 inverse LTC matrix 参数。
- `assets/textures/Renderer/LTC/ltc_amp.dds`：`RGBA16F` 或 `RGBA32F`，保存 amplitude / Fresnel scale-bias 参数。
- `VulkanLtcLutResource`：负责加载 LUT image、image view、sampler 和 descriptor 更新。
- `assets/shaders/Renderer/Vulkan/common/area_light_ltc.hlsli`：封装 LUT lookup、多边形裁剪、边积分和 specular evaluate。

Render settings 建议：

```cpp
struct RenderRectLightSettings {
    bool enabled = true;
    uint32_t max_lights = 16;
    bool ltc_specular_enabled = true;
    float specular_intensity_scale = 1.0f;
    bool debug_ltc_only = false;
};
```

如果 R38 已经引入 `RenderRectLightSettings`，R38.5 只扩展 LTC 相关字段；如果 R38 只靠默认预算，R38.5 可以顺手把 rect light settings 正式化。

Vulkan / descriptor 组织：

- Rect light SSBO 继续沿用 R38 的独立 buffer，不和 LUT 资源耦合。
- 如果采用外部 LUT，LTC LUT 作为 renderer 静态采样资源，可以挂在 frame-global descriptor set 后续 binding，或建立独立 lighting resource descriptor set。
- LUT sampler 使用 `linear + clamp`；LUT texture 必须按 linear float 读取，不走 sRGB。
- LUT 加载失败时 shader 或 C++ 应 fallback 到 shader 内置解析系数或 R38 representative point specular，避免资源缺失导致黑屏或 NaN。

shader 拆分建议：

```text
assets/shaders/Renderer/Vulkan/common/
  pbr_brdf.hlsli
  shadow_sampling.hlsli
  area_light_ltc.hlsli
```

`vulkan_forward.hlsl` 只保留 light loop：

```hlsl
float3 rect_lighting = 0.0;
rect_lighting += EvaluateRectLightDiffuseApprox(material, surface, rect_light);
rect_lighting += EvaluateRectLightSpecularLtc(material, surface, rect_light, g_ltc_mat, g_ltc_amp);
```

核心算法步骤：

1. 根据 `N` 和 `V` 构造 shading tangent frame，计算 `NoV`。
2. 使用 `roughness` 和 `NoV` 查询 LTC LUT，得到 inverse transform matrix 参数和 amplitude / Fresnel 参数。
3. 将 rect light 四个世界空间角点转换到 shading local frame。
4. 用 inverse LTC matrix 变换四边形，使 GGX specular 积分近似为 cosine lobe 对多边形的积分。
5. 对变换后的多边形做 horizon clipping，剔除 `z <= 0` 的部分。
6. 将剩余顶点归一化到单位球面，通过 edge integral 计算 form factor。
7. 将 form factor 乘以 light radiance、LUT amplitude、F0/Fresnel scale-bias，得到 specular contribution。

参考伪代码：

```hlsl
float3 EvaluateRectLightSpecularLtc(MaterialData material, SurfaceData surface, RectLightData light)
{
    float NoV = saturate(dot(surface.normal, surface.view));
    float roughness = max(material.roughness, 0.045);
    float2 uv = BuildLtcUv(roughness, NoV);

    float4 ltc_mat = g_ltc_mat.SampleLevel(g_ltc_sampler, uv, 0);
    float4 ltc_amp = g_ltc_amp.SampleLevel(g_ltc_sampler, uv, 0);

    float3x3 tangent_to_world = BuildTangentFrame(surface.normal, surface.view);
    float3x3 world_to_tangent = transpose(tangent_to_world);
    float3x3 Minv = BuildLtcInverseMatrix(ltc_mat);

    float3 corners[4] = BuildRectCorners(light);
    for (int i = 0; i < 4; ++i) {
        float3 local_corner = mul(world_to_tangent, corners[i] - surface.position);
        corners[i] = mul(Minv, local_corner);
    }

    LtcPolygon polygon = ClipPolygonToHorizon(corners);
    float form_factor = IntegratePolygonOnUnitSphere(polygon);

    float3 fresnel = material.f0 * ltc_amp.x + ltc_amp.y;
    float3 radiance = light.color * light.intensity;
    return radiance * fresnel * max(form_factor, 0.0);
}
```

实现拆分：

- R38.5-A：引入 LTC LUT 资产和 `VulkanLtcLutResource`，完成 resource lifetime / resize-independent descriptor 更新。
- R38.5-B：新增 `area_light_ltc.hlsli`，把 polygon clipping、edge integral、LUT decode 从 forward shader 中隔离。
- R38.5-C：Forward shader rect light specular 从 R38 近似切换到 LTC，并保留 fallback path。
- R38.5-D：Render Settings / Debug Panel 增加 `LTC Specular` 开关、LUT loaded 状态、rect light specular-only debug。
- R38.5-E：增加 roughness / metallic / grazing angle smoke 场景或在 Cornell scene 中加入测试材质面板。

范围约束：

- 不新增 area light shadow；rect light 阴影仍单独规划。
- 不做 textured area light / IES / barn door。
- 不做多重散射 GGX 能量补偿；如现有 PBR BRDF 已有能量补偿，R38.5 只负责不破坏它。
- 不把 LTC LUT 生成为运行时 compute pass；第一版使用离线 LUT 资产或静态内置表。
- 不要求完全替换 R38 diffuse 近似；specular 质量优先。

验收标准：

- 光滑或金属表面上的 rect light 高光能呈现矩形形状，随 roughness 增大自然变宽。
- grazing angle 下没有明显闪烁、NaN、黑斑或能量爆炸。
- 关闭 `ltc_specular_enabled` 后回到 R38 representative point specular。
- LUT 缺失或加载失败时 renderer 仍能启动，并在 Debug Panel 明确显示 fallback。
- Debug 构建通过，HLSL 编译通过；必要 smoke 覆盖 LUT resource、settings 传递和 Sandbox 短启动。

### 4.6 PR-R38.6：Rect Light Shadow / Area Shadow Baseline （已完成）

目标：

- 去掉 Cornell 中依赖低强度 point light 作为 shadow proxy 的过渡方案，让顶部 rect light 自己提供可解释的投影。
- 为矩形面光源建立第一版 shadow pipeline：稳定、可调试、可关闭，优先服务 Cornell 和少量室内灯板。
- 让 R38 rect direct lighting 与 R38.5 LTC specular 都能共享同一份 rect shadow visibility。

阶段定位：

- 这是 R38 / R38.5 之后的独立 PR，不和 LTC 同时做。
- 第一版目标不是完美软阴影，而是先让 rect light 有自己的阴影所有权。
- 完成后 Cornell Box 可以正式移除顶部 point light shadow proxy，只保留 `CornellBox RectLight` 作为主光源。

推荐第一版方案：

- 新增 `RectLightComponent::cast_shadow`、`shadow_strength`，并复用现有 `range` 作为第一版 shadow depth range，继续保持组件语义简单。
- Frame builder 按 `RenderRectShadowSettings::max_shadowed_lights` 给 rect light 分配 shadow slot。
- Vulkan 复用 `VulkanShadowMapTarget` 创建 rect shadow 2D depth array，每个 shadowed rect light 占 1 个 layer。
- 新增 `RectShadowMap` graph pass，复用现有 depth-only mesh draw 思路，从 rect light 中心沿 light normal 渲染正交 shadow map。
- Forward shader 在 rect light direct lighting 前采样 rect shadow visibility，第一版使用 hard/小核 PCF。
- Rect shadow 默认只在 Cornell 或显式 `cast_shadow` 的 rect light 上启用，不自动覆盖所有 area lights。

为什么第一版不用 cube shadow map：

- Rect light 有明确灯面方向和覆盖范围，最自然的第一版 shadow projection 是沿灯面 normal 的 orthographic map。
- cube shadow map 适合 point light 的全向投影；rect light 用 cube 会浪费 6 面，并且和矩形灯面形状无关。
- 真正高质量 rect soft shadow 后续需要面积采样、PCSS/contact hardening 或 ray tracing；第一版先建立可调试的 shadow ownership。

建议数据流：

```text
RectLightComponent(cast_shadow)
  -> RendererRectLightData(shadow intent)
  -> RenderFrameRectLight(shadow slot)
  -> RenderRectShadowFrame(light view-projection)
  -> VulkanShadowMapTarget(rect depth array)
  -> Forward shader rect shadow sampling
```

建议新增设置：

```cpp
struct RenderRectShadowSettings {
    bool enabled = true;
    uint32_t max_shadowed_lights = 1;
    uint32_t map_resolution = 1024;
    float strength = 0.85f;
    float constant_bias = 0.01f;
    float normal_bias = 0.02f;
    float filter_radius = 1.0f;
    float projection_margin = 0.35f;
};
```

核心算法第一版：

1. CPU 为每个 shadowed rect light 计算 light view matrix：eye 在 rect center，forward 为 `normal`，up 使用 rect `up`。
2. 正交投影范围来自 rect `size` 加 `projection_margin`，深度范围来自 `shadow_range`。
3. Shadow pass 将 opaque geometry 渲染到对应 depth layer。
4. Forward shader 对当前 fragment 做 rect light-space projection，得到 `uv/depth`。
5. 超出 shadow frustum 的 fragment 默认可见；落在 frustum 内则按 depth compare 得到 visibility。
6. `rect_shadow_visibility` 乘到 rect light diffuse/specular contribution 上。
7. 第一版 PCF tap 数保持小，避免 Cornell 之外的默认性能成本不可控。

建议代码组织：

- `render_shadow_cascade.h` 或独立 `render_rect_shadow.h`：新增 `RenderRectShadowFrame`。
- `VulkanShadowMapTarget`：复用现有 depth array、sampler、resize / resolution rebuild 能力，作为 rect shadow target。
- `VulkanRendererSystem::buildRectShadowFrame()`：根据 draw list 和 settings 生成 shadow matrices。
- `VulkanFrameLightingResource`：上传 rect shadow view-projection、bias、filter、slot 信息。
- `assets/shaders/Renderer/Vulkan/common/shadow_sampling.hlsli`：新增 rect shadow sampling helper。
- `vulkan_forward.hlsl`：rect light loop 中调用 rect shadow visibility。
- Render Settings / Renderer Debug：暴露 rect shadow enable、budget、resolution、bias、filter radius、shadowed rect count、debug layer。

范围约束：

- 不做 area-light physically correct soft shadow。
- 不做 VSM / EVSM / moment shadow。
- 不做 ray tracing shadow。
- 不做 textured rect light shadow 或透明物体阴影。
- 不做多 rect light 自动 culling；第一版预算建议 1-2 个。
- 不要求 spot light shadow 同步实现。

验收标准：

- Cornell 顶部 `RectLight` 开启 shadow 后，block 能在地面和墙体产生稳定投影。
- 移除或关闭 Cornell point light 后，主要直接照明和局部投影仍来自 rect light。
- Rect shadow debug view 能查看当前 shadow layer；Renderer Debug 显示 request / shadowed / clipped 数量。
- 关闭 rect shadow 后回到 R38 / R38.5 rect lighting baseline。
- Debug 构建通过，HLSL 编译通过；必要 smoke 覆盖 rect shadow settings、shadow slot 分配和 Sandbox 短启动。

### 4.7 PR-R38.7：Local Shadow Quality Pass / Cube Shadow Polish （已完成）

目标：

- 在 R37/R38.6 的 shadow pipeline 已经可用后，集中修正局部光阴影的质量问题，而不是继续叠新效果。
- 让 point light cube shadow map 在室内、Cornell 和小范围局部光测试中更稳定，减少 seam、acne、peter-panning 和远近距离抖动。
- 统一 point / rect shadow 的调试口径、bias 参数习惯和 Render Settings 命名，为后续 soft shadow 做干净基线。

阶段定位：

- 这是 R38.6 之后的质量收口 PR，优先完善已有 cube shadow map，不改变 `PointLightComponent` 的基础语义。
- 不和 rect soft shadow 混做；R38.7 关注 hard/PCF shadow 的稳定性，R38.8 再处理 penumbra。
- 如果 R38.6 暴露出 bias / sampler / debug view 的重复逻辑，本 PR 负责抽出共用 helper。

主要工作：

- Cube shadow face selection / seam polish：检查 face basis、depth compare、边界采样和 normal bias，减少立方体面交界处断裂。
- Point shadow PCF polish：支持小核可调 PCF、filter radius、tap pattern 稳定化，避免默认成本失控。
- Bias 统一：整理 constant bias、normal bias、slope bias 的命名和应用位置，保证 point / rect shadow 行为可解释。
- Shadow budget debug：Renderer Debug 统一显示 requested / shadowed / clipped、resolution、filter radius、active debug layer。
- Shadow sampling helper：把 point / rect 的 compare、PCF、visibility saturate 放进 `common/shadow_sampling.hlsli`，减少 forward shader 分支膨胀。
- Cornell 验证：点光 shadow proxy 如果仍存在，应作为对照路径，而不是主光源依赖。

建议数据和代码组织：

- `RenderPointShadowSettings` 保持 point shadow 专属参数；新增共用命名时避免引入过宽的 `RenderShadowSettings` 大对象。
- `VulkanFrameLightingResource` 只上传 shader 真正需要的 shadow 参数，debug 文本从 CPU stats 侧读取。
- `shadow_sampling.hlsli` 提供小函数：`SampleShadowMapPcf`、`ComputePointShadowFace`、`ApplyShadowBias`、`ClampShadowVisibility`。
- `RendererDebugEffectsStats` 增加 point / rect shadow quality 字段时保持只读观测，不反向驱动 renderer 状态。

范围约束：

- 不做 rect soft shadow / PCSS。
- 不做 VSM / EVSM / moment shadow。
- 不做 clustered / forward+ shadow culling。
- 不做 spot light shadow；spot shadow 仍可留在后置能力池。
- 不改变 R38.6 rect shadow 的基本投影模型。

验收标准：

- Point light cube shadow 在 Cornell 或局部灯测试中 face seam 明显减少。
- Bias 调节能在 acne 与 peter-panning 之间形成可控平衡。
- 小核 PCF 开启后阴影边缘更稳定，关闭后可回到 hard shadow baseline。
- Renderer Debug 能同时观察 point / rect shadow 的预算和实际启用数量。
- Debug 构建通过，HLSL 编译通过；必要 smoke 覆盖 shadow settings 和 Sandbox 短启动。

### 4.8 PR-R38.8：Rect Soft Shadow / PCSS Baseline （已完成）

目标：

- 在 R38.6 rect hard shadow 可用、R38.7 shadow 质量基线稳定之后，为矩形面光增加第一版可解释的软阴影。
- 让阴影半影宽度随 light size、receiver/blocker 距离变化，Cornell 顶灯的投影不再像点光或方向光一样硬。
- 保持性能和复杂度可控，优先服务 1-2 个显式开启 shadow 的 rect light。

阶段定位：

- 这是 area light 阴影质量升级，不负责建立 shadow pipeline；pipeline ownership 已由 R38.6 完成。
- 第一版建议采用 PCSS-style approximation，不追求路径追踪级物理正确。
- 软阴影只影响 rect light shadow visibility，不改变 R38.5 LTC specular 的基础求值接口。

核心算法第一版：

1. 使用 R38.6 的 rect orthographic shadow map 和 light-space projection。
2. 对 receiver 周围做 blocker search，得到平均 blocker depth 和 blocker count。
3. 根据 `penumbra = (receiverDepth - blockerDepth) / blockerDepth * lightRadiusInShadowSpace` 估算半影半径。
4. 用半影半径驱动 PCF filter radius，得到 contact hardening 效果。
5. blocker 不存在时 visibility 为 1；半影半径做上下限 clamp，避免远距离过糊或 tap 数爆炸。
6. `shadow_strength` 继续控制最终 visibility 混合，保证美术可调。

建议新增设置：

```cpp
struct RenderRectShadowSettings {
    bool soft_shadow_enabled = true;
    float pcss_light_radius = 0.75f;
    float pcss_search_radius = 3.0f;
    float pcss_min_filter_radius = 0.5f;
    float pcss_max_filter_radius = 5.0f;
    uint32_t pcss_blocker_taps = 8;
    uint32_t pcss_filter_taps = 16;
};
```

建议代码组织：

- 复用 R38.6 的 `VulkanShadowMapTarget` rect depth array 和 rect shadow matrices，不新增第二套 shadow target。
- `shadow_sampling.hlsli` 增加 rect PCSS helper：`FindRectShadowBlocker`、`EstimateRectPenumbra`、`SampleRectShadowPcss`。
- `RenderRectShadowSettings` 可以直接包含 soft shadow 参数，或拆成 `RenderRectSoftShadowSettings` 子结构，避免污染 point shadow。
- Render Settings 面板用 `Soft Shadow` 开关控制 PCSS，关闭时回退 R38.6 hard/PCF path。
- Renderer Debug 显示 soft shadow enabled、blocker taps、pcf taps、effective max radius。

范围约束：

- 不做 ray traced soft shadow。
- 不做 stochastic shadow denoise / temporal accumulation。
- 不做透明物体阴影。
- 不做多光源复杂 culling；默认仍限制 shadowed rect light 数量。
- 不要求和 LTC specular 做 shadowed specular 的多重散射能量补偿。

验收标准：

- Cornell 顶部 rect light 的 block 投影近处更硬、远处更软，且不会出现明显闪烁或黑斑。
- `Soft Shadow` 关闭后回到 R38.6 hard/PCF baseline。
- 调整 rect light size 或 softness scale 时，半影变化方向符合直觉。
- 默认参数下 Sandbox 启动和 Cornell 测试成本可控。
- Debug 构建通过，HLSL 编译通过；必要 smoke 覆盖 settings 传递和 Sandbox 短启动。

### 4.9 PR-R39：Color Grading + Anti-Aliasing Polish

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

### 4.10 PR-R40：Reflection Probe / SSR v1

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

### 4.11 PR-R41：Material / Texture Sampling Polish

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

### 4.12 PR-R42：Atmosphere / Fog / Depth Polish

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
4. 用 LTC 升级 rect light specular，让灯板在光滑/金属测试面上呈现正确矩形高光。
5. 给 rect light 增加自己的 shadow map，移除 point light shadow proxy。
6. 对 point cube shadow / rect shadow 做质量收口，统一 bias、PCF 和 debug 口径。
7. 给 rect light 增加 soft shadow，让顶部面光投影具备合理半影。
8. 增加 contact shadow 或 SSAO，让 block 落地。
9. 锁定 exposure / bloom 默认值，避免靠过曝制造亮度。
10. 保留 Cornell debug view：direct、indirect、AO、shadow、final lit。

## 6. 推荐优先级

最推荐的实际开发顺序分为两个阶段。

阶段一：必须收口，优先做小而稳的能力。

```text
R35.5 Lighting Calibration / Render Presets
  -> R36 SSAO / GTAO Baseline
  -> R37 Point Light Cube Shadow Map + Contact Shadow
  -> R38 Rect Area Light / Cornell Lighting
  -> R38.5 LTC Rect Light Specular
  -> R38.6 Rect Light Shadow / Area Shadow Baseline
```

阶段一增强：在 rect shadow ownership 建立后，优先收口局部阴影质量。

```text
R38.7 Local Shadow Quality Pass / Cube Shadow Polish
  -> R38.8 Rect Soft Shadow / PCSS Baseline
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

Reflection、LTC Area Light、TAA、SSR 会进一步提高上限，但不建议在没有光照基线和 debug view 的情况下提前堆叠。

## 7. 第一阶段完成定义

第一阶段不是追求所有效果都高级，而是让 NexAur 的画面具备稳定、可解释、可继续扩展的基础质感。

第一阶段完成时应满足：

- Outdoor / Studio / Cornell / AssetPreview preset 可切换。
- Cornell 场景不会被全局 IBL 和方向光误照亮。
- 物体和地面/墙体之间有 AO 或 contact shadow。
- Point light cube shadow map 能服务 Cornell 或室内测试场景。
- Rect light 能作为正式 light 数据流进入 renderer。
- 如果纳入 R38.5，LTC rect light specular 能在关闭时回退到 R38 近似路径。
- 如果纳入 R38.6，Cornell 可以不依赖 point light shadow proxy 仍保持顶部灯投影。
- 如果纳入 R38.7，point cube shadow 的 seam、bias 和 PCF 行为应稳定可调。
- 如果纳入 R38.8，rect light soft shadow 应能在关闭时回退到 R38.6 hard/PCF path。
- Renderer Debug / Render Settings 能观察和调节上述效果。
- 默认 Sandbox 不因为新增效果明显变慢或变暗。
- 所有效果关闭后能回到当前 baseline。

## 8. 后置能力池

以下能力有价值，但不进入第一阶段：

- 多局部光源 clustered / forward+ shadow culling。
- Spot light shadow。
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

### 2026-07-02：PR-R38 Rect Area Light / Cornell Lighting

完成内容：
- 新增正式 `RectLightComponent`，支持 `color`、`intensity`、`size`、`range`、`two_sided`，并接入 Scene serializer、Properties Panel、Scene Hierarchy 创建/复制。
- Scene extraction 按约定将本地 X/Z 作为灯面宽/高方向、本地 -Y 作为发光法线，输出 backend-neutral `RendererRectLightData`，并提供矩形灯 debug gizmo。
- `RenderDataPacket`、`RenderSceneFrame`、`RenderSceneFrameBuilder`、`VulkanDrawList` 打通 rect light 数据链路，新增 `RenderRectLightSettings` 和 lighting preset 的 `rect_light_intensity_scale`。
- Vulkan frame-global descriptor 新增 rect light SSBO binding；`VulkanFrameLightingResource` 上传 vec4 对齐的 `VulkanGpuRectLight`。
- Forward shader 新增 rect light direct lighting loop：第一版使用矩形最近代表点、solid-angle 近似和基于面积的 roughness widening，复用现有 PBR direct BRDF。
- Renderer Debug 显示 rect light count 和预算裁剪数量；Render Settings 暴露 rect light enable / max lights。
- Cornell Box 新增 `CornellBox RectLight`，可见 emissive panel 仍只负责灯板外观；原 point light 降为低强度 shadow proxy，服务 R37 cube shadow 过渡路径。

范围说明：
- 本 PR 只交付 rect area light direct lighting，不做 emissive GI、LTC、textured area light、rect light shadow 或 clustered / forward+ culling。
- 第一版 specular 是近似代表点方案；更高质量的矩形高光积分留给 R38.5 LTC Rect Light Specular。
- Rect light 与可见 mesh 不强绑定，避免材质系统和 light system 过早耦合。

验证：
- `cmake --build build\msvc-vcpkg --config Debug --target Sandbox`
- `ctest --test-dir build\msvc-vcpkg -C Debug -R "NexAur\.(SceneSerializerSmoke|RenderSettingsSmoke)" --output-on-failure`
- `bin\msvc-vcpkg\Debug\Sandbox.exe` 短启动 smoke

### 2026-07-02：PR-R38.5 LTC Rect Light Specular

完成内容：
- `RenderRectLightSettings` 新增 `ltc_specular_enabled`、`specular_intensity_scale`、`debug_ltc_only`，Render Settings 面板可切换 LTC specular、调节 specular scale，并支持 LTC-only debug。
- Renderer Debug Effects 区显示 rect LTC specular 开关、LTC-only 状态和 specular scale。
- Frame lighting UBO 新增 rect LTC 参数，Forward shader 可在 R38 fallback direct specular 与 LTC specular 之间切换。
- 新增 `assets/shaders/Renderer/Vulkan/common/area_light_ltc.hlsli`，封装 rect light decode、diffuse 近似、R38 fallback direct、LTC-style polygon integral 和 specular evaluate。
- Forward shader 的 rect light loop 改为：LTC 关闭时回退 R38 representative direct；LTC 开启时使用 solid-angle diffuse + LTC rect specular；debug-only 时只显示 LTC specular contribution。

范围说明：
- 本 PR 交付 shader 内置解析系数版本的 LTC-style rect specular，不新增外部 LUT 资产和 `VulkanLtcLutResource`。
- 外部 `ltc_mat / ltc_amp` LUT 仍保留为后续 refinement；如果接入，应替换 `area_light_ltc.hlsli` 的系数采样层，而不是改变 `RectLightComponent` 语义。
- 本 PR 不做 rect light shadow、textured area light、IES、barn door 或多重散射 GGX 能量补偿。

验证：
- `cmake --build build\msvc-vcpkg --config Debug --target Sandbox`
- `ctest --test-dir build\msvc-vcpkg -C Debug -R "NexAur\.(RenderSettingsSmoke|SceneSerializerSmoke)" --output-on-failure`
- `bin\msvc-vcpkg\Debug\Sandbox.exe` 短启动 smoke

### 2026-07-02：PR-R38.6 Rect Light Shadow / Area Shadow Baseline

完成内容：
- `RectLightComponent` / serializer / Properties Panel 新增 rect light shadow intent：`cast_shadow`、`shadow_strength`，并复用 rect light `range` 作为第一版正交 shadow depth range。
- 新增 `RenderRectShadowSettings`，Render Settings 面板暴露 rect shadow enable、budget、resolution、strength、bias、normal bias、filter radius、projection margin。
- `RenderSceneFrameBuilder` 为 rect light 记录 `shadow_requested`、`cast_shadow`、`shadow_slot`，按 `max_shadowed_lights` 分配 shadow layer。
- Vulkan 侧复用 `VulkanShadowMapTarget` 创建 rect shadow depth array；新增 `RenderRectShadowFrame`、rect shadow view-projection 上传和 `RectShadowMap` graph pass。
- Forward shader 新增 rect shadow visibility，R38 fallback direct 与 R38.5 LTC diffuse/specular 都共享同一份 rect shadow 遮挡。
- PostProcess / Render Settings / Renderer Debug 支持 `Rect Shadow Map` debug view、rect shadow layer、rect shadow target 和 request / shadowed 统计。
- Cornell Box 顶部 `CornellBox RectLight` 默认开启 shadow，并移除低强度 point light shadow proxy。

范围说明：
- 本 PR 交付 rect light hard / 小核 PCF shadow baseline，不做 physically correct rect soft shadow。
- 第一版每个 shadowed rect light 占一个 2D depth layer，投影为沿灯面 normal 的 orthographic map。
- 不做 VSM / EVSM / moment shadow、ray tracing shadow、透明物体阴影、spot light shadow 或 clustered / forward+ shadow culling。
- Rect soft shadow / PCSS 留给 R38.8；point cube shadow seam / bias / PCF 收口留给 R38.7。

验证：
- `cmake --build build\msvc-vcpkg --config Debug --target Sandbox`
- `ctest --test-dir build\msvc-vcpkg -C Debug -R "NexAur\.(RenderSettingsSmoke|SceneSerializerSmoke)" --output-on-failure`
- `bin\msvc-vcpkg\Debug\Sandbox.exe` 短启动 smoke

### 2026-07-02：PR-R38.7 Local Shadow Quality Pass / Cube Shadow Polish

完成内容：
- Point light cube shadow face projection 改为 90.5 度轻微 overlap，减少 cube face 边界处的深度覆盖断裂。
- Point shadow PCF 改为 seam-aware sampling：每个 PCF tap 在 world-space 小平面偏移后重新选择 cube face 和 layer，再做 depth compare，避免固定在单个 face 内偏移导致的边缘漏光。
- `shadow_sampling.hlsli` 新增 local shadow 通用 helper：`NxComputeLocalShadowBias`、`NxApplyLocalShadowVisibility`，point / rect shadow 共用同一套 bias 和 visibility clamp 语义。
- Rect shadow sampling 复用同一 local bias/visibility helper，保持 R38.6 hard / 小核 PCF baseline 不变。
- Renderer Debug 增加 point / rect shadow clipped 数量、budget、map resolution、strength、bias、normal bias、filter radius、rect projection margin 和 contact shadow 状态，方便调试 local shadow 质量。

范围说明：
- 本 PR 不改变 `PointLightComponent` / `RectLightComponent` 的基础语义，不新增 soft shadow 设置。
- 本 PR 不做 rect PCSS、VSM / EVSM / moment shadow、clustered / forward+ shadow culling 或 spot light shadow。
- Point shadow PCF 仍保持 3x3 小核；更高阶采样模式、face seam 深度扩边或 cube border copy 可作为后续进一步优化。

验证：
- `cmake --build build\msvc-vcpkg --config Debug --target Sandbox`
- `ctest --test-dir build\msvc-vcpkg -C Debug -R "NexAur\.(RenderSettingsSmoke|SceneSerializerSmoke)" --output-on-failure`
- `bin\msvc-vcpkg\Debug\Sandbox.exe` 短启动 smoke

### 2026-07-02：PR-R38.8 Rect Soft Shadow / PCSS Baseline

完成内容：
- `RenderRectShadowSettings` 新增 rect 专属 soft shadow / PCSS 参数：`soft_shadow_enabled`、light/search/min/max radius、blocker/filter tap 数，默认保持 1 个 shadowed rect light 的低成本路径。
- Frame globals 新增 `rect_shadow_pcss_params` 和 `rect_shadow_pcss_quality_params`，只上传 shader 需要的 soft-shadow 参数，不新增第二套 shadow target。
- `shadow_sampling.hlsli` 为 rect shadow 增加 PCSS-style helper：中心 sample + Poisson blocker search、平均 blocker depth、light-size 参与的 penumbra radius 估计和 Poisson PCF。
- `Soft Shadow` 关闭时保留 R38.6 hard / 小核 PCF fallback；开启时只影响 rect light shadow visibility，R38.5 LTC diffuse/specular 继续共享同一份 visibility。
- Render Settings 面板暴露 rect PCSS 参数；Renderer Debug 显示 soft shadow 状态、blocker/filter taps 和半影半径参数。
- `RenderSettingsSmoke` 覆盖新增 rect soft-shadow 参数的 runtime settings 传递。

范围说明：
- 本 PR 不改变 `RectLightComponent` 语义，不新增 area light shadow target，也不引入 ray traced shadow、VSM / EVSM / moment shadow 或 temporal denoise。
- 第一版 PCSS 是可解释的近似：使用现有 rect orthographic shadow map 的深度差估计半影，不追求路径追踪级物理正确。
- 多 rect light 仍受 `max_shadowed_lights` 预算约束；tap 数默认限制在 16 以内。

验证：
- `cmake --build build\msvc-vcpkg --config Debug --target Sandbox`
- `ctest --test-dir build\msvc-vcpkg -C Debug -R "NexAur\.RenderSettingsSmoke" --output-on-failure`
- `bin\msvc-vcpkg\Debug\Sandbox.exe` 短启动 smoke

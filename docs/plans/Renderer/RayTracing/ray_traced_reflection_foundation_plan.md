# NexAur RT-10 Ray-Traced Reflection Foundation Development Plan

日期：2026-08-17

状态：RT-10.0 已完成；下一工作包为 RT-10.1

## 1. 文档目的

本文档将 `RT-10：Ray-Traced Reflection Foundation` 拆分为可独立实现、验证和回退的工作包，定义 NexAur 第一版 Ray-Traced Reflection 的数据契约、Feature 所有权、RenderGraph 顺序、资源生命周期、fallback、诊断和测试标准。

RT-10 的目标不是直接实现完整 Path Tracer，也不是把 reflection query 塞入当前 Forward 或 PostProcess shader。它要建立一条可维护的单次反射路径：

```text
Forward Surface Data
  -> SSR
  -> Ray Query Reflection for SSR miss / off-screen region
  -> Temporal / Spatial Denoise
  -> SSR / RT / Local Probe / Global IBL Composite
  -> Bloom / PostProcess
```

第一版只覆盖 static opaque triangle mesh、单次 reflection ray 和简化命中点着色。RT Pipeline、SBT、递归反射、透明材质和生产级 Path Tracing 不属于本阶段。

## 2. 核心结论

### 2.1 RT-10 不能作为单个实现提交

Shadow 和 AO 只需要知道 `hit / miss`。Reflection 在命中后还必须回答：

```text
instanceCustomIndex
  -> instance record
  -> geometry record
  -> primitive index
  -> vertex / index fetch
  -> barycentric interpolation
  -> material record
  -> texture sampling
  -> reflected radiance
```

当前 TLAS 的 `instanceCustomIndex` 只是当帧有效实例的紧凑顺序编号，没有对应 GPU object table。当前材质也仍按 draw call 绑定 descriptor set。直接实现 reflection shader 会导致 CPU draw order、TLAS filtering、GPU material lookup 三套索引彼此漂移，因此必须先完成 scene shading contract。

### 2.2 第一版继续使用 Ray Query

RT-10 继续使用 `VK_KHR_ray_query`，不引入 `VK_KHR_ray_tracing_pipeline`、Shader Binding Table 或 closest-hit shader。Reflection trace 推荐运行在 compute shader 中：

- 可以自然写入 storage image。
- 便于半分辨率调度、early-out 和后续 denoiser workgroup 优化。
- 与 RT-11 Full RT Pipeline 保持清晰边界。

这要求补充 compute pipeline 和 RenderGraph storage access，但不要求重写现有 graphics pipeline。

### 2.3 RT Reflection 必须是独立 Feature

目标所有权：

```text
VulkanRayTracedReflectionFeature
  owns trace pipeline / descriptors
  owns raw reflection target
  owns hit distance / confidence
  owns temporal history / moments
  owns denoiser passes
  exposes graph resources and composite input

VulkanReflectionCompositeFeature
  owns SSR / RT / fallback selection
  owns composited HDR scene target
  does not own trace or history resources
```

禁止把 RT reflection texture、history 和 scene table bindings继续追加到当前 PostProcess descriptor set。PostProcess 只消费已经完成 reflection composite 的 HDR scene color。

### 2.4 Fallback 必须保持稳定

最终优先级：

```text
SSR valid hit
  -> denoised RT reflection
  -> local Reflection Probe specular
  -> global IBL specular
```

任意 RT capability、TLAS、scene table、pipeline、history 或 output 失败都不得阻止 Raster renderer 启动。RT 不可用时继续使用：

```text
SSR -> Local Probe -> Global IBL
```

## 3. 当前架构基线

### 3.1 已具备能力

- RT-00 至 RT-09 已完成 Ray Query capability、device-address buffer、BLAS、TLAS、RenderGraph AS synchronization、descriptor、lifetime、profiling、Directional Ray Query Shadow 和 Optional RTAO。
- `VulkanMeshResource` 已提供 vertex / index buffer device address、vertex count、index count、stable mesh identity 和 generation。
- `VulkanDrawList` 已提供 opaque draw item 的 mesh、material、world transform 和 entity ID。
- TLAS 已按当帧有效 opaque static mesh 构建，并支持 Build / Update / Reuse。
- SSR 已提供 raw reflection、hit confidence 和 surface reflection mask。
- Reflection Probe 和 Global IBL 已构成稳定 fallback。
- RR-11 / RR-12 已提供 deferred destruction 和 FrameContext 生命周期。

### 3.2 当前硬缺口

| 领域 | 当前状态 | RT Reflection 阻塞 |
|---|---|---|
| Instance lookup | `instanceCustomIndex` 是临时紧凑序号 | 命中后无法查找 geometry / material |
| Geometry lookup | mesh 有 device address，但没有 GPU geometry table | 无法按 primitive index 读取三角形 |
| Material lookup | 每 draw 一个 material descriptor set | 命中 shader 无法随机访问材质 |
| Texture lookup | 没有 descriptor-indexed texture table | 无法按 hit material 采样纹理 |
| Surface attributes | Forward 只输出 HDR color + depth，SSR 法线由 depth 重建 | 缺 material normal、roughness 和稳定 reflection mask |
| Fallback specular | Probe / IBL specular 已混入 final lit color | 无法正确替换 fallback specular，容易重复计算 |
| Motion vector | 不存在 | 无法稳定 temporal reprojection |
| History contract | 不存在 | Resize、camera cut、scene reload 后容易读取 stale history |
| Compute pipeline | Pipeline cache 只支持 graphics | 不支持正式 compute trace / denoiser dispatch |
| Graph storage image | Image usage 只有 attachment / fragment read / transfer | 无法表达 compute storage read / write hazard |

### 3.3 当前可复用但不能直接扩张的模块

- `VulkanSsrFeature` 可提供 SSR raw / hit mask，但不应拥有 RT output 或 temporal history。
- `VulkanPostProcessFeature` 可继续 tone mapping、AO、Bloom 和 final debug routing，但不应成为 reflection composite owner。
- `VulkanFrameContext` 可承载 per-frame descriptor 和 timing；temporal history 本身是跨 frame 顺序资源，不应简单复制为互不关联的 frame-slot resource。
- `VulkanTlasManager` 可继续拥有 TLAS build，但 instance table 和 TLAS instance 必须来自同一份 canonical accepted-instance list。

## 4. 范围与非目标

### 4.1 第一版范围

- Static opaque triangle meshes。
- 32-bit index buffer 和当前 `NexAur::Vertex` layout。
- 每像素最多一条 primary reflection ray。
- 默认 half-resolution，可切换 full-resolution。
- Material factors、base color、normal、metallic / roughness 和 emissive texture。
- 单次 hit shading；miss 返回 0 confidence，由 Probe / IBL fallback 接管。
- SSR-first hybrid composite。
- Motion-vector reprojection、history rejection、spatial / temporal denoiser。
- Capability、fallback、debug views 和 GPU timing。

### 4.2 明确非目标

- Transparent、transmission、refraction 和 colored shadow。
- Alpha mask candidate intersection；第一版 alpha mask 不进入 RT reflection TLAS。
- Skinned、morph target 和其他 deforming geometry。
- Recursive reflection、reflection of reflection 和多 bounce GI。
- `VK_KHR_ray_tracing_pipeline`、raygen / miss / closest-hit、SBT。
- Async compute queue；第一版所有 graph pass 继续在 graphics queue 上按顺序执行。
- TAA；本阶段建立的 motion vector / history contract应可复用，但不顺带实现 TAA。
- Production-grade cross-vendor denoiser或第三方 denoiser SDK 集成。

## 5. 目标帧流程

```text
PrepareFrame
  -> build canonical accepted RT instance records
  -> prepare BLAS / TLAS
  -> upload instance / geometry / material tables
  -> update per-frame RT shading descriptor

ForwardScene MRT
  -> HDR scene color
  -> reflection surface data
  -> fallback specular
  -> motion vector

SSAO / RTAO

SSR
  -> SSR raw reflection
  -> SSR confidence

RayQueryReflectionTrace (compute)
  reads depth / surface / SSR confidence / TLAS / scene tables
  writes RT raw radiance / confidence / hit distance

ReflectionTemporalResolve (compute)
  reads raw / velocity / depth / normal / previous history
  writes temporal radiance / moments / history length

ReflectionSpatialFilter (compute)
  reads temporal output / depth / normal / roughness / hit distance
  writes denoised RT reflection

ReflectionComposite
  reads scene color / fallback specular / SSR / denoised RT
  writes composited HDR scene color

DebugDraw -> Bloom -> PostProcess -> SMAA -> UI
```

Reflection Probe capture 不进入这条流程。Probe capture 继续使用 color-only Forward target，避免为六面 capture 分配 viewport temporal resources。

## 6. GPU Scene Shading Contract

### 6.1 Canonical accepted-instance list

禁止 `VulkanTlasManager` 和 scene table 分别遍历 `opaque_items` 后自行过滤。必须先生成一份 canonical CPU list：

```cpp
struct VulkanRayTracingInstanceRecord {
    const VulkanMeshResource* mesh = nullptr;
    const VulkanMaterialResource* material = nullptr;
    const VulkanAccelerationStructure* blas = nullptr;
    glm::mat4 transform{ 1.0f };
    VulkanMeshResourceKey mesh_key;
    uint64_t material_generation = 0;
    int entity_id = -1;
};
```

该 list 只包含 BLAS、mesh address、material 和 transform 全部有效的实例。随后：

- TLAS instance array 从该 list 生成。
- GPU instance table 从同一 list 生成。
- `instanceCustomIndex` 等于 GPU instance record index。
- Diagnostics 的 accepted / skipped count 也来自该 list。

索引 `0` 保留为 invalid / fallback record；真实 instance 从 `1` 开始。Shader 必须检查 `index > 0 && index < instance_count`。

### 6.2 GPU instance record

推荐 baseline：

```cpp
struct GpuRtInstanceRecord {
    uint32_t geometry_index = 0;
    uint32_t material_index = 0;
    uint32_t object_flags = 0;
    uint32_t entity_id = 0;
};
```

Ray Query 提供 committed object-to-world / world-to-object transform时，不在 table 中重复存储矩阵。若 DXC Ray Query transform intrinsic 验证失败，再显式增加 `object_to_world` 和 `normal_to_world`，不得在 shader 中猜测矩阵布局。

### 6.3 GPU geometry record

推荐 baseline：

```cpp
struct GpuRtGeometryRecord {
    uint64_t vertex_address = 0;
    uint64_t index_address = 0;
    uint32_t vertex_stride = 0;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    uint32_t flags = 0;
};
```

Geometry fetch 优先复用 buffer device address，并在 RT-10.0 开始时完成 DXC `PhysicalStorageBuffer` shader probe。该 probe 必须证明：

- HLSL 可以从 address table 读取 `uint32_t` index 和当前 `Vertex` 字段。
- SPIR-V capability / extension 与 logical-device feature 匹配。
- address、stride、index 和 primitive bounds 均有显式校验。
- Vulkan validation 无 physical-storage-buffer 错误。

如果该路径在目标 DXC / GPU 组合上不稳定，则回退 descriptor-indexed `ByteAddressBuffer` array。禁止同时维护两套生产 geometry lookup；probe 结束后必须在 RT-10.0 内固定一种方案。

### 6.4 GPU material record

Material table 必须是纯 GPU POD，不持有 CPU pointer：

```cpp
struct GpuRtMaterialRecord {
    glm::vec4 base_color_factor{ 1.0f };
    glm::vec4 emissive_factor_normal_scale{ 0.0f, 0.0f, 0.0f, 1.0f };
    glm::vec4 metallic_roughness_alpha_flags{ 0.0f, 1.0f, 0.5f, 0.0f };
    glm::uvec4 texture_indices0{ 0u };
    glm::uvec4 texture_indices1{ 0u };
};
```

字段语义必须由共享 C++ / HLSL contract 文档和 static assertions 固定。至少包含：

- base color factor。
- metallic / roughness factor。
- emissive factor / strength。
- normal scale / AO strength。
- texture-present flags。
- base color、normal、metallic、roughness、packed MR、AO、emissive texture index。
- alpha mode 只用于诊断；第一版只接受 Opaque。

### 6.5 Indexed texture table

第一版采用 fixed-capacity sampled-image descriptor array，而不是 update-after-bind 的无限 bindless：

- capacity 根据 device limits 截断，并在 diagnostics 中显示。
- 每个 FrameContext 持有自己的 shading descriptor set。
- descriptor 只在对应 frame slot fence 完成后更新。
- 不使用 update-after-bind，降低 descriptor lifetime 风险。
- HLSL texture index 使用 `NonUniformResourceIndex` 或 DXC Vulkan 等价语义。
- `shaderSampledImageArrayNonUniformIndexing` 作为 RT reflection shading capability，不得变成整个 Raster renderer 的 required feature。

保留固定 fallback texture slot：

```text
0 white
1 black
2 flat normal
3 default metallic-roughness
```

材质超过 texture capacity 时不得产生越界 descriptor；该材质使用 fallback texture，并累计 overflow diagnostics。

### 6.6 Descriptor layout isolation

保留现有 `RayTracingScene` layout 给 Shadow / RTAO 使用。新增独立 layout，例如：

```text
RayTracingShadingScene set
  binding 0: TLAS
  binding 1: instance storage buffer
  binding 2: geometry storage buffer
  binding 3: material storage buffer
  binding 4: sampled image array
  binding 5: sampler
```

这样 RT Reflection 的 descriptor indexing capability 不会污染只需要 TLAS 的 Shadow / AO pipeline。

## 7. Reflection Surface Contract

### 7.1 Forward MRT 输出

主 viewport Forward pass 增加 MRT 输出：

| Target | Baseline format | 内容 |
|---|---|---|
| HDR Scene Color | 现有 HDR format | final lit color，alpha 继续保留 reflection surface mask |
| Reflection Surface | `R16G16B16A16_SFLOAT` | oct normal RG、perceptual roughness B、reflection mask A |
| Fallback Specular | HDR format | 已选择 Local Probe / Global IBL 后的 specular radiance |
| Motion Vector | `R16G16_SFLOAT` | current UV 到 previous UV 的屏幕速度 |

第一版先使用可验证格式；显存与带宽稳定后再评估 packed normal format。禁止在同一工作包中同时实现算法和格式压缩。

### 7.2 为什么需要 Fallback Specular

当前 Probe / IBL specular 已经混入 final lit scene color。如果直接把 SSR 或 RT reflection additive 到 scene color，会重复计算环境反射；如果直接 `lerp(scene_color, reflection)`，又会错误替换 diffuse 和 direct light。

正确 baseline：

```text
non_reflection_lighting = max(scene_color - fallback_specular, 0)
final_color = non_reflection_lighting + selected_reflection_specular
```

这使 SSR、RT、Probe 和 IBL 真正成为同一个 specular source hierarchy，而不是多个后处理叠加层。

### 7.3 Motion vector

Motion vector 第一版只覆盖 camera motion 和 static mesh object transform：

```text
current clip = current_view_projection * current_model * position
previous clip = previous_view_projection * previous_model * position
velocity = current_uv - previous_uv
```

Previous transform 按 `scene_id + entity_id` 缓存。只有成功提交 frame 后才推进 previous state，record / submit 失败不得污染 temporal state。

### 7.4 Main viewport 与 Probe capture 分离

- Main viewport 使用 MRT Forward pipeline / target。
- Reflection Probe capture 继续使用 color-only Forward variant。
- Object ID / picking 保持独立 owner。
- Transparent pass 第一版不写 reflection surface history。

## 8. Compute 与 RenderGraph Foundation

### 8.1 Compute pipeline

新增窄职责 compute pipeline 支持：

```text
VulkanComputePipelineDesc
  shader program id
  descriptor set layouts
  push constant ranges
  specialization constants if needed
  debug name
```

`VulkanPipelineCache` 分别缓存 graphics 和 compute pipeline。不得用无 color attachment 的 graphics pipeline 模拟 compute，也不得在 RT Reflection Feature 内私建第二套 pipeline cache。

Shader manifest 增加 compute program record和 `cs_6_5` Ray Query compile option。Graphics manifest 的现有显式文件映射必须保持不变。

### 8.2 RenderGraph image access

新增明确 compute access，不复用当前固定为 fragment stage 的 `ShaderRead`：

```text
ComputeShaderRead
  layout: SHADER_READ_ONLY_OPTIMAL
  access: SHADER_READ
  stage: COMPUTE_SHADER

ComputeStorageWrite
  layout: GENERAL
  access: SHADER_WRITE
  stage: COMPUTE_SHADER

ComputeStorageReadWrite
  layout: GENERAL
  access: SHADER_READ | SHADER_WRITE
  stage: COMPUTE_SHADER
```

Acceleration structure usage增加 compute Ray Query read，不能继续映射到 fragment stage。

Focused planner tests 必须覆盖：

- Forward color attachment write -> compute sampled read。
- Compute storage write -> next compute sampled read。
- Compute storage write -> fragment composite read。
- TLAS build write -> compute Ray Query read。
- History image previous-frame write -> current-frame read。

### 8.3 Storage image contract

Reflection raw、hit distance、temporal output、moments 和 filter ping-pong image 必须包含：

```text
VK_IMAGE_USAGE_STORAGE_BIT
VK_IMAGE_USAGE_SAMPLED_BIT
```

Image layout 由 graph commit callback 维护，禁止 Feature 在 graph 外手写与 graph 冲突的 transition。

## 9. Reflection Trace Contract

### 9.1 Dispatch eligibility

每个 output pixel 在以下情况 early-out：

- Scene depth 是 sky。
- Reflection surface mask 为 0。
- Roughness 超过设置阈值。
- SSR hit confidence 已达到有效阈值。
- TLAS、scene table或shading descriptor 不可用。

SSR disabled 时，所有 eligible pixel 均可进入 RT trace。默认 half-resolution，source surface 和 depth 使用稳定映射。

### 9.2 Ray generation

- World position 从 depth + inverse view projection 重建。
- Origin 使用 material normal 和 configurable normal bias。
- 第一版 smooth surface 使用 deterministic reflection vector。
- Rough surface sampling在 temporal contract 可用后使用 frame-varying low-discrepancy GGX sample。
- `TMin`、`TMax` 和 roughness cutoff 必须暴露为 settings / diagnostics。

### 9.3 Committed hit reconstruction

命中后执行：

```text
CommittedInstanceID / InstanceIndex
  -> instanceCustomIndex
  -> GpuRtInstanceRecord
  -> GpuRtGeometryRecord
  -> primitiveIndex * 3
  -> three uint32 indices
  -> three Vertex records
  -> committed barycentrics
  -> interpolate position / normal / tangent / bitangent / UV
  -> transform to world space
  -> GpuRtMaterialRecord
  -> material texture sampling
```

所有 index、count 和 address 都必须先验证。Invalid record 输出 `confidence = 0` 并增加 GPU 可诊断 counter的需求留到后续；第一版至少提供 CPU table validation 和 debug fallback color。

### 9.4 第一版 hit shading

第一版只做单次、非递归 shading：

- Material base color、metallic、roughness、normal map、emissive。
- Global environment / IBL lighting。
- 可选无 shadow directional direct light，必须单独开关。
- 不从命中点再次发 reflection ray。
- 不查询 transparent / alpha-mask candidate。

Miss 不直接烘焙 Probe / IBL 到 RT output，而是写 `confidence = 0`，由 composite 使用 originating surface 的 Local Probe / Global IBL fallback。这避免 trace 与 composite 各自维护一套 Probe selection。

### 9.5 Raw output

| Target | Baseline format | 内容 |
|---|---|---|
| RT Raw Reflection | `R16G16B16A16_SFLOAT` | RGB radiance，A confidence |
| RT Hit Distance | `R16_SFLOAT` | world-space committed distance，miss 为 0 |

NaN、Inf 和负 radiance 必须在 shader 输出前清理。Debug view 应能分别查看 raw radiance、confidence、hit distance、instance index 和 primitive index。

## 10. Temporal 与 Spatial Denoiser Contract

### 10.1 History resources

每个 viewport history chain至少包含：

- previous filtered radiance。
- previous moments / variance。
- history length。
- previous depth 或可比较 linear depth。
- previous normal / roughness reference。

History 按提交顺序 ping-pong，不按 FrameContext slot 各自形成互不相干的时间线。Feature 只有在 frame 成功提交后才能交换 read / write history index。

### 10.2 History reset 条件

以下任一变化必须 reset：

- `scene_id` 改变。
- frame serial 不连续。
- camera cut / teleport。
- viewport extent 或 output route 改变。
- Ray-Traced Reflection enable / disable 或分辨率模式改变。
- surface / history format generation 改变。
- TLAS scene generation发生不兼容重建。
- reflection settings发生会改变采样分布的重大变化。

Reset frame 只使用 current raw input，history length 从 1 开始。

### 10.3 Temporal reprojection

推荐顺序：

1. Motion vector 计算 previous UV。
2. Reject out-of-bounds history。
3. Depth difference reject。
4. Normal dot reject。
5. Roughness difference reject。
6. Hit distance / confidence reject。
7. Neighborhood luminance clamp。
8. 按 history length 和 variance 混合。

第一版不依赖 object ID，但 diagnostics 应记录 depth / normal rejection。若 disocclusion ghosting仍明显，再增加 object identity buffer，不在第一轮盲目增加 MRT。

### 10.4 Spatial filter

- 使用 depth、material normal、roughness 和 hit distance 的 bilateral / atrous filter。
- 默认 3 个 iteration，settings 范围 0 至 5。
- 每个 iteration 使用 ping-pong storage image。
- Sky、invalid confidence 和强 disocclusion 不跨边缘扩散。
- `spatial_iterations = 0` 必须可显示 temporal-only output。

### 10.5 无 temporal fallback

Temporal pipeline 不可用时允许 spatial-only RT reflection，但 diagnostics 必须显示 degraded mode。不得因为 history allocation 失败而关闭整个 renderer。

## 11. Hybrid Reflection Composite

### 11.1 权重关系

每像素 source 权重必须互斥并归一化：

```text
ssr_weight = SSR confidence * surface mask
rt_weight = (1 - ssr_weight) * RT confidence * surface mask
fallback_weight = 1 - ssr_weight - rt_weight

selected_specular =
    SSR radiance * ssr_weight +
    RT radiance * rt_weight +
    fallback_specular * fallback_weight

final_hdr =
    max(scene_hdr - fallback_specular, 0) +
    selected_specular
```

Roughness、edge 和 distance fade 可以调节 source confidence，但不得让总权重超过 1。禁止 SSR 和 RT 同时 additive。

### 11.2 Composite output ownership

Composite 读取原始 HDR scene color，写入独立 composited HDR target，避免同一 image 同时 sampled read / color write。该 target 成为后续 DebugDraw、Bloom 和 PostProcess 的输入。

### 11.3 FeaturePlan

建议增加：

```cpp
enum class VulkanReflectionTechnique {
    FallbackOnly,
    ScreenSpace,
    HybridRayQuery
};
```

FeaturePlan 统一决定 SSR、RT trace、denoiser和composite是否运行。Graph builder、diagnostics 和 PostProcess 不得分别重新判断 settings。

### 11.4 Fallback matrix

| SSR | RT | Scene table / TLAS | 结果 |
|---|---|---|---|
| Off | Off | 任意 | Probe / IBL only |
| On | Off | 任意 | SSR -> Probe / IBL |
| Off | On | Ready | RT -> Probe / IBL |
| On | On | Ready | SSR -> RT -> Probe / IBL |
| On | On | Not ready | SSR -> Probe / IBL |
| Off | On | Not ready | Probe / IBL only |

## 12. 工作包拆分

### 12.1 RT-10.0：GPU Scene Shading Tables

风险：高。

开始条件：RT-08 AS lifetime稳定，mesh / material resource generation可查询。

工作内容：

- 建立 canonical accepted-instance list。
- 固定 `instanceCustomIndex -> instance table` contract。
- 实现 instance / geometry / material GPU table。
- 完成 BDA geometry fetch shader probe并固定生产方案。
- 增加 fixed-capacity indexed texture descriptor array和reflection shading capability negotiation。
- 将 per-frame shading descriptor接入 FrameContext / deferred lifetime。
- 增加 instance / geometry / material / texture count diagnostics。

验收：

- TLAS filtering后 instance table 索引仍一一对应。
- Debug compute / fragment probe 可输出正确 entity、primitive、barycentrics和base color。
- Mesh / material generation变化后不访问 stale address或descriptor。
- Texture overflow使用fallback资源且无 descriptor VUID。
- RT reflection shading capability缺失时 Shadow和RTAO仍可使用。

必要测试：

- POD size / offset static assertions。
- Accepted-instance ordering / skip policy。
- Geometry primitive bounds和32-bit index fetch。
- Material / texture fallback index。
- Descriptor capacity和frame-slot update lifetime。
- SPIR-V physical storage / non-uniform indexing validation。

### 12.2 RT-10.1：Reflection Surface, Compute and History Foundation

风险：高。

开始条件：RT-10.0 table contract固定。

工作内容：

- Pipeline cache和shader manifest支持compute。
- RenderGraph支持compute sampled / storage image和compute AS read。
- Main Forward增加Reflection Surface、Fallback Specular和Motion Vector MRT。
- 建立previous camera / object transform和camera-cut reset contract。
- 创建raw / history / moments / ping-pong targets及resize lifecycle。
- Probe capture保持color-only Forward路径。

验收：

- Surface normal、roughness、mask、fallback specular和velocity debug view正确。
- 静止camera / object velocity接近0。
- Camera和object移动方向正确，无Y翻转。
- Resize、scene reload、camera cut后history立即invalid。
- Compute write/read graph transitions无sync validation错误。

必要测试：

- Previous/current clip projection和velocity CPU contract。
- History reset decision table。
- Multi-color pipeline key / MRT format contract。
- Compute pipeline cache key。
- RenderGraph compute storage planner focused tests。

### 12.3 RT-10.2：Ray Query Reflection Trace and Hit Shading

风险：很高。

开始条件：RT-10.0 / RT-10.1通过GPU smoke。

工作内容：

- 新增`VulkanRayTracedReflectionFeature`和trace descriptor。
- 实现half/full-resolution compute dispatch。
- 使用SSR confidence跳过有效screen-space hit。
- 实现world ray generation、Ray Query、committed hit reconstruction。
- 实现static opaque material factor和texture sampling。
- 输出raw radiance、confidence和hit distance。
- 增加raw / hit / miss / instance / primitive debug view和trace GPU timing。

验收：

- 离屏物体可出现在镜面反射中。
- On-screen SSR valid区域不重复trace / composite RT结果。
- 不同mesh / material命中不会串表。
- Miss稳定回退Probe / IBL。
- Force-disabled、empty TLAS和table overflow均不影响Raster frame。

必要测试：

- Synthetic triangle barycentric reconstruction。
- Primitive / vertex / material table bounds。
- Identity、rotation、non-uniform和mirrored transform hit normal。
- RT-capable device raw reflection smoke。
- Force-disabled fallback smoke。

### 12.4 RT-10.3：Spatial and Temporal Reflection Denoiser

风险：很高。

开始条件：RT-10.2 raw output和motion vector稳定。

工作内容：

- Temporal reprojection、history validation和neighborhood clamp。
- Moments / variance和history length更新。
- Depth / normal / roughness / hit-distance aware spatial filter。
- History ping-pong只在successful submission后推进。
- 增加history、variance、rejection和spatial iteration debug view。

验收：

- 静止场景噪声随history稳定下降。
- Camera cut / resize无上一场景残影。
- 物体边缘和disocclusion不出现明显长尾ghosting。
- Temporal关闭时spatial-only路径仍可运行。
- 多帧并行下history无read/write alias VUID。

必要测试：

- History reset / advance state machine。
- Reprojection bounds和depth / normal rejection math。
- Graph previous-write -> current-read synchronization。
- Camera pan、object motion、scene reload和resize GPU smoke。

### 12.5 RT-10.4：Hybrid Composite, Fallback and Diagnostics

风险：高。

开始条件：RT-10.2可用；RT-10.3可作为optional input。

工作内容：

- 新增`VulkanReflectionCompositeFeature`和composited HDR target。
- 实现SSR / RT / fallback specular互斥权重。
- FeaturePlan统一选择FallbackOnly / ScreenSpace / HybridRayQuery。
- 调整FrameGraph顺序，使composite位于DebugDraw / Bloom / PostProcess之前。
- Editor增加mode、distance、roughness、resolution、temporal / spatial设置。
- Renderer Debug增加capability、active mode、fallback reason、resource count、history状态和GPU timing。
- 更新主Ray Tracing开发计划和最终视觉验证记录。

验收：

- SSR valid、SSR miss、RT miss三种区域按优先级选择source。
- Probe / IBL fallback不被重复additive。
- RT runtime disable可在下一帧安全回退。
- AO、Bloom、tone mapping、SMAA和reflection debug互不污染。
- Validation与sync validation无新增错误。

必要测试：

- Composite weight sum和source priority CPU test。
- FeaturePlan capability / debug isolation matrix。
- FrameGraph pass order和resource requirement test。
- SSR-only、RT-only、Hybrid和force-disabled visual smoke。

## 13. 生命周期与更新策略

### 13.1 Scene table

- Instance table按frame slot更新，必须等待对应FrameContext fence。
- Geometry / material / texture index由stable resource key + generation管理。
- Table capacity只增长或按明确budget回收，禁止每帧精确reallocate。
- 旧buffer和descriptor引用的resource通过retirement queue延迟销毁。
- BLAS replacement与geometry address generation必须在同一prepared frame snapshot中一致。

### 13.2 History

- History属于viewport reflection feature，不属于mesh/material cache。
- Resize和format变化创建新generation，旧image延迟销毁。
- History read / write index在successful submit后交换。
- Frame recording失败、swapchain acquire失败或feature未执行时不推进history。

### 13.3 Runtime toggle

- Disable RT Reflection停止trace / denoiser，但可保留已分配target以避免频繁allocation。
- Re-enable后的第一帧强制history reset。
- Force-disable Ray Query时不创建shading descriptor array、trace pipeline或RT history。
- Shadow / RTAO capability与Reflection Shading capability分别诊断。

## 14. Editor 与 Diagnostics

### 14.1 Render Settings

建议设置：

```text
Reflection Method
  Fallback Only
  Screen Space
  Hybrid Ray Query

RT Max Distance
RT Normal Bias
RT Roughness Cutoff
Resolution
  Half
  Full
Temporal Accumulation
Spatial Filter
Spatial Iterations
History Weight
```

默认保持现有 `Screen Space` 或当前项目设置，不在RT-10首次合入时自动切换Hybrid。

### 14.2 Debug views

- Reflection Surface Normal。
- Reflection Surface Roughness。
- Fallback Specular。
- Motion Vector。
- SSR Confidence。
- RT Raw Radiance。
- RT Confidence。
- RT Hit Distance。
- RT Instance / Primitive ID。
- Temporal History Length。
- Temporal Rejection。
- Reflection Variance。
- Denoised RT Reflection。
- Composite Source：SSR / RT / Probe-IBL颜色编码。
- Final Reflection Composite。

Debug isolation必须由FeaturePlan决定。Raw debug可以强制运行对应Feature，但不得把debug output混入normal final-lit history。

### 14.3 Stats

- Reflection shading capability / enabled / active。
- Fallback reason。
- Accepted / skipped RT instance count。
- Geometry / material / texture table count和capacity。
- Texture overflow count。
- Trace resolution、ray count和eligible pixel estimate。
- History valid / reset reason / length。
- Trace、temporal、spatial和composite GPU time。
- Raw / history / filter memory bytes。

## 15. 必要测试计划

测试遵循focused原则，不执行无关全量测试。

### 15.1 CPU focused

- Scene table index、generation和fallback record。
- C++ / HLSL POD layout。
- Primitive / vertex bounds和barycentric interpolation。
- Material texture index和capacity overflow。
- Velocity和history reset contract。
- Composite source权重。
- FeaturePlan fallback matrix。
- RenderGraph compute / storage / AS planner。

### 15.2 Shader validation

- 所有compute SPIR-V通过`spirv-val --target-env vulkan1.3`。
- Reflection trace确认包含`RayQueryKHR`指令。
- Geometry fetch确认包含预期physical-storage或descriptor-indexed能力。
- Texture access确认non-uniform indexing decoration。
- Descriptor set / binding和push constant reflection与C++ layout一致。

### 15.3 GPU device smoke

- RT capability Auto。
- Ray Query force-disabled。
- Empty TLAS。
- Single triangle hit / miss / barycentrics。
- 多mesh、多material和texture fallback。
- TLAS Build / Update / Reuse后table mapping稳定。
- Resize、scene reload、camera cut和runtime toggle。
- Vulkan validation和sync validation。

### 15.4 视觉场景

至少建立以下可重复场景：

1. 镜面平面，屏幕外放置高对比色立方体，验证RT补足SSR miss。
2. 屏幕内物体同时可被SSR和RT命中，验证SSR优先且无double reflection。
3. RT miss区域包含Local Probe和Global IBL，验证fallback连续。
4. Metallic / roughness阶梯，验证roughness cutoff和denoiser。
5. Camera pan、快速转向和object transform，验证motion vector和ghosting。
6. Resize / camera cut / scene reload，验证history reset。

## 16. 风险矩阵

| 风险 | 后果 | 缓解 |
|---|---|---|
| TLAS与instance table索引漂移 | 读取错误mesh/material或GPU fault | canonical accepted-instance list + index 0 fallback + bounds test |
| Stale buffer address | device lost / undefined fetch | resource generation + frame retirement + descriptor/table snapshot |
| Descriptor indexing被设为全局required | 不支持设备无法启动 | reflection-only capability cluster + Raster fallback |
| Material layout不一致 | 颜色、roughness、texture index错乱 | explicit POD + static assert + shader reflection test |
| Forward MRT带宽过高 | frame time和显存显著上升 | baseline format先正确，随后独立format optimization |
| History推进时机错误 | 多帧并行读写冲突 | successful-submit commit + graph state tracking |
| Ghosting / disocclusion | 运动画面拖影 | velocity + depth/normal/hit-distance rejection + reset reason diagnostics |
| SSR / RT / IBL double counting | 反射过亮或吞掉diffuse | fallback specular target + normalized source weights |
| RT-10改动Backend过宽 | 难review、难定位回归 | 五个工作包顺序合入，每包独立fallback和focused test |

## 17. 推荐目录组织

```text
source/Engine/Function/Renderer/Vulkan/
  ray_tracing/
    vulkan_ray_tracing_scene_table.h/.cpp
    vulkan_ray_tracing_shading_resource.h/.cpp

  features/
    vulkan_ray_traced_reflection_feature.h/.cpp
    vulkan_reflection_composite_feature.h/.cpp

  passes/
    vulkan_ray_traced_reflection_pass.h/.cpp
    vulkan_reflection_denoiser_pass.h/.cpp
    vulkan_reflection_composite_pass.h/.cpp

  targets/
    vulkan_reflection_surface_target.h/.cpp
    vulkan_ray_traced_reflection_target.h/.cpp
    vulkan_reflection_history_target.h/.cpp

assets/shaders/Renderer/Vulkan/reflection/
  vulkan_rt_reflection_trace.hlsl
  vulkan_rt_reflection_temporal.hlsl
  vulkan_rt_reflection_spatial.hlsl
  vulkan_reflection_composite.hlsl
  vulkan_reflection_scene_table.hlsli
```

类型名称可以在实现前微调，但 ownership 边界不得退回Backend单文件或PostProcess descriptor扩张。

## 18. 合入顺序与停止条件

```text
RT-10.0 Scene Shading Tables
  -> RT-10.1 Surface / Compute / History
  -> RT-10.2 Trace / Hit Shading
  -> RT-10.3 Denoiser
  -> RT-10.4 Composite / Fallback / Diagnostics
```

每个工作包完成后必须保持：

- 默认Raster / SSR视觉不变。
- Ray Query force-disabled可启动。
- 对应debug output可解释当前状态。
- 只运行与改动相关的focused tests。
- 不依赖后续包才能释放本包GPU资源。

出现以下情况应停止进入下一包：

- Geometry fetch shader probe不能跨目标GPU稳定运行。
- TLAS与scene table mapping没有自动化测试。
- Compute storage graph有validation / sync VUID。
- MRT surface或motion vector方向未通过debug验证。
- History在resize / camera cut后仍读取stale image。

## 19. RT-10 完成标准

RT-10.0 至 RT-10.4 全部完成后应满足：

1. RT Reflection仍基于Ray Query，不依赖Full RT Pipeline / SBT。
2. Shader可从committed hit稳定映射到instance、geometry、material和texture。
3. 离屏static opaque物体可出现在reflection output中。
4. SSR、RT、Local Probe和Global IBL按互斥优先级组合，无double counting。
5. Motion vector、history reset、temporal和spatial filter在camera / object运动中稳定。
6. RT capability、TLAS、scene table、history或pipeline不可用时自动回退。
7. Default setting不强制开启Hybrid RT Reflection。
8. Resize、scene reload、runtime toggle和多帧并行无stale descriptor、use-after-free或history alias。
9. Renderer Debug可解释资源容量、active technique、fallback reason、history状态和GPU成本。
10. Vulkan validation、sync validation、SPIR-V validation和focused tests全部通过。

## 20. 最终建议

RT-10.0 已完成。下一步只启动 `RT-10.1：Reflection Surface / Compute / History`，不要同时提前实现 Trace、Denoiser 或 Composite。

RT-10.0 的首个子任务应是 geometry fetch shader probe与canonical accepted-instance contract。它们分别验证“GPU能否安全读取命中三角形”和“CPU / TLAS / shader是否使用同一索引真相”。这两个条件未固定前，任何反射视觉算法都建立在不可靠的数据基础上。

## RT-10.0 Implementation Notes (2026-08-18)

The RT-10.0 foundation is implemented without changing Forward MRT, compute reflection, temporal history, or composite behavior.

Implemented:

- A canonical accepted-instance list is built once by the TLAS path and retained per frame slot.
- `instanceCustomIndex` maps to the GPU instance table with index `0` reserved for the invalid/fallback record; real instances start at `1`.
- Per-frame instance, geometry, and material GPU tables are owned by `VulkanFrameContext` and use the existing deferred-retirement resource primitives.
- Material shading data is exported as GPU-only POD, including texture indices, material generation, and double-sided state.
- Fixed-capacity sampled-image arrays use stable white, black, flat-normal, and metallic-roughness fallback slots.
- Reflection shading capability negotiation is isolated from the existing Shadow/RTAO Ray Query capability. It requires `runtimeDescriptorArray`, sampled-image non-uniform indexing, and storage-buffer non-uniform indexing; missing any of them leaves those features available.
- A descriptor-indexed vertex/index `ByteAddressBuffer` geometry path is the production path. The DXC `vk::buffer_reference` probe was attempted and rejected by the project compiler, so physical-storage-buffer lookup is not maintained as a second production path.
- The geometry fetch probe is compiled to SPIR-V as part of `NexAurVulkanShaders` and passes `spirv-val` for Vulkan 1.3.
- Renderer debug state exposes table capability, active path, counts, capacities, overflow counts, buffer sizes, and failure reason.

Focused verification completed:

- Ray tracing capability, descriptor capacity, primitive bounds, and instance-index contract tests.
- RT-capable Vulkan device smoke with validation enabled, including table descriptor population and fallback texture resources.
- Ray Query disabled and force-disabled device smoke.

The next package remains RT-10.1: reflection surface data, compute pipeline support, and history resources. No reflection trace shader is part of RT-10.0.

# NexAur Ray Tracing Development Plan

日期：2026-08-17

状态：开发中；RT-00 至 RT-06 已完成，下一工作包为 RT-07

## 1. 文档目的

本文档定义 NexAur 在现有 Vulkan Renderer 上引入硬件光线追踪的技术路线、架构边界、前置工作、实施工作包和验证标准。

本计划的第一目标不是直接实现完整 Path Tracer，也不是立即用光追替换现有 Raster Pipeline，而是建立一条可回退、可诊断、可逐步扩展的硬件光追路径：

```text
现有 Raster Forward Pipeline
  + Ray Query Directional Shadow
  + Existing CSM / PCSS fallback
```

第一项正式视觉功能选择 Directional Ray Query Shadow，原因如下：

- 只需要回答遮挡与否，不需要完整的命中点材质求值。
- Ray Query 可以直接运行在现有 fragment shader 中，不需要 Ray Tracing Pipeline 和 Shader Binding Table。
- 可以继续使用当前 Forward、材质、IBL、SSR、Reflection Probe 和后处理链路。
- 可以与现有 CSM / PCSS 做同场景对照，并保留稳定 fallback。
- 能先验证 device capability、buffer device address、BLAS、TLAS、descriptor 和同步这些真正的底层难点。

所有光追能力必须是可选能力。NexAur 在不支持硬件光追的设备上仍必须正常启动并使用 Raster 路径。

## 2. 决策摘要

### 2.1 技术路线

```text
Phase A：Ray Tracing Foundation
  RT-00 至 RT-06

Phase B：First Production Feature
  RT-07 Directional Ray Query Shadow
  RT-08 Lifetime / Compaction / Profiling

Phase C：Additional Hybrid Effects
  RT-09 Optional RTAO
  RT-10 Ray-Traced Reflection Foundation

Phase D：Full Ray Tracing Pipeline
  RT-11 RT Pipeline / SBT / Path Tracer，后续阶段
```

### 2.2 第一阶段明确选择

| 项目 | 第一阶段选择 |
|---|---|
| Vulkan 光追方式 | `VK_KHR_ray_query` |
| 第一个功能 | Directional Ray Query Shadow |
| Shader 执行阶段 | 现有 Forward fragment shader 的独立 pipeline variant |
| 几何范围 | 静态 opaque triangle mesh |
| BLAS | Mesh 级持久缓存 |
| TLAS | 从当前 `VulkanDrawList` opaque items 构建 |
| 不支持设备 | 自动回退 CSM / PCSS |
| 透明与 alpha mask | 第一阶段不进入 TLAS，后续单独设计 |
| Skinned / deforming mesh | 第一阶段不支持 |
| Full RT Pipeline / SBT | 第一阶段不引入 |

### 2.3 最终混合反射顺序

光追反射不是第一项功能。后续进入 RT Reflection 时，推荐使用以下优先级：

```text
Valid SSR hit
  -> Ray-traced reflection for SSR miss / off-screen region
  -> Local Reflection Probe
  -> Global IBL
```

SSR 仍负责低成本、与当前屏幕内容一致的近场反射；Ray Tracing 补足离屏和深度缓冲无法表达的命中；Probe 和 Global IBL 继续负责粗糙表面、未命中和不支持光追设备的稳定 fallback。

## 3. 当前 Renderer 能力评估

### 3.1 可以直接复用的基础

当前 Renderer 已具备以下可复用条件：

- Vulkan device 最低版本为 Vulkan 1.3。
- 已启用 `dynamicRendering` 和 `synchronization2` device feature。
- 已完成 Renderer Rebuild RR-00 至 RR-14，具备稳定 frame contract、Feature ownership、FrameContext、deferred destruction 和 focused test target。
- RT-00 已建立可选 Ray Query capability negotiation、logical-device feature chain 和 diagnostics。
- RT-01 已建立 VMA buffer-device-address contract、统一 addressable buffer primitive 和 RT-enabled mesh buffer usage。
- RT-02 已建立 acceleration structure 函数表、move-only AS primitive、aligned scratch buffer 和 deferred destruction contract。
- RT-03 已建立 opaque static mesh BLAS cache、stable mesh identity / generation 和一次性 fence build path。
- RT-04 已建立按 frame slot 重建的 TLAS instance path、transform conversion 和有效 BLAS filtering。
- Shader 使用 HLSL，经 DXC 编译为 Vulkan 1.3 SPIR-V。
- `VulkanDrawList` 已包含 mesh、material、world transform 和 entity ID。
- `VulkanMeshResource` 已持有 GPU vertex / index buffer；CPU Mesh 也保留标准三角形顶点和 `uint32_t` index 数据。
- Opaque 与 transparent draw item 已在 frontend 分开。
- 已有 CSM / PCSS、SSR、Reflection Probe 和 Global IBL，可作为混合渲染 fallback。
- Descriptor layout cache 已能表达任意 `VkDescriptorType`，但 allocator 和 writer 仍需补充 acceleration structure 支持。

`VulkanDrawList` 的数据已经足以作为 TLAS instance 的 CPU 输入：

```text
VulkanMeshDrawItem
  mesh pointer
  material pointer
  world transform
  entity id
  sort key
```

因此第一阶段不需要修改 Scene / ECS 的主体数据模型，也不需要另建一套光追场景抽取系统。

### 3.2 当前阻塞项

| 领域 | 当前状态 | 光追影响 |
|---|---|---|
| Device capability | RT-00 已完成 capability cluster 查询、`Auto` / `Disabled` / force-disable 和可选 feature chain | 已解除；不支持或关闭时继续创建 Raster logical device |
| GPU allocator | RT-01 已按实际 device feature 设置 VMA device-address flag，并统一地址与 alignment 校验 | 已解除；RT-02 可直接复用 addressable `VulkanOwnedBuffer` |
| Mesh buffer | Ray Query enabled 时附加 build-input / device-address usage，Disabled 时保持 Raster usage | 已解除；RT-03 已从 ready mesh 读取非零 vertex / index address |
| AS primitive | RT-02 已建立 AS backing / handle ownership、build-size query、scratch buffer 和 build command 录制 | 已解除；RT-03 已复用该 primitive 建立 static mesh BLAS cache |
| BLAS cache | RT-03 已按 model asset + mesh index + generation 缓存 opaque static mesh BLAS | 已解除；RT-04 可直接引用 ready BLAS device address |
| TLAS instance | RT-04 已建立 per-frame instance buffer、TLAS build 和空 scene fallback | 已解除；AS access 可由 RenderGraph 描述 |
| RenderGraph | 已支持 imported buffer / acceleration structure resource 和对应 pass access | 可表达 instance buffer、BLAS/TLAS build 和 shader read hazard |
| Synchronization | Graph image、buffer 和 AS state 均由 synchronization2 planner 描述 | 已覆盖 AS build write -> shader read 及连续 write hazard |
| Descriptor allocator | RT-06 已按 capability 条件分配 acceleration structure descriptor pool capacity | 可分配 Ray Query descriptor set；RT-disabled 不创建 AS pool entry |
| Descriptor writer | RT-06 已支持 `VkWriteDescriptorSetAccelerationStructureKHR` pNext 写入 | 可更新有效 TLAS；BLAS 会在 CPU 侧拒绝 |
| Shader library | RT-06 已加入 `ForwardRayQuery` manifest、DXC profile 和 `SPV_KHR_ray_query` 编译选项 | 可创建独立 fragment Ray Query variant；RT-disabled 不创建 pipeline |
| Pipeline cache | 只支持 graphics pipeline | 第一阶段足够；Full RT Pipeline 时必须新增独立 pipeline 类型 |
| Scene material table | 仍按 draw call 绑定 material | 足够做 shadow visibility，不足以做通用 hit shading |
| Temporal data | 没有正式 motion vector / history contract | 不足以实现稳定的 RT reflection denoiser |
| GPU lifetime | 已有 FrameContext、frame serial、retirement queue 和 deferred destruction | 后续 AS 对象必须接入现有 retirement contract |

### 3.3 结论

当前 Renderer 已具备进入第一项实际 Ray Query 视觉功能的基础。RT-00 capability negotiation、RT-01 device-address buffer foundation、RT-02 acceleration structure primitive、RT-03 static mesh BLAS cache、RT-04 TLAS instance build、RT-05 RenderGraph AS synchronization 和 RT-06 descriptor / debug shader 已完成；下一步进入 RT-07 directional shadow integration。

第一阶段不需要完整 RHI 重写，也不需要完整 Ray Tracing Pipeline。正确做法是在现有 Vulkan backend 内新增窄职责的 Ray Tracing Foundation，并保持 Renderer frontend 和 Raster Pipeline 稳定。

## 4. Ray Query 与 Full RT Pipeline 边界

### 4.1 Ray Query

Ray Query 在 graphics 或 compute shader 内主动执行 traversal：

```text
Existing Forward Fragment Shader
  -> ray origin / direction
  -> RayQuery.TraceRayInline
  -> committed hit or miss
  -> shadow visibility
```

优点：

- 不需要 raygen、miss、closest-hit shader。
- 不需要 Shader Binding Table。
- 不需要立即泛化当前 graphics-only pipeline cache。
- 与现有 Forward shading、material descriptor 和 framebuffer 路径兼容。
- 适合 shadow、AO、简单 visibility 和有限的 reflection query。

限制：

- 复杂命中点材质求值需要自行根据 instance / primitive / barycentric 查询 scene table。
- alpha-tested intersection 需要候选命中处理和材质纹理访问。
- 大规模反射仍需要 bindless geometry / material / texture 数据以及 denoiser。

### 4.2 Full Ray Tracing Pipeline

Full RT Pipeline 需要：

- `VK_KHR_ray_tracing_pipeline` feature 和 properties。
- raygen、miss、closest-hit、any-hit 等 shader stage。
- 独立 Ray Tracing Pipeline cache / builder。
- Shader Binding Table buffer、record layout 和 alignment 处理。
- hit group 与 scene material / geometry table 的稳定索引。
- 更完整的 recursion、payload、stack 和 dispatch 管理。

这些能力对未来 Path Tracer 有价值，但不是 Ray Query Shadow 的前置。过早引入会同时扩大 device、pipeline、shader、scene binding 和 lifetime 五个改动面。

## 5. 与 Renderer Rebuild 的依赖

本计划依赖 `renderer_rebuild.md` 中若干基础工作。正式合入 Ray Query Feature 前，建议至少完成：

```text
RR-01 RenderGraph Access Model
  -> RR-02 Synchronization2 Migration
  -> RR-03 Canonical Frame Contract
  -> RR-05 Vulkan Backend Responsibility Split
  -> RR-06 Common Vulkan Resource Primitives
```

依赖原因：

| Rebuild 工作包 | 对光追的价值 |
|---|---|
| RR-01 | Graph 必须表达 read / write、stage、access 和同 layout hazard |
| RR-02 | AS build / shader read 应统一使用 synchronization2 stage / access |
| RR-03 | TLAS 更新需要稳定的 scene ID、frame serial 和 prepared-frame 生命周期 |
| RR-05 | Capability、device feature chain 和 function ownership 应进入 `VulkanDeviceContext` |
| RR-06 | AS backing、scratch、instance buffer 应复用统一 `VulkanOwnedBuffer` / allocator |

截至 2026-08-17，RR-00 至 RR-14 已全部完成，上述前置依赖均已满足：Shadow Frame Builder、Feature ownership、deferred destruction、FrameContext、异步上传和 Renderer focused tests 均可直接复用。光追工作包不得重新建立平行生命周期、buffer ownership 或 feature 调度路径。

## 6. 目标架构

### 6.1 建议目录

```text
source/Engine/Function/Renderer/Vulkan/
  ray_tracing/
    vulkan_ray_tracing_capabilities.h/.cpp
    vulkan_acceleration_structure.h/.cpp
    vulkan_blas_cache.h/.cpp
    vulkan_ray_tracing_scene.h/.cpp
    vulkan_ray_query_shadow_feature.h/.cpp
```

如果 RR-09 已建立统一 Feature 目录，则 `VulkanRayQueryShadowFeature` 应遵循 Feature 的最终目录约定；其余 AS 基础类型继续留在 `ray_tracing/`，不放入通用 pass 目录。

### 6.2 所有权

```text
VulkanDeviceContext
  owns negotiated ray-tracing capabilities
  owns enabled feature / extension contract

VulkanGpuAllocator
  creates device-address capable buffers

VulkanRenderResourceCache / VulkanBlasCache
  owns persistent mesh BLAS
  invalidates BLAS with mesh resource generation

VulkanRayTracingScene
  consumes current VulkanDrawList
  owns TLAS, instance buffer and reusable scratch capacity
  records build/update commands

VulkanRayQueryShadowFeature
  owns feature descriptors and runtime decision
  exposes graph build/read dependencies
  binds the Ray Query forward pipeline variant

VulkanPassGraph
  imports buffer / acceleration-structure resources
  plans build-write -> shader-read synchronization
  does not own persistent BLAS or TLAS
```

### 6.3 核心依赖规则

- Scene / ECS 不得依赖 Vulkan acceleration structure 类型。
- `VulkanDrawList` 可以作为第一阶段 TLAS 输入，不新增平行的 scene extraction 链路。
- BLAS cache 不得以未经生命周期约束的裸指针作为唯一 cache key，应使用稳定 mesh identity 和 resource generation。
- Graph 只描述资源访问和执行顺序，不拥有持久 AS。
- Feature 不直接创建 logical device、allocator 或同步对象。
- Ray Query 不可用时，Feature plan 必须在录制 command 前选择 Raster fallback。
- 不允许向 shader 绑定 null / stale TLAS 后依赖动态分支规避访问。

## 7. Device Capability Negotiation

### 7.1 可选扩展簇

Ray Query capability 必须作为一个完整簇协商，不能只检查单个 extension name：

```text
VK_KHR_acceleration_structure
VK_KHR_ray_query
VK_KHR_deferred_host_operations
bufferDeviceAddress support
accelerationStructure feature
rayQuery feature
```

Vulkan 1.3 已包含部分早期扩展的核心能力，但对应 feature bit 仍必须通过正确的 `pNext` chain 查询并显式启用。

建议 capability 数据：

```cpp
struct VulkanRayTracingCapabilities {
    bool acceleration_structure = false;
    bool ray_query = false;
    bool ray_tracing_pipeline = false;
    bool buffer_device_address = false;
    uint64_t min_scratch_alignment = 0;
    uint64_t max_geometry_count = 0;
    uint64_t max_instance_count = 0;
    std::string unavailable_reason;

    bool supportsRayQuery() const;
};
```

Full RT Pipeline capability 要独立记录，不能用 `supportsRayQuery()` 暗示设备也支持 SBT / RT Pipeline。

### 7.2 设备选择约束

当前项目可能运行在同时存在独显和集显的机器上，因此：

- 普通启动不得将 Ray Query extensions 加为全局 required extensions。
- Physical device 只满足 Raster baseline 时仍是合法候选。
- 支持完整 Ray Query capability cluster 的设备才启用相应 feature chain。
- `Disabled` 模式不创建 BLAS/TLAS，也不改变 mesh buffer 分配以外的运行行为。
- `Auto` 模式在 capability 完整时启用，否则记录原因并回退。
- 测试可提供 `force_disable_ray_query` 覆盖，用于在光追 GPU 上验证 fallback。

### 7.3 Diagnostics

Debug snapshot 至少记录：

```text
Ray Query supported / enabled
Fallback reason
BLAS count / bytes / build count
TLAS instance count / mode / bytes
AS scratch peak bytes
BLAS and TLAS build GPU time
Ray Query shadow active / fallback
```

## 8. Acceleration Structure 数据契约

### 8.1 Device-address buffer

统一 buffer primitive 需要支持：

- `VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT`。
- `VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR`。
- `VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR`。
- `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT`，用于后续 scene table。
- VMA allocator 的 buffer device address allocation contract。
- `vkGetBufferDeviceAddress()` 结果查询和非零校验。
- scratch buffer alignment 校验。

Mesh vertex / index buffer 在 RT capability 启用时应附加 build-input 和 device-address usage。Raster-only device 仍沿用普通 buffer 路径。

### 8.2 BLAS geometry

第一版每个 `VulkanMeshResource` 对应一个 triangle BLAS：

```text
vertex format  = VK_FORMAT_R32G32B32_SFLOAT
vertex stride  = sizeof(Vertex)
position offset = offsetof(Vertex, position)
index type     = VK_INDEX_TYPE_UINT32
primitive count = index_count / 3
geometry flags = opaque baseline
```

必须显式检查：

- vertex / index buffer address 非零。
- index count 是 3 的整数倍。
- `maxVertex`、primitive count 和 Vulkan 限制不溢出。
- 空 mesh 不创建 BLAS。
- build size 和 scratch size 均满足 device alignment。

第一阶段只包含 `opaque_items`。Transparent、alpha mask 和需要 vertex deformation 的 mesh 不进入 TLAS，避免在没有 material table 和 candidate intersection 处理时制造错误遮挡。

### 8.3 TLAS instance

TLAS instance 从当帧 opaque draw items 生成：

```text
mesh BLAS address
world transform
instance custom index / object slot
visibility mask
instance flags
```

GLM `mat4` 与 `VkTransformMatrixKHR` 的存储布局不同，必须使用显式逐元素转换 helper，禁止直接 `memcpy`。该 helper 需要覆盖 translation、rotation、non-uniform scale 和 mirrored transform focused test。

第一版可每帧重建 TLAS，以先固定正确性。RT-08 再根据以下 dirty state 决定 update 或 rebuild：

- instance count 改变。
- BLAS identity / generation 改变。
- world transform 改变。
- visibility mask 改变。
- scene identity 改变。

没有有效 instance 时不得绑定 stale TLAS。Feature 应选择 Raster fallback；后续如果需要常驻 empty TLAS，应作为显式、已验证的资源策略实现。

### 8.4 生命周期

- BLAS backing buffer 的生命周期必须覆盖所有引用它的 TLAS 和 submitted frame。
- TLAS backing、instance 和 scratch buffer 在 GPU build / trace 完成前不得重用或销毁。
- Mesh resource eviction 必须先使 BLAS generation 失效，再按 frame retirement 销毁旧 AS。
- 单帧 baseline 可沿用当前 frame fence，但不得依赖无记录的 `vkDeviceWaitIdle()` 维持正确性。
- 进入 frames-in-flight 前必须接入 RR-11 / RR-12 的 deferred destruction 和 per-frame resource ownership。

## 9. RenderGraph 与同步

### 9.1 新资源类型

RenderGraph 至少需要表达：

```text
GraphBuffer
  instance build input
  scratch buffer
  AS backing buffer

GraphAccelerationStructure
  BLAS build/read
  TLAS build/write
  TLAS shader read
```

第一版允许 persistent resource 以 imported handle 注册，不要求同时实现 transient aliasing。

### 9.2 必须覆盖的 hazard

关键依赖是：

```text
TLAS Build
  stage  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR
  access = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR

Forward Ray Query Read
  stage  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
  access = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
```

还需要覆盖：

- transfer / host instance data write -> AS build input read。
- BLAS build write -> TLAS build read。
- scratch reuse write -> next AS build write。
- TLAS update write -> fragment shader read。
- 相同资源状态但 access 存在 write hazard 时仍生成 barrier。

这些 barrier 必须由 graph state planner 生成。Backend 内临时手写 barrier 可以用于未合入的技术验证，但不得作为正式 Feature 的最终实现。

## 10. Descriptor 与 Shader 约定

### 10.1 Descriptor

新增独立的 Ray Tracing Scene descriptor set，避免把 TLAS 强行加入所有 Raster pipeline：

```text
VulkanDescriptorSetLayoutId::RayTracingScene
  binding 0: VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
```

所需修改：

- Descriptor allocator pool 增加 acceleration structure descriptor 数量。
- Descriptor writer 增加 `writeAccelerationStructure()`。
- Writer 在 `vkUpdateDescriptorSets()` 时构造稳定的 `VkWriteDescriptorSetAccelerationStructureKHR` pNext chain。
- Ray Query pipeline variant 才包含并绑定该 set layout。
- Raster pipeline layout 保持不变。

### 10.2 HLSL / DXC

Ray Query shader 使用支持 inline ray tracing 的 shader model，并显式启用本地 DXC 所要求的 SPIR-V Ray Query extension 参数。构建后必须使用 Vulkan validation 和 SPIR-V validation 验证 capability / extension 声明。

第一版不创建 raygen / miss / hit shader。Shader library 只需新增 Forward Ray Query variant，仍返回普通 vertex / fragment module pair。

### 10.3 Shadow query contract

Directional shadow query 至少需要：

```text
origin       = world position + normal / light direction bias
direction    = surface to directional light
TMin         = positive epsilon
TMax         = configured RT shadow distance
geometry     = opaque triangle BLAS only
result       = visible or occluded
```

建议第一版使用 first-hit opaque visibility，不做多层透明、colored transmission 或 alpha test。Ray bias、normal offset 和 maximum distance 必须放入 `RenderSettings` 或明确的 frame constant，不能散落为 shader magic number。

## 11. 实施工作包

### 11.1 RT-00：Capability Baseline

状态：已完成（2026-08-17）。

风险：低到中。

工作内容：

- 建立 `VulkanRayTracingCapabilities`。
- 查询 extension、feature 和 acceleration structure properties。
- 为 device feature `pNext` chain 增加 optional Ray Query 分支。
- Raster-only physical device 仍可创建 logical device。
- 增加 `Disabled`、`Auto` 和测试用 force-disable 路径。
- 将 capability 与 unavailable reason 暴露给 diagnostics。

验收：

- 不支持光追时引擎正常启动，功能明确回退。
- 支持光追时 feature / properties 记录正确。
- 关闭光追时不创建任何 AS 对象。
- Vulkan validation 不新增 feature-chain 或 extension VUID。

实现结果：

- 新增 `VulkanRayTracingCapabilities` 和纯数据 capability negotiation。
- `VulkanDeviceContext` 查询 Ray Query extension cluster、BDA / AS / Ray Query feature 及 AS properties。
- 仅在完整 capability 且模式允许时启用 extension cluster 与 device feature `pNext` chain。
- `VulkanRendererInitContext` 提供默认 `Auto`、显式 `Disabled` 和测试用 force-disable 配置。
- Renderer diagnostics 暴露 Ray Query supported / enabled、fallback reason、RT Pipeline support 和 AS limits。
- focused test 覆盖完整 capability、缺失任一依赖、query failure、Disabled、force-disable 和 Ray Query / Full RT Pipeline 能力分离。

验证记录：

- Debug `NexAurRendererTests` 与 `Sandbox` 构建通过。
- capability CPU focused test 通过。
- RT-capable GPU 上 `Auto`、`Disabled`、force-disable 三条 logical-device 路径通过。
- Debug Vulkan validation 已启用，无新增 feature-chain 或 extension VUID。

### 11.2 RT-01：Device Address Buffer Foundation

状态：已完成（2026-08-17）。

风险：中。

工作内容：

- 为 backend allocator 建立 device-address capability。
- 扩展 `VulkanOwnedBuffer` 创建描述，支持 AS storage / build input / scratch usage。
- 为 RT-enabled mesh vertex / index buffer 添加 build-input 和 device-address usage。
- 提供统一 buffer device address helper。
- 对 size、usage、alignment 和 address 做错误检查。

验收：

- RT-enabled mesh vertex / index address 非零。
- Raster-only 路径 buffer 创建行为不回归。
- 资源关闭和创建失败 cleanup 不泄漏。

实现结果：

- `VulkanResourceContext` 只传播 logical device 上实际启用的 buffer-device-address 状态。
- `VulkanGpuAllocator` 按 capability 设置 `VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT`，并统一封装 `vkGetBufferDeviceAddress()`。
- 新增 `VulkanOwnedBufferCreateInfo`，集中表达 size、usage、memory usage、allocation flags、minimum alignment 和 debug name。
- addressable buffer 创建后立即校验非零地址和 alignment；失败时直接释放未发布的 buffer / allocation。
- RT-enabled mesh vertex / index buffer 附加 AS build-input 与 shader-device-address usage；Disabled 路径保持原 Raster usage。
- Mesh upload barrier 在 RT 路径同时覆盖 acceleration-structure build read。
- Renderer diagnostics 暴露 BDA enabled 状态和 device-address mesh 数量。

验证记录：

- Debug `NexAurRendererTests` 与 `Sandbox` 构建通过。
- capability 与 buffer create-info 两项 CPU focused test 通过。
- RT-capable GPU 上 mesh vertex / index、aligned scratch 和 AS-storage buffer 地址非零。
- `Disabled` 模式 mesh 保持 Raster usage 且 device address 为零。
- Debug Vulkan validation 已启用，无新增 buffer usage、allocation flag 或 alignment VUID。

### 11.3 RT-02：Acceleration Structure Primitive

状态：已完成（2026-08-17）。

风险：中到高。

工作内容：

- 建立 move-only `VulkanAccelerationStructure`。
- 封装 AS backing buffer、handle、device address、type 和 size。
- 封装 build size query、create、destroy 和 build command 录制。
- 建立 scratch capacity / alignment helper。
- 统一 Vulkan error 和 debug name。

约束：

- Primitive 不读取 AssetManager。
- Primitive 不决定 BLAS cache 策略。
- Primitive 不调用 device-wide idle。

验收：

- 可创建并销毁空的测试 backing / AS primitive 路径。
- 失败路径无 handle 或 allocation 泄漏。
- Validation 不报告 address、alignment 或 build size 错误。

实现结果：

- `VulkanDeviceContext` 加载并持有 acceleration structure device function table；加载失败时保持 Raster fallback。
- 新增 move-only `VulkanAccelerationStructure`，统一持有 backing buffer、AS handle、type、size、device address 和 debug name。
- AS handle 与 backing buffer 作为同一 deferred-retirement 对象，严格按 handle 先于 buffer 的顺序销毁。
- 新增 build-size query、单 AS build command 录制和 geometry pointer contract 校验。
- 新增可复用 aligned scratch buffer，按设备 `minAccelerationStructureScratchOffsetAlignment` 分配并验证 address。
- Renderer diagnostics 暴露 AS function table 加载状态。

验证记录：

- Debug `NexAurRendererTests` 与 `Sandbox` 构建通过。
- capability、device-address buffer 与 AS primitive 三项 CPU focused test 通过。
- RT-capable GPU 上完成 triangle BLAS size query、空 AS 创建、scratch 对齐、move ownership、build command 录制和 deferred destruction smoke。
- Sandbox 完整启动到 `NexAur Engine started`；Debug Vulkan validation 无新增 VUID，stderr 为空。

### 11.4 RT-03：Static Mesh BLAS Cache

状态：已完成（2026-08-17）。

风险：高。

工作内容：

- 从 `VulkanMeshResource` vertex / index buffer 生成 triangle BLAS geometry。
- 建立 stable mesh identity + generation cache key。
- 首次使用时构建 BLAS，后续帧复用。
- Mesh generation 变化时使旧 BLAS 失效。
- 第一版只构建 opaque static mesh。
- 记录 build count、cache hit、bytes 和 failure reason。

第一版允许在现有单帧模型下通过明确 fence 完成一次性 BLAS build，但不得每帧重建未变化 BLAS，也不得使用 `vkDeviceWaitIdle()` 隐藏生命周期问题。

验收：

- 同一 mesh 多 instance 只对应一个 BLAS。
- scene transform 变化不触发 BLAS rebuild。
- mesh resource generation 变化会触发一次正确 rebuild。
- 空 mesh、非三角 index 和 build failure 可诊断并回退。

实现结果：

- `VulkanMeshResource` 使用 model asset、mesh index 和 GPU resource generation 组成稳定 key；transform 不进入 BLAS identity。
- 新增 `VulkanStaticMeshBlasCache`，只接收 ready opaque mesh，并在每帧请求内按 stable identity 去重。
- 首次使用时从 vertex / index device address 生成 triangle geometry，查询尺寸并创建 BLAS；相同 generation 后续直接命中缓存。
- generation 改变时在新 BLAS 构建成功后替换旧 entry，旧 AS 继续使用 deferred destruction。
- 多个缺失 BLAS 在一个 command buffer 中构建，共享 aligned scratch，并在连续 build 间插入 synchronization2 AS dependency。
- 一次性 build 使用专用 fence 精确等待，不调用 `vkDeviceWaitIdle()`；初始化或单 mesh 失败时继续 Raster fallback。
- 非三角、未 ready、缺少 device address 和 Vulkan build failure 会形成可诊断 failed entry，同 generation 不会每帧重试。
- Renderer diagnostics 暴露 cache ready、entry / ready / failed、build count、cache hit、AS bytes 和 last failure。

验证记录：

- Debug `NexAurRendererTests` 与 `Sandbox` 构建通过。
- RT-00 至 RT-03 四项 CPU focused contract test 通过。
- RT-capable GPU 上完成真实 mesh upload、empty mesh fallback、duplicate instance dedup、双 BLAS batch build / reuse、generation rebuild、非三角失败记忆和 deferred destruction smoke。
- Auto device-address 与 Disabled Raster fallback 两条 GPU mesh smoke 通过。
- Sandbox 完整启动到 `NexAur Engine started`；Debug Vulkan validation 无新增 VUID，stderr 为空。

### 11.5 RT-04：TLAS Instance Build / Update Baseline

状态：已完成（2026-08-17）。

风险：高。

工作内容：

- 从 `VulkanDrawList::opaque_items` 构建 instance 数组。
- 实现并测试 GLM matrix -> `VkTransformMatrixKHR` 转换。
- 创建 instance device-address buffer、TLAS backing 和 scratch buffer。
- 第一版每帧 rebuild TLAS。
- 跳过未 ready 或 BLAS build 失败的 mesh，并记录数量。
- 空 scene 或无有效 BLAS 时选择 Raster fallback。

验收：

- translation、rotation、non-uniform scale 的实例位置正确。
- 增加、删除、移动 object 后 TLAS 与当前 draw list 一致。
- scene reload 后不引用旧 BLAS / TLAS。
- 多个 instance 引用同一个 BLAS 时遮挡结果正确。

实现结果：

- 新增 `VulkanTlasManager`，为每个 `VulkanFrameContext` slot 独立持有 instance buffer 和 TLAS，复用只在 slot fence 完成后发生。
- 新增 GLM `mat4` 到 `VkTransformMatrixKHR` 的显式 row-major 3x4 转换，支持 translation、rotation 和 non-uniform scale。
- 从当前 opaque draw list 逐项解析 BLAS device address；同一 BLAS 可被多个 instance 引用，transform 不影响 BLAS cache。
- instance buffer 使用 host-visible、device-addressable allocation，写入后 flush，并在 TLAS command 中加入 host write -> AS build read barrier。
- TLAS backing、scratch 和 build size query 接入现有 AS primitive；每个有效 frame slot 每帧 rebuild，旧 slot 资源继续使用 deferred destruction。
- 未 ready 或 BLAS 失败的 mesh 会被跳过并计数；空 scene 会清除当前 slot 的 ready 状态，避免引用旧 TLAS。
- Renderer diagnostics 暴露 source / built / skipped instance、build count、instance bytes、TLAS bytes 和 failure reason。

验证记录：

- Debug `NexAurRendererTests` 与 `Sandbox` 构建通过。
- RT-00 至 RT-04 五项 CPU focused contract test 通过。
- RT-capable GPU 上完成真实 TLAS instance buffer、重复 BLAS 引用、frame-slot rebuild、无效 BLAS skip 和空 scene reset smoke。
- Sandbox 完整启动到 `NexAur Engine started`；Debug Vulkan validation 无新增 VUID，stderr 为空。

### 11.6 RT-05：RenderGraph AS Synchronization

风险：高。

工作内容：

- Graph 增加 imported buffer / acceleration structure resource。
- Access model 增加 AS build input、AS build write 和 Ray Query shader read。
- 由 synchronization2 planner 生成 AS barrier。
- 增加 BLAS build -> TLAS build -> fragment read dependency。
- 增加纯逻辑 barrier planner focused test。

验收：

- AS build write -> fragment read 生成正确 stage / access。
- 同一 TLAS 连续 update / read 不会因“状态相同”漏 barrier。
- Validation synchronization 检查无新增错误。

状态：已完成（2026-08-17）。

实现结果：

- `VulkanPassGraph` 增加 imported buffer / acceleration structure resource、对应 handle、initial state 和 pass access API。
- `VulkanGraphStatePlanner` 增加 AS build input、AS build write、Ray Query shader read、scratch buffer 的 state / transition plan。
- `VulkanGraphExecutor` 接入 `VkBufferMemoryBarrier2` 和 AS 语义的 `VkMemoryBarrier2`，并保持 image transition 行为不变。
- BLAS batch build、TLAS build、TLAS instance upload 和 mesh upload 的 AS 依赖改为复用 graph planner 的 stage / access 状态计算。
- 新增 `RenderGraph AS planner` focused test，覆盖 host instance write、BLAS -> TLAS、TLAS -> fragment、scratch reuse 和连续 TLAS write hazard。

验证记录：

- Debug `NexAurRendererTests` 目标构建通过。
- `--render-graph-as-planner`、`--render-graph-state-planner` 和 `--tlas-instance-contract` 通过。
- `--tlas-instance-device` 通过；Debug Vulkan validation 无新增 synchronization VUID。

边界：

- RT-03 / RT-04 当前仍使用独立的一次性 build submission；RT-06 已将实际 debug shader 的 TLAS read 声明接入 frame graph，生产级 directional shadow query 将在 RT-07 接入光照路径。

### 11.7 RT-06：Ray Query Descriptor and Debug Shader

风险：中到高。

工作内容：

- Descriptor pool、layout、writer 支持 acceleration structure descriptor。
- 新增 Forward Ray Query graphics pipeline variant。
- 新增最小 Ray Query debug shader，只输出 directional visibility / hit 状态。
- 增加 debug view：Ray Query Visibility。
- Shader compile 输出保持显式映射并接受 SPIR-V validation。

验收：

- TLAS descriptor 更新无 VUID。
- Debug view 能稳定区分 hit / miss。
- RT-disabled 时不创建或绑定该 pipeline variant。
- Resize、scene reload 和开关切换后 descriptor 不引用 stale TLAS。

状态：已完成（2026-08-17）。

实现结果：

- `VulkanDescriptorSetLayoutId::RayTracingScene`、capability-gated descriptor pool capacity 和 `writeAccelerationStructure()` 已接入。
- 新增 per-frame `VulkanRayTracingSceneResource`，按 frame slot 分配并更新 TLAS descriptor；无效、BLAS 或空 scene 会主动清除 ready 状态。
- Forward pass 新增 `ForwardRayQuery` graphics pipeline variant 和 `RayQueryVisibility` debug shader，使用 first-hit opaque triangle query 输出 hit / miss 颜色。
- Feature plan、Editor debug view、diagnostics 和 RenderGraph 均接入 Ray Query debug；没有 capability、有效 TLAS 或 pipeline 时回退 Raster / Final Lit。
- Ray Query debug 的 TLAS 作为 imported graph AS resource，在 `ForwardScene` 中声明 `RayQueryShaderRead`，由 graph executor 生成 build-write -> fragment-read barrier。

验证记录：

- `NexAurVulkanShaders` 构建通过，`ForwardRayQuery` vertex / fragment SPIR-V 生成通过。
- Debug `NexAurRendererTests`、FrameFeaturePlan、RenderGraph AS planner 和 RT contract tests 通过。
- RT-capable GPU 上完成 TLAS descriptor allocation、两个 frame-slot 更新和空 scene stale descriptor 清除；Debug Vulkan validation 无新增 VUID。
- Sandbox 成功创建 Ray Query capability、Forward Ray Query pipeline 和 renderer；Debug validation stderr 为空。

边界：

- 当前 shader 只用于 visibility debug，不改变 directional / point / rect 光照；正式阴影选择、bias 和 fallback 逻辑属于 RT-07。

### 11.8 RT-07：Directional Ray Query Shadow

风险：高，第一项视觉功能。

工作内容：

- 将 Ray Query visibility 接入 directional direct-light BRDF。
- 使用 feature plan 在 Ray Query 和 CSM / PCSS 之间选择。
- 保持 point / rect light shadow 路径不变。
- 增加 shadow distance、normal bias、direction bias 和 enable mode。
- 增加 diagnostics 和 debug isolation。
- capability、TLAS readiness 或 feature build 失败时自动回退 CSM / PCSS。

视觉验收场景：

- 地面 + 单个 box occluder。
- 细长几何、近接触面和远距离遮挡。
- 旋转、non-uniform scale 和 mirrored transform。
- 多 instance 相互遮挡。
- Camera 移出 CSM cascade 后的 RT shadow distance 边界。
- RT on / off 同帧对照。

第一版接受 opaque-only 语义，但 UI / diagnostics 必须明确，不能把缺失 alpha-tested shadow 误判为通用光追阴影已完成。

### 11.9 RT-08：Compaction, Update, Lifetime and Profiling

风险：高。

工作内容：

- BLAS build batching 和可选 compaction。
- TLAS 在 instance capacity 稳定时使用 update / refit。
- Scratch buffer capacity 复用，避免每帧 allocation。
- 接入 GPU timestamp，统计 BLAS / TLAS build 和 Ray Query cost。
- 接入 RR-11 / RR-12 deferred destruction / FrameContext。
- 建立 BLAS memory budget、retirement 和 cache eviction。

验收：

- 未变化 mesh 不 rebuild BLAS。
- 只变化 transform 时不重新创建 BLAS。
- 多帧并行下 AS、backing 和 descriptor 无 use-after-free。
- 统计可以解释 build spike 和 steady-state cost。

### 11.10 RT-09：Optional RTAO

风险：中到高。

开始条件：RT-07 / RT-08 稳定，并且已有可复用 RT output、history 和 filter 基础。

工作内容：

- 半分辨率或可配置分辨率 Ray Query AO。
- depth / normal-aware spatial filter。
- 明确 AO 与现有 SSAO 的 fallback 和组合关系。
- 后续再引入 temporal accumulation，不在第一版强行混入。

### 11.11 RT-10：Ray-Traced Reflection Foundation

风险：很高。

开始条件：完成以下 scene shading 基础：

- GPU object / instance table。
- vertex / index address table。
- material parameter table。
- bindless 或等价的 texture indexing contract。
- reflection output storage image。
- motion vector、history validity 和 temporal reset contract。
- spatial / temporal denoiser。

推荐组合：

```text
SSR valid hit
  -> RT reflection for miss / off-screen
  -> Local Probe
  -> Global IBL
```

RT Reflection 不应直接塞入当前 post-process descriptor set。它应是独立 Feature，拥有 output、history、descriptor 和 diagnostics，再由 reflection composite 统一组合。

### 11.12 RT-11：Full RT Pipeline / Path Tracer

风险：很高，长期工作。

工作内容：

- `VK_KHR_ray_tracing_pipeline` capability negotiation。
- raygen / miss / closest-hit / any-hit shader library。
- RT pipeline desc、cache 和 pipeline layout。
- Shader Binding Table record、buffer、stride 和 alignment。
- `vkCmdTraceRaysKHR` 调度。
- Progressive accumulation、camera reset 和 sample counter。
- Editor viewport Path Tracer mode。

该工作包必须建立在 RT-00 至 RT-08 和 scene material table 之上，不作为第一轮光追开发范围。

## 12. 推荐执行顺序

```text
Renderer prerequisites
  RR-01 -> RR-02 -> RR-03 -> RR-05 -> RR-06

Ray Query foundation
  RT-00 -> RT-01 -> RT-02 -> RT-03 -> RT-04 -> RT-05 -> RT-06

First visual feature
  RT-07 -> RT-08

Optional hybrid effects
  RT-09 -> RT-10

Long-term pipeline
  RT-11
```

可以有限并行：

- RT-00 可在 RR-05 期间先完成 capability 设计和测试，但最终实现应进入 `VulkanDeviceContext`。
- RT-01 可与 RR-06 一起设计，不能再创建第二套 buffer ownership。
- RT-03 的纯 geometry descriptor helper 和 RT-04 的 matrix conversion test 可提前开发。

第一轮建议只审批：

```text
RT-00 Capability Baseline
RT-01 Device Address Buffer Foundation
RT-02 Acceleration Structure Primitive
```

完成并验证基础对象后，再审批 BLAS / TLAS 和第一个 shader feature，能显著缩小首次改动的 blast radius。

## 13. 风险矩阵

| 工作包 | 风险 | 主要问题 | 缓解方式 |
|---|---|---|---|
| RT-00 | 中 | 把 optional extension 变成 required，导致集显无法启动 | capability cluster + Raster fallback + force-disable test |
| RT-01 | 中 | allocation flag / usage 不完整，device address 无效 | 统一 buffer primitive + 非零 address validation |
| RT-02 | 中到高 | AS size、alignment、cleanup 错误 | RAII primitive + failure-path test + validation |
| RT-03 | 高 | BLAS cache stale、mesh eviction 后悬空 | stable identity + generation + retirement |
| RT-04 | 高 | matrix transpose 或 instance lifetime 错误 | 显式转换 helper + transform focused test |
| RT-05 | 高 | AS build / trace hazard 缺 barrier | synchronization2 planner + Sync Validation |
| RT-06 | 中到高 | descriptor pNext 生命周期或 shader capability 错误 | writer focused review + SPIR-V / Vulkan validation |
| RT-07 | 高 | acne、漏光、shadow distance 和 opaque-only 语义 | debug view + bias settings + CSM 对照场景 |
| RT-08 | 高 | compaction / update 后 use-after-free | frame retirement + GPU timing + staged enable |
| RT-09 | 中到高 | 噪声、AO double counting | 与 SSAO 隔离 + half-res debug + filter validation |
| RT-10 | 很高 | hit shading、bindless、history 和 denoiser同时扩张 | 先完成 scene table，拆分 output / temporal / composite PR |
| RT-11 | 很高 | SBT alignment、pipeline group 和 payload contract | 独立长期阶段，不与 Ray Query baseline 混合 |

## 14. 必要测试

测试遵循 focused 原则，不执行与改动无关的全量测试。

### 14.1 CPU focused test

- Capability cluster negotiation：完整、缺一项、force disabled。
- GLM matrix -> `VkTransformMatrixKHR`：identity、translation、rotation、non-uniform scale、mirrored transform。
- BLAS geometry descriptor：vertex stride / offset、primitive count、overflow 和空 mesh。
- TLAS dirty decision：transform、instance count、BLAS generation、scene identity。
- RenderGraph planner：
  - BLAS build write -> TLAS build read。
  - TLAS build write -> fragment shader read。
  - TLAS update write -> fragment shader read。
  - 相同 state 下的 write hazard。

### 14.2 Build 与 GPU smoke

- 只构建受影响的 Engine / Sandbox target。
- RT-capable GPU：Ray Query disabled / auto 各启动一次。
- 强制 fallback：在 RT-capable GPU 上模拟 unsupported 路径。
- Vulkan validation 和 synchronization validation 无新增错误。
- 空 scene、单 mesh、多 instance、scene reload、viewport resize 各做一次 smoke。

### 14.3 手工视觉验证

- Final Lit 在 RT disabled 时与 Raster baseline 一致。
- Ray Query Visibility debug view 的 hit / miss 可解释。
- RT shadow 与 CSM 对照时，遮挡方向和 instance transform 正确。
- Feature toggle 不产生闪烁、全黑、stale frame 或崩溃。
- Unsupported / not-ready / failed 路径确实显示 Raster fallback，而不是无阴影。

### 14.4 性能记录

RT-03 以后每个性能相关 PR 至少记录：

```text
BLAS build count and GPU time
TLAS build/update GPU time
BLAS/TLAS/scratch memory
TLAS instance count
Ray Query shadow GPU delta
steady-state rebuild count
```

第一版不以追求最低耗时为验收目标，但 steady state 不得反复 rebuild 未变化 BLAS，也不得每帧 device-wide idle。

## 15. 明确非目标

第一轮不包含：

- 完整 Path Tracer。
- Ray Tracing Pipeline 和 Shader Binding Table。
- Transparent、refraction、colored transmission。
- Alpha-tested any-hit / candidate material evaluation。
- Skinned mesh BLAS update。
- Procedural AABB geometry。
- Ray-traced global illumination。
- 生产级 RT reflection denoiser。
- 多 GPU、跨 Vulkan device AS sharing。
- DirectX 12 / Metal backend 光追抽象。
- 用通用大而全 RHI 重写现有 Renderer。

## 16. PR 组织原则

- 每个 RT 工作包拆成可独立验证的小 PR；RT-03、RT-04、RT-05 如改动过大应继续拆分。
- Capability、resource primitive、visual feature 不混在同一个 PR。
- 结构迁移 PR 不修改 shadow bias、lighting 参数或视觉算法。
- Shader 算法 PR 不顺便移动 Backend ownership。
- 每个 PR 更新对应 diagnostics 和本工作包状态。
- 新 Vulkan internal 类型默认不导出 `NEXAUR_API`。
- 不复制 VMA allocator、descriptor allocator、graph executor 或 shader library 建立“光追专用平行基础设施”。

## 17. 第一阶段完成标准

完成 RT-00 至 RT-08 后应满足：

1. Raster-only device 或 force-disabled 模式可以正常运行，画面保持现有 baseline。
2. 支持 Ray Query 的 device 可以按 capability 自动启用，不需要把光追 extension 设为全局 required。
3. 静态 opaque mesh 只构建一次 BLAS，并由稳定 cache / generation 管理。
4. TLAS 与当前 prepared frame 的 instance 和 transform 一致。
5. AS build 和 shader read 由 RenderGraph synchronization2 模型正确同步。
6. Forward Ray Query variant 能生成可诊断的 directional shadow visibility。
7. Capability、TLAS not-ready、build failure 和 runtime disable 均会回退 CSM / PCSS。
8. Validation layer 和 synchronization validation 不出现新增错误。
9. 多帧并行启用前，AS 资源已接入 deferred destruction 和 FrameContext 生命周期。
10. Debug snapshot 可以解释当前是否启用 RT、为什么回退、构建了多少 AS 以及主要 GPU 成本。

## 18. 最终结论

NexAur 当前 Renderer 能支持硬件光追开发，但正确起点是 Ray Query Foundation，而不是直接实现 Path Tracer。

推荐先完成 Renderer 的 access model、synchronization2、canonical frame、backend responsibility 和 common resource primitive，再依次落地 capability、device-address buffer、BLAS、TLAS、graph synchronization 和 descriptor。第一个合入的视觉功能应是带 CSM / PCSS fallback 的 Directional Ray Query Shadow。

这条路线能复用现有 Forward、SSR、Reflection Probe 和 IBL，同时把风险集中在可验证的小工作包中。等 AS 生命周期、scene table、history 和 denoiser 成熟后，再进入 RT Reflection 和 Full Ray Tracing Pipeline。

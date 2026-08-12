# NexAur Renderer Architecture Rebuild Plan

日期：2026-07-10
状态：Draft，待审核

## 1. 文档目的

本文档记录 NexAur 当前渲染模块的架构评估、主要问题、目标结构和推荐重构顺序。

这轮工作的目标不是重写 Renderer，也不是改变现有画面效果，而是在保留当前外部契约和 Vulkan 主线能力的前提下，逐步解决以下问题：

- RenderGraph 的读写与同步语义不完整。
- Renderer frontend 的 frame contract 与 Vulkan clip-space 转换边界不够明确。
- `VulkanRendererSystem::Backend` 承担过多职责。
- AO、SSR、Bloom、SMAA 等效果虽然已有 Pass 和 Target，但功能所有权仍散落在 Backend 中。
- 多个 Target 重复实现 image、memory、sampler 和 layout 生命周期。
- 当前单帧同步模型依赖大量 fence wait、`vkDeviceWaitIdle()` 和同步 readback。
- Renderer 内部仍直接访问 `AssetManager` 单例。
- Renderer facade、Shader 声明、构建目标、目录和测试组织仍有继续收口空间。

所有工作应拆成可独立验证的小 PR。结构迁移和行为修改不能混在同一个 PR 中。

## 2. 当前架构判断

### 2.1 总体评价

当前 Renderer 的外部架构方向正确，内部 Vulkan 执行层已经达到需要第二轮整理的节点。

可以概括为：

```text
外部模块边界：清晰
渲染数据流：清晰
Pass / Target / Resource 基础分层：合理
Frontend contract：存在 backend-neutral / Vulkan convention 混合
Vulkan 核心协调层：职责过度集中
底层 GPU resource primitive：重复实现较多
资源同步与生命周期：依赖单帧串行假设
扩展新渲染功能的修改面：偏大
```

当前代码不是无序代码，也不需要推倒重来。应保留现有外部边界，重点收缩 Vulkan backend 的内部职责。

### 2.2 当前主数据流

```text
SceneV2 / ECS
  -> RenderDataPacket
  -> RenderSceneFrame
  -> VulkanDrawList
  -> VulkanPassGraph
  -> Vulkan Pass
  -> Viewport / Swapchain
```

这条链路的方向是正确的：

- Scene 只输出 backend-neutral 数据和 `AssetHandle`。
- Renderer frontend 负责清洗、裁剪和组织渲染帧数据。
- Vulkan frontend 负责把 frame data 解析成 draw list。
- GPU 资源由 `VulkanRenderResourceCache` 解析和持有。
- Editor 通过 `RendererService`、`RendererDebugService` 与 Renderer 交互，不直接持有 Vulkan 对象。

### 2.3 值得保留的设计

1. `RendererService` 隔离 Editor / Runtime 与具体图形 API。
2. `VulkanRendererSystem` 使用 PImpl 隐藏 Vulkan instance、device、swapchain 等类型。
3. `AssetHandle` 与 Vulkan GPU resource 分离。
4. `passes/`、`targets/`、`resources/`、`descriptors/`、`pipeline/`、`graph/` 已具备合理的领域目录。
5. Pass 普遍使用显式 Context，不从深层代码读取全局上下文。
6. `RenderSettings` 与只读 `RendererDebugSnapshot` 已分离。

后续重构不得破坏这些边界。

## 3. 当前主要问题

### 3.1 RenderGraph 只完整表达了 layout，没有完整表达 hazard

当前 `VulkanGraphPassBuilder::readImage()` 和 `writeImage()` 最终写入相同的 `VulkanGraphImageAccess` 数据，没有保存访问类型。

`VulkanGraphExecutor` 在新旧 layout 相同时直接跳过 barrier，因此无法可靠表达：

- Write -> Read，layout 不变。
- Write -> Write，layout 不变。
- Read -> Write，layout 不变。
- 不同 mip、array layer 或 aspect 的独立访问。
- 更准确的 producer / consumer pipeline stage。

当前实现更接近 pass sequencer 和 image layout tracker，还不是完整的 RenderGraph hazard model。

这是正确性问题，应优先于多帧并行和性能优化处理。

### 3.2 VulkanRendererSystem::Backend 职责过度集中

`vulkan_renderer_system.cpp` 已超过 5000 行，Backend 同时负责：

- Vulkan instance、surface、physical device、logical device。
- Queue、swapchain、command pool、command buffer 和同步对象。
- Viewport resize 和所有 render target 生命周期。
- RenderGraph 构建与执行。
- Directional、point、rect shadow frame 构建。
- AO、SSR、Bloom、SMAA 和 post-process 调度。
- Reflection Probe capture queue、residency 和 bake。
- Picking readback。
- ImGui renderer 接入。
- Debug snapshot 收集。

函数本身命名较清楚，但对象内同时存在过多状态和所有权。阅读单个功能时需要理解整个 Backend 的生命周期假设。

### 3.3 渲染效果缺少完整 Feature ownership

当前 AO、SSR、Bloom、SMAA 已分别拥有 Pass 和 Target，但 Backend 仍负责：

- 创建和关闭 Pass / Target。
- resize。
- descriptor input 更新。
- 是否执行的判断。
- graph resource 注册和 pass 插入。
- debug snapshot 数据收集。

因此新增一个 screen-space effect 通常需要修改 settings、target、pass、Backend init、shutdown、resize、graph、diagnostics、Editor panel 和 CMake shader 列表。

需要把这些职责组合成具体 Feature 对象，但第一版不需要引入通用虚基类或动态插件系统。

### 3.4 单帧同步假设限制性能和资源生命周期

当前 backend 只有一套主 command buffer、semaphore 和 fence，并在每帧等待 in-flight fence。

另外还存在：

- Target resize 前的 `vkDeviceWaitIdle()`。
- Texture / mesh / environment upload 后的同步 fence wait。
- Reflection Probe immediate command submit 和 wait。
- Picking readback 前后的 device / queue idle。

这保证了当前资源替换和销毁相对简单，但隐藏了一个重要假设：资源修改时 GPU 已经不再使用旧对象。

因此不能直接把帧数改成 2 或 3。多帧并行前必须先建立 deferred deletion 和 per-frame ownership。

### 3.5 Renderer 仍直接依赖 AssetManager 单例

`RendererModule` 已经处于模块组合层，但 Vulkan backend 和 resource cache 仍存在 `AssetManager::getInstance()` 调用。

问题包括：

- Renderer 的依赖没有完全通过构造或初始化参数表达。
- focused test 难以注入替代资源服务。
- Asset 生命周期和 Renderer 生命周期之间仍存在隐式全局关系。
- 后续异步加载和资源淘汰会更难落地。

Renderer 重构阶段先完成依赖注入，不在本计划中一次性实现完整异步 Asset Pipeline。

### 3.6 RendererService 能力范围正在扩大

当前 `RendererService` 同时包含：

- Frame render。
- Viewport size / output。
- Picking。
- Reflection Probe capture。
- ImGui context 生命周期。

现阶段仍可工作，但继续添加 bake、streaming、GPU profiling 或截图能力后会逐渐成为综合接口。

是否拆分 facade 应以真实调用方为依据，不为了接口数量而拆分。

### 3.7 Renderer frontend contract 存在层次混合

当前帧准备顺序是：

```text
RenderDataPacket
  -> VulkanRenderDataTranslator::buildRenderView()
  -> RenderSceneFrameBuilder::buildRenderSceneFrame()
  -> VulkanDrawListBuilder::buildDrawList()
```

这里存在两个不够直观的点：

- `VulkanRenderDataTranslator` 先把 camera projection 转换成 Vulkan clip-space，随后通用 `RenderSceneFrameBuilder` 再消费这个已经带 backend convention 的 `RenderView`。
- `RenderSceneFrame` 没有包含 `scene_id`、`RenderSettings` 和 debug options，Backend 后续仍需同时携带原始 `RenderDataPacket`、`RenderSceneFrame`、`VulkanDrawList` 和 settings。

这会让 frontend 看起来 backend-neutral，实际又依赖 Vulkan 已完成的投影转换；也会让 Reflection Probe、graph builder 和 diagnostics 的函数参数持续增长。

需要建立唯一且有文档的 frame contract：

```text
Scene extraction data
  -> backend-neutral RenderSceneFrame
  -> VulkanPreparedFrame / VulkanDrawList
  -> Vulkan frame graph
```

`RenderView` 的 handedness、depth range、Y direction 和 projection ownership 必须有明确约定。不能再由类型名称之外的隐式调用顺序决定。

RR-03 canonical contract:

- `RenderView` is right-handed, uses `-Z` as view forward and `+Y` as up, keeps clip-space Y up, and uses NDC depth `[-1, 1]`.
- Matrices use GLM column-major convention. The canonical composition is `view_projection = projection * view`.
- `RenderSceneFrameBuilder` owns camera validation, canonical view construction, and all canonical derived matrices. `RenderDataPacket` carries only source camera matrices and no cached VP matrix.
- `VulkanRenderView` is a distinct Vulkan-native type. It keeps right-handed view space but uses flipped clip Y and NDC depth `[0, 1]`.
- `VulkanRenderDataTranslator` owns the single `Y flip + depth remap` conversion. Vulkan passes consume `VulkanRenderView`, never canonical `RenderView`.
- `RenderSceneFrame` owns `frame_serial`, `scene_id`, settings, debug options, source diagnostics, and complete Reflection Probe references. `VulkanPreparedFrame` owns the canonical scene snapshot and the Vulkan draw list derived from that snapshot.

### 3.8 Shadow frame 构建算法放错了所有权层

Directional cascade split、frustum corner、texel snapping、point light cube face 和 rect light projection 等 CPU 数学逻辑，目前集中在 `vulkan_renderer_system.cpp`。

这些逻辑主要依赖 `RenderView`、light data 和 `RenderSettings`，并不拥有 Vulkan resource。它们应进入可独立测试的 shadow frame builder，而不是继续作为 Backend 私有方法。

建议目标：

```text
Renderer/frontend/render_shadow_frame_builder.h/.cpp
  buildDirectionalShadowFrame()
  buildPointShadowFrame()
  buildRectShadowFrame()
```

如果其中某一步确实依赖 Vulkan clip convention，应通过显式 convention 参数或 Vulkan translation stage 表达，而不是依赖调用者已经悄悄修改 projection。

### 3.9 Vulkan 基础资源与错误处理重复实现

当前多个 CPP 各自实现 `checkVk()`，多个 Target 各自实现：

- `findMemoryType()`。
- `OwnedImage`。
- `vkCreateImage()` / `vkAllocateMemory()` / `vkBindImageMemory()`。
- image view 和 sampler 创建。
- cleanup 和 layout 字段维护。

AO、SSR、Bloom、SMAA、scene color、viewport、shadow、picking 和 Reflection Probe target 都存在相似代码。与此同时 `VulkanRenderResourceCache` 已使用 VMA，Target 和部分 frame resource 仍直接管理 `VkDeviceMemory`。

这类重复会放大以下风险：

- partial initialization cleanup 不一致。
- resize 失败后对象状态不一致。
- memory type、allocation 和 debug naming 策略分散。
- deferred destruction 需要为每种 Target 重复实现。
- GPU memory diagnostics 无法统一。

应建立少量 move-only 基础对象，而不是建立复杂 RHI 类层级：

```text
VulkanResult / VulkanError utilities
VulkanGpuAllocator
VulkanOwnedImage
VulkanOwnedBuffer
VulkanOwnedSampler
```

长期应统一通过 VMA 或单一 allocator boundary 分配普通 image / buffer。Swapchain image 等非 owning handle 继续使用独立 view 类型，不伪装成 owned resource。

### 3.10 Frame graph 构建和 Feature 决策分散

Backend 同时包含 `buildViewportRenderGraph()`、`buildSwapchainRenderGraph()` 和大量 `add*Pass()`。两个 graph builder 共享大部分 scene pipeline，但 final output 和 ImGui/present tail 不同。

另外 `shouldRenderAo()`、`shouldRenderSsr()`、`shouldRenderBloom()`、`shouldRenderSmaa()` 与 debug isolation 判断分散在 Backend 中。Graph、debug snapshot 和最终输出容易对“本帧到底启用了什么”产生不同理解。

建议每帧先生成一个不可变计划：

```cpp
struct VulkanRenderFeaturePlan {
    bool render_ao = false;
    bool render_ssr = false;
    bool render_bloom = false;
    bool render_smaa = false;
    bool isolate_forward_debug = false;
    RenderEffectDebugView debug_view = RenderEffectDebugView::FinalLit;
};
```

随后由 `VulkanFrameGraphBuilder` 构建：

```text
common scene graph
  -> viewport output tail
  or
  -> direct swapchain output tail
```

Feature plan、graph 和 diagnostics 必须使用同一份决策结果。

### 3.11 Prepared frame 与 GPU resource lifetime 没有正式契约

`VulkanDrawList` 保存指向 resource cache 内对象的裸指针。单帧 fence 模型下，Backend 通过等待 GPU 后再替换资源，使这些指针通常保持有效。

多帧并行后必须明确：

- Prepared frame 从创建到对应 frame fence 完成期间保持有效。
- Draw item 引用的 mesh、material 和 environment 不能提前销毁或移动。
- Material generation 替换、Probe eviction 和 cache clear 必须进入 retirement queue。
- Resource cache 是 persistent resource owner；FrameContext 是本帧临时资源 owner；Feature 是固定 render target owner；SwapchainManager 是 swapchain image/view owner。

这份 lifetime contract 应先落到设计和 focused test，再进入 Frames in Flight 工作。

### 3.12 构建、Shader 和目录边界没有完全收口

所有 Engine 代码最终进入一个 `NexAurEngine` target，部分第三方依赖以 PUBLIC 方式传播。

Shader 通过 `NexAurEngine` 的 `POST_BUILD` 命令统一编译，无法对每个 HLSL 输出建立准确增量依赖。

Shader program 到输出文件的映射还同时维护在 CMake compile command 和 `VulkanShaderLibrary` switch 中。新增 shader 容易漏改其中一处。

目录边界目前主要依赖约定，CMake 尚不能阻止 Runtime 或 Editor 错误依赖 Vulkan backend。

Renderer 根目录还保留空的 `Passes/`、`Resources/`、`RHI/`、`Platform/OpenGL/`、`Platform/Vulkan/` 旧目录，以及被 Git 跟踪的 `.DS_Store`。这些残留会误导新的代码归属判断，应在独立 hygiene PR 中清理。

## 4. 目标结构

### 4.1 顶层结构

```text
RendererModule
  owns RendererService facade
  owns RendererDebugService facade
  injects WindowService and AssetService

Renderer Frontend
  owns canonical RenderView contract
  builds backend-neutral RenderSceneFrame
  builds directional / point / rect shadow frame data

VulkanRendererSystem
  public backend facade
  forwards lifecycle and frame requests

VulkanBackend
  owns VulkanDeviceContext
  owns VulkanGpuAllocator
  owns VulkanSwapchainManager
  owns VulkanFrameScheduler
  owns VulkanRenderPipeline
  owns VulkanRenderResourceCache
  owns VulkanReflectionProbeManager
  owns VulkanPickingManager
  owns VulkanDiagnosticsCollector
  owns VulkanImGuiRenderer

VulkanRenderPipeline
  owns VulkanFrameGraphBuilder
  builds immutable VulkanRenderFeaturePlan
  owns common scene passes
  owns render features
  builds and executes frame graph

Render Features
  VulkanAoFeature
  VulkanSsrFeature
  VulkanBloomFeature
  VulkanSmaaFeature
  VulkanPostProcessFeature
```

### 4.2 类职责建议

#### RenderFrameBuilder

- 从 `RenderDataPacket` 生成完整、不可变、backend-neutral 的帧描述。
- 统一携带 `frame_serial`、`scene_id`、`RenderSettings`、debug options、view、objects、lights、environment 和 probes。
- 明确 `RenderView` 的 canonical clip-space contract。
- 不包含 Vulkan handle、descriptor 或 GPU resource pointer。

#### RenderShadowFrameBuilder

- 构建 directional cascade、point cube face 和 rect light shadow matrices。
- 负责 shadow budget 对应的 CPU frame data，不创建 shadow target。
- 使用纯输入/纯输出接口，支持 focused math test。
- 不依赖 `VulkanRendererSystem`、resource cache 或 command buffer。

#### VulkanDeviceContext

- Instance、surface、physical device、logical device。
- Queue family 和 queue。
- API feature / extension 校验。
- VMA allocator 所需基础句柄。

#### VulkanGpuAllocator

- 统一普通 owned image / buffer 的 VMA allocation。
- 统一 allocation debug name、memory usage 和 diagnostics。
- 为 move-only `VulkanOwnedImage` / `VulkanOwnedBuffer` 提供 allocation boundary。
- 不拥有 swapchain image，也不决定资源何时 retirement。

Image view 由 `VulkanOwnedImage` 使用 device 创建并随 image 销毁；sampler 使用独立 move-only `VulkanOwnedSampler`。Allocator 不负责没有 memory allocation 的 Vulkan object。

#### VulkanSwapchainManager

- Swapchain 创建、重建和关闭。
- Swapchain images、format、extent 和 image layouts。
- Acquire / present 结果处理。

#### VulkanFrameScheduler

- `VulkanFrameContext` 数组。
- Per-frame command pool / command buffer。
- Fence / semaphore。
- Deferred deletion queue。
- Frame index 和 submitted frame tracking。

#### VulkanRenderPipeline

- `RenderSceneFrame` / `VulkanDrawList` 消费。
- Shadow、forward、debug、post-process 顺序。
- Graph resource 注册与 graph 构建。
- Feature 调度，不直接拥有 Editor 或 Asset 逻辑。

#### VulkanFrameGraphBuilder

- 只消费 prepared frame、feature plan 和各 Feature 暴露的 graph resource view。
- 构建 common scene graph。
- 根据 output route 附加 viewport 或 direct swapchain tail。
- 不创建 persistent GPU resource，不执行 synchronous upload。

#### VulkanReflectionProbeManager

- Capture request queue。
- Runtime capture state。
- Residency、pin、LRU 和 over-budget 策略。
- Capture target 与 bake generation。
- Scene identity 同步。

#### VulkanPickingManager

- Picking target。
- Request 和 readback state。
- 后续异步 staging readback。

#### VulkanDiagnosticsCollector

- 从各子系统收集普通数字、枚举和字符串。
- 生成 backend-neutral `RendererDebugSnapshot`。
- 不拥有可写 RenderSettings。

### 4.3 Feature 对象原则

Feature 对象应组合现有 Pass 和 Target，而不是立即引入复杂继承体系。

示例：

```cpp
class VulkanSsrFeature {
public:
    bool initialize(const VulkanFeatureContext& context);
    bool resize(uint32_t width, uint32_t height);
    bool prepare(const VulkanSsrPrepareContext& context);
    bool addPasses(VulkanPassGraph& graph, const VulkanSsrGraphContext& context);
    RendererDebugSsrStats getDebugStats(const RenderSettings& settings) const;
    void shutdown();

private:
    VulkanSsrTarget m_target;
    VulkanSsrPass m_pass;
};
```

第一版保持具体类型和显式调用。只有多个 Feature 出现稳定重复协议后，再评估公共 interface。

### 4.4 所有权和依赖规则

目标架构必须遵守以下所有权：

| 对象 | 唯一所有者 | 生命周期 |
|---|---|---|
| Instance / device / queues | `VulkanDeviceContext` | Renderer backend lifetime |
| VMA allocator | `VulkanGpuAllocator` | Device 之后创建，device 之前销毁 |
| Swapchain image / view | `VulkanSwapchainManager` | Swapchain generation |
| Command pool / buffer / fence / semaphore | `VulkanFrameContext` | Frame slot lifetime |
| Persistent model / mesh / material / texture / environment | `VulkanRenderResourceCache` | Cache entry lifetime |
| AO / SSR / Bloom / SMAA targets and passes | 对应 `Vulkan*Feature` | Feature lifetime |
| Shadow targets and passes | `VulkanShadowFeature` | Feature lifetime |
| Reflection Probe runtime environment | `VulkanReflectionProbeManager` | Scene/capture residency lifetime |
| Picking target / readback slots | `VulkanPickingManager` | Manager / request lifetime |
| Per-frame transient allocations | `VulkanFrameContext` | 对应 frame fence 完成前 |

依赖方向固定为：

```text
Editor / Runtime
  -> Renderer public services and data

Renderer frontend
  -> Renderer public data
  -> Core / Resource handles

Vulkan frontend
  -> Renderer frontend data
  -> Vulkan resources / core

Vulkan features / frame graph
  -> Vulkan frontend prepared frame
  -> Vulkan passes / targets / core

Vulkan passes / resources / targets
  -> Vulkan core primitives
```

禁止出现：

- Renderer frontend include `Function/Renderer/Vulkan/**`。
- Pass / Feature 访问 `ModuleRegistry`、Editor 或全局上下文。
- Resource wrapper 直接决定 frame wait。
- Diagnostics 反向拥有或修改 Feature。
- Graph callback 引用超出本次 graph build/execute 作用域的临时对象。

### 4.5 目录整理目标

不进行一次性大规模改名。新增代码按以下归属放置，并在迁移完成后删除空旧目录：

```text
Renderer/
  data/                 backend-neutral frame contracts
  frontend/             frame/view/shadow builders
  Vulkan/
    core/               result, device, allocator, owned image/buffer
    frame/              frame context, scheduler, frame graph builder
    features/           AO, SSR, Bloom, SMAA, Shadow, PostProcess
    graph/              graph model, barrier planner, executor
    passes/             focused command recording units
    resources/          persistent GPU resource types/cache
    targets/            target types not yet absorbed by a Feature
    diagnostics/        snapshot collection
    ui/                 ImGui bridge
```

目录只是职责结果，不是重构目标本身。只有当一个类已经具有明确所有权时才移动文件。

### 4.6 代码组织规则

#### Context 规则

- `*Context` 只携带完成一次操作所需的窄依赖。
- Context 中的裸指针和引用全部是 non-owning，必须由命名和注释说明有效期。
- 初始化 Context 可以在对象初始化成功后保存稳定依赖，但不能包含 `ModuleRegistry`、Backend 全对象或任意服务查询入口。
- Frame Context 与 initialization Context 分开，不使用一个万能 Context 覆盖所有阶段。

#### 生命周期规则

- Vulkan owning type 必须 move-only，析构安全，`shutdown()` 幂等。
- Partial initialization failure 必须恢复到可安全析构、可再次初始化的状态。
- `resize()` 成功后整体提交新资源；失败时不能留下半套新旧资源。
- Owner 负责决定何时销毁，resource wrapper 只负责如何销毁。
- 非 owning view 不提供销毁 API，也不伪装成 owning resource。

#### 错误处理规则

- `VkResult` 文本、validation 和 vk-bootstrap error formatting 只维护一份。
- 在拥有完整操作语义的边界记录错误，避免 low-level 和 caller 对同一失败重复输出多条日志。
- Public service 返回 backend-neutral 状态；Vk 类型和 VkResult 不越过 Vulkan facade。
- 不使用 silent fallback 掩盖 initialization、graph synchronization 或 resource lifetime 错误。

#### Feature 规则

- 一个 Feature 拥有自己的固定 Pass、Target 和 descriptor input 状态。
- Feature 不拥有 device、swapchain、global resource cache 或 frame scheduler。
- Feature 只通过 graph contribution 声明 image access，不在 graph 外私自转换同一 graph resource layout。
- 跨 Feature 资源通过显式 output view 连接，不直接访问对方 private target。
- Debug stats 是 Feature 状态的只读投影，不反向驱动 Feature 行为。

#### 可读性规则

- 不以文件行数作为唯一拆分标准，以“一个状态只有一个 owner”为标准。
- 对连续出现三次以上且语义一致的 Vulkan 创建/销毁流程再提取 helper。
- 不为只有一个实现的流程预先创建抽象基类。
- Structural PR 不顺手改视觉参数、Shader 算法或命名无关代码。
- 新增类型放入最窄可见范围，Vulkan internal class 默认不导出 `NEXAUR_API`。

## 5. 实施工作包

### 5.1 RR-00：Renderer Rebuild Baseline

风险：低。

工作内容：

- 固定当前必要 build、smoke 和 GPU/editor 手工验证矩阵。
- 为 RenderGraph state planner 建立不依赖 Vulkan device 的 focused test 入口。
- 记录 Final Lit、SSR、AO、Bloom、SMAA、shadow、picking 和 Reflection Probe 的当前基线。
- 开启 Vulkan validation layer 验证路径，记录当前已知 warning / error。

本工作包不修改渲染行为。

### 5.2 RR-01：RenderGraph Access Model

风险：中。

工作内容：

- 为 graph access 增加 `Read`、`Write`、`ReadWrite`。
- Graph image state 保存 layout、stage、access mask 和最近访问类型。
- 增加 aspect、base mip、mip count、base layer、layer count。
- 提取纯逻辑 barrier planner。
- 覆盖以下 focused test：
  - ColorWrite -> ShaderRead。
  - DepthWrite -> ShaderRead。
  - ColorWrite -> ColorWrite，layout 相同。
  - ShaderRead -> ColorWrite。
  - 不重叠 subresource 不生成错误依赖。

约束：

- 保持现有 pass 顺序。
- 保持单 command buffer 和单帧 fence。
- 第一版允许保守 barrier，不在同一 PR 优化 barrier 数量。

### 5.3 RR-02：Synchronization2 Migration

风险：中。

工作内容：

- 将 graph executor 的 image barrier 迁移到 `vkCmdPipelineBarrier2`。
- 使用 `VkImageMemoryBarrier2` 和 `VkDependencyInfo`。
- 保持非 graph 的 Reflection Probe capture transition 暂时不变。
- validation layer 不得新增同步错误。

禁止混入：

- Frames in flight。
- Async upload。
- Pass 顺序修改。
- 视觉参数调整。

### 5.4 RR-03：Canonical Frame Contract

风险：中。

工作内容：

- 明确并记录 `RenderView` 的 handedness、depth range、Y direction 和 matrix ownership。
- 让 backend-neutral `RenderSceneFrame` 携带 `frame_serial`、`scene_id`、settings 和 debug options。
- 调整 frame builder / Vulkan translator 顺序，使 Vulkan clip conversion 只发生在 Vulkan boundary。
- Backend 后续只消费 prepared frame，不再同时向深层函数传递原始 `RenderDataPacket`。
- 删除空的 `VulkanRenderDataTranslator::resetFrame()` 或赋予真实、可验证的 frame state 职责。

约束：

- 不修改现有相机画面、depth convention 和 shader matrix 结果。
- 通过 camera projection、SSR depth reconstruction 和 picking focused test 固定当前结果。
- 不在这一 PR 引入 render thread 或 packet queue。

### 5.5 RR-04：Shadow Frame Builder Extraction

风险：低到中。

工作内容：

- 提取 directional cascade split、frustum corner 和 texel snapping。
- 提取 point shadow cube face 和 rect shadow projection。
- 新增 `RenderShadowFrameBuilder` 纯输入/纯输出接口。
- Main viewport 和 Reflection Probe capture 共用同一个 builder。
- 为 cascade count、split depth、stabilization、point face orientation 和 rect projection 增加 focused math test。

该工作包只移动 CPU 算法，不移动 shadow target、pass 或 descriptor。

### 5.6 RR-05：Vulkan Backend Responsibility Split

风险：中。

工作内容：

- 将 PImpl `Backend` 移入独立实现文件，保留 `VulkanRendererSystem` facade。
- 从 Backend 提取 `VulkanDeviceContext`。
- 提取 `VulkanSwapchainManager`。
- 提取 `VulkanDiagnosticsCollector`。
- 集中 `VkResult` / vk-bootstrap error 文本和 capability helper。
- 保持 `RendererService` 和现有 runtime 行为不变。

这是结构迁移工作。除修复迁移中暴露的明确 bug 外，不改变渲染结果、pass 顺序或同步模型。

### 5.7 RR-06：Common Vulkan Resource Primitives

风险：中到高。

工作内容：

- 建立 `VulkanGpuAllocator`，统一普通 image / buffer allocation。
- 建立 move-only `VulkanOwnedImage`、`VulkanOwnedBuffer` 和 `VulkanOwnedSampler`。
- 统一 image view 创建、layout state、debug name 和 cleanup。
- 将 Target 中重复的 `OwnedImage`、`findMemoryType()` 和直接 `VkDeviceMemory` 管理逐步迁移到公共 primitive。
- `VulkanRenderResourceCache` 不再独占 allocator，改为消费 backend-owned allocator。

迁移顺序建议：

```text
SceneColor / Viewport
  -> AO / SSR
  -> Bloom / SMAA
  -> Shadow / Picking
  -> Reflection Probe capture
  -> frame lighting / debug buffers
```

每个 PR 只迁移一组资源。禁止为了统一接口创建深继承的通用 Texture/Framebuffer 类层级。

### 5.8 RR-07：Reflection Probe and Picking Ownership

风险：中。

工作内容：

- 将 Reflection Probe queue、capture state、residency、target 和 runtime environment 所有权移动到 `VulkanReflectionProbeManager`。
- 将 picking target、request 和 readback slot 移动到 `VulkanPickingManager`。
- 复用公共 Vulkan resource primitives。
- 保持现有 `ReflectionProbeCaptureState` 和 `ViewportPickResult` 契约。
- 保持 transform dirty、capture freshness、scene identity、bake pin 和 lifetime 行为。

### 5.9 RR-08：Frame Feature Plan and Graph Builder

风险：中。

工作内容：

- 引入不可变 `VulkanRenderFeaturePlan`，集中 debug isolation 和 Feature enable 决策。
- 提取 `VulkanFrameGraphBuilder`。
- 合并 viewport / direct swapchain graph 的 common scene pipeline。
- 将 final viewport、ImGui 和 present 组织为不同 output tail。
- Graph、diagnostics 和 final output 使用同一 Feature plan。

本工作包只整理 graph orchestration，不改变 Feature 内部资源所有权。

### 5.10 RR-09：Render Feature Ownership

风险：中。

工作内容：

- 依次组合 AO、SSR、Bloom、SMAA、Shadow 和 post-process 的 Pass / Target。
- Feature 自己处理 init、shutdown、resize、prepare、graph contribution 和 debug stats。
- Pipeline 只负责 Feature 顺序和跨 Feature 输入连接。
- Feature 不直接读取 `RenderDataPacket`、Editor state 或 ModuleRegistry。

建议每个 PR 只迁移一个 Feature：

```text
AO -> SSR -> Bloom -> SMAA -> Shadow -> PostProcess
```

第一版使用具体 Feature 类型和显式调用，不引入 `IRenderFeature` 动态注册系统。

### 5.11 RR-10：Asset Dependency Injection

风险：中。

工作内容：

- `RendererModule` 从 ModuleRegistry 获取 Asset 服务。
- 通过 Vulkan renderer 初始化上下文传入资源服务。
- `VulkanDrawListBuilder` 和 `VulkanRenderResourceCache` 不再调用 `AssetManager::getInstance()`。
- 保持 `AssetHandle` 和现有 CPU cache 行为不变。

非目标：

- 完整去除 AssetManager 单例。
- 异步导入。
- Disk asset registry。
- GPU streaming。

### 5.12 RR-11：Deferred GPU Destruction Baseline

风险：高。

工作内容：

- 在仍保持单帧 in-flight 的情况下引入 retirement queue。
- 为 prepared frame 引用的 persistent resource 定义 fence/serial lifetime。
- Material generation 更新不立即销毁旧 GPU resource。
- Resize、hot edit、probe eviction 和 cache clear 使用统一 retirement API。
- 为资源 retirement 增加 submitted/completed frame serial 和调试计数。

这是多帧并行的必要前置工作。不能用 `shared_ptr` 无限制延长所有 GPU 资源生命周期来替代明确 retirement。

### 5.13 RR-12：Vulkan FrameContext

风险：高。

工作内容：

- 引入 2 到 3 个 `VulkanFrameContext`。
- 每帧独立 command pool、command buffer、fence 和 semaphore。
- Per-frame descriptor / transient buffer 按 frame ownership 管理。
- Swapchain image 单独跟踪占用 fence。
- Deferred deletion 在对应 GPU serial 完成后执行。
- Debug snapshot 增加 frame wait 和 in-flight 状态。

禁止通过增加 `vkDeviceWaitIdle()` 规避生命周期问题。

### 5.14 RR-13：Async Picking and Upload

风险：高。

工作内容：

- Picking 使用 staging buffer 和延迟一帧或多帧返回。
- Mesh / texture upload 使用持久 upload command pool 和 staging ring。
- 上传完成前 Renderer 使用 fallback resource。
- 添加每帧 upload byte / request budget。
- 明确 `Pending`、`Ready`、`Failed` 和 cancellation 状态。

本工作包必须在 FrameContext 和 deferred destruction 稳定后执行。

### 5.15 RR-14：Facade, Shader, Build and Directory Polish

风险：低到中。

工作内容：

- 删除 `.DS_Store` 和空 legacy Renderer 目录。
- 根据真实调用方评估拆分 `ViewportRendererService` 和 `ReflectionProbeCaptureService`。
- 仅 facade 和跨模块 contract 保留必要的 `NEXAUR_API`。
- 建立 `NexAurRendererFrontend`、`NexAurRendererVulkan` 等内部 CMake target。
- 收窄 Vulkan、GLFW、ImGui 等依赖传播范围。
- 每个 HLSL 输出使用 `add_custom_command(OUTPUT ... DEPENDS ...)` 增量编译。
- 为 shader compile 与 runtime lookup 建立单一 manifest 或生成表，避免 CMake/C++ 双重登记。
- 将 graph planner、shadow builder 和 resource lifetime policy 测试移入独立测试目标。

Facade 拆分只有在能降低调用方依赖时才执行。目录大小写和批量移动必须使用独立 PR，避免和行为变更混合。

## 6. 推荐执行顺序

```text
RR-00 Baseline
  -> RR-01 RenderGraph Access Model
  -> RR-02 Synchronization2
  -> RR-03 Canonical Frame Contract
  -> RR-04 Shadow Frame Builder
  -> RR-05 Backend Responsibility Split
  -> RR-06 Common Vulkan Resource Primitives
  -> RR-07 Reflection Probe / Picking Ownership
  -> RR-08 Feature Plan / Frame Graph Builder
  -> RR-09 Render Feature Ownership
  -> RR-10 Asset Dependency Injection
  -> RR-11 Deferred GPU Destruction
  -> RR-12 FrameContext
  -> RR-13 Async Picking / Upload
  -> RR-14 Facade / Shader / Build / Directory Polish
```

第一轮建议只审批并执行 RR-00 到 RR-02。

原因：

- RenderGraph hazard 是当前最明确的正确性缺口。
- 单帧同步仍然保留，可以限制资源生命周期风险。
- 完成后先整理 frame contract、shadow math 和 Backend 所有权，再进入 GPU lifetime 改造。

整个计划分为四个阶段：

| 阶段 | 工作包 | 目标 |
|---|---|---|
| A. Correctness | RR-00 至 RR-02 | 固定基线并修正 graph 同步语义 |
| B. Architecture Clarity | RR-03 至 RR-10 | 澄清 frame contract、所有权、Feature 和依赖方向 |
| C. Lifetime and Performance | RR-11 至 RR-13 | 延迟销毁、多帧并行和异步传输 |
| D. Build Boundary | RR-14 | 收口 facade、Shader、CMake、测试和目录 |

阶段 B 完成后，即使暂不进行多帧并行，Renderer 的清晰度和可维护性也应已经显著改善。阶段 C 不应成为判断架构整理是否成功的唯一标准。

## 7. 风险矩阵

| 工作包 | 风险 | 主要风险 | 控制措施 |
|---|---|---|---|
| RR-00 | 低 | 基线覆盖不足 | 固定 focused test 和手工矩阵 |
| RR-01 | 中 | barrier 缺失或过度同步 | 纯逻辑 planner test，第一版保守同步 |
| RR-02 | 中 | stage/access 转换错误 | 保持 pass 顺序，开启 validation |
| RR-03 | 中 | clip-space 或 frame data 回归 | 固定 matrix/depth 测试，不改视觉结果 |
| RR-04 | 低到中 | shadow matrix 迁移偏差 | 纯数学 golden/focused test |
| RR-05 | 中 | shutdown 顺序或句柄迁移遗漏 | 纯结构迁移，保持外部行为 |
| RR-06 | 中到高 | allocation/cleanup 回归 | 每次迁移一组资源，validation + resize |
| RR-07 | 中 | Probe bake / picking 生命周期回归 | 复用 residency/freshness/picking 验证 |
| RR-08 | 中 | viewport/swapchain graph 路由错误 | common graph 与 output tail 分步迁移 |
| RR-09 | 中 | Feature 输入连接错误 | 每次只迁移一个 Feature |
| RR-10 | 中 | Asset service 生命周期错误 | 只做注入，不改加载策略 |
| RR-11 | 高 | 延迟释放过早或泄漏 | frame serial、retirement diagnostics |
| RR-12 | 高 | GPU resource use-after-free | FrameContext 独立资源和 deferred deletion |
| RR-13 | 高 | 异步结果时序和 fallback 错误 | 明确 Pending/Ready/Failed 状态 |
| RR-14 | 低到中 | 链接依赖和 shader 输出错误 | 独立 build PR，保持运行逻辑不变 |

## 8. 每个 PR 的验证原则

### 8.1 必要自动验证

- 构建 `Sandbox` Debug target。
- 运行与修改范围直接相关的 smoke test。
- `git diff --check`。
- RenderGraph planner、shadow builder 和 resource lifetime policy 使用 focused test。

不默认执行全量测试。

### 8.2 Renderer 手工验证矩阵

根据改动范围选择必要项：

- Final Lit 正常显示。
- SSR 开关、SSR debug view 和 IBL isolation。
- AO debug view。
- Bloom mip/debug view。
- SMAA debug view。
- Directional / point / rect shadow。
- Viewport resize 和 swapchain resize。
- Entity picking。
- Reflection Probe Bake、dirty rebake、clear 和 resident limit。
- Material runtime edit 和 texture replacement。
- Vulkan validation layer 无新增 error。

### 8.3 性能工作验证

RR-11 之后应逐步记录：

- Renderer CPU frame time。
- Per-pass GPU timestamp。
- Frame wait 时间。
- Upload bytes / frame。
- Deferred deletion pending count。
- GPU resource cache 数量和估算内存。

在没有 profiler 数据前，不对每帧 vector 分配或 graph 小对象分配做大规模微优化。

## 9. 明确非目标

本计划暂不包含：

- 独立 render thread。
- 通用动态插件式 renderer。
- 第二图形 API 后端。
- 为假想后端建立完整 RHI 抽象层。
- 完整 transient resource aliasing RenderGraph。
- 自动 pass culling 和异步 compute 调度。
- 完整异步 Asset Pipeline。
- 全量 ECS 反射系统。
- 为重构目的调整 SSR、IBL、Shadow 或后处理视觉参数。
- 只为了降低文件行数而机械拆分类。

这些能力只有在 FrameContext、资源生命周期和 Feature ownership 稳定后再单独规划。

## 10. 完成标准

Renderer 重构主线可以认为完成，当满足：

- Graph 能正确表达 image read/write hazard 和 subresource 范围。
- Vulkan validation layer 在目标验证场景中没有同步错误。
- `RenderView` 和 prepared frame 的 clip-space、数据内容与生命周期契约明确。
- Shadow frame CPU 算法离开 Vulkan Backend，并有 focused math test。
- `VulkanRendererSystem` 只保留 backend facade 和高层 frame orchestration。
- Reflection Probe、Picking 和主要 post effect 具有明确所有者。
- Feature plan、graph builder 和 diagnostics 对本帧启用状态只有一个真相来源。
- 普通 Vulkan image / buffer 通过统一 allocator 和 move-only owned resource 管理。
- 不再在各 Target 中重复维护 `checkVk()`、`findMemoryType()` 和同构 `OwnedImage`。
- Renderer 内部不再读取 `AssetManager` 单例。
- GPU resource 通过统一 deferred deletion 生命周期释放。
- Renderer 支持至少 2 frames in flight，且不依赖每帧 device idle。
- Picking 和常规资源上传不再阻塞整个 graphics queue。
- 外部 `RendererService` 契约保持 backend-neutral。
- Renderer frontend 不 include Vulkan backend 文件。
- CMake target 能表达 Renderer frontend / Vulkan backend 的依赖方向。
- Shader compile 与 runtime lookup 不再依赖两份手工维护的清单。
- 空 legacy Renderer 目录和仓库中的平台垃圾文件已清理。
- 现有效果、Editor viewport、Probe bake 和 material editing 没有行为回归。

## 11. 最终结论

NexAur Renderer 当前已经有可继续演进的正确骨架。真正的问题不是目录不够多，而是 frame contract、Vulkan backend 的功能所有权、底层 GPU resource primitive、同步语义和资源生命周期还没有完全收口。

推荐先完成 RR-00、RR-01 和 RR-02，修正 RenderGraph access/hazard 基础，同时保留单帧同步模型。随后执行 RR-03 至 RR-10，先让 frame data、shadow math、GPU primitive、Backend、Feature 和 Asset 依赖形成清楚的所有权，再进入 RR-11 至 RR-13 的高风险 GPU 生命周期改造。

这轮整理的核心衡量标准不是 `vulkan_renderer_system.cpp` 最终减少了多少行，而是新增或修改一个渲染功能时，开发者能否快速回答：

1. 输入数据由谁构建？
2. GPU resource 由谁拥有？
3. Layout 和 hazard 由谁跟踪？
4. Resize、replacement 和 retirement 由谁负责？
5. 本帧是否执行该 Feature 由谁决定？
6. 哪个 focused test 能验证它？

当这些问题都有唯一、稳定的答案时，Renderer 才真正达到清晰、可维护的目标。

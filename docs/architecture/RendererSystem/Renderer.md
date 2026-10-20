# Vulkan 渲染架构

最后核对：2026-10-20。RT-10.2 已完成，并已加入光追资源按需准备和场景表增量更新；后续降噪和最终混合反射合成尚未完成。

## 外部契约

[VulkanRendererSystem](../../../source/Engine/Function/Renderer/Vulkan/vulkan_renderer_system.h)使用 PImpl 隐藏后端对象，并实现四个窄服务：

| 服务 | 能力 |
| --- | --- |
| RendererService | 渲染一帧、UI 后端生命周期 |
| ViewportRendererService | resize、尺寸、视口输出和拾取 |
| ReflectionProbeCaptureService | Capture/Bake 请求、清理和状态 |
| RendererDebugService | 只读诊断快照 |

Scene 和 Editor 不持有 VkImage、VkBuffer 等对象。输入使用 AssetHandle 和普通帧数据，GPU 资源由 VulkanRenderResourceCache 解析与持有。

## 数据流

```text
SceneV2 / Editor camera
  -> RenderDataPacket
  -> RenderContext swap
  -> RenderSceneFrameBuilder
  -> RenderSceneFrame
  -> VulkanRenderDataTranslator + VulkanDrawListBuilder
  -> VulkanPreparedFrame { scene, draw_list }
  -> VulkanRenderFeaturePlan
  -> VulkanFrameGraphBuilder / VulkanPassGraph
  -> VulkanGraphExecutor
```

RenderSceneFrame 是后端无关的规范帧；Vulkan 投影约定转换集中在 translator。PreparedFrame 使资源准备、绘制、Probe capture 和诊断使用同一帧输入。

协调入口为 [vulkan_renderer_backend.cpp](../../../source/Engine/Function/Renderer/Vulkan/vulkan_renderer_backend.cpp)，图构建入口为 [vulkan_frame_graph_builder.cpp](../../../source/Engine/Function/Renderer/Vulkan/frame/vulkan_frame_graph_builder.cpp)。

## 内部组织

| 目录/对象 | 职责 |
| --- | --- |
| core | DeviceContext、SwapchainManager、VMA allocator、owned resources、retirement |
| frontend | Vulkan view、draw list 与 GPU 资源解析 |
| frame | FeaturePlan、FrameContext、帧图组装 |
| features | Shadow、AO、SSR、Bloom、SMAA、PostProcess、ReflectionSurface、RT Reflection 的所有权和调度 |
| graph | image/buffer/AS 访问声明、状态规划与 synchronization2 barrier |
| passes / targets | 绘制命令录制与 render target 资源 |
| descriptors / pipeline / shaders | 描述符布局和分配、graphics/compute pipeline、ShaderLibrary |
| resources / upload | 材质、纹理、Mesh、环境等 GPU 资源和上传 |
| ray_tracing | 可选能力、addressable buffer、AS、BLAS cache、TLAS、GPU scene table |
| diagnostics / ui | GPU timing/诊断和 Vulkan ImGui backend |

PickingManager 和 ReflectionProbeManager 分别管理拾取工作流和 Probe capture/cache 生命周期。

## 当前帧图

FeaturePlan 根据设置、能力和资源可用性决定实际 pass。典型 viewport 路径为：

```text
Directional / Point / Rect Shadow
  -> Skybox
  -> ForwardScene（可选 Reflection MRT）
  -> Reflection preparation（需要时重置 history）
  -> SSAO / RTAO（可选）
  -> SSR（可选）
  -> Ray Query Reflection Trace（可选，当前提供 raw/debug 结果）
  -> DebugDraw / ObjectId
  -> Bloom
  -> PostProcess
  -> SMAA（可选）
  -> ImGuiComposite
  -> Present
```

直接 swapchain 输出使用另一条尾部路径。Probe capture 使用 color-only Forward，不进入 viewport temporal history 流程。

RenderGraph 已表达 image、buffer 和 acceleration structure 的读写及 stage/access 状态，并用 `vkCmdPipelineBarrier2` 执行同步。旧基线中“相同 layout 就跳过所有 barrier”的描述仅适用于 RR-00 历史阶段。

## 生命周期

- GPU 使用两个 FrameContext；各自持有 command pool/buffer、fence/semaphore、lighting/RT descriptor 数据和 timing。
- frame serial 跟踪提交与完成，retirement/deferred destruction 延后释放仍可能被 GPU 使用的资源。
- Reflection history 是跨帧 ping-pong 数据；scene、viewport、camera、settings 等变化可触发 reset。
- 上一帧 camera/object transform 仅在成功提交后推进。
- RenderContext 的 CPU 数据包双缓冲与 GPU frames in flight 是不同机制。

## 光追资源准备与场景表缓存

渲染器先用设置和已初始化管线生成准备计划，再用本帧资源就绪状态生成最终 FeaturePlan。Ray Query 阴影、RTAO、Visibility debug 或 RT reflection 请求 BLAS/TLAS；只有 RT reflection（含反射 debug）请求 shading table。关闭消费者后不再遍历、准备对应光追资源，并清除当前帧槽对旧描述符的就绪状态；重新开启时按当前场景恢复。

每个 FrameContext 独立保存场景表输入快照和可复用容器。快照检查 canonical instance 顺序、实体 ID、网格版本与 buffer、材质版本与内容、纹理就绪状态及上传标识；不包含由 TLAS 管理的物体 transform。未变化时复用已有表；变化时分别比较 instance、geometry、material 表，只上传变化的表，并只改写变化的 descriptor binding。TLAS 对象替换仅更新 acceleration structure binding。失败或显式 clear 会使缓存失效，下一次有效更新重新填充。

Renderer Debug 的 **RT Scene Table** 区域显示当前帧槽的累计更新、重建、复用、上传字节数和 descriptor binding 写入次数。输入变化检查仍遍历 accepted instances；这些计数反映减少的资源更新工作，不代表已测得的帧率提升。

CPU 功能需求回归包含在 `NexAur.Renderer.FrameFeaturePlan`。真实 GPU 的场景表增量回归可在项目根目录运行 `./bin/msvc-vcpkg/Debug/NexAurRendererTests.exe --tlas-instance-device`，覆盖静态/位移复用、实例重排、材质与纹理更新、TLAS 替换、独立帧槽及失效恢复；设备缺少相应光追能力时会跳过对应检查。

## Shader 与构建

Shader 位于 [assets/shaders/Renderer/Vulkan](../../../assets/shaders/Renderer/Vulkan)，由 DXC 编译为 Vulkan 1.3 SPIR-V。统一 [shader manifest](../../../source/Engine/Function/Renderer/Vulkan/shaders/vulkan_shader_manifest.cmake)生成程序表并驱动 graphics/compute 编译。

Frontend 和 Vulkan backend 分别编译为静态 target，Engine 链接它们；没有当前可用的 OpenGL 后端切换。参见[构建与测试](../../development/build_and_test.md)。

## 光追当前状态

- RT-00～RT-09：可选 Ray Query、BLAS/TLAS、graph AS 同步、方向光阴影、生命周期/compaction/profiling 和 RTAO。
- RT-10.0：canonical accepted instances、GPU geometry/material/texture tables。正式 geometry fetch 使用 descriptor-indexed ByteAddressBuffer，未采用被 DXC probe 否决的 buffer-reference 路径。
- RT-10.1：Forward MRT 的 surface/fallback specular/motion vector、compute pipeline、history/reset 契约。
- RT-10.2：半/全分辨率 compute trace、SSR hit 跳过、静态 opaque 命中重建、材质/IBL 着色、raw radiance/confidence/hit distance 和 debug views。
- RT-10.3：时空降噪，待完成。
- RT-10.4：SSR → RT → Local Probe → Global IBL 的正式合成，待完成。

因此当前 Final Lit 不应被描述为已经使用完整 RT 混合反射。RT reflection 默认关闭；缺少支持时继续 Raster/SSR/Probe/IBL 路径。透明、alpha-mask、skinned/deforming geometry 和完整 RT Pipeline/SBT 不在当前 RT reflection 范围。

详细设计和历史验证记录见 [RT 总计划](../../plans/Renderer/RayTracing/ray_tracing_development_plan.md)及 [RT-10 专项](../../plans/Renderer/RayTracing/ray_traced_reflection_foundation_plan.md)。

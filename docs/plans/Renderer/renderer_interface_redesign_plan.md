# NexAur 渲染模块接口重构方案

日期：2026-06-24

进度主文档：

```text
docs/plans/Renderer/renderer_vulkan_development_roadmap.md
```

本文是接口设计参考文档；后续开发进度、阶段状态和完成记录以进度主文档为准。

## 1. 文档目标

本文档专门讨论 NexAur 渲染模块的对外接口如何重构，目标是让接口足够清爽、干净、后端无关，并适合后续只保留新的 Vulkan renderer。

这份文档不讨论 vcpkg / CMake 构建问题，构建方案见：

```text
docs/plans/Renderer/renderer_dependency_cmake_plan.md
```

总体集成路线见：

```text
docs/plans/Renderer/ark_renderer_integration_plan.md
```

## 2. 当前接口问题

当前 `RendererService` 的方向是正确的：Editor / Runtime 不直接依赖 `RendererSystem` 或 OpenGL 实现。

但接口里仍然有 OpenGL 时代的泄漏：

```cpp
virtual uint32_t getViewportColorAttachment() const = 0;
virtual int readViewportEntityID(int x, int y) = 0;
```

问题：

- `uint32_t getViewportColorAttachment()` 默认返回 OpenGL texture id。
- Vulkan 不能自然映射成 OpenGL texture id。
- Editor viewport 通过这个接口默认自己拿到的是 `ImGui::Image()` 可直接显示的 OpenGL texture。
- `readViewportEntityID()` 默认是同步 readPixel 思路，Vulkan 需要 readback / staging / frame latency 设计。
- 接口没有表达当前 backend 类型、viewport 输出是否有效、输出是 swapchain 还是 offscreen image。

如果这些接口不改，Vulkan backend 会被迫伪装成 OpenGL，这会污染新的渲染模块。

## 3. 重构目标

新的接口应该满足：

- Editor / Runtime 仍然只依赖 `RendererService`。
- `RendererService` 不暴露 OpenGL / Vulkan 原生类型。
- `RenderDataPacket` 仍然是引擎层到渲染层的数据契约。
- viewport 输出用后端无关结构描述。
- picking 用 request / result 描述，不强制每个 backend 同步返回有效 ID。
- OpenGL 可以作为 legacy adapter 临时实现新接口。
- Vulkan 稳定后可以删除 OpenGL legacy，不影响上层接口。

## 4. 推荐接口分层

推荐分三层：

```text
Engine-facing contract
  RendererService
  RenderDataPacket
  ViewportOutput
  PickRequest / PickResult

Backend adapter
  OpenGLLegacyRendererSystem
  VulkanRendererSystem

Backend internal
  OpenGL old classes
  Vulkan renderer facade / RenderScene / RenderView / resources
```

上层只看到第一层。

### 4.1 现代引擎接口参考

Godot / Unreal / Unity 在接口设计上有一个共同点：上层不会拿着图形 API 原生对象到处传。它们会通过 service、server、module 或 pipeline 把“我要渲染什么”和“后端如何渲染”分开。

对 NexAur 来说，这意味着：

- `RendererService` 是引擎侧 facade，不应该变成 Vulkan 或 OpenGL 对象容器。
- `RenderDataPacket` 表达一帧场景数据，不表达 descriptor set、framebuffer、pipeline state 这类后端细节。
- `ViewportOutput` 只描述 Editor 如何展示结果，不让 Editor 解释 Vulkan image / sampler / descriptor 生命周期。
- picking 用 `ViewportPickRequest` / `ViewportPickResult` 表达异步 GPU readback 的现实，不强迫所有后端同步返回。
- `AssetHandle` 是资产身份，GPU buffer / image / material cache 由 renderer backend 自己持有。

这个边界更接近 Godot 的 RenderingServer / driver 隔离、Unreal 的 RendererModule / RHI 隔离，以及 Unity 的 Scene 数据到 Render Pipeline 数据转换。它带来的直接好处是：D12 删除 OpenGL 时，主要删除 backend implementation 和过渡期 wrapper，而不是修改 Editor / Scene / Resource 的设计。

## 5. RendererBackendType

建议新增：

```cpp
enum class RendererBackendType {
    Unknown,
    OpenGLLegacy,
    Vulkan,
};
```

用途：

- Editor 判断当前 viewport output 如何显示。
- 日志显示当前 renderer backend。
- 配置系统选择 backend。
- 后续删除 OpenGL 后仍保留 `Vulkan` 和 `Unknown` 即可。

## 6. ViewportOutput

建议新增后端无关 viewport 输出结构。

第一版：

```cpp
enum class ViewportOutputKind {
    None,
    OpenGLTexture,
    VulkanImGuiTexture,
    ExternalSwapchain,
};

struct ViewportOutput {
    RendererBackendType backend = RendererBackendType::Unknown;
    ViewportOutputKind kind = ViewportOutputKind::None;

    uint32_t width = 0;
    uint32_t height = 0;

    // OpenGL legacy: texture id.
    uint64_t numeric_handle = 0;

    // Vulkan path: backend-owned descriptor / image token.
    // Editor 只能把它传回 Renderer/UI bridge，不能解释内部类型。
    void* native_handle = nullptr;

    bool valid() const {
        return kind != ViewportOutputKind::None && width > 0 && height > 0;
    }
};
```

说明：

- OpenGL 过渡期可以把 texture id 放入 `numeric_handle`。
- Vulkan 早期可以返回 `ExternalSwapchain` 或 `None`。
- Vulkan viewport 嵌入 Editor 后，可以返回 `VulkanImGuiTexture`。
- `native_handle` 是不透明 token，不允许 Scene / Runtime 解释它。

后续如果希望更类型安全，可以用 `std::variant` 或 backend-specific wrapper，但第一版先保持简单。

## 7. Picking 接口

建议把同步 `readViewportEntityID()` 改成 request / result。

```cpp
struct ViewportPickRequest {
    int x = 0;
    int y = 0;
};

struct ViewportPickResult {
    bool supported = false;
    bool ready = false;
    int entity_id = -1;
};
```

`RendererService`：

```cpp
virtual ViewportPickResult pickViewport(const ViewportPickRequest& request) = 0;
```

OpenGL legacy：

- 可以同步 readPixel。
- 返回 `supported = true`，`ready = true`。

Vulkan 第一版：

- 返回 `supported = false`。

Vulkan 后续：

- ObjectId pass 写 integer target。
- copy 到 readback buffer。
- 可以同步等待，也可以延迟一帧返回。
- 如果延迟返回，则 `ready = false` 表示本帧没有结果。

## 8. RendererService 推荐形态

建议新接口：

```cpp
class RendererService {
public:
    virtual ~RendererService() = default;

    virtual RendererBackendType getBackendType() const = 0;

    virtual void render(TimeStep ts, const RenderDataPacket& render_data) = 0;
    virtual void resizeViewport(uint32_t width, uint32_t height) = 0;
    virtual std::pair<uint32_t, uint32_t> getViewportSize() const = 0;

    virtual ViewportOutput getViewportOutput() const = 0;
    virtual ViewportPickResult pickViewport(const ViewportPickRequest& request) = 0;
};
```

如果想保留兼容期旧接口，可以临时提供非推荐 wrapper：

```cpp
uint32_t getViewportColorAttachmentLegacy() const;
int readViewportEntityIDLegacy(int x, int y);
```

但不要继续把它们作为核心接口。

## 9. RenderDataPacket 保留策略

`RenderDataPacket` 仍然推荐保留为 Scene / Runtime / Editor 到 Renderer 的跨模块契约。

保留：

```cpp
struct RenderObjectData {
    AssetHandle model_asset;
    glm::mat4 transform;
    int entity_id = -1;
};
```

避免：

```cpp
struct RenderObjectData {
    std::shared_ptr<VertexArray> vertex_array;
    VulkanModelResource* model;
    VkImage image;
};
```

后续可以扩展：

```cpp
struct RendererPostProcessData {
    bool enable_bloom = true;
    bool enable_ssao = true;
    float exposure = 1.0f;
};

struct RenderDataPacket {
    RendererCameraData camera_data;
    RendererLightingData lighting_data;
    RendererEnvironmentData environment_data;
    RendererPostProcessData post_process_data;
    std::vector<RenderObjectData> opaque_objects;
    std::vector<RenderObjectData> transparent_objects;
};
```

但是不要让它包含 backend GPU resource。

## 10. VulkanRendererSystem 接口边界

`VulkanRendererSystem` 应实现 `RendererService`。它的公开头文件只依赖 NexAur 的引擎接口；Vulkan / externalRenderer 参考实现细节放在 `.cpp` 或私有 `Impl` 中。

内部成员可以是：

```cpp
class VulkanRendererSystem final : public RendererService {
public:
    void init(WindowService& window_service);
    void shutdown();

    RendererBackendType getBackendType() const override;
    void render(TimeStep ts, const RenderDataPacket& render_data) override;
    void resizeViewport(uint32_t width, uint32_t height) override;
    std::pair<uint32_t, uint32_t> getViewportSize() const override;
    ViewportOutput getViewportOutput() const override;
    ViewportPickResult pickViewport(const ViewportPickRequest& request) override;

private:
    struct Backend;
    std::unique_ptr<Backend> m_backend;
    std::unique_ptr<VulkanRenderResourceCache> m_resource_cache;
    std::unique_ptr<VulkanRenderDataTranslator> m_translator;
};
```

注意：

- 可以在 `.cpp` 或 `Backend` 内部 include Vulkan / externalRenderer 参考实现相关头文件。
- 不要把 Vulkan 原生对象或 externalRenderer 类型暴露到 `RendererService`。
- 不要让 Editor panel include backend 的 RenderView / RenderScene 头文件。

## 11. Editor Viewport 使用方式

ViewportPanel 不再直接调用：

```cpp
uint32_t texture_id = renderer_service->getViewportColorAttachment();
ImGui::Image(reinterpret_cast<ImTextureID>(texture_id), size);
```

推荐改成：

```cpp
ViewportOutput output = renderer_service->getViewportOutput();

switch (output.kind) {
case ViewportOutputKind::OpenGLTexture:
    drawOpenGLViewport(output);
    break;
case ViewportOutputKind::VulkanImGuiTexture:
    drawVulkanViewport(output);
    break;
case ViewportOutputKind::ExternalSwapchain:
    drawExternalSwapchainNotice(output);
    break;
case ViewportOutputKind::None:
    drawNoViewportOutputNotice();
    break;
}
```

第一版 Vulkan backend 可以只做到：

```text
ViewportOutputKind::ExternalSwapchain
```

或者：

```text
ViewportOutputKind::None
```

这样 Editor 不会因为 texture id 为 0 崩溃。

## 12. OpenGL Legacy 适配

短期为了保持现有功能，可以让旧 OpenGL backend 实现新接口：

```text
getBackendType() -> OpenGLLegacy
getViewportOutput() -> OpenGLTexture + texture id
pickViewport() -> readPixel
```

但是不要继续为 OpenGL 添加新的抽象能力。它只是迁移期 fallback。

当 Vulkan backend 覆盖 viewport 和 picking 后：

- 删除 `getViewportColorAttachmentLegacy()`。
- 删除旧 `Framebuffer` 对 Editor 的公开依赖。
- OpenGL 代码移入 legacy 或移除默认构建。

## 13. 不建议的接口设计

不建议：

```cpp
class RendererService {
public:
    virtual uint32_t getTextureId() = 0;
    virtual VkImageView getVulkanImageView() = 0;
    virtual void* getRawBackendObject() = 0;
};
```

原因：

- 上层会开始根据 backend 分支写大量特殊逻辑。
- 渲染 backend 细节逃逸。
- 后续删除 OpenGL 或更换 UI backend 会变困难。

建议：

```cpp
class RendererService {
public:
    virtual ViewportOutput getViewportOutput() const = 0;
};
```

上层只按 output kind 做显示，不理解内部资源生命周期。

## 14. 实施步骤

和主路线文档对齐时，建议这样拆：

```text
D1 RendererService 接口清理
  -> PR-I1 新增接口类型
  -> PR-I2 RendererService 增加新接口
  -> PR-I4 OpenGL legacy 实现新接口
  -> 旧 getViewportColorAttachment/readViewportEntityID 暂时保留为 wrapper

D2 ViewportPanel 适配新输出
  -> PR-I3 ViewportPanel 改用 ViewportOutput
  -> picking 调用点改用 pickViewport

后续 Vulkan 接入
  -> PR-I5 VulkanRendererSystem 实现新接口
  -> PR-I6 删除旧 OpenGL 泄漏接口
```

D1 不要求马上删除旧函数，也不要求 ViewportPanel 立刻切到新接口。它的核心价值是先让 `RendererService` 的主契约变干净，同时保证当前 OpenGL 编辑器路径不破。

### PR-I1：新增接口类型

- 新增 `RendererBackendType`。
- 新增 `ViewportOutputKind`。
- 新增 `ViewportOutput`。
- 新增 `ViewportPickRequest` / `ViewportPickResult`。

### PR-I2：RendererService 改为新接口

- 新增 `getBackendType()`。
- 新增 `getViewportOutput()`。
- 新增 `pickViewport()`。
- 保留旧接口为 legacy wrapper 或直接迁移调用点。

### PR-I3：ViewportPanel 适配新接口

- 根据 `ViewportOutputKind` 显示 OpenGL texture / Vulkan placeholder / no output。
- picking 改用 `pickViewport()`。

### PR-I4：OpenGL legacy 实现新接口

- `getViewportOutput()` 返回 OpenGL texture id。
- `pickViewport()` 包装旧 readPixel。

### PR-I5：VulkanRendererSystem 实现新接口

- 第一版返回 `ExternalSwapchain` 或 `None`。
- picking 返回 `supported = false`。

### PR-I6：移除旧 OpenGL 泄漏接口

- 删除 `getViewportColorAttachment()`。
- 删除 `readViewportEntityID()` 或转为 private legacy。

## 15. 完成标准

- `RendererService` 不再暴露 OpenGL texture id 作为核心接口。
- Editor viewport 可以优雅处理 OpenGL / Vulkan / no output。
- Picking 支持“暂不可用”的结果表达。
- Scene / Resource / Runtime 不 include OpenGL / Vulkan backend 头文件。
- Vulkan backend 可以逐步接入，不需要伪装成 OpenGL framebuffer。

## 16. 总结

渲染模块接口重构的关键不是做一个更复杂的 RHI，而是把引擎侧接口抬高：

```text
Engine 只提交场景数据和视图参数
RendererService 只暴露后端无关能力
具体 GPU API 留在 backend adapter 内部
```

这会让后续废除 OpenGL、只保留 Vulkan renderer 路径更自然。

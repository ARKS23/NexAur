# NexAur Resource / Renderer / Scene 渐进重构方案

日期：2026-05-28

## 目标

这份文档规划 NexAur 下一阶段的架构重构。重点不是马上做 Vulkan，而是先把当前 OpenGL 渲染路径整理成更清晰的引擎边界，让后续 Vulkan 后端、场景保存/加载、编辑器工具链都能自然接上。

核心目标：

- Resource 负责资产身份、磁盘加载、CPU 数据缓存，不直接管理 GPU 生命周期。
- Renderer 负责 GPU 对象、渲染设备、渲染资源缓存和 Pass 执行。
- Scene 只描述世界数据和组件状态，不在构造函数里加载默认 HDR、不持有编辑器相机。
- Editor 通过独立上下文管理选择、视口、相机和命令，不把编辑器状态塞进运行时 Scene。
- 为 Scene Serialization 打基础：组件里保存可序列化的资产引用，而不是直接保存 GPU 对象指针。

## 当前问题

### 1. AssetManager 职责过宽

当前 `AssetManager` 同时承担：

- 路径到 UUID 的映射。
- CPU 模型加载和缓存。
- Texture / Shader / EnvironmentMap 等 GPU 资源创建。
- 调用 `RenderResourceUploader::uploadModelData` 生成 GPU 模型。
- 调用 `IBLBuilder::bakeIBLFromHDR` 烘焙环境光。

这导致 Resource 层直接依赖 Renderer 层，后续 Vulkan 接入时，资源系统会被 OpenGL 对象和上下文生命周期绑住。

### 2. RenderDataPacket 持有 GPU 对象

当前 `RenderDataPacket`、`RendererMaterialData`、`RenderModelData` 中直接存储：

- `std::shared_ptr<Texture2D>`
- `std::shared_ptr<TextureCubeMap>`
- `std::shared_ptr<VertexArray>`

这让 Scene 提取阶段和 Renderer 资源阶段混在一起。更理想的方式是 Scene 输出 `AssetHandle` / `RenderResourceHandle`，Renderer 在自己的资源缓存中解析为 GPU 对象。

### 3. SceneV2 带有初始化 demo 和编辑器状态

当前 `SceneV2` 构造函数中会：

- 创建默认 Camera / Light / Environment。
- 加载 `warm_restaurant_8k.hdr`。
- 持有并更新 `EditorCamera`。

这些适合放在 Sample Scene / Editor / SceneBootstrap 中，不适合成为运行时 Scene 的默认行为。

### 4. RendererSystem 与 RendererFactory 边界模糊

`RendererFactory` 是静态工厂，散落创建 Framebuffer、Shader、Texture、Buffer、Material 等资源。短期可用，但长期会限制：

- 多后端选择。
- 资源生命周期统一管理。
- 设备丢失/重建。
- RenderGraph / FrameGraph。
- 后端能力查询。

## 目标架构

建议采用渐进式四层结构：

```text
Application / Sandbox / Editor
        |
        v
Scene / ECS / Serialization
        |
        v
Resource / Asset System
        |
        v
Renderer Frontend
        |
        v
RHI Backend: OpenGL now, Vulkan later
```

每层只依赖下一层的抽象，不反向依赖上层。

## 模块设计

### Resource 层

目标目录建议：

```text
source/Engine/Function/Resource/
  asset_handle.h
  asset_metadata.h
  asset_registry.h/.cpp
  asset_manager.h/.cpp
  importers/
    model_importer.h/.cpp
    texture_importer.h/.cpp
```

职责划分：

- `AssetHandle`
  - 类型安全的 UUID 包装。
  - 可序列化。
  - 不等于 GPU handle。

- `AssetMetadata`
  - 保存资产路径、类型、导入设置、版本号。
  - 示例字段：`uuid`、`type`、`path`、`import_settings_hash`。

- `AssetRegistry`
  - 管理路径到 UUID 的稳定映射。
  - 后续可以落盘为 `assets/asset_registry.yaml` 或 `.nxasset`。

- `AssetManager`
  - 只负责资产查询、CPU 数据加载和缓存。
  - 不直接创建 `Texture2D`、`VertexArray`、`Shader`。
  - 对 Renderer 暴露的是 CPU 资产或资产 handle。

- `ModelImporter`
  - 基于 Assimp 加载 CPU 模型。
  - 输出 `ModelAsset` / `MeshAsset` / `MaterialAsset`。

- `TextureImporter`
  - 读取图片元信息，暂时可延后实际像素缓存。
  - 后续给压缩纹理、mipmap、sRGB 选项留接口。

建议 API：

```cpp
enum class AssetType {
    Unknown,
    Model,
    Texture2D,
    Shader,
    EnvironmentMap,
    Material
};

struct AssetHandle {
    UUID id = INVALID_UUID;
    explicit operator bool() const { return id != INVALID_UUID; }
};

struct AssetMetadata {
    AssetHandle handle;
    AssetType type = AssetType::Unknown;
    std::string path;
};

class AssetManager {
public:
    AssetHandle importAsset(const std::string& path, AssetType type);
    AssetHandle getOrImport(const std::string& path, AssetType type);
    const AssetMetadata* getMetadata(AssetHandle handle) const;

    std::shared_ptr<Model> loadModelCPU(AssetHandle handle);
};
```

### Renderer 资源层

目标目录建议：

```text
source/Engine/Function/Renderer/
  Resources/
    render_resource_cache.h/.cpp
    render_resource_uploader.h/.cpp
    render_handles.h
  RHI/
    render_device.h
    render_context.h
    render_texture.h
    render_buffer.h
```

职责划分：

- `RenderDevice`
  - 后端设备入口。
  - 创建 Buffer / Texture / Shader / Framebuffer / PipelineState。
  - 当前只有 `OpenGLRenderDevice`。

- `RenderResourceCache`
  - 把 `AssetHandle` 转成 GPU 资源。
  - 缓存模型 GPU 数据、纹理、shader、environment map。
  - 只在 Renderer 初始化后可用。

- `RenderResourceUploader`
  - 负责 CPU mesh 到 GPU buffer 的上传。
  - 不再回调 `AssetManager::getInstance()`。
  - 依赖注入 `AssetManager` 和 `RenderDevice`。

- `RenderResourceHandle`
  - Renderer 内部资源句柄，可先用 `UUID` 或轻量 struct。
  - 不要求 Scene 认识底层 GPU 对象。

建议 API：

```cpp
class RenderResourceCache {
public:
    RenderModelHandle getOrCreateModel(AssetHandle model_asset);
    RenderTextureHandle getOrCreateTexture(AssetHandle texture_asset);
    RenderEnvironmentHandle getOrCreateEnvironment(AssetHandle hdr_asset);

    const RenderModelData* getModel(RenderModelHandle handle) const;
    const Texture2D* getTexture(RenderTextureHandle handle) const;
};
```

短期不必一次写完整 `RenderDevice`，可以先把现有 `Texture2D::create`、`VertexArray::create` 包进 `OpenGLRenderDevice` 或 `RendererFactory` 的实例化版本中。

### Renderer Frontend

目标是让渲染流程变成：

```text
SceneExtractor
  -> RenderScene / RenderDataPacket
  -> RendererSystem
  -> RenderResourceCache resolve
  -> RenderPassContext
  -> ForwardPipeline
  -> Passes
```

建议新增：

- `SceneRenderer`
  - 管理每帧渲染入口。
  - 与 `RendererSystem` 的职责可逐步合并或替换。

- `RenderPassContext`
  - Pass 执行上下文。
  - 包含 frame index、viewport framebuffer、shadow map、resource cache、render device。
  - Pass 不直接访问全局 `RendererFactory`。

- `RenderTarget`
  - 抽象视口 framebuffer、shadow framebuffer、未来 swapchain image。

Pass 命名建议：

- `IRenderPass` -> `RenderPass`
- `ShadowPassV2` -> `ShadowPass`
- `SkyboxPassV2` -> `SkyboxPass`

V2 后缀已经没有对照物，继续保留会降低代码表达力。

### Scene 层

目标目录建议：

```text
source/Engine/Function/Scene/
  scene.h/.cpp
  scene_serializer.h/.cpp
  entity.h/.cpp
  component.h
  scene_factory.h/.cpp
```

设计原则：

- `SceneV2` 后续重命名为 `Scene`。
- Scene 构造函数不加载默认资源。
- 默认灯光、相机、环境图由 `SceneFactory::createDefaultScene()` 或 Sandbox 创建。
- Component 中保存 `AssetHandle`，不要保存 GPU 指针。
- EditorCamera 移到 Editor，不属于 Scene。

组件建议：

```cpp
struct IDComponent {
    UUID id;
};

struct MeshRendererComponent {
    AssetHandle model;
    bool is_transparent = false;
};

struct EnvironmentComponent {
    AssetHandle hdr_environment;
    float intensity = 1.0f;
};
```

### Editor 层

目标是把编辑器状态统一收口：

```text
EditorContext
  active_scene
  renderer_system / scene_renderer
  viewport_camera
  selection_service
  command_stack
```

建议新增：

- `EditorSelection`
  - 管理 selected entity。
  - 记录来源：hierarchy / viewport / inspector。
  - 后续支持多选。

- `EditorCameraController`
  - 管理当前视口相机。
  - Scene 中的 CameraComponent 可作为游戏相机，不再被编辑器相机覆盖。

- `EditorSceneCommands`
  - 后续 Save / Load / Undo / Redo 的基础。

## 推荐数据流

### 模型加载

```text
Sandbox / Editor
  AssetManager::getOrImport("assets/models/xxx.gltf", Model)
        |
        v
Scene Entity: MeshRendererComponent { AssetHandle model }
        |
        v
SceneExtractor 输出 RenderObjectRef { model_asset, transform, entity_id }
        |
        v
RenderResourceCache::getOrCreateModel(model_asset)
        |
        v
OpenGL buffers / vertex arrays
```

### 环境光加载

```text
Scene Entity: EnvironmentComponent { AssetHandle hdr_environment }
        |
        v
SceneExtractor 输出 environment asset handle
        |
        v
RenderResourceCache::getOrCreateEnvironment(hdr_handle)
        |
        v
IBLBuilder 使用 RenderDevice 生成 skybox / irradiance / prefilter / brdf lut
```

### 场景保存

Scene Serialization 只保存：

- Entity UUID。
- Tag。
- Transform。
- MeshRenderer 的 model asset handle 或 path。
- Light 参数。
- Environment 的 HDR asset handle 或 path。

不保存：

- Texture2D 指针。
- VertexArray 指针。
- Framebuffer。
- Shader runtime pointer。

## 分阶段计划

### PR9：重构设计落地前置清理

内容：

- 将 `SceneV2`、`ShadowPassV2`、`SkyboxPassV2` 的 V2 后缀列入命名清理计划。
- 新增 `AssetHandle` 和 `AssetType`，但暂时不替换所有调用。
- 给 `AssetManager` 增加 `getPath(UUID)` 或 `getMetadata()`，为序列化铺路。
- 给 `MeshRendererComponent` 增加兼容字段或 helper，准备从 UUID 过渡到 `AssetHandle`。

验收：

- 不改变渲染行为。
- Engine/Sandbox 构建通过。

### PR10：AssetManager 拆出 CPU Asset 职责

内容：

- 把 `loadModel` 拆成 `importModelAsset` / `loadModelCPU`。
- `AssetManager` 只缓存 CPU `Model` 和路径元数据。
- 保留旧 `loadModel` 作为兼容入口，但内部标记为过渡 API。
- Procedural model 注册路径单独保留为 `registerProceduralModel`。

验收：

- Sandbox PBR 球体和可选 DamagedHelmet 行为不变。
- `AssetManager` 不再直接负责模型 GPU 上传。

### PR11：RenderResourceCache 接管 GPU 模型和贴图

内容：

- 新增 `RenderResourceCache`。
- 将 `RenderResourceUploader` 移入 `Renderer/Resources`。
- `RenderResourceUploader` 不再调用 `AssetManager::getInstance()`，改由调用方传入依赖。
- `SceneV2::extractSceneData` 不再从 AssetManager 获取 `RenderModelData`，而是输出 asset handle。
- `RendererSystem` 在渲染前通过 cache 解析 GPU 数据。

验收：

- `RenderDataPacket` 中的对象数据不再直接来自 `AssetManager::getRenderModel`。
- Engine/Sandbox 构建通过。

### PR12：EnvironmentMap / IBLBuilder 移入 Renderer

内容：

- `IBLBuilder` 从 `Resource` 移到 `Renderer/Resources` 或 `Renderer/IBL`。
- `EnvironmentMap` 改名为 `RenderEnvironmentMap` 或放入 Renderer 命名空间语义。
- `AssetManager::loadEnvironmentMap` 改为记录 HDR asset；GPU IBL 由 `RenderResourceCache` 创建。

验收：

- Resource 层不再依赖 IBL 烘焙。
- Scene 只引用 HDR asset handle。

### PR13：Scene 与 EditorCamera 解耦

内容：

- `SceneV2` 构造函数不再创建 `EditorCamera`。
- 新增 `SceneFactory::createDefaultScene()`，负责创建默认 Camera / Light / Environment。
- `EditorLayer` 或 `EditorContext` 持有 `EditorCamera`。
- `Scene::tick` 不再用编辑器相机覆盖 CameraComponent。

验收：

- Editor viewport 仍能移动观察。
- Scene 中的 CameraComponent 可独立作为游戏相机。

### PR14：Scene Serialization 第一版

内容：

- 新增 `SceneSerializer`。
- 先支持 YAML 或 JSON。
- 序列化组件：ID、Tag、Transform、MeshRenderer、DirectionalLight、PointLight、Environment。
- Sandbox 增加保存/加载测试入口，Editor 暂不做 UI。

验收：

- 保存当前 Sandbox 场景。
- 重新加载后实体数量、Transform、材质/模型引用、灯光参数一致。

### PR15：Editor Save / Load Scene

内容：

- Editor 菜单或快捷入口增加 Save / Load。
- `EditorSelection` 在加载场景后自动清空或恢复。
- Properties / Hierarchy 使用新 Scene 数据刷新。

验收：

- 编辑 Transform 后保存，重启加载仍保留。
- 缺失资产能给出清晰 warn，不崩溃。

## 目录演进建议

短期可以保持当前目录，只做轻量新增：

```text
source/Engine/Function/Resource/
  asset_handle.h
  asset_metadata.h
  asset_manager.*

source/Engine/Function/Renderer/Resources/
  render_resource_cache.*
  render_resource_uploader.*

source/Engine/Function/Scene/
  scene_serializer.*
  scene_factory.*
```

等模块稳定后，再考虑更大的目录重排：

```text
source/Engine/Runtime/
  Asset/
  Scene/
  Renderer/
  Editor/
```

当前不建议立刻大迁移目录，因为 include 和 CMake 已经刚清理完，下一步更应该先稳定职责边界。

## Vulkan 友好的接口原则

现在不写 Vulkan，但接口设计要避免 OpenGL 习惯外泄：

- 不在上层保存 OpenGL renderer id。
- 不让 Scene / Resource 调 `bind()`。
- 不让 Pass 随意访问全局单例创建 GPU 资源。
- 把每帧状态放进 `RenderPassContext`。
- 纹理槽位绑定逻辑先集中到 Renderer，后续可映射为 descriptor set。
- Framebuffer / RenderTarget 抽象不要假设只有 default framebuffer。

当前 OpenGL 中的：

```cpp
texture->bind(slot);
shader->setInt("u_AlbedoMap", slot);
```

未来可以逐步收口成：

```cpp
material_instance.setTexture("u_AlbedoMap", albedo_handle);
render_context.bindMaterial(material_instance);
```

这样 Vulkan 后端可以把它翻译成 descriptor update，而不是在高层写死 OpenGL texture unit。

## 风险与控制

### 风险 1：拆得太快导致渲染断掉

控制：

- 每个 PR 保持 Sandbox 可运行。
- 优先保留旧 API 作为兼容层。
- 每步只移动一个方向的依赖。

### 风险 2：过早抽象 RenderDevice

控制：

- 不一开始追求完整 Vulkan RHI。
- 先把资源创建从静态工厂变成实例化服务。
- OpenGL 后端仍然直接实现当前能力。

### 风险 3：Scene 序列化被 UUID 稳定性卡住

控制：

- 先支持 path-based fallback。
- `AssetHandle` 可以先包装当前随机 UUID。
- 后续再引入稳定 asset registry。

### 风险 4：Editor 与 Runtime 切分影响现有操作

控制：

- 先把 EditorCamera 移到 EditorContext。
- 不急着改 Hierarchy / Properties 面板接口。
- 选择状态先保持单选，再考虑多选和 undo。

## 优先级结论

推荐下一步不要直接加大功能，也不要做大而空的 Vulkan 抽象。最合适的路线是：

1. 建立 `AssetHandle` / `AssetMetadata`。
2. 让 Resource 和 Renderer 的 GPU 资源职责分离。
3. 去掉 `V2` 命名和 Scene 中的编辑器相机。
4. 做 Scene Serialization 第一版。
5. 在 Editor 里接入 Save / Load。

这条路线的价值是：每一步都能提升架构，同时最终落到一个真实功能：场景保存和加载。它会自然检验资产引用、组件设计、编辑器状态和渲染资源缓存是否真的解耦。

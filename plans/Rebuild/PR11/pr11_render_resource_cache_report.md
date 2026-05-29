# PR11 RenderResourceCache 执行报告

日期：2026-05-29

## 本次目标

PR11 的目标是把 PR10 中临时存在的模型 GPU 缓存进一步整理为 Renderer 侧资源缓存，让 Scene 提取阶段只输出资产引用，RendererSystem 在真正渲染前统一解析 GPU 数据。这样后续接 Vulkan 时，模型上传、贴图绑定和 GPU 生命周期可以自然收口到 Renderer 模块。

## 已完成内容

### 1. 新增 Renderer 资源缓存入口

新增文件：

```text
source/Engine/Function/Renderer/Resources/render_resource_cache.h
source/Engine/Function/Renderer/Resources/render_resource_cache.cpp
```

`RenderResourceCache` 接管原 `RenderModelCache` 的职责：

- `getOrCreateModel(AssetHandle, AssetManager&)`：根据模型资产句柄按需加载 CPU Model，并上传为 `RenderModelData`。
- `getModel(AssetHandle)`：查询已缓存的 GPU 模型。
- `getOrCreateTexture2D(AssetHandle, AssetManager&)`：根据贴图资产句柄创建并缓存 GPU `Texture2D`。
- `getTexture2D(AssetHandle)`：查询已缓存的 GPU 贴图。
- `registerProceduralModel(...)`：登记运行期生成的 GPU 模型，并在 `AssetManager` 中注册运行期资产身份。
- `init()` / `shutdown()`：随 RendererSystem 生命周期初始化和释放。

当前内部维护模型缓存和 `Texture2D` 缓存；环境光、shader、材质实例后续再继续迁移。

### 2. RenderResourceUploader 移入 Renderer/Resources

新增文件：

```text
source/Engine/Function/Renderer/Resources/render_resource_uploader.h
source/Engine/Function/Renderer/Resources/render_resource_uploader.cpp
```

删除旧文件：

```text
source/Engine/Function/Renderer/data/render_uploader.h
source/Engine/Function/Renderer/data/render_uploader.cpp
```

`RenderResourceUploader` 不再直接调用 `AssetManager::getInstance()`，而是由调用方传入 `AssetManager&` 和 `RenderResourceCache&`。这一步很关键：上传器现在是一个纯 Renderer 资源转换工具，依赖从外部注入，后续替换成 Vulkan 上传队列或资源命令列表时不用再拆单例调用。

模型材质贴图的路径现在会先通过 `AssetManager::importTextureAsset` 登记为 `Texture2D` 资产，再交给 `RenderResourceCache::getOrCreateTexture2D` 创建 GPU 贴图。

### 3. Scene 输出资产句柄，不再解析 GPU 模型

`RenderObjectData` 从：

```cpp
std::shared_ptr<RenderModelData> model_data;
```

调整为：

```cpp
AssetHandle model_asset;
```

`SceneV2::extractSceneData` 现在只从 `MeshRendererComponent` 读取 `AssetHandle`，写入 transform、entity id 和透明标记，不再访问 `RenderResourceCache`，也不再触发模型 GPU 上传。

这让 Scene 的职责更干净：Scene 描述世界状态，Renderer 决定这些状态如何变成可绘制 GPU 数据。

### 4. RendererSystem 增加解析阶段

新增 `ResolvedRenderDataPacket` / `ResolvedRenderObjectData`，作为 RendererSystem 解析后的内部绘制数据。

渲染数据流变成：

```text
SceneV2::extractSceneData
  -> RenderDataPacket(model_asset)
  -> RendererSystem::resolveRenderData
  -> RenderResourceCache::getOrCreateModel
  -> ResolvedRenderDataPacket(model_data)
  -> RenderForwardPipeline / ShadowPass / SkyboxPass
```

`RenderForwardPipeline`、`IRenderPass`、`ShadowPassV2`、`SkyboxPassV2` 的输入类型已改为 `ResolvedRenderDataPacket`，Pass 层继续消费 GPU 数据，但这些 GPU 数据不再来自 Scene。

### 5. CMake 与 Sandbox 同步

`source/Engine/CMakeLists.txt` 已移除旧 uploader/cache 文件，加入新的 `Renderer/Resources` 文件。

Sandbox 中过程模型注册入口从：

```cpp
RenderModelCache::getInstance()
```

更新为：

```cpp
RenderResourceCache::getInstance()
```

Sandbox 中过程模型材质贴图也改为走 `RenderResourceCache`，避免示例层继续通过 `AssetManager::loadTexture` 创建 GPU 贴图。

## 当前架构状态

现在模型和贴图资源边界已经变成：

```text
AssetManager
  负责 AssetHandle / AssetMetadata / CPU Model 缓存 / 贴图资产身份

RenderResourceCache
  负责 AssetHandle -> RenderModelData / Texture2D 的 GPU 解析和缓存

RendererSystem
  负责每帧把 Scene 输出的资产引用解析成 Pass 可绘制数据
```

这比之前优雅的地方在于：Scene 不再知道 GPU 模型缓存，AssetManager 不再负责模型上传和主渲染路径的贴图创建，RendererSystem 成为渲染资源解析的明确边界。

## 遗留问题

- `RendererEnvironmentData` 仍然直接持有 `TextureCubeMap` / `Texture2D`，环境光和 IBL 还在 `AssetManager::loadEnvironmentMap` 路径中创建，计划在 PR12 迁移到 Renderer 侧。
- `ProceduralModelFactory` 目前仍位于 Resource 目录，但它生成的是 GPU `RenderModelData`。这次只更新调用入口，后续建议移动到 `Renderer/Resources` 或拆成 CPU procedural mesh + Renderer 上传两段。
- `RendererMaterialData` 仍直接持有贴图指针。贴图创建已经收口到 `RenderResourceCache`，但长期更理想的是材质保存 `AssetHandle`，Pass 绑定阶段再解析成 descriptor/texture view。
- `AssetManager::loadTexture` 和内部 GPU texture cache 暂时保留为兼容 API；主渲染路径已经改走 `importTextureAsset` + `RenderResourceCache`。

## 验证结果

已通过：

```text
cmake -S . -B build
cmake --build build --config Debug --target NexAurEngine
cmake --build build --config Debug --target Sandbox
```

构建仍有项目既有的 C4251 DLL 接口警告；本次额外修正了 `SceneV2` 中 `RenderDataPacket` 前置声明 class/struct 不一致导致的 C4099 警告来源。

# PR10 AssetManager CPU Asset 职责拆分说明

日期：2026-05-29

## 本次目标

PR10 的目标是把模型资源链路拆成更清晰的两段：

- `AssetManager` 负责资产身份、路径元数据和 CPU `Model` 数据。
- Renderer 侧负责把 CPU `Model` 上传成 GPU 可渲染的 `RenderModelData`。

这一步不改 `RenderDataPacket` 的结构，也不引入完整 `RenderResourceCache`。当前只针对模型资源做最小闭环，后续 PR11 再把模型、贴图、环境资源统一纳入 Renderer Resource Cache。

## 完成内容

### 1. AssetManager 拆出模型导入 API

修改文件：

- `source/Engine/Function/Resource/asset_manager.h`
- `source/Engine/Function/Resource/asset_manager.cpp`

新增 API：

```cpp
AssetHandle importModelAsset(const std::string& path);
std::shared_ptr<Model> loadModelCPU(AssetHandle handle);
```

保留兼容 API：

```cpp
UUID loadModel(const std::string& path);
AssetHandle loadModelAsset(const std::string& path);
std::shared_ptr<Model> getModel(const UUID& handle);
```

行为变化：

- `importModelAsset` 只解析磁盘模型并缓存 CPU `Model`。
- `loadModel` 作为旧入口保留，但现在只返回模型资产 UUID，不再触发 GPU 上传。
- `loadModelCPU` 可通过 `AssetHandle` 获取 CPU 模型；如果未来只有元数据没有 CPU 缓存，也可以按路径重新加载。

移除内容：

- `AssetManager` 不再包含模型 GPU 缓存。
- `AssetManager` 不再调用 `RenderResourceUploader::uploadModelData`。
- `AssetManager` 不再提供 `getRenderModel` / `registerRenderModel` 作为模型 GPU 管理入口。

### 2. 新增 Renderer 侧 RenderModelCache

新增文件：

- `source/Engine/Function/Renderer/Resources/render_model_cache.h`
- `source/Engine/Function/Renderer/Resources/render_model_cache.cpp`

核心职责：

- 按 `AssetHandle` 缓存 `RenderModelData`。
- 对磁盘模型执行 CPU `Model` -> GPU `RenderModelData` 的按需上传。
- 管理 procedural model 这类已经是 GPU 数据的运行期模型。

核心 API：

```cpp
std::shared_ptr<RenderModelData> getOrCreateModel(AssetHandle model_asset, AssetManager& asset_manager);
std::shared_ptr<RenderModelData> getModel(AssetHandle model_asset) const;

AssetHandle registerProceduralModel(
    const std::shared_ptr<RenderModelData>& gpu_model,
    AssetManager& asset_manager,
    const std::string& debug_name = "ProceduralRenderModel");
```

设计取舍：

- `RenderModelCache` 现在是 PR10 的过渡缓存，命名保持具体，不提前冒充完整资源系统。
- 缓存容器隐藏在 cpp 内部，避免把 STL 缓存细节暴露在导出类 ABI 上。
- procedural model 的 GPU 数据保存在 Renderer 侧，资产身份和运行期元数据仍由 `AssetManager::registerRuntimeAsset` 记录。

### 3. RendererSystem 接管缓存生命周期

修改文件：

- `source/Engine/Function/Renderer/RHI/renderer_system.cpp`

调整内容：

- `RendererSystem::init()` 初始化 `RenderModelCache`。
- `RendererSystem::shutdown()` 在底层 Renderer 关闭前释放 `RenderModelCache`。

这样模型 GPU 资源会在 OpenGL 上下文仍然有效时释放，生命周期位置比放在 `AssetManager::shutdown()` 更自然。

### 4. Scene 提取阶段改走 Renderer 缓存

修改文件：

- `source/Engine/Function/Scene/scene_v2.cpp`

调整内容：

- Mesh 提取时仍读取 `MeshRendererComponent` 的 `AssetHandle`。
- 原先调用 `asset_manager.getRenderModel(...)`。
- 现在调用 `render_model_cache.getOrCreateModel(model_handle, asset_manager)`。

结果：

- Scene 当前仍输出 `RenderObjectData.model_data`，因此渲染管线行为不变。
- 模型 GPU 上传职责已经从 `AssetManager` 移到 Renderer 侧。

### 5. Sandbox procedural model 路径迁移

修改文件：

- `source/Sandbox/scene_test.h`
- `source/Sandbox/scene_test.cpp`

调整内容：

- PBR 球体和立方体不再调用 `AssetManager::registerRenderModel`。
- 改为调用 `RenderModelCache::registerProceduralModel`。
- 普通模型加载改为 `AssetManager::importModelAsset`，组件保存 `AssetHandle`。

这样 Sandbox 的测试模型路径也符合新的职责划分：

```text
ProceduralModelFactory
  -> RenderModelCache::registerProceduralModel
  -> MeshRendererComponent { AssetHandle }
```

普通模型路径变为：

```text
AssetManager::importModelAsset
  -> MeshRendererComponent { AssetHandle }
  -> SceneV2::extractSceneData
  -> RenderModelCache::getOrCreateModel
  -> RenderResourceUploader::uploadModelData
```

### 6. 修正 Entity 头文件自包含

修改文件：

- `source/Engine/Function/Scene/entity.h`

调整内容：

- `Entity` 的模板函数使用 `NX_CORE_ASSERT`。
- `NX_CORE_ASSERT` 会展开到 `NX_CORE_ERROR`。
- 之前该头文件依赖其他 include 间接提供日志宏；PR10 收窄 Sandbox include 后暴露了这个问题。
- 现在 `entity.h` 显式包含 `Core/Log/log_system.h`，让头文件自身依赖完整。

## 当前边界

本次 PR10 已完成：

- 模型 GPU 上传不再由 `AssetManager` 直接执行。
- 模型 GPU 缓存不再存放在 Resource 层。
- procedural model 注册路径从 AssetManager 移到 Renderer 侧。

本次 PR10 未处理：

- `RenderDataPacket` 仍然保存 `std::shared_ptr<RenderModelData>`。
- `RenderResourceUploader` 仍然会通过 `AssetManager` 加载材质贴图。
- Texture / Shader / EnvironmentMap 仍在 AssetManager 中创建 GPU 资源。
- `RenderModelCache` 还不是完整 `RenderResourceCache`。

这些内容保留给 PR11 / PR12，避免一次性重构过大。

## 验证结果

已执行：

```powershell
cmake -S . -B build
cmake --build build --config Debug --target NexAurEngine
cmake --build build --config Debug --target Sandbox
```

结果：

- CMake 配置通过。
- `NexAurEngine` Debug 构建通过。
- `Sandbox` Debug 构建通过。

备注：

- 构建中仍有项目既有的 C4251 / C4099 / C4267 warning，本次 PR10 未扩大处理范围。
- 新增 `RenderModelCache` 已避免在导出类中暴露 GPU 模型缓存容器，未额外引入该类自身的 STL 成员导出 warning。

## 后续建议

PR11 可以在 `RenderModelCache` 的基础上扩展出真正的 `RenderResourceCache`：

- `RenderModelCache` 并入或重命名为 `RenderResourceCache` 的 model 子缓存。
- `RenderResourceUploader` 改为显式注入依赖，不再在内部调用 `AssetManager::getInstance()`。
- `SceneV2::extractSceneData` 输出 asset handle / render object ref，RendererSystem 在渲染前统一解析 GPU 资源。
- 贴图资源也迁移到 Renderer 侧缓存，Resource 层只保存贴图资产身份和 CPU/磁盘信息。

# PR9 资产句柄与元数据前置改造说明

日期：2026-05-29

## 本次目标

PR9 是 Resource / Renderer / Scene 渐进重构的第一步，重点是先补齐“可序列化资产身份”这层基础设施，而不是立即重写资源加载或渲染后端。

本次修改遵循三个原则：

- 保持当前 OpenGL 渲染行为不变。
- 保留旧 UUID 调用路径，给后续迁移留出兼容窗口。
- 让 Scene 组件开始具备保存 `AssetHandle` 的能力，为后续场景序列化和资源/渲染解耦铺路。

## 完成内容

### 1. 新增 AssetHandle / AssetType

新增文件：

- `source/Engine/Function/Resource/asset_handle.h`

新增内容：

- `AssetType`
  - 描述当前资源类型：`Model`、`Texture2D`、`TextureCube`、`Shader`、`EnvironmentMap`、`ProceduralModel`。
  - 暂时保持轻量枚举，不引入复杂导入配置。
- `AssetHandle`
  - 对当前 `UUID` 做轻量包装。
  - 明确表达“资产身份”和“GPU 对象句柄”不是同一个概念。
  - 支持 `operator bool()`，便于后续组件和序列化代码判断有效性。
  - 提供 `std::hash<AssetHandle>`，方便未来作为缓存或注册表 key。

设计取舍：

- `AssetHandle` 当前仍然复用已有 `UUID`，避免 PR9 直接触碰资源分配策略。
- 没有引入稳定磁盘 GUID 或 `.nxasset` 文件，这部分留到后续 AssetRegistry 阶段。

### 2. 新增 AssetMetadata

新增文件：

- `source/Engine/Function/Resource/asset_metadata.h`

新增字段：

- `handle`：资产身份。
- `type`：资产类型。
- `path`：当前磁盘路径或组合路径。
- `debug_name`：调试名称。
- `runtime_generated`：标记运行期生成资源，例如 procedural render model。

设计目的：

- 后续 Scene Serializer 可以通过 `AssetHandle` 查询路径和类型。
- Renderer 资源缓存可以从 AssetManager 获得资源身份信息，而不是直接依赖 Scene 中的 GPU 指针。
- 运行时资源和磁盘资源可以先在元数据层区分出来。

### 3. AssetManager 增加兼容式元数据查询

修改文件：

- `source/Engine/Function/Resource/asset_manager.h`
- `source/Engine/Function/Resource/asset_manager.cpp`

新增 API：

```cpp
const AssetMetadata* getMetadata(const UUID& handle) const;
const AssetMetadata* getMetadata(AssetHandle handle) const;

std::string getPath(const UUID& handle) const;
std::string getPath(AssetHandle handle) const;

AssetType getAssetType(const UUID& handle) const;
AssetType getAssetType(AssetHandle handle) const;
```

新增兼容包装入口：

```cpp
AssetHandle loadModelAsset(const std::string& path);
AssetHandle loadTextureAsset(const std::string& path);
AssetHandle loadTextureCubeAsset(const std::string& path);
AssetHandle loadShaderAsset(const std::string name, const std::string& vertex_path, const std::string& fragment_path);
AssetHandle loadEnvironmentMapAsset(const std::string& hdr_path);
AssetHandle registerRenderModelAsset(const std::shared_ptr<RenderModelData>& gpu_model);
```

新增内部记录：

- `m_uuid_metadata`
- `recordAssetMetadata(...)`

当前记录时机：

- 模型加载成功后记录 `AssetType::Model`。
- 贴图加载成功后记录 `AssetType::Texture2D`。
- Cubemap 加载成功后记录 `AssetType::TextureCube`。
- Shader 创建时记录 `AssetType::Shader`。
- 环境贴图烘焙成功后记录 `AssetType::EnvironmentMap`。
- procedural GPU model 注册时记录 `AssetType::ProceduralModel`。

兼容性说明：

- 原有 `loadModel` / `loadTexture` / `loadEnvironmentMap` 等 UUID API 没有删除。
- 新增 `AssetHandle` API 只是薄包装，内部仍调用旧 API。
- 当前渲染数据缓存和 OpenGL 对象创建流程不变。

### 4. Scene 组件增加 AssetHandle 过渡层

修改文件：

- `source/Engine/Function/Scene/component.h`

`MeshRendererComponent` 变化：

- 保留旧字段 `model_id`。
- 新增 `model_asset`。
- 新增 `setModel(UUID)` / `setModel(AssetHandle)`。
- 新增 `getModelHandle()` / `getModelUUID()`。

`EnvironmentComponent` 变化：

- 保留旧字段 `environment_map_id`。
- 新增 `environment_asset`。
- 新增 `setEnvironmentMap(UUID)` / `setEnvironmentMap(AssetHandle)`。
- 新增 `getEnvironmentHandle()` / `getEnvironmentUUID()`。

设计目的：

- 老代码继续写 UUID 时，组件会同步维护 `AssetHandle`。
- 新代码可以优先写 `AssetHandle`，同时兼容旧渲染路径读取 UUID。
- 后续序列化时可以逐步切换到保存 `AssetHandle` 字段。

### 5. Scene 提取阶段改用 helper

修改文件：

- `source/Engine/Function/Scene/scene_v2.cpp`

调整内容：

- 环境光提取从直接读取 `environment_map_id` 改为 `getEnvironmentHandle()`。
- Mesh 提取从直接读取 `model_id` 改为 `getModelHandle()`。
- 传给当前 AssetManager / Renderer 路径时仍使用 `handle.id`，保证行为不变。

这一步让 Scene 提取代码先适应资产句柄语义，后续替换为 RendererResourceCache 时会更自然。

### 6. CMake 显式纳入新增头文件

修改文件：

- `source/Engine/CMakeLists.txt`

新增：

- `Function/Resource/asset_handle.h`
- `Function/Resource/asset_metadata.h`

## 未在 PR9 中处理的内容

以下内容保持在后续 PR 中处理，避免本次改动范围过大：

- 不重命名 `SceneV2` / `ShadowPassV2` / `SkyboxPassV2`。
- 不拆分 `AssetManager` 的 CPU Asset 与 GPU Resource 职责。
- 不新增 `AssetRegistry` 磁盘注册表。
- 不改动 `RenderDataPacket` 中已有 GPU 对象指针。
- 不引入 Vulkan 或新的 RHI 抽象。

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

- 曾并行触发过一次 Sandbox 构建，MSBuild 因 `ZERO_CHECK.lastbuildstate` 文件锁失败；随后顺序构建通过。这是同一 build tree 的并发构建锁问题，不是代码错误。
- 构建中仍有项目既有的 DLL interface / deprecated API 警告，本次 PR9 没有扩大处理范围。

## 后续建议

PR10 可以继续拆 `AssetManager`：

- 将模型导入与 GPU 上传拆成更清晰的 CPU Asset / Render Resource 两段。
- 保留旧 `loadModel` 作为兼容入口。
- 引入 `RenderResourceCache` 的雏形，让 Renderer 负责从 `AssetHandle` 解析 GPU 资源。

这样可以逐步把 Resource 层从 OpenGL 对象生命周期中解开，为后续 Vulkan 后端打基础。

# PR12 Environment / IBL Renderer 迁移执行报告

日期：2026-05-29

## 本次目标

PR12 的目标是继续拆开 Resource 与 Renderer 的职责：环境光 HDR 在 Resource 层只作为资产身份存在，GPU 侧的 skybox、irradiance、prefilter、BRDF LUT 烘焙全部归到 Renderer 资源层。这样后续 Vulkan 接入时，IBL 烘焙可以自然落到 Renderer 的设备、命令队列和资源缓存体系里。

## 已完成内容

### 1. IBLBuilder 移入 Renderer/Resources

迁移并重命名文件：

```text
source/Engine/Function/Resource/ibl_builder.h
source/Engine/Function/Resource/ibl_builder.cpp
```

变为：

```text
source/Engine/Function/Renderer/Resources/render_ibl_builder.h
source/Engine/Function/Renderer/Resources/render_ibl_builder.cpp
```

类型也同步改名：

```cpp
EnvironmentMap      -> RenderEnvironmentMap
IBLBuilder          -> RenderIBLBuilder
bakeIBLFromHDR(...) -> bakeFromHDR(...)
```

现在 `RenderIBLBuilder` 明确属于 Renderer 资源层，负责把 HDR 纹理烘焙为 Renderer 可直接绑定的环境光 GPU 数据。

### 2. AssetManager 不再烘焙环境光

`AssetManager::loadEnvironmentMap` 改为兼容 API，只登记环境 HDR 资产身份，不再调用 IBL 烘焙。

新增：

```cpp
AssetHandle importEnvironmentMapAsset(const std::string& hdr_path);
```

保留：

```cpp
UUID loadEnvironmentMap(const std::string& hdr_path);
AssetHandle loadEnvironmentMapAsset(const std::string& hdr_path);
```

但它们现在都只返回 `AssetHandle` / `UUID`，不会创建 `TextureCubeMap`、`Texture2D` 或任何 IBL GPU 资源。

### 3. Scene 只输出 HDR 环境资产句柄

`SceneV2` 构造默认环境时改为：

```cpp
asset_manager.importEnvironmentMapAsset(...)
```

`SceneV2::extractSceneData` 不再调用 `AssetManager::getEnvironmentMap`，而是写入：

```cpp
render_packet->environment_data.environment_asset
render_packet->environment_data.intensity
```

这让 Scene 层只描述“使用哪个 HDR 环境资产”，不关心这个 HDR 如何变成 skybox 和 PBR IBL 贴图。

### 4. RenderResourceCache 接管环境光 GPU 缓存

`RenderResourceCache` 新增：

```cpp
std::shared_ptr<RenderEnvironmentMap> getOrCreateEnvironmentMap(AssetHandle environment_asset, AssetManager& asset_manager);
std::shared_ptr<RenderEnvironmentMap> getEnvironmentMap(AssetHandle environment_asset) const;
```

首次解析环境资产时：

```text
AssetHandle -> AssetManager metadata path -> RenderIBLBuilder::bakeFromHDR -> RenderEnvironmentMap cache
```

`RenderResourceCache::shutdown` 现在也会释放环境光缓存，并调用 `RenderIBLBuilder::shutdown` 清理共享 BRDF LUT。

### 5. RendererSystem 解析环境光

`RenderDataPacket` 中的环境数据变为 Scene 侧引用：

```cpp
struct RendererEnvironmentData {
    AssetHandle environment_asset;
    float intensity = 1.0f;
};
```

新增 Renderer 侧解析结果：

```cpp
struct ResolvedRendererEnvironmentData {
    std::shared_ptr<TextureCubeMap> skybox_texture;
    std::shared_ptr<TextureCubeMap> irradiance_map;
    std::shared_ptr<TextureCubeMap> prefilter_map;
    std::shared_ptr<Texture2D> brdf_lut_map;
    float intensity = 1.0f;
};
```

`RendererSystem::resolveRenderData` 会在渲染前通过 `RenderResourceCache` 解析环境贴图，然后传给 ForwardPipeline / SkyboxPass。

另外调整了 `RendererSystem::tick` 顺序：先解析资源，再绑定视口 framebuffer。原因是环境光首次 IBL 烘焙会使用临时 FBO，先解析可以避免它影响本帧主渲染目标。

## 当前架构状态

环境光数据流现在是：

```text
Scene EnvironmentComponent
  -> AssetHandle environment_asset
  -> RenderDataPacket
  -> RendererSystem::resolveRenderData
  -> RenderResourceCache::getOrCreateEnvironmentMap
  -> RenderIBLBuilder::bakeFromHDR
  -> ResolvedRenderDataPacket.environment_data
  -> ForwardPipeline / SkyboxPass
```

Resource 层不再包含 IBL 烘焙代码，也不再保存 `EnvironmentMap` GPU 缓存。

## 遗留问题

- `AssetType::EnvironmentMap` 的名称仍保留，语义上现在更接近 HDR Environment Asset；后续可以在资产系统稳定后改名为 `EnvironmentHDR` 或 `HDRI`。
- `AssetManager` 仍保留旧的 `loadTexture`、`loadTextureCube`、`loadShader` GPU 创建兼容 API；PR12 只迁移环境光链路。
- `RenderIBLBuilder` 仍直接使用 OpenGL 风格的 framebuffer / shader / texture 创建接口。下一步 Vulkan 化前，应继续把这些调用收口到更明确的 RenderDevice / command context。
- `SceneV2` 构造函数里仍创建默认环境和测试相机，这属于 PR13 的 Scene / EditorCamera 解耦范围。

## 验证结果

已通过：

```text
cmake -S . -B build
cmake --build build --config Debug --target NexAurEngine
cmake --build build --config Debug --target Sandbox
```

构建仍有项目既有的 C4251 DLL 接口警告，没有新增构建错误。

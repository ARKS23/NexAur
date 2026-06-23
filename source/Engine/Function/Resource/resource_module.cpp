#include "pch.h"
#include "resource_module.h"

#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Function/Resource/asset_manager.h"

namespace NexAur {
    namespace {
        class ResourceModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return {
                    BuiltinModuleNames::Resource,
                    ModuleStage::Resource,
                    { BuiltinModuleNames::Core, BuiltinModuleNames::FileSystem }
                };
            }

            void initialize(ModuleContext& context) override {
                // AssetManager 目前仍是单例，这里用不拥有删除权的 shared_ptr 作为过渡服务视图。
                AssetManager::getInstance().init();
                m_asset_manager = std::shared_ptr<AssetManager>(&AssetManager::getInstance(), [](AssetManager*) {});
                context.registry.registerService<AssetManager>(m_asset_manager);
            }

            void shutdown(ModuleContext& context) override {
                context.registry.resetService<AssetManager>();
                m_asset_manager.reset();
                AssetManager::getInstance().shutdown();
            }

        private:
            std::shared_ptr<AssetManager> m_asset_manager;
        };
    } // namespace

    std::unique_ptr<EngineModule> createResourceModule() {
        return std::make_unique<ResourceModule>();
    }
} // namespace NexAur

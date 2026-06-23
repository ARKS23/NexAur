#include "pch.h"
#include "core_engine_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"

namespace NexAur {
    namespace {
        // Core 最先启动，提供日志等基础能力。
        class CoreEngineModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return { BuiltinModuleNames::Core, ModuleStage::Core, {} };
            }

            void initialize(ModuleContext& context) override {
                (void)context;
                LogSystem::init();
                NX_CORE_INFO("Core module initialized.");
            }
        };
    } // namespace

    std::unique_ptr<EngineModule> createCoreEngineModule() {
        return std::make_unique<CoreEngineModule>();
    }
} // namespace NexAur

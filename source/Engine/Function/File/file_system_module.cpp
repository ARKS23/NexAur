#include "pch.h"
#include "file_system_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Function/File/file_system.h"

#ifndef ENGINE_ROOT_DIR
#define ENGINE_ROOT_DIR "."
#endif

namespace NexAur {
    namespace {
        class FileSystemModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return { BuiltinModuleNames::FileSystem, ModuleStage::Core, { BuiltinModuleNames::Core } };
            }

            void initialize(ModuleContext& context) override {
                m_file_system = std::make_shared<FileSystem>(ENGINE_ROOT_DIR);
                context.registry.registerService<FileSystem>(m_file_system);
                NX_CORE_INFO("FileSystem module initialized.");
            }

            void shutdown(ModuleContext& context) override {
                context.registry.resetService<FileSystem>();
                m_file_system.reset();
            }

        private:
            std::shared_ptr<FileSystem> m_file_system;
        };
    } // namespace

    std::unique_ptr<EngineModule> createFileSystemModule() {
        return std::make_unique<FileSystemModule>();
    }
} // namespace NexAur

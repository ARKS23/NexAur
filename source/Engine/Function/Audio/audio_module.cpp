#include "pch.h"
#include "audio_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Function/Audio/audio_service.h"
#include "Function/Audio/audio_system.h"

namespace NexAur {
    namespace {
        class AudioModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return {
                    BuiltinModuleNames::Audio,
                    ModuleStage::Audio,
                    { BuiltinModuleNames::Resource }
                };
            }

            void initialize(ModuleContext& context) override {
                m_audio_system = std::make_shared<AudioSystem>();
                const bool initialized = m_audio_system->initialize();

                context.registry.registerService<AudioService>(
                    std::static_pointer_cast<AudioService>(m_audio_system));
                context.registry.registerService<AudioSystem>(m_audio_system);

                if (initialized) {
                    NX_CORE_INFO("Audio module initialized.");
                } else {
                    NX_CORE_WARN("Audio module initialized in disabled mode.");
                }
            }

            void tick(const TickContext& tick_context) override {
                (void)tick_context;
                if (m_audio_system) {
                    m_audio_system->tick();
                }
            }

            void shutdown(ModuleContext& context) override {
                context.registry.resetService<AudioService>();
                context.registry.resetService<AudioSystem>();
                if (m_audio_system) {
                    m_audio_system->shutdown();
                    m_audio_system.reset();
                }
                NX_CORE_INFO("Audio module shut down.");
            }

        private:
            std::shared_ptr<AudioSystem> m_audio_system;
        };
    } // namespace

    std::unique_ptr<EngineModule> createAudioModule() {
        return std::make_unique<AudioModule>();
    }
} // namespace NexAur

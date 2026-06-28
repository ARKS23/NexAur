#include "NexAur.h"
#include "smoke_tests.h"

int main(int argc, char** argv) {
    if (NexAur::Sandbox::isSmokeCommand(argc, argv)) {
        return NexAur::Sandbox::runSmokeCommand(argc, argv);
    }

    NexAur::Engine* engine = new NexAur::Engine();
    engine->startEngine();

    setupScene();

    engine->enableEditorMode(true);

    engine->run();
    engine->shutdownEngine();
    NX_CORE_INFO("Sandbox Stopped!");
    return 0;
}

#include <iostream>
#include "NexAur.h"

int main() {
    NexAur::Engine *engine = new NexAur::Engine();
    engine->startEngine();
    engine->run();
    engine->shutdownEngine();
    NX_CORE_INFO("Sandbox Stopped!");
    return 0;
}
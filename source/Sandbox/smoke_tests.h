#pragma once

void setupScene();

namespace NexAur::Sandbox {
    bool isSmokeCommand(int argc, char** argv);
    int runSmokeCommand(int argc, char** argv);
    int runAllSmokeTests();
} // namespace NexAur::Sandbox

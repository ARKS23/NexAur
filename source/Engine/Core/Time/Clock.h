//
// Created by ASUS on 2026/1/14.
//

#pragma once

#include <chrono>

#include "Core/Base.h"
#include "Core/Time/TimeStep.h"

namespace NexAur
{
    class NEXAUR_API Clock {
    public:
        Clock();

    public:
        void reset();   // 重置start
        TimeStep getElapsedTime();  // delta_time
        TimeStep restart(); // 重置start并返回delta_time

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> m_start_time;
    };
} // namespace NexAur

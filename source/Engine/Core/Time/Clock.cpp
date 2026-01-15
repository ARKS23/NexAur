//
// Created by ASUS on 2026/1/14.
//

#include "Clock.h"

namespace NexAur {
    Clock::Clock() {
        reset();
    }

    void Clock::reset() {
        m_start_time = std::chrono::high_resolution_clock::now();
    }

    TimeStep Clock::getElapsedTime() {
        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seconds = current_time - m_start_time;
        return TimeStep(elapsed_seconds.count());
    }

    TimeStep Clock::restart() {
        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seconds = current_time - m_start_time;
        m_start_time = current_time;
        return TimeStep(elapsed_seconds.count());
    }
} // namespace NexAur

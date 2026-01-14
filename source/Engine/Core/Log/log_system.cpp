#include "pch.h"

#include "spdlog/sinks/stdout_color_sinks.h"

#include "log_system.h"

namespace NexAur {
    std::shared_ptr<spdlog::logger> LogSystem::s_core_logger;

    void LogSystem::init() {
        //设置日志模式：颜色 | 时间戳 | logger名字 | 具体的日志信息
        spdlog::set_pattern("%^[%T] %n: %v%$");
        s_core_logger = spdlog::stdout_color_mt("NEXAUR");
        s_core_logger->set_level(spdlog::level::trace);
    }
} // namespace NexAur
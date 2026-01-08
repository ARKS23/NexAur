#include "pch.h"
#include "Log.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace NexAur {
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

    void Log::Init() {
        //设置日志模式：颜色 | 时间戳 | logger名字 | 具体的日志信息
        spdlog::set_pattern("%^[%T] %n: %v%$");
        s_CoreLogger = spdlog::stdout_color_mt("NEXAUR");
        s_CoreLogger->set_level(spdlog::level::trace);

        s_ClientLogger = spdlog::stdout_color_mt("APP");
        s_ClientLogger->set_level(spdlog::level::trace);
    }
}
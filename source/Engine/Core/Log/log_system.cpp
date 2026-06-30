#include "pch.h"

#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "log_system.h"

#include <cstddef>
#include <mutex>
#include <utility>

namespace NexAur {
    std::shared_ptr<spdlog::logger> LogSystem::s_core_logger;
    std::vector<LogMessage> LogSystem::s_recent_messages;
    std::mutex LogSystem::s_recent_messages_mutex;

    class RecentLogSink final : public spdlog::sinks::base_sink<std::mutex> {
    protected:
        void sink_it_(const spdlog::details::log_msg& message) override {
            LogMessage recent_message;
            recent_message.level = message.level;
            recent_message.logger_name = std::string(message.logger_name.data(), message.logger_name.size());
            recent_message.message = std::string(message.payload.data(), message.payload.size());
            LogSystem::appendRecentMessage(std::move(recent_message));
        }

        void flush_() override {}
    };

    namespace {
        constexpr size_t kMaxRecentLogMessages = 512;

        void configureLogger(const std::shared_ptr<spdlog::logger>& logger) {
            if (!logger) {
                return;
            }

            logger->set_level(spdlog::level::trace);
            logger->set_pattern("%^[%T] %n: %v%$");
        }
    } // namespace

    void LogSystem::init() {
        if (s_core_logger) {
            return;
        }

        //设置日志模式：颜色 | 时间戳 | logger名字 | 具体的日志信息
        spdlog::set_pattern("%^[%T] %n: %v%$");

        s_core_logger = spdlog::get("NEXAUR");
        if (s_core_logger) {
            s_core_logger->sinks().push_back(std::make_shared<RecentLogSink>());
            configureLogger(s_core_logger);
            return;
        }

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto recent_sink = std::make_shared<RecentLogSink>();
        s_core_logger = std::make_shared<spdlog::logger>(
            "NEXAUR",
            spdlog::sinks_init_list{ console_sink, recent_sink });
        spdlog::register_logger(s_core_logger);
        configureLogger(s_core_logger);
    }

    std::vector<LogMessage> LogSystem::getRecentMessages() {
        std::scoped_lock lock(s_recent_messages_mutex);
        return s_recent_messages;
    }

    void LogSystem::clearRecentMessages() {
        std::scoped_lock lock(s_recent_messages_mutex);
        s_recent_messages.clear();
    }

    void LogSystem::appendRecentMessage(LogMessage message) {
        std::scoped_lock lock(s_recent_messages_mutex);

        s_recent_messages.push_back(std::move(message));
        if (s_recent_messages.size() <= kMaxRecentLogMessages) {
            return;
        }

        const size_t overflow_count = s_recent_messages.size() - kMaxRecentLogMessages;
        s_recent_messages.erase(s_recent_messages.begin(), s_recent_messages.begin() + static_cast<std::ptrdiff_t>(overflow_count));
    }

    const char* LogSystem::levelToString(spdlog::level::level_enum level) {
        switch (level) {
        case spdlog::level::trace:
            return "Trace";
        case spdlog::level::debug:
            return "Debug";
        case spdlog::level::info:
            return "Info";
        case spdlog::level::warn:
            return "Warn";
        case spdlog::level::err:
            return "Error";
        case spdlog::level::critical:
            return "Critical";
        case spdlog::level::off:
        default:
            return "Off";
        }
    }
} // namespace NexAur

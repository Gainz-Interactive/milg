#pragma once

#include <memory>

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace milg {
    class Logging {
    public:
        static void init();

        static std::shared_ptr<spdlog::logger> &logger();

    private:
        static std::shared_ptr<spdlog::logger> s_logger;
    };
} // namespace milg

#define MILG_TRACE(...)    ::milg::Logging::logger()->trace(__VA_ARGS__)
#define MILG_INFO(...)     ::milg::Logging::logger()->info(__VA_ARGS__)
#define MILG_DEBUG(...)    ::milg::Logging::logger()->debug(__VA_ARGS__)
#define MILG_WARN(...)     ::milg::Logging::logger()->warn(__VA_ARGS__)
#define MILG_ERROR(...)    ::milg::Logging::logger()->error(__VA_ARGS__)
#define MILG_CRITICAL(...) ::milg::Logging::logger()->critical(__VA_ARGS__)

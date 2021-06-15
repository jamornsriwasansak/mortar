#pragma once

#include <sstream>

#pragma warning(push, 0)
#include "spdlog/spdlog.h"
#pragma warning(pop)

struct Logger
{
    Logger() {}

    // info
    template <typename... Args>
    static void
    Info(const Args &... args)
    {
        std::ostringstream oss;
        using List = int[];
        (void)List{ 0, ((void)(oss << args), 0)... };
        spdlog::info(oss.str());
    }

    // warning
    template <typename... Args>
    static void
    Warn(const Args &... args)
    {
        std::ostringstream oss;
        using List = int[];
        (void)List{ 0, ((void)(oss << args), 0)... };
        spdlog::warn(oss.str());
    }

    // unexpected (e.g. file not found)
    template <bool IsThrow, typename... Args>
    static void
    Error(const Args &... args)
    {
        std::ostringstream oss;
        using List = int[];
        (void)List{ 0, ((void)(oss << args), 0)... };
        spdlog::error(oss.str());
        if (IsThrow)
        {
            throw std::runtime_error(oss.str());
        }
    }

    // unfixable (e.g. cannot create render device)
    template <bool IsThrow, typename... Args>
    static void
    Critical(const Args &... args)
    {
        std::ostringstream oss;
        using List = int[];
        (void)List{ 0, ((void)(oss << args), 0)... };
        spdlog::critical(oss.str());
        if (IsThrow)
        {
            throw std::runtime_error(oss.str());
        }
    }

    // debug
    template <typename... Args>
    static void
    Debug(const Args &... args)
    {
#ifndef NDEBUG
        std::ostringstream oss;
        using List = int[];
        (void)List{ 0, ((void)(oss << args), 0)... };

        // initialize debug level
        static bool init = false;
        if (!init)
        {
            spdlog::set_level(spdlog::level::level_enum::debug);
        }
        spdlog::debug(oss.str());
#endif
    }
};
#pragma once

#include <fmt/format.h>

namespace Logger {
    
template <typename... Args> inline void Debug(fmt::format_string<Args...> f, Args&&... args) {
    fmt::print(stderr, "[{}] [Debug] ", "terminal_apple");
    fmt::vprint(stderr, f, fmt::make_format_args(args...));
    fmt::print(stderr, "\n");
}

template <typename... Args> inline void Warning(fmt::format_string<Args...> f, Args&&... args) {
    fmt::print(stderr, "[{}] [Warning] ", "terminal_apple");
    fmt::vprint(stderr, f, fmt::make_format_args(args...));
    fmt::print(stderr, "\n");
}

template <typename... Args> inline void Error(fmt::format_string<Args...> f, Args&&... args) {
    fmt::print(stderr, "[{}] [Error] ", "terminal_apple");
    fmt::vprint(stderr, f, fmt::make_format_args(args...));
    fmt::print(stderr, "\n");
}

}
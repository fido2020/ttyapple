#pragma once

#if defined(__unix)

#include <unistd.h>

#define us_sleep(x) usleep(x)

#elif defined(WIN32)

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#define us_sleep(x) Sleep(x / 1000)

#else

#error "Unsupported platform!"

#endif

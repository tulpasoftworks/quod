/* SPDX-License-Identifier: MIT */
/* The engine's clock (quod_engine.h) on a Mac, Windows and other POSIX systems. (On the web the Rust crate gives it.)
 * Copyright (c) 2026 Kip DeCastro. See ../LICENSE. */
#include <stdint.h>
#include "quod_engine.h"

#if defined(__APPLE__)
#include <time.h>
uint64_t quod_clock_us(void) { return clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1000u; }
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
uint64_t quod_clock_us(void)
{
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (uint64_t)(c.QuadPart / f.QuadPart * 1000000 + c.QuadPart % f.QuadPart * 1000000 / f.QuadPart);
}
#else
#include <time.h>
uint64_t quod_clock_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}
#endif

#pragma once

#include <Arduino.h>

#ifndef ICSN_LOG_LEVEL
#define ICSN_LOG_LEVEL 3
#endif

#ifdef PERFORMANCE_MEASURE
#define ICSN_PERF_ENABLED 1
#else
#define ICSN_PERF_ENABLED 0
#endif

// LOG_*: level-controlled diagnostics
// LOG_*F: printf-style formatted output
// LOG_* : line-oriented output via println
#if ICSN_LOG_LEVEL >= 3
#define LOG_DEBUGF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#define LOG_DEBUG(msg) Serial.println(msg)
#else
#define LOG_DEBUGF(fmt, ...) ((void)0)
#define LOG_DEBUG(msg) ((void)0)
#endif

#if ICSN_LOG_LEVEL >= 2
#define LOG_INFOF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#define LOG_INFO(msg) Serial.println(msg)
#else
#define LOG_INFOF(fmt, ...) ((void)0)
#define LOG_INFO(msg) ((void)0)
#endif

#if ICSN_LOG_LEVEL >= 1
#define LOG_WARNF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#define LOG_WARN(msg) Serial.println(msg)
#else
#define LOG_WARNF(fmt, ...) ((void)0)
#define LOG_WARN(msg) ((void)0)
#endif

// CLI_* are for explicit command/interactive output and are always visible.
// Use LOG_* for profile-controlled diagnostic logging.
#define CLI_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#define CLI_PRINTLN(...) Serial.println(__VA_ARGS__)

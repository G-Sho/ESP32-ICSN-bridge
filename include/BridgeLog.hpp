#pragma once

#include <Arduino.h>

#define BRIDGE_LOG_LEVEL_WARN 1
#define BRIDGE_LOG_LEVEL_INFO 2
#define BRIDGE_LOG_LEVEL_DEBUG 3

#ifndef BRIDGE_LOG_LEVEL
#define BRIDGE_LOG_LEVEL BRIDGE_LOG_LEVEL_DEBUG
#endif

#define LOG_WARNF(component, eventFmt, ...)                                       \
    do                                                                            \
    {                                                                             \
        if (BRIDGE_LOG_LEVEL >= BRIDGE_LOG_LEVEL_WARN)                            \
        {                                                                         \
            Serial.printf("[WARN][%s] " eventFmt "\n", component, ##__VA_ARGS__); \
        }                                                                         \
    } while (0)

#define LOG_INFOF(component, eventFmt, ...)                                       \
    do                                                                            \
    {                                                                             \
        if (BRIDGE_LOG_LEVEL >= BRIDGE_LOG_LEVEL_INFO)                            \
        {                                                                         \
            Serial.printf("[INFO][%s] " eventFmt "\n", component, ##__VA_ARGS__); \
        }                                                                         \
    } while (0)

#define LOG_DEBUGF(component, eventFmt, ...)                                       \
    do                                                                             \
    {                                                                              \
        if (BRIDGE_LOG_LEVEL >= BRIDGE_LOG_LEVEL_DEBUG)                            \
        {                                                                          \
            Serial.printf("[DEBUG][%s] " eventFmt "\n", component, ##__VA_ARGS__); \
        }                                                                          \
    } while (0)

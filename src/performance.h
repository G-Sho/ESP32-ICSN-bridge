#pragma once
#include <stdint.h>
#include <esp_timer.h>

#define MAX_MEASUREMENTS 200

struct BridgeMeasurement {
    uint32_t interest_rx_us;      // Interest受信時刻
    uint32_t ota_start_us;        // OTA検証開始
    uint32_t ota_end_us;          // OTA検証終了
    uint32_t bridge_tx_us;        // bridge→A送信完了
    uint32_t data_rx_us;          // Data受信完了
};

/**
 * PerformanceBuffer — lock-free in-memory ring for bridge timing measurements.
 *
 * Thread-safety note: recordDataRx() (which advances `index`) is called from
 * the ESP-NOW receive callback (a FreeRTOS task), while getCount()/getEntry()
 * are called from the main loop.  `index` is declared volatile so the compiler
 * does not cache its value.  For the sequential, one-at-a-time Interest/Data
 * pattern this implementation targets, no additional synchronisation is needed.
 *
 * Capacity: once MAX_MEASUREMENTS is reached all record*() calls are no-ops.
 * Call reset() to start a new measurement run.
 */
class PerformanceBuffer {
private:
    BridgeMeasurement buffer[MAX_MEASUREMENTS];
    volatile uint16_t index = 0;

public:
    inline void recordInterestRx() {
        if (index >= MAX_MEASUREMENTS) return;
        buffer[index].interest_rx_us = (uint32_t)esp_timer_get_time();
    }

    inline void recordOtaStart() {
        if (index >= MAX_MEASUREMENTS) return;
        buffer[index].ota_start_us = (uint32_t)esp_timer_get_time();
    }

    inline void recordOtaEnd() {
        if (index >= MAX_MEASUREMENTS) return;
        buffer[index].ota_end_us = (uint32_t)esp_timer_get_time();
    }

    inline void recordBridgeTx() {
        if (index >= MAX_MEASUREMENTS) return;
        buffer[index].bridge_tx_us = (uint32_t)esp_timer_get_time();
    }

    inline void recordDataRx() {
        if (index >= MAX_MEASUREMENTS) return;
        buffer[index].data_rx_us = (uint32_t)esp_timer_get_time();
        index++;  // 1サンプル完了
    }

    uint16_t getCount() const { return index; }

    // Callers must ensure i < getCount().
    const BridgeMeasurement& getEntry(uint16_t i) const {
        if (i >= index) return buffer[0];
        return buffer[i];
    }

    void reset() { index = 0; }
};

extern PerformanceBuffer g_bridge_perf;

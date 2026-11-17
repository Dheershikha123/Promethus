#pragma once

#include <chrono>
#include <optional>

namespace prometheus::common {

/**
 * @brief PROVISIONAL TEAM CONTRACT (Owned by Developer A - Sensory & Windows Monitoring).
 * 
 * Developer B Notes:
 * - Time convention: std::chrono::steady_clock::time_point for event timestamps.
 * - Sensor validity is explicitly separated from zero values (invalid != 0.0).
 * - Battery, thermal, and GPU telemetry are strictly optional and may be std::nullopt.
 * 
 * STATUS: PROVISIONAL - REQUIRES APPROVAL FROM DEVELOPER A AND DEVELOPER C.
 */
struct HostObservation {
    // Timestamp using monotonic steady_clock
    std::chrono::steady_clock::time_point timestamp{std::chrono::steady_clock::now()};

    // Primary mandatory telemetry [0.0f, 1.0f]
    float cpu_load{0.0f};
    bool cpu_valid{false};

    float memory_pressure{0.0f};
    bool memory_valid{false};

    // Optional battery telemetry
    struct BatteryInfo {
        float level{1.0f};          // [0.0f, 1.0f], 1.0 = 100% charged
        bool is_discharging{false};  // true if discharging on battery power
        bool is_low_battery{false};  // true if critically low
    };
    std::optional<BatteryInfo> battery{std::nullopt};

    // Optional thermal telemetry
    struct ThermalInfo {
        float thermal_headroom{1.0f}; // [0.0f, 1.0f], 1.0 = cold/nominal, 0.0 = throttling limit reached
        bool is_throttling{false};
    };
    std::optional<ThermalInfo> thermal{std::nullopt};

    // Optional GPU telemetry
    struct GpuInfo {
        float load{0.0f};            // [0.0f, 1.0f]
        float memory_pressure{0.0f}; // [0.0f, 1.0f]
    };
    std::optional<GpuInfo> gpu{std::nullopt};

    // User interaction / desktop activity
    bool is_user_active{false};
};

} // namespace prometheus::common

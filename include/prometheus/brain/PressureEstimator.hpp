#pragma once

#include "prometheus/common/HostObservation.hpp"
#include "prometheus/brain/EmaFilter.hpp"
#include <cstdint>

namespace prometheus::brain {

/**
 * @brief Configurable weights for environmental pressure estimation.
 * 
 * Invariants:
 * - All weights must be finite and non-negative (>= 0.0f).
 * - Zero weights are permitted (the corresponding sensor will not contribute).
 */
struct PressureWeights {
    float cpu{0.40f};
    float memory{0.25f};
    float thermal{0.15f};
    float gpu_load{0.10f};
    float gpu_memory{0.10f};
};

/**
 * @brief Detailed result of an environmental pressure evaluation.
 */
struct PressureResult {
    float raw_pressure{0.0f};          // Instantaneous normalized pressure [0.0f, 1.0f]
    float smoothed_pressure{0.0f};     // EMA-filtered pressure [0.0f, 1.0f]
    bool is_valid{false};              // True if at least one usable sensor contributed to raw_pressure
    bool has_smoothed_value{false};    // True if EMA filter has been seeded with a valid estimate
    uint32_t contributing_signals{0};  // Count of valid signals with non-zero weight
    float total_valid_weight{0.0f};    // Sum of weights of contributing signals
};

/**
 * @brief Computes normalized environmental pressure from host observation telemetry.
 * 
 * Formula:
 *   P_env = sum(weight_i * pressure_i) / sum(valid_weight_i)
 * 
 * Refinement Guarantees:
 * 1. Missing or invalid sensors are omitted from both numerator and denominator.
 * 2. Invalid dt_seconds (<= 0.0f, NaN, Inf) is rejected; previous smoothed value is preserved.
 * 3. EMA is not seeded until the first valid pressure estimate arrives.
 * 4. If all sensors are invalid, is_valid is false, smoothed value is preserved (not corrupted to 0).
 * 5. Invalid weight configurations are rejected; last known-good weights are preserved.
 * 6. Thermal throttling overrides thermal headroom to 1.0f maximum pressure.
 */
class PressureEstimator {
public:
    explicit PressureEstimator(
        const PressureWeights& weights = PressureWeights{},
        float smoothing_tau_seconds = 1.0f
    ) noexcept;

    /**
     * @brief Stateless evaluation of instantaneous raw pressure.
     * Does not advance internal EMA smoothing filter.
     */
    [[nodiscard]] PressureResult estimate_raw(const common::HostObservation& obs) const noexcept;

    /**
     * @brief Stateful evaluation: calculates raw pressure and advances EMA smoothing filter.
     * 
     * @param obs Current host observation telemetry.
     * @param dt_seconds Elapsed simulation time in seconds. Must be finite and > 0.0f.
     *                   If invalid, raw pressure is calculated but EMA filter is not advanced.
     */
    PressureResult update(const common::HostObservation& obs, float dt_seconds) noexcept;

    /**
     * @brief Configure weights. Rejects invalid configurations and preserves previous weights.
     * @return true if weights were valid and accepted; false if rejected.
     */
    bool set_weights(const PressureWeights& weights) noexcept;
    [[nodiscard]] const PressureWeights& weights() const noexcept { return weights_; }

    /**
     * @brief Validate whether a weights structure contains finite, non-negative values.
     */
    [[nodiscard]] static bool validate_weights(const PressureWeights& weights) noexcept;

    void set_smoothing_tau(float tau_seconds) noexcept;
    void reset() noexcept;

    [[nodiscard]] const EmaFilter& filter() const noexcept { return filter_; }

private:
    [[nodiscard]] static bool is_valid_scalar(float v) noexcept;
    [[nodiscard]] static bool is_valid_dt(float dt) noexcept;

    PressureWeights weights_;
    EmaFilter filter_;
};

} // namespace prometheus::brain

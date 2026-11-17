#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace prometheus::brain {

/**
 * @brief Exponential Moving Average (EMA) Filter with safe missing-sensor handling.
 * 
 * Mathematical Formulation:
 * 
 * 1. Discrete Formulation:
 *    \bar{x}_k = \alpha \cdot x_k + (1 - \alpha) \cdot \bar{x}_{k-1}
 *    where \alpha \in (0, 1] is the smoothing weight.
 * 
 * 2. Continuous / Time-Aware Formulation:
 *    Given a time constant \tau (seconds) and elapsed time \Delta t (seconds):
 *    \alpha(\Delta t) = 1 - e^{-\Delta t / \tau}
 *    This ensures consistent smoothing dynamics even when simulation tick rates fluctuate.
 * 
 * Engineering Guarantees:
 * - Cold Start: First valid sample immediately seeds the filter, preventing false zero ramps.
 * - Sensor Safety: Invalid readings (is_valid == false, NaN, Inf) are skipped without corrupting state.
 * - Value Invariants: Output is strictly clamped within [min_bound, max_bound].
 * - Platform Independent: Pure C++20, noexcept operations, zero dynamic allocations.
 */
class EmaFilter {
public:
    /**
     * @brief Construct an uninitialized EMA filter with a discrete alpha.
     * @param alpha Smoothing factor in range (0.0f, 1.0f].
     * @param min_bound Minimum valid value clamp.
     * @param max_bound Maximum valid value clamp.
     */
    explicit EmaFilter(float alpha = 0.2f, float min_bound = 0.0f, float max_bound = 1.0f) noexcept;

    /**
     * @brief Factory method for creating an EMA filter configured with a time constant.
     * @param tau_seconds Response time constant in seconds (\tau > 0.0f).
     * @param min_bound Minimum valid value clamp.
     * @param max_bound Maximum valid value clamp.
     */
    static EmaFilter with_time_constant(float tau_seconds, float min_bound = 0.0f, float max_bound = 1.0f) noexcept;

    /**
     * @brief Update the filter with a discrete sample.
     * @param sample Incoming metric reading.
     * @param is_valid Whether this sample originates from a healthy, valid sensor reading.
     * @return The updated or retained smoothed value.
     */
    float update(float sample, bool is_valid = true) noexcept;

    /**
     * @brief Update the filter with an elapsed-time-aware sample.
     * @param sample Incoming metric reading.
     * @param dt_seconds Elapsed simulation time since last update in seconds.
     * @param is_valid Whether this sample is valid.
     * @return The updated or retained smoothed value.
     */
    float update_timed(float sample, float dt_seconds, bool is_valid = true) noexcept;

    /**
     * @brief Reset filter to cold, uninitialized state.
     */
    void reset() noexcept;

    /**
     * @brief Seed the filter directly with an initial estimate.
     */
    void seed(float initial_value) noexcept;

    // Configuration
    void set_alpha(float alpha) noexcept;
    void set_time_constant(float tau_seconds) noexcept;
    void set_bounds(float min_bound, float max_bound) noexcept;

    // Inspection
    [[nodiscard]] bool has_value() const noexcept { return has_value_; }
    [[nodiscard]] float value() const noexcept { return value_; }
    [[nodiscard]] float alpha() const noexcept { return alpha_; }
    [[nodiscard]] float time_constant() const noexcept { return tau_seconds_; }
    [[nodiscard]] uint32_t consecutive_invalid_count() const noexcept { return consecutive_invalid_count_; }
    [[nodiscard]] float min_bound() const noexcept { return min_bound_; }
    [[nodiscard]] float max_bound() const noexcept { return max_bound_; }

private:
    [[nodiscard]] static bool is_valid_float(float val) noexcept;

    float alpha_{0.2f};
    float tau_seconds_{1.0f};
    float min_bound_{0.0f};
    float max_bound_{1.0f};

    float value_{0.0f};
    bool has_value_{false};
    uint32_t consecutive_invalid_count_{0};
};

} // namespace prometheus::brain

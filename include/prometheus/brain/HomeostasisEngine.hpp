#pragma once

#include "prometheus/common/OrganismState.hpp"
#include "prometheus/brain/PressureEstimator.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace prometheus::brain {

/**
 * @brief Configurable parameters governing organism biological homeostasis.
 * 
 * Units:
 * - All state variables (E, H, S, C, A) are dimensionless in [0.0f, 1.0f].
 * - Time is measured in seconds (s).
 * - All rate parameters (c, r, k, d) are in units per second (s^-1).
 * - All threshold parameters are dimensionless fractions in [0.0f, 1.0f].
 */
struct HomeostasisConfig {
    // Energy parameters (all rates in s^-1)
    float basal_energy_burn_rate{0.010f};      // Base drain per second (lasts 100s if no recovery)
    float energy_pressure_burn_factor{0.030f}; // Additional drain per unit pressure
    float energy_rest_gain_rate{0.080f};       // Base replenishment rate when resting
    float low_energy_threshold{0.20f};         // E_low: Starvation stress begins (strictly > 0)
    float critical_energy_threshold{0.10f};    // E_crit: Health damage begins (strictly > 0)

    // Stress parameters
    float stress_rise_rate{0.50f};             // Fast relaxation rate constant (s^-1)
    float stress_decay_rate{0.10f};            // Slow relaxation rate constant (s^-1)
    float starvation_stress_weight{0.60f};     // Weight of starvation drive on S_target [0, 1]

    // Health parameters (all rates in s^-1)
    float health_starve_damage_rate{0.050f};   // Health drain rate when energy is 0
    float health_stress_damage_rate{0.025f};   // Health drain rate under extreme stress
    float health_heal_rate{0.020f};            // Health healing rate per second
    float critical_stress_threshold{0.80f};    // Stress level where damage begins (< 0.999)
    float heal_stress_threshold{0.30f};        // Max stress allowing healing (> 0.001)
    float heal_energy_threshold{0.40f};        // Min energy required to heal (< 0.999)

    // Curiosity parameters
    float curiosity_nominal_target{0.70f};     // Baseline calm target [0, 1]
    float curiosity_relax_rate{0.15f};         // Relaxation rate toward target (s^-1)
    float user_activity_boost{0.15f};          // Target boost when user active [0, 1]

    // Adaptation parameters
    float adaptation_rate{0.005f};             // Resilience accumulation rate (s^-1)
    float adaptation_stress_threshold{0.50f};  // Min stress to trigger adaptation [0, 1)
    float adaptation_min_health{0.20f};        // Min health to trigger adaptation [0, 1)
    float adaptation_mitigation_factor{0.30f}; // Max stress reduction at A=1.0 [0, 1]

    // Fallback & Sub-step parameters
    float fallback_pressure{0.50f};            // Assumed pressure when no sensor data exists
    float max_substep_seconds{0.10f};          // Sub-step interval ceiling for stability (s)
    float max_total_dt_seconds{5.00f};         // Cap on elapsed time processed per call (s)
};

/**
 * @brief Deterministic, lightweight C++20 homeostasis engine.
 * 
 * Updates the organism's five internal variables (Energy, Health, Stress, Curiosity, Adaptation)
 * over elapsed simulation time in seconds.
 * 
 * Safety & Invariant Guarantees:
 * 1. Strict Denominator Safety: All divisor thresholds (E_low, E_crit, critical_stress_threshold,
 *    heal_energy_threshold, heal_stress_threshold) are validated with non-zero margins.
 * 2. Freshness vs. Availability: Adaptation only accumulates from fresh valid telemetry
 *    (pressure_result.is_valid == true). Stale smoothed pressure or fallback is used for
 *    conservative metabolism updates but never accumulates adaptation.
 * 3. Bounded Sub-stepping: Input dt is capped to max_total_dt_seconds (excess discarded to
 *    prevent leap artifacts after pauses), and max_total_dt_seconds <= 50 * max_substep_seconds
 *    guarantees sub-step duration never exceeds max_substep_seconds.
 * 4. Boundary Enforcement & Ingress Sanitization: All incoming and outgoing state variables
 *    are guaranteed finite and within [0.0f, 1.0f].
 */
class HomeostasisEngine {
public:
    explicit HomeostasisEngine(const HomeostasisConfig& config = HomeostasisConfig{}) noexcept;

    /**
     * @brief Advance organism internal drives over dt_seconds.
     * 
     * @param current_state Current organism state (reads drives and current_behaviour).
     * @param pressure_result Evaluated environmental pressure from PressureEstimator.
     * @param dt_seconds Elapsed simulation time in seconds (must be finite and > 0.0f).
     * @param is_user_active Contextual desktop user activity flag.
     * @return Updated OrganismState with all continuous variables clamped to [0.0f, 1.0f].
     */
    [[nodiscard]] common::OrganismState update(
        const common::OrganismState& current_state,
        const PressureResult& pressure_result,
        float dt_seconds,
        bool is_user_active = false
    ) const noexcept;

    /**
     * @brief Configure engine parameters. Rejects invalid configurations and preserves current config.
     * @return true if accepted; false if rejected.
     */
    bool set_config(const HomeostasisConfig& config) noexcept;
    [[nodiscard]] const HomeostasisConfig& config() const noexcept { return config_; }

    /**
     * @brief Validate that all parameters in config are finite, non-negative, and mathematically consistent.
     */
    [[nodiscard]] static bool validate_config(const HomeostasisConfig& config) noexcept;

private:
    [[nodiscard]] static bool is_valid_scalar(float v) noexcept;
    [[nodiscard]] static bool is_in_range_01(float v) noexcept;

    [[nodiscard]] common::OrganismState sanitize_incoming_state(const common::OrganismState& state) const noexcept;

    struct ResolvedPressure {
        float value{0.50f};
        bool is_fresh{false};
    };
    [[nodiscard]] ResolvedPressure resolve_pressure(const PressureResult& pressure_result) const noexcept;

    [[nodiscard]] common::OrganismState single_substep(
        const common::OrganismState& state,
        float resolved_pressure,
        bool is_fresh_telemetry,
        float dt,
        bool is_user_active
    ) const noexcept;

    HomeostasisConfig config_;
};

} // namespace prometheus::brain

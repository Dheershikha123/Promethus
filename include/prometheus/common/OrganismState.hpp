#pragma once

#include <cstdint>

namespace prometheus::common {

/**
 * @brief PROVISIONAL TEAM CONTRACT (Owned by Developer B - Brain & Metabolism).
 * 
 * High-level behavioural states of the artificial organism.
 */
enum class Behaviour : uint8_t {
    Dormant = 0,    // Minimal survival mode, lowest possible computational footprint
    Resting,        // Recovery mode following severe stress or energy depletion
    Idle,           // Baseline equilibrium state
    Observing,      // Low-stress monitoring with heightened curiosity
    Exploring,      // Peak curiosity and active simulation
    Stressed        // Hostile host environment, survival adaptations active
};

/**
 * @brief PROVISIONAL TEAM CONTRACT (Owned by Developer B - Brain & Metabolism).
 * 
 * Internal biological drives of PROMETHEUS.
 * Invariants: all continuous state variables are strictly clamped to [0.0f, 1.0f].
 * Time convention: dwell_time_seconds is elapsed simulation time in seconds as float.
 * 
 * STATUS: PROVISIONAL - REQUIRES APPROVAL FROM DEVELOPER A AND DEVELOPER C.
 */
struct OrganismState {
    // Core biological drives [0.0f, 1.0f]
    float energy{1.0f};      // Vital drive: depleted by active computation/stress, replenished by resting
    float health{1.0f};      // Organism integrity: degrades under prolonged stress/starvation
    float stress{0.0f};      // Reaction to host resource pressure and low energy
    float curiosity{0.5f};   // Spontaneous exploratory drive: suppressed by stress
    float adaptation{0.0f};  // Long-term resilience gained by successfully enduring stress cycles

    // Current behaviour and dwell tracking
    Behaviour current_behaviour{Behaviour::Idle};
    float dwell_time_seconds{0.0f}; // Elapsed time in current behaviour (seconds)
};

} // namespace prometheus::common

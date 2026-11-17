#pragma once

#include <cstdint>

namespace prometheus::common {

/**
 * @brief PROVISIONAL TEAM CONTRACT (Owned by Developer B, consumed by Developer C).
 * 
 * Quality tiers 0 through 4 governing organism computational complexity.
 */
enum class QualityTier : uint8_t {
    Tier0_Dormant     = 0, // Emergency / Host Protection: minimum possible execution
    Tier1_Critical    = 1, // Severe resource constraints, degrade fast
    Tier2_Constrained = 2, // Moderate resource load
    Tier3_Nominal     = 3, // Baseline equilibrium operation
    Tier4_Optimal     = 4  // Host idle, unconstrained fidelity
};

/**
 * @brief PROVISIONAL TEAM CONTRACT (Owned by Developer B, consumed by Developer C).
 * 
 * Concrete execution constraints dispatched to Developer C (Body / Renderer / Runtime).
 * 
 * STATUS: PROVISIONAL - REQUIRES APPROVAL FROM DEVELOPER C.
 */
struct ComputeBudget {
    QualityTier tier{QualityTier::Tier3_Nominal};

    uint32_t max_particles{500};
    uint32_t target_sim_hz{30};
    uint32_t target_render_hz{30};
    bool enable_complex_physics{true};
    bool enable_secondary_effects{true};

    // Hard safety limits that cannot be overridden by any dynamic configuration
    static constexpr uint32_t HARD_MAX_PARTICLES{1000};
    static constexpr uint32_t HARD_MAX_SIM_HZ{60};
    static constexpr uint32_t HARD_MAX_RENDER_HZ{60};
    static constexpr uint32_t HARD_MIN_SIM_HZ{1};
    static constexpr uint32_t HARD_MIN_RENDER_HZ{1};
};

} // namespace prometheus::common

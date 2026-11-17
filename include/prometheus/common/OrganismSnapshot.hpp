#pragma once

#include "HostObservation.hpp"
#include "OrganismState.hpp"
#include "ComputeBudget.hpp"

namespace prometheus::common {

/**
 * @brief PROVISIONAL TEAM CONTRACT (Shared across Dev A, Dev B, Dev C).
 * 
 * Immutable snapshot of the organism and host at a single point in time.
 * Consumed by Developer C (UI/Rendering) and Developer A (Memory/Persistence).
 * 
 * STATUS: PROVISIONAL - REQUIRES APPROVAL FROM DEVELOPER A AND DEVELOPER C.
 */
struct OrganismSnapshot {
    HostObservation observation;
    OrganismState state;
    ComputeBudget budget;
    float environmental_pressure{0.0f}; // Computed environmental pressure [0.0f, 1.0f]
    float sim_time_seconds{0.0f};        // Total elapsed simulation time in seconds
};

} // namespace prometheus::common

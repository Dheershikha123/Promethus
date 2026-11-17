#include "gtest/gtest.h"
#include "prometheus/brain/EmaFilter.hpp"
#include <limits>
#include <cmath>

using prometheus::brain::EmaFilter;

// 1. Cold Start: First valid sample must immediately seed the filter without ramp-up lag
TEST(EmaFilterTest, ColdStartInitializesImmediately) {
    EmaFilter filter(0.2f);
    EXPECT_FALSE(filter.has_value());
    EXPECT_EQ(filter.consecutive_invalid_count(), 0u);

    float output = filter.update(0.75f, true);
    EXPECT_TRUE(filter.has_value());
    EXPECT_FLOAT_EQ(output, 0.75f);
    EXPECT_FLOAT_EQ(filter.value(), 0.75f);
}

// 2. Step Response: Exponential convergence toward steady-state input
TEST(EmaFilterTest, DiscreteStepResponseConvergence) {
    const float alpha = 0.5f;
    EmaFilter filter(alpha);

    // Initial seed at 0.0
    filter.update(0.0f, true);
    EXPECT_FLOAT_EQ(filter.value(), 0.0f);

    // Step input to 1.0:
    // Tick 1: 0.5 * 1.0 + 0.5 * 0.0 = 0.5
    float t1 = filter.update(1.0f, true);
    EXPECT_FLOAT_EQ(t1, 0.5f);

    // Tick 2: 0.5 * 1.0 + 0.5 * 0.5 = 0.75
    float t2 = filter.update(1.0f, true);
    EXPECT_FLOAT_EQ(t2, 0.75f);

    // Tick 3: 0.5 * 1.0 + 0.5 * 0.75 = 0.875
    float t3 = filter.update(1.0f, true);
    EXPECT_FLOAT_EQ(t3, 0.875f);
}

// 3. Time-Aware Update: Continuous EMA dynamics with variable delta-time
TEST(EmaFilterTest, TimedUpdateMatchesTimeConstant) {
    const float tau = 2.0f; // 2 seconds time constant
    auto filter = EmaFilter::with_time_constant(tau);

    // Seed at 0.0
    filter.update_timed(0.0f, 0.0f, true);
    EXPECT_FLOAT_EQ(filter.value(), 0.0f);

    // After dt = tau (2.0s), response to step input 1.0 should reach ~63.2%
    // alpha = 1 - e^(-2/2) = 1 - e^(-1) ~= 0.63212
    float output = filter.update_timed(1.0f, tau, true);
    float expected = 1.0f - std::exp(-1.0f);
    EXPECT_NEAR(output, expected, 0.001f);
}

// 4. Invalid Sensor Handling: Must NOT treat invalid as 0.0 or corrupt prior state
TEST(EmaFilterTest, InvalidSensorDoesNotCorruptStateOrSkewToZero) {
    EmaFilter filter(0.3f);
    filter.update(0.80f, true);
    EXPECT_FLOAT_EQ(filter.value(), 0.80f);

    // Inject invalid sensor readings (e.g. sensor failure or disconnected provider)
    float ret1 = filter.update(0.0f, false);
    EXPECT_FLOAT_EQ(ret1, 0.80f);
    EXPECT_FLOAT_EQ(filter.value(), 0.80f);
    EXPECT_EQ(filter.consecutive_invalid_count(), 1u);

    float ret2 = filter.update(0.10f, false);
    EXPECT_FLOAT_EQ(ret2, 0.80f);
    EXPECT_FLOAT_EQ(filter.value(), 0.80f);
    EXPECT_EQ(filter.consecutive_invalid_count(), 2u);

    // Recovery: valid reading resumes smoothing from the preserved 0.80 state
    float ret3 = filter.update(0.90f, true);
    // Expected: 0.3 * 0.90 + 0.7 * 0.80 = 0.27 + 0.56 = 0.83
    EXPECT_FLOAT_EQ(ret3, 0.83f);
    EXPECT_EQ(filter.consecutive_invalid_count(), 0u);
}

// 5. Numerical Robustness: NaN and Infinity protection
TEST(EmaFilterTest, ProtectsAgainstNanAndInfinity) {
    EmaFilter filter(0.25f);
    filter.update(0.5f, true);

    const float nan_val = std::numeric_limits<float>::quiet_NaN();
    const float inf_val = std::numeric_limits<float>::infinity();

    // Even if marked valid, NaN and Inf must be rejected safely
    filter.update(nan_val, true);
    EXPECT_FLOAT_EQ(filter.value(), 0.5f);
    EXPECT_EQ(filter.consecutive_invalid_count(), 1u);

    filter.update(inf_val, true);
    EXPECT_FLOAT_EQ(filter.value(), 0.5f);
    EXPECT_EQ(filter.consecutive_invalid_count(), 2u);
}

// 6. Invariant Bounds Clamping
TEST(EmaFilterTest, EnforcesRangeBounds) {
    EmaFilter filter(0.5f, 0.0f, 1.0f);

    // Input above max bound (1.0) must clamp
    filter.update(5.0f, true);
    EXPECT_FLOAT_EQ(filter.value(), 1.0f);

    // Input below min bound (0.0) must clamp
    filter.update(-10.0f, true);
    EXPECT_FLOAT_EQ(filter.value(), 0.5f); // 0.5 * 0.0 + 0.5 * 1.0 = 0.5

    filter.update(-10.0f, true);
    EXPECT_FLOAT_EQ(filter.value(), 0.25f); // 0.5 * 0.0 + 0.5 * 0.5 = 0.25
}

// 7. Reset and Seeding
TEST(EmaFilterTest, ResetAndSeedBehavior) {
    EmaFilter filter(0.2f);
    filter.update(0.9f, true);
    EXPECT_TRUE(filter.has_value());

    filter.reset();
    EXPECT_FALSE(filter.has_value());
    EXPECT_FLOAT_EQ(filter.value(), 0.0f);
    EXPECT_EQ(filter.consecutive_invalid_count(), 0u);

    // Seed directly with 0.42
    filter.seed(0.42f);
    EXPECT_TRUE(filter.has_value());
    EXPECT_FLOAT_EQ(filter.value(), 0.42f);
}

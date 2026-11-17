#include "gtest/gtest.h"
#include "prometheus/brain/PressureEstimator.hpp"
#include <limits>
#include <cmath>

using prometheus::brain::PressureEstimator;
using prometheus::brain::PressureWeights;
using prometheus::common::HostObservation;

// 1. CPU-only input: Only CPU is valid; pressure must equal CPU load regardless of other weights
TEST(PressureEstimatorTest, CpuOnlyInputNormalizesCorrectly) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_load = 0.70f;
    obs.cpu_valid = true;
    obs.memory_valid = false;
    obs.thermal = std::nullopt;
    obs.gpu = std::nullopt;

    auto result = estimator.estimate_raw(obs);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.contributing_signals, 1u);
    EXPECT_FLOAT_EQ(result.total_valid_weight, estimator.weights().cpu);
    EXPECT_FLOAT_EQ(result.raw_pressure, 0.70f);
}

// 2. RAM-only input: Only RAM is valid; pressure must equal RAM pressure
TEST(PressureEstimatorTest, RamOnlyInputNormalizesCorrectly) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_valid = false;
    obs.memory_pressure = 0.85f;
    obs.memory_valid = true;
    obs.thermal = std::nullopt;
    obs.gpu = std::nullopt;

    auto result = estimator.estimate_raw(obs);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.contributing_signals, 1u);
    EXPECT_FLOAT_EQ(result.total_valid_weight, estimator.weights().memory);
    EXPECT_FLOAT_EQ(result.raw_pressure, 0.85f);
}

// 3. CPU and RAM combined: Normalized dynamically over valid weights sum
TEST(PressureEstimatorTest, CpuAndRamCombined) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_load = 0.60f;
    obs.cpu_valid = true;
    obs.memory_pressure = 0.80f;
    obs.memory_valid = true;
    obs.thermal = std::nullopt;
    obs.gpu = std::nullopt;

    // Expected: (0.40 * 0.60 + 0.25 * 0.80) / (0.40 + 0.25) = (0.24 + 0.20) / 0.65 = 0.44 / 0.65
    const float expected = (0.40f * 0.60f + 0.25f * 0.80f) / (0.40f + 0.25f);

    auto result = estimator.estimate_raw(obs);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.contributing_signals, 2u);
    EXPECT_FLOAT_EQ(result.total_valid_weight, 0.65f);
    EXPECT_FLOAT_EQ(result.raw_pressure, expected);
}

// 4. GPU unavailable: GPU omission does not penalize denominator
TEST(PressureEstimatorTest, GpuUnavailableDoesNotPenalizeScore) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_load = 0.50f;
    obs.cpu_valid = true;
    obs.memory_pressure = 0.50f;
    obs.memory_valid = true;
    obs.gpu = std::nullopt;

    auto result = estimator.estimate_raw(obs);
    EXPECT_TRUE(result.is_valid);
    // Both signals are 0.50, so weighted sum / valid weight should exactly be 0.50
    EXPECT_FLOAT_EQ(result.raw_pressure, 0.50f);
}

// 5. GPU available: All 5 signals active and factored into calculation
TEST(PressureEstimatorTest, AllFiveSignalsActive) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_load = 0.50f;
    obs.cpu_valid = true;
    obs.memory_pressure = 0.60f;
    obs.memory_valid = true;

    HostObservation::ThermalInfo thermal;
    thermal.thermal_headroom = 0.70f; // thermal pressure = 1.0 - 0.70 = 0.30
    thermal.is_throttling = false;
    obs.thermal = thermal;

    HostObservation::GpuInfo gpu;
    gpu.load = 0.80f;
    gpu.memory_pressure = 0.90f;
    obs.gpu = gpu;

    // Default weights: cpu=0.40, mem=0.25, thermal=0.15, gpu_load=0.10, gpu_mem=0.10 (sum=1.0)
    // Numerator: 0.40*0.50 + 0.25*0.60 + 0.15*0.30 + 0.10*0.80 + 0.10*0.90
    //          = 0.20 + 0.15 + 0.045 + 0.08 + 0.09 = 0.565
    const float expected = 0.40f * 0.50f + 0.25f * 0.60f + 0.15f * 0.30f + 0.10f * 0.80f + 0.10f * 0.90f;

    auto result = estimator.estimate_raw(obs);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.contributing_signals, 5u);
    EXPECT_FLOAT_EQ(result.total_valid_weight, 1.00f);
    EXPECT_FLOAT_EQ(result.raw_pressure, expected);
}

// 6. Thermal unavailable: Thermal omission leaves total weight normalized
TEST(PressureEstimatorTest, ThermalUnavailable) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_load = 0.50f;
    obs.cpu_valid = true;
    obs.memory_pressure = 0.50f;
    obs.memory_valid = true;
    obs.thermal = std::nullopt;

    auto result = estimator.estimate_raw(obs);
    EXPECT_TRUE(result.is_valid);
    EXPECT_FLOAT_EQ(result.raw_pressure, 0.50f);
}

// 7. Thermal headroom inversion: P_thermal = 1.0 - headroom
TEST(PressureEstimatorTest, ThermalHeadroomInversion) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_valid = false;
    obs.memory_valid = false;

    HostObservation::ThermalInfo thermal;
    thermal.thermal_headroom = 0.80f; // P_thermal = 0.20
    thermal.is_throttling = false;
    obs.thermal = thermal;

    auto result = estimator.estimate_raw(obs);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.contributing_signals, 1u);
    EXPECT_FLOAT_EQ(result.raw_pressure, 0.20f);
}

// 8. Thermal throttling override: is_throttling = true forces P_thermal to 1.0
TEST(PressureEstimatorTest, ThermalThrottlingOverridesHeadroom) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_valid = false;
    obs.memory_valid = false;

    HostObservation::ThermalInfo thermal;
    thermal.thermal_headroom = 0.90f; // High headroom, but throttling active!
    thermal.is_throttling = true;
    obs.thermal = thermal;

    auto result = estimator.estimate_raw(obs);
    EXPECT_TRUE(result.is_valid);
    EXPECT_FLOAT_EQ(result.raw_pressure, 1.00f);
}

// 9. All sensors invalid: Returns is_valid = false, contributing_signals = 0
TEST(PressureEstimatorTest, AllSensorsInvalidReturnsInvalid) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_valid = false;
    obs.memory_valid = false;
    obs.thermal = std::nullopt;
    obs.gpu = std::nullopt;

    auto result = estimator.estimate_raw(obs);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.contributing_signals, 0u);
    EXPECT_FLOAT_EQ(result.total_valid_weight, 0.0f);
    EXPECT_FLOAT_EQ(result.raw_pressure, 0.0f);
}

// 10. NaN and Infinity inputs: Rejected safely without contaminating valid signals
TEST(PressureEstimatorTest, NanAndInfinityRejectedSafely) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_load = std::numeric_limits<float>::quiet_NaN();
    obs.cpu_valid = true; // Claimed valid, but payload is NaN!

    obs.memory_pressure = 0.60f;
    obs.memory_valid = true;

    auto result = estimator.estimate_raw(obs);
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.contributing_signals, 1u); // Only memory should be accepted
    EXPECT_FLOAT_EQ(result.raw_pressure, 0.60f);

    // Now test infinity on memory
    obs.cpu_load = 0.40f;
    obs.memory_pressure = std::numeric_limits<float>::infinity();

    auto result2 = estimator.estimate_raw(obs);
    EXPECT_TRUE(result2.is_valid);
    EXPECT_EQ(result2.contributing_signals, 1u); // Only CPU should be accepted
    EXPECT_FLOAT_EQ(result2.raw_pressure, 0.40f);
}

// 11. Values outside [0, 1] clamped safely
TEST(PressureEstimatorTest, OutOfRangeValuesClamped) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_load = 2.50f; // > 1.0
    obs.cpu_valid = true;
    obs.memory_pressure = -0.75f; // < 0.0
    obs.memory_valid = true;

    // Expected: CPU clamped to 1.0, RAM clamped to 0.0
    // (0.40 * 1.0 + 0.25 * 0.0) / 0.65 = 0.40 / 0.65
    const float expected = 0.40f / 0.65f;

    auto result = estimator.estimate_raw(obs);
    EXPECT_TRUE(result.is_valid);
    EXPECT_FLOAT_EQ(result.raw_pressure, expected);
}

// 12. Weight normalization: Dynamic sum in denominator
TEST(PressureEstimatorTest, DynamicWeightNormalization) {
    PressureWeights custom_weights;
    custom_weights.cpu = 2.0f;
    custom_weights.memory = 3.0f;
    PressureEstimator estimator(custom_weights);

    HostObservation obs;
    obs.cpu_load = 0.20f;
    obs.cpu_valid = true;
    obs.memory_pressure = 0.70f;
    obs.memory_valid = true;

    // (2.0 * 0.20 + 3.0 * 0.70) / (2.0 + 3.0) = (0.40 + 2.10) / 5.0 = 2.50 / 5.0 = 0.50
    auto result = estimator.estimate_raw(obs);
    EXPECT_TRUE(result.is_valid);
    EXPECT_FLOAT_EQ(result.total_valid_weight, 5.0f);
    EXPECT_FLOAT_EQ(result.raw_pressure, 0.50f);
}

// 13. Zero total valid weight: Returns invalid without division by zero
TEST(PressureEstimatorTest, ZeroTotalValidWeightReturnsInvalid) {
    PressureWeights zero_weights;
    zero_weights.cpu = 0.0f;
    zero_weights.memory = 0.0f;
    zero_weights.thermal = 0.0f;
    zero_weights.gpu_load = 0.0f;
    zero_weights.gpu_memory = 0.0f;

    PressureEstimator estimator(zero_weights);

    HostObservation obs;
    obs.cpu_load = 0.90f;
    obs.cpu_valid = true;

    auto result = estimator.estimate_raw(obs);
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.contributing_signals, 0u);
    EXPECT_FLOAT_EQ(result.total_valid_weight, 0.0f);
    EXPECT_FLOAT_EQ(result.raw_pressure, 0.0f);
}

// 14. Deterministic results: Identical inputs yield identical results
TEST(PressureEstimatorTest, DeterministicResults) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_load = 0.4242f;
    obs.cpu_valid = true;
    obs.memory_pressure = 0.7171f;
    obs.memory_valid = true;

    auto res1 = estimator.estimate_raw(obs);
    auto res2 = estimator.estimate_raw(obs);
    EXPECT_EQ(res1.is_valid, res2.is_valid);
    EXPECT_FLOAT_EQ(res1.raw_pressure, res2.raw_pressure);
    EXPECT_FLOAT_EQ(res1.total_valid_weight, res2.total_valid_weight);
}

// 15. Invalid elapsed time (dt <= 0, NaN, Inf) preserves smoothed value without updating EMA
TEST(PressureEstimatorTest, InvalidDtPreservesSmoothedState) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_load = 0.50f;
    obs.cpu_valid = true;

    // First valid update seeds the filter at 0.50
    auto r1 = estimator.update(obs, 1.0f);
    EXPECT_TRUE(r1.is_valid);
    EXPECT_TRUE(r1.has_smoothed_value);
    EXPECT_FLOAT_EQ(r1.smoothed_pressure, 0.50f);

    // Next update has new raw pressure 1.00, but invalid dt <= 0
    obs.cpu_load = 1.00f;
    auto r2 = estimator.update(obs, 0.0f);
    EXPECT_TRUE(r2.is_valid);
    EXPECT_FLOAT_EQ(r2.raw_pressure, 1.00f);
    // Smoothed pressure must remain preserved at 0.50!
    EXPECT_FLOAT_EQ(r2.smoothed_pressure, 0.50f);
    EXPECT_TRUE(r2.has_smoothed_value);

    // Negative dt
    auto r3 = estimator.update(obs, -2.5f);
    EXPECT_FLOAT_EQ(r3.smoothed_pressure, 0.50f);

    // NaN dt
    auto r4 = estimator.update(obs, std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(r4.smoothed_pressure, 0.50f);

    // Inf dt
    auto r5 = estimator.update(obs, std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(r5.smoothed_pressure, 0.50f);
}

// 16. First-valid-sample EMA initialization directly seeds filter without zero-ramp lag
TEST(PressureEstimatorTest, FirstValidSampleDirectlySeedsEma) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_load = 0.88f;
    obs.cpu_valid = true;

    auto r = estimator.update(obs, 0.1f);
    EXPECT_TRUE(r.is_valid);
    EXPECT_TRUE(r.has_smoothed_value);
    // Must immediately equal 0.88, not ramp from 0.0
    EXPECT_FLOAT_EQ(r.smoothed_pressure, 0.88f);
}

// 17. All-invalid observation after a valid estimate preserves smoothed value
TEST(PressureEstimatorTest, AllInvalidAfterValidEstimatePreservesSmoothed) {
    PressureEstimator estimator;
    HostObservation obs;
    obs.cpu_load = 0.70f;
    obs.cpu_valid = true;

    estimator.update(obs, 1.0f);

    // Sensor dies
    obs.cpu_valid = false;
    obs.memory_valid = false;
    obs.thermal = std::nullopt;
    obs.gpu = std::nullopt;

    auto r = estimator.update(obs, 1.0f);
    EXPECT_FALSE(r.is_valid);
    EXPECT_TRUE(r.has_smoothed_value);
    // Preserved at 0.70, not corrupted to 0.0
    EXPECT_FLOAT_EQ(r.smoothed_pressure, 0.70f);
}

// 18. Invalid weight configurations rejected; last known-good weights preserved
TEST(PressureEstimatorTest, InvalidWeightsRejectedPreservesLastKnownGood) {
    PressureEstimator estimator;
    const auto original_weights = estimator.weights();

    PressureWeights bad_weights = original_weights;
    bad_weights.cpu = -0.5f; // Negative weight!

    EXPECT_FALSE(estimator.set_weights(bad_weights));
    EXPECT_FLOAT_EQ(estimator.weights().cpu, original_weights.cpu);

    bad_weights.cpu = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(estimator.set_weights(bad_weights));
    EXPECT_FLOAT_EQ(estimator.weights().cpu, original_weights.cpu);

    bad_weights.cpu = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(estimator.set_weights(bad_weights));
    EXPECT_FLOAT_EQ(estimator.weights().cpu, original_weights.cpu);
}

// 19. Zero-weight signals do not contribute to sum, valid weight, or count
TEST(PressureEstimatorTest, ZeroWeightSignalsIgnored) {
    PressureWeights weights;
    weights.cpu = 0.50f;
    weights.memory = 0.0f; // Disabled sensor
    weights.thermal = 0.0f;
    weights.gpu_load = 0.0f;
    weights.gpu_memory = 0.0f;

    PressureEstimator estimator(weights);
    HostObservation obs;
    obs.cpu_load = 0.60f;
    obs.cpu_valid = true;
    obs.memory_pressure = 0.99f; // High memory, but weight is 0
    obs.memory_valid = true;

    auto r = estimator.estimate_raw(obs);
    EXPECT_TRUE(r.is_valid);
    EXPECT_EQ(r.contributing_signals, 1u); // Only CPU counted
    EXPECT_FLOAT_EQ(r.total_valid_weight, 0.50f);
    EXPECT_FLOAT_EQ(r.raw_pressure, 0.60f);
}

// 20. Constructor rejects invalid weights and falls back to default
TEST(PressureEstimatorTest, ConstructorRejectsInvalidWeights) {
    PressureWeights invalid_weights;
    invalid_weights.cpu = -1.0f;

    PressureEstimator estimator(invalid_weights);
    EXPECT_FLOAT_EQ(estimator.weights().cpu, 0.40f); // Default preserved
}

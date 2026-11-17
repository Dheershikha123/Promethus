#include "prometheus/brain/PressureEstimator.hpp"
#include <algorithm>
#include <cmath>

namespace prometheus::brain {

bool PressureEstimator::is_valid_scalar(float v) noexcept {
    return !std::isnan(v) && !std::isinf(v);
}

bool PressureEstimator::is_valid_dt(float dt) noexcept {
    return is_valid_scalar(dt) && (dt > 0.0f);
}

bool PressureEstimator::validate_weights(const PressureWeights& w) noexcept {
    auto is_valid_w = [](float v) noexcept {
        return is_valid_scalar(v) && (v >= 0.0f);
    };

    return is_valid_w(w.cpu) &&
           is_valid_w(w.memory) &&
           is_valid_w(w.thermal) &&
           is_valid_w(w.gpu_load) &&
           is_valid_w(w.gpu_memory);
}

PressureEstimator::PressureEstimator(
    const PressureWeights& weights,
    float smoothing_tau_seconds
) noexcept
    : filter_(EmaFilter::with_time_constant(smoothing_tau_seconds, 0.0f, 1.0f))
{
    if (validate_weights(weights)) {
        weights_ = weights;
    } else {
        weights_ = PressureWeights{};
    }
}

bool PressureEstimator::set_weights(const PressureWeights& weights) noexcept {
    if (!validate_weights(weights)) {
        return false;
    }
    weights_ = weights;
    return true;
}

void PressureEstimator::set_smoothing_tau(float tau_seconds) noexcept {
    filter_.set_time_constant(tau_seconds);
}

void PressureEstimator::reset() noexcept {
    filter_.reset();
}

PressureResult PressureEstimator::estimate_raw(const common::HostObservation& obs) const noexcept {
    PressureResult result{};
    float weighted_sum = 0.0f;
    float valid_weight_sum = 0.0f;
    uint32_t contributing_count = 0;

    auto consider_signal = [&](bool is_valid, float val, float weight) noexcept {
        if (!is_valid || !is_valid_scalar(val) || (weight <= 0.0f)) {
            return;
        }
        const float clamped_val = std::clamp(val, 0.0f, 1.0f);
        weighted_sum += (weight * clamped_val);
        valid_weight_sum += weight;
        ++contributing_count;
    };

    // 1. CPU Load
    consider_signal(obs.cpu_valid, obs.cpu_load, weights_.cpu);

    // 2. Memory Pressure
    consider_signal(obs.memory_valid, obs.memory_pressure, weights_.memory);

    // 3. Thermal Telemetry
    if (obs.thermal.has_value() && is_valid_scalar(obs.thermal->thermal_headroom)) {
        float thermal_pressure = 0.0f;
        if (obs.thermal->is_throttling) {
            thermal_pressure = 1.0f;
        } else {
            thermal_pressure = 1.0f - std::clamp(obs.thermal->thermal_headroom, 0.0f, 1.0f);
        }
        consider_signal(true, thermal_pressure, weights_.thermal);
    }

    // 4. GPU Telemetry
    if (obs.gpu.has_value()) {
        consider_signal(true, obs.gpu->load, weights_.gpu_load);
        consider_signal(true, obs.gpu->memory_pressure, weights_.gpu_memory);
    }

    result.contributing_signals = contributing_count;
    result.total_valid_weight = valid_weight_sum;

    if (valid_weight_sum > 0.0f) {
        result.raw_pressure = std::clamp(weighted_sum / valid_weight_sum, 0.0f, 1.0f);
        result.is_valid = true;
    } else {
        result.raw_pressure = 0.0f;
        result.is_valid = false;
    }

    // Mirror current filter state without mutating it
    result.has_smoothed_value = filter_.has_value();
    result.smoothed_pressure = filter_.has_value() ? filter_.value() : 0.0f;

    return result;
}

PressureResult PressureEstimator::update(const common::HostObservation& obs, float dt_seconds) noexcept {
    PressureResult result = estimate_raw(obs);

    const bool valid_dt = is_valid_dt(dt_seconds);

    if (result.is_valid && valid_dt) {
        // Advance EMA filter with valid pressure and valid elapsed time
        result.smoothed_pressure = filter_.update_timed(result.raw_pressure, dt_seconds, true);
        result.has_smoothed_value = true;
    } else {
        // Missing/invalid sensor data or invalid dt: preserve previous smoothed state
        result.has_smoothed_value = filter_.has_value();
        result.smoothed_pressure = filter_.has_value() ? filter_.value() : 0.0f;
    }

    return result;
}

} // namespace prometheus::brain

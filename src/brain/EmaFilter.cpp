#include "prometheus/brain/EmaFilter.hpp"

namespace prometheus::brain {

bool EmaFilter::is_valid_float(float val) noexcept {
    return !std::isnan(val) && !std::isinf(val);
}

EmaFilter::EmaFilter(float alpha, float min_bound, float max_bound) noexcept {
    set_bounds(min_bound, max_bound);
    set_alpha(alpha);
}

EmaFilter EmaFilter::with_time_constant(float tau_seconds, float min_bound, float max_bound) noexcept {
    EmaFilter filter(0.2f, min_bound, max_bound);
    filter.set_time_constant(tau_seconds);
    return filter;
}

void EmaFilter::set_alpha(float alpha) noexcept {
    alpha_ = std::clamp(alpha, 0.0001f, 1.0f);
}

void EmaFilter::set_time_constant(float tau_seconds) noexcept {
    tau_seconds_ = std::max(tau_seconds, 0.0001f);
}

void EmaFilter::set_bounds(float min_bound, float max_bound) noexcept {
    if (min_bound > max_bound) {
        std::swap(min_bound, max_bound);
    }
    min_bound_ = min_bound;
    max_bound_ = max_bound;
    if (has_value_) {
        value_ = std::clamp(value_, min_bound_, max_bound_);
    }
}

void EmaFilter::reset() noexcept {
    value_ = 0.0f;
    has_value_ = false;
    consecutive_invalid_count_ = 0;
}

void EmaFilter::seed(float initial_value) noexcept {
    if (is_valid_float(initial_value)) {
        value_ = std::clamp(initial_value, min_bound_, max_bound_);
        has_value_ = true;
        consecutive_invalid_count_ = 0;
    }
}

float EmaFilter::update(float sample, bool is_valid) noexcept {
    if (!is_valid || !is_valid_float(sample)) {
        ++consecutive_invalid_count_;
        return value_;
    }

    consecutive_invalid_count_ = 0;
    const float clamped_sample = std::clamp(sample, min_bound_, max_bound_);

    if (!has_value_) {
        // Cold start: directly initialize with first valid sample to avoid zero-ramp lag
        value_ = clamped_sample;
        has_value_ = true;
    } else {
        value_ = (alpha_ * clamped_sample) + ((1.0f - alpha_) * value_);
        value_ = std::clamp(value_, min_bound_, max_bound_);
    }

    return value_;
}

float EmaFilter::update_timed(float sample, float dt_seconds, bool is_valid) noexcept {
    if (!is_valid || !is_valid_float(sample)) {
        ++consecutive_invalid_count_;
        return value_;
    }

    consecutive_invalid_count_ = 0;
    const float clamped_sample = std::clamp(sample, min_bound_, max_bound_);

    if (!has_value_) {
        value_ = clamped_sample;
        has_value_ = true;
        return value_;
    }

    if (dt_seconds <= 0.0f) {
        return value_;
    }

    // Continuous time-aware alpha: alpha(dt) = 1 - e^(-dt / tau)
    float effective_alpha = 1.0f - std::exp(-dt_seconds / tau_seconds_);
    effective_alpha = std::clamp(effective_alpha, 0.0f, 1.0f);

    value_ = (effective_alpha * clamped_sample) + ((1.0f - effective_alpha) * value_);
    value_ = std::clamp(value_, min_bound_, max_bound_);

    return value_;
}

} // namespace prometheus::brain

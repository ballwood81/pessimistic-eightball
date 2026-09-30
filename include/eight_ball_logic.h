#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

struct Acceleration {
    float x_g;
    float y_g;
    float z_g;
};

// Glass outward normal, and accel while seated upright. The face is ~45° to horizontal,
// so gravity splits across the normal (Z) and the glass (mostly Y, a little X).
// ponytail: flip kSeatedAccel if that pose comes up upside down.
constexpr Acceleration kScreenNormal{0.0F, 0.0F, -1.0F};
constexpr Acceleration kSeatedAccel{-0.04F, 0.67F, -0.70F};

struct MountLockConfig {
    float still_band_g{0.15F};          // | |a| - 1 | ; shakes are dropped, not averaged
    float min_plane_g{0.25F};           // in-plane up below this is face-on, not a mount
    std::uint32_t need_still_ms{2000};
    std::uint32_t gap_ms{40};           // a longer gap is not continuous stillness
    std::uint32_t deadline_ms{10000};
    std::uint8_t fallback{0};
};

struct MountLock {
    float sum_x{0};
    float sum_y{0};
    float sum_z{0};
    std::uint32_t still_ms{0};

    // True once enough quiet samples have accumulated.
    bool add(const Acceleration& sample, std::uint32_t now_ms, const MountLockConfig& config = {});
    std::uint8_t rotation(const MountLockConfig& config = {}) const;

private:
    std::uint32_t last_ms_{0};
    bool have_last_{false};
};

// After boot, one gravity sample a second. A slow turn flips 0 or 2 once up crosses the deadband.
struct FaceOrientConfig {
    std::uint32_t probe_ms{1000};
    float still_band_g{0.60F};
    float flip_g{0.20F};
};

struct FaceOrient {
    void arm(std::uint32_t now_ms, const FaceOrientConfig& config = {});
    bool wants_sample(std::uint32_t now_ms, const FaceOrientConfig& config = {}) const;
    // True when `out` (0 or 2) should replace the current rotation.
    bool update(
        const Acceleration& sample,
        std::uint32_t now_ms,
        std::uint8_t current,
        std::uint8_t& out,
        const FaceOrientConfig& config = {});

private:
    std::uint32_t next_probe_ms_{0};
};

inline bool still_gravity(const Acceleration& sample, float still_band_g) {
    const float mag = std::sqrt(
        sample.x_g * sample.x_g + sample.y_g * sample.y_g + sample.z_g * sample.z_g);
    return mag >= 0.2F && std::fabs(mag - 1.0F) <= still_band_g;
}

inline float dot_g(const Acceleration& a, const Acceleration& b) {
    return a.x_g * b.x_g + a.y_g * b.y_g + a.z_g * b.z_g;
}

// Gravity with the glass-normal removed. What is left is up/down on the picture.
inline Acceleration on_screen(const Acceleration& sample) {
    const float along = dot_g(sample, kScreenNormal);
    return {
        sample.x_g - along * kScreenNormal.x_g,
        sample.y_g - along * kScreenNormal.y_g,
        sample.z_g - along * kScreenNormal.z_g,
    };
}

// Positive when this pose matches the seated picture. Uses all three axes.
inline float screen_up_g(const Acceleration& sample) {
    return dot_g(on_screen(sample), on_screen(kSeatedAccel));
}

// False when down on the glass is inside the deadband. 0 or 2 only.
inline bool rotation_from_axes(const Acceleration& sample, float min_up_g, std::uint8_t& rotation) {
    const float up = screen_up_g(sample);
    if (std::fabs(up) < min_up_g) {
        return false;
    }
    rotation = up >= 0.0F ? static_cast<std::uint8_t>(0) : static_cast<std::uint8_t>(2);
    return true;
}

// Tunables for shake — edit here, not in main.
struct ShakeConfig {
    float enter_threshold_g{0.75F};   // | |a| - 1g | to count a hit
    float exit_threshold_g{0.30F};    // must fall below this to arm next hit
    std::uint8_t required_hits{3};
    std::uint32_t hit_window_ms{450};
    std::uint32_t cooldown_ms{400};   // leftover motion after a shake
};

class ShakeDetector {
public:
    explicit ShakeDetector(ShakeConfig config = {});

    bool update(const Acceleration& sample, std::uint32_t now_ms);
    void reset();

private:
    ShakeConfig config_;
    std::uint32_t first_hit_ms_{0};
    std::uint32_t fired_at_ms_{0};
    std::uint8_t hit_count_{0};
    bool armed_{true};
    bool latched_{false};
};

class ResponsePicker {
public:
    explicit ResponsePicker(std::size_t response_count);

    std::size_t pick(std::uint32_t random_value);

private:
    std::size_t response_count_;
    std::size_t previous_index_;
    bool has_previous_{false};
};

// IDLE → TRIGGERED → REVEALING → VISIBLE → RESETTING → IDLE
enum class BallState : std::uint8_t {
    idle,
    triggered,
    revealing,
    visible,
    resetting,
};

struct BallTiming {
    std::uint32_t reveal_ms{700};
    std::uint32_t visible_ms{7500};    // half of the old 15s hold
    std::uint32_t reset_ms{400};
    std::uint32_t input_lock_ms{280};  // one gesture, then tap/shake again
};

class BallMachine {
public:
    explicit BallMachine(BallTiming timing = {});

    bool request_trigger(std::uint32_t now_ms);
    bool request_dismiss(std::uint32_t now_ms);
    void update(std::uint32_t now_ms);

    BallState state() const;
    float phase_progress(std::uint32_t now_ms) const;  // 0..1 within current phase
    bool accepts_shake() const;
    bool accepts_tap_reveal() const;
    bool accepts_tap_dismiss() const;

private:
    BallTiming timing_;
    BallState state_{BallState::idle};
    std::uint32_t phase_started_ms_{0};
    std::uint32_t last_input_ms_{0};
};

inline bool MountLock::add(
    const Acceleration& sample, const std::uint32_t now_ms, const MountLockConfig& config) {
    std::uint32_t dt = 0;
    if (have_last_) {
        dt = now_ms - last_ms_;
    }
    last_ms_ = now_ms;
    have_last_ = true;

    if (!still_gravity(sample, config.still_band_g) || dt == 0 || dt > config.gap_ms) {
        return still_ms >= config.need_still_ms;
    }

    const float w = static_cast<float>(dt);
    sum_x += sample.x_g * w;
    sum_y += sample.y_g * w;
    sum_z += sample.z_g * w;
    still_ms += dt;
    return still_ms >= config.need_still_ms;
}

inline std::uint8_t MountLock::rotation(const MountLockConfig& config) const {
    if (still_ms < config.need_still_ms) {
        return config.fallback;
    }
    const float inv = 1.0F / static_cast<float>(still_ms);
    std::uint8_t rotation = config.fallback;
    if (!rotation_from_axes(
            {sum_x * inv, sum_y * inv, sum_z * inv}, config.min_plane_g, rotation)) {
        return config.fallback;
    }
    return rotation;
}

inline void FaceOrient::arm(const std::uint32_t now_ms, const FaceOrientConfig& config) {
    next_probe_ms_ = now_ms + config.probe_ms;
}

inline bool FaceOrient::wants_sample(const std::uint32_t now_ms, const FaceOrientConfig&) const {
    return now_ms >= next_probe_ms_;
}

inline bool FaceOrient::update(
    const Acceleration& sample,
    const std::uint32_t now_ms,
    const std::uint8_t current,
    std::uint8_t& out,
    const FaceOrientConfig& config) {
    if (now_ms < next_probe_ms_) {
        return false;
    }
    next_probe_ms_ = now_ms + config.probe_ms;
    if (!still_gravity(sample, config.still_band_g)) {
        return false;
    }
    const float up = screen_up_g(sample);
    if (up > config.flip_g) {
        out = 0;
    } else if (up < -config.flip_g) {
        out = 2;
    } else {
        return false;
    }
    return out != current;
}

inline ShakeDetector::ShakeDetector(const ShakeConfig config) : config_{config} {}

inline bool ShakeDetector::update(const Acceleration& sample, const std::uint32_t now_ms) {
    if (latched_) {
        if (now_ms - fired_at_ms_ < config_.cooldown_ms) {
            return false;
        }
        latched_ = false;
        hit_count_ = 0;
        armed_ = true;
        return false;
    }

    const float magnitude = std::sqrt(
        sample.x_g * sample.x_g + sample.y_g * sample.y_g + sample.z_g * sample.z_g);
    // All-zero reads mean the IMU bus missed. Do not treat that as a shake.
    if (magnitude < 0.2F) {
        return false;
    }

    const float shake_strength = std::fabs(magnitude - 1.0F);
    if (shake_strength < config_.enter_threshold_g) {
        armed_ = true;
        return false;
    }
    // One shake is a burst of high samples. Count those samples.
    // ponytail: no per-peak re-arm — that needed three separate jolts and felt dead.
    if (hit_count_ == 0 || now_ms - first_hit_ms_ > config_.hit_window_ms) {
        first_hit_ms_ = now_ms;
        hit_count_ = 1;
    } else {
        ++hit_count_;
    }

    if (hit_count_ < config_.required_hits) {
        return false;
    }

    latched_ = true;
    fired_at_ms_ = now_ms;
    return true;
}

inline void ShakeDetector::reset() {
    first_hit_ms_ = 0;
    fired_at_ms_ = 0;
    hit_count_ = 0;
    armed_ = true;
    latched_ = false;
}

inline ResponsePicker::ResponsePicker(const std::size_t response_count)
    : response_count_{response_count}, previous_index_{0} {}

inline std::size_t ResponsePicker::pick(const std::uint32_t random_value) {
    if (response_count_ < 2) {
        return 0;
    }

    auto candidate = static_cast<std::size_t>(random_value) % response_count_;
    if (has_previous_ && candidate == previous_index_) {
        candidate = (candidate + 1) % response_count_;
    }

    previous_index_ = candidate;
    has_previous_ = true;
    return candidate;
}

inline BallMachine::BallMachine(const BallTiming timing) : timing_{timing} {}

inline bool BallMachine::request_trigger(const std::uint32_t now_ms) {
    // Re-roll from idle or while an answer is already up. Magic 8-ball habit.
    if (state_ == BallState::triggered) {
        return false;
    }
    if (now_ms - last_input_ms_ < timing_.input_lock_ms) {
        return false;
    }
    state_ = BallState::triggered;
    phase_started_ms_ = now_ms;
    last_input_ms_ = now_ms;
    return true;
}

inline bool BallMachine::request_dismiss(const std::uint32_t now_ms) {
    if (state_ != BallState::visible) {
        return false;
    }
    if (now_ms - last_input_ms_ < timing_.input_lock_ms) {
        return false;
    }
    state_ = BallState::resetting;
    phase_started_ms_ = now_ms;
    last_input_ms_ = now_ms;
    return true;
}

inline void BallMachine::update(const std::uint32_t now_ms) {
    const auto elapsed = now_ms - phase_started_ms_;

    switch (state_) {
        case BallState::idle:
            break;
        case BallState::triggered:
            state_ = BallState::revealing;
            phase_started_ms_ = now_ms;
            break;
        case BallState::revealing:
            if (elapsed >= timing_.reveal_ms) {
                state_ = BallState::visible;
                phase_started_ms_ = now_ms;
            }
            break;
        case BallState::visible:
            if (elapsed >= timing_.visible_ms) {
                state_ = BallState::resetting;
                phase_started_ms_ = now_ms;
            }
            break;
        case BallState::resetting:
            if (elapsed >= timing_.reset_ms) {
                state_ = BallState::idle;
                phase_started_ms_ = now_ms;
            }
            break;
    }
}

inline BallState BallMachine::state() const {
    return state_;
}

inline float BallMachine::phase_progress(const std::uint32_t now_ms) const {
    std::uint32_t duration = 1;
    switch (state_) {
        case BallState::revealing:
            duration = timing_.reveal_ms == 0 ? 1 : timing_.reveal_ms;
            break;
        case BallState::visible:
            duration = timing_.visible_ms == 0 ? 1 : timing_.visible_ms;
            break;
        case BallState::resetting:
            duration = timing_.reset_ms == 0 ? 1 : timing_.reset_ms;
            break;
        default:
            return state_ == BallState::visible ? 1.0F : 0.0F;
    }
    const auto elapsed = now_ms - phase_started_ms_;
    if (elapsed >= duration) {
        return 1.0F;
    }
    return static_cast<float>(elapsed) / static_cast<float>(duration);
}

inline bool BallMachine::accepts_shake() const {
    // Not during the short reveal — the shake that just fired is still latched.
    return state_ == BallState::idle || state_ == BallState::visible || state_ == BallState::resetting;
}

inline bool BallMachine::accepts_tap_reveal() const {
    return state_ != BallState::triggered;
}

inline bool BallMachine::accepts_tap_dismiss() const {
    return state_ == BallState::visible;
}

/**
 * @file FourMotorChassis.hpp
 * @brief C++ fluent Builder + ChassisController — zero-overhead wrapper
 *        around the C99 core.
 *
 * Two styles are available:
 *
 *   ① Fluent Builder (recommended for setup):
 *
 *      auto chassis = ChassisBuilder()
 *          .frontLeft (2, 3)
 *          .frontRight(4, 5)
 *          .rearLeft  (6, 7)
 *          .rearRight (8, 9)
 *          .build();
 *
 *      chassis.forward(300).turnRight(200).stop();
 *
 *   ② Direct C++ construction (quick):
 *
 *      ChassisController chassis(2,3, 4,5, 6,7, 8,9);
 *      chassis.backward(500);
 *
 *   ③ Raw C99 API — always available via four_motor_chassis.h
 */

#pragma once
#ifndef FOUR_MOTOR_CHASSIS_HPP
#define FOUR_MOTOR_CHASSIS_HPP

#include "four_motor_chassis.h"
#include <stdexcept>

namespace fmcc {

/* ══════════════════════════════════════════════════════════════════════════
 *  ChassisController
 *  Thin, move-only RAII wrapper around fmcc_chassis_t.
 * ══════════════════════════════════════════════════════════════════════════ */

class ChassisController {
public:

    /**
     * @brief Construct from a pre-filled fmcc_config_t.
     * @throws std::runtime_error on HAL init failure.
     */
    explicit ChassisController(const fmcc_config_t &cfg)
        : _alive(false)
    {
        int r = fmcc_init(&_ch, &cfg);
        if (r < 0) {
#ifndef ARDUINO   /* Arduino has no exceptions; guard is enough */
            throw std::runtime_error("fmcc_init failed, code=" + std::to_string(r));
#endif
            return;
        }
        _alive = true;
    }

    /**
     * @brief Convenience constructor: all four motors, individual pins.
     *
     * Pass FMCC_PIN_NONE for any pin pair you don't have wired.
     */
    ChassisController(fmcc_pin_t fl_fwd, fmcc_pin_t fl_rev,
                      fmcc_pin_t fr_fwd, fmcc_pin_t fr_rev,
                      fmcc_pin_t rl_fwd, fmcc_pin_t rl_rev,
                      fmcc_pin_t rr_fwd, fmcc_pin_t rr_rev)
    {
        fmcc_config_t cfg = FMCC_CONFIG_INIT;
        auto set = [&](fmcc_motor_id_t id, fmcc_pin_t f, fmcc_pin_t r) {
            if (f != FMCC_PIN_NONE && r != FMCC_PIN_NONE)
                fmcc_motor_set(&cfg, id, f, r);
        };
        set(FMCC_MOTOR_FL, fl_fwd, fl_rev);
        set(FMCC_MOTOR_FR, fr_fwd, fr_rev);
        set(FMCC_MOTOR_RL, rl_fwd, rl_rev);
        set(FMCC_MOTOR_RR, rr_fwd, rr_rev);

        int r = fmcc_init(&_ch, &cfg);
        _alive = (r == 0);
#ifndef ARDUINO
        if (!_alive)
            throw std::runtime_error("fmcc_init failed, code=" + std::to_string(r));
#endif
    }

    ~ChassisController() { if (_alive) fmcc_deinit(&_ch); }

    /* non-copyable, movable */
    ChassisController(const ChassisController &) = delete;
    ChassisController &operator=(const ChassisController &) = delete;

    ChassisController(ChassisController &&o) noexcept
        : _ch(o._ch), _alive(o._alive) { o._alive = false; }

    /* ── fluent movement API ────────────────────────────────────────────── */

    ChassisController &forward       (uint32_t ms = 0) { fmcc_forward       (&_ch, ms); return *this; }
    ChassisController &backward      (uint32_t ms = 0) { fmcc_backward      (&_ch, ms); return *this; }
    ChassisController &turnRight     (uint32_t ms = 0) { fmcc_turn_right    (&_ch, ms); return *this; }
    ChassisController &turnLeft      (uint32_t ms = 0) { fmcc_turn_left     (&_ch, ms); return *this; }
    ChassisController &curveForwardRight (uint32_t ms = 0) { fmcc_curve_forward_right (&_ch, ms); return *this; }
    ChassisController &curveForwardLeft  (uint32_t ms = 0) { fmcc_curve_forward_left  (&_ch, ms); return *this; }
    ChassisController &curveBackwardRight(uint32_t ms = 0) { fmcc_curve_backward_right(&_ch, ms); return *this; }
    ChassisController &curveBackwardLeft (uint32_t ms = 0) { fmcc_curve_backward_left (&_ch, ms); return *this; }
    ChassisController &stop          ()                { fmcc_stop          (&_ch);     return *this; }

    /** Drive a single motor directly. */
    ChassisController &drive(fmcc_motor_id_t id, fmcc_dir_t dir)
    {
        fmcc_motor_drive(&_ch, id, dir);
        return *this;
    }

    /** Arbitrary direction pattern. */
    ChassisController &pattern(const fmcc_dir_t dirs[FMCC_MOTOR_MAX],
                                uint32_t         ms = 0)
    {
        fmcc_pattern(&_ch, dirs, ms);
        return *this;
    }

    /** Access the underlying C handle (escape hatch). */
    fmcc_chassis_t       *handle()       { return &_ch; }
    const fmcc_chassis_t *handle() const { return &_ch; }

    bool ok() const { return _alive; }

private:
    fmcc_chassis_t _ch;
    bool           _alive;
};

/* ══════════════════════════════════════════════════════════════════════════
 *  ChassisBuilder
 *  Fluent, step-by-step configuration; call build() to get a
 *  ready-to-use ChassisController.
 *
 *  ChassisBuilder returns *this on every configuration step so you can
 *  chain calls. build() consumes the builder (move semantics).
 *
 *  Example:
 *    auto robot = ChassisBuilder()
 *        .frontLeft(2,3).frontRight(4,5)
 *        .rearLeft(6,7) .rearRight(8,9)
 *        .build();
 * ══════════════════════════════════════════════════════════════════════════ */

class ChassisBuilder {
public:
    ChassisBuilder() : _cfg(FMCC_CONFIG_INIT) {}

    ChassisBuilder &frontLeft (fmcc_pin_t fwd, fmcc_pin_t rev)
    { fmcc_motor_set(&_cfg, FMCC_MOTOR_FL, fwd, rev); return *this; }

    ChassisBuilder &frontRight(fmcc_pin_t fwd, fmcc_pin_t rev)
    { fmcc_motor_set(&_cfg, FMCC_MOTOR_FR, fwd, rev); return *this; }

    ChassisBuilder &rearLeft  (fmcc_pin_t fwd, fmcc_pin_t rev)
    { fmcc_motor_set(&_cfg, FMCC_MOTOR_RL, fwd, rev); return *this; }

    ChassisBuilder &rearRight (fmcc_pin_t fwd, fmcc_pin_t rev)
    { fmcc_motor_set(&_cfg, FMCC_MOTOR_RR, fwd, rev); return *this; }

    /** Generic slot setter — useful when the slot is determined at runtime. */
    ChassisBuilder &motor(fmcc_motor_id_t id, fmcc_pin_t fwd, fmcc_pin_t rev)
    { fmcc_motor_set(&_cfg, id, fwd, rev); return *this; }

    /**
     * @brief Finalise configuration and return a live ChassisController.
     * @throws std::runtime_error (non-Arduino) on HAL failure.
     */
    ChassisController build()
    {
        return ChassisController(_cfg);
    }

private:
    fmcc_config_t _cfg;
};

} /* namespace fmcc */

#endif /* FOUR_MOTOR_CHASSIS_HPP */

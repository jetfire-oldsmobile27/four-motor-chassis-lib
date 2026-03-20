/**
 * @file four_motor_chassis.h
 * @brief four-motor-chassis-control  — public C99 API
 *
 * L9110S 4-pin oriented, but can work with fewer than 4 servos.
 * Modern, simple to use and stable.
 *
 *  Wiring one L9110S channel:
 *    A-IA → forward pin   (HIGH = forward, LOW = stop)
 *    A-IB → reverse pin   (HIGH = reverse, LOW = stop)
 *    (same for B channel)
 *
 *  Motor indexing: 0 = front-left  1 = front-right
 *                  2 = rear-left   3 = rear-right
 *
 * ─── C99 quick-start ────────────────────────────────────────────────────
 *
 *  fmcc_config_t cfg = FMCC_CONFIG_INIT;
 *  fmcc_motor_set(&cfg, FMCC_MOTOR_FL, 2, 3);   // pin_fwd=2, pin_rev=3
 *  fmcc_motor_set(&cfg, FMCC_MOTOR_FR, 4, 5);
 *  fmcc_motor_set(&cfg, FMCC_MOTOR_RL, 6, 7);
 *  fmcc_motor_set(&cfg, FMCC_MOTOR_RR, 8, 9);
 *
 *  fmcc_chassis_t chassis;
 *  fmcc_init(&chassis, &cfg);
 *
 *  fmcc_forward(&chassis, 300);  // 300 ms
 *  fmcc_turn_right(&chassis, 200);
 *  fmcc_stop(&chassis);
 *  fmcc_deinit(&chassis);
 * ────────────────────────────────────────────────────────────────────────
 */

#ifndef FOUR_MOTOR_CHASSIS_H
#define FOUR_MOTOR_CHASSIS_H

#include "fmcc_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ────────────────────────────────────────────────────────────── */
#define FMCC_VERSION_MAJOR 1
#define FMCC_VERSION_MINOR 0
#define FMCC_VERSION_PATCH 0

/* ── Motor slot identifiers ─────────────────────────────────────────────── */
typedef enum {
    FMCC_MOTOR_FL = 0,   /**< Front-Left  */
    FMCC_MOTOR_FR = 1,   /**< Front-Right */
    FMCC_MOTOR_RL = 2,   /**< Rear-Left   */
    FMCC_MOTOR_RR = 3,   /**< Rear-Right  */
    FMCC_MOTOR_MAX = 4
} fmcc_motor_id_t;

/* ── Per-motor direction ────────────────────────────────────────────────── */
typedef enum {
    FMCC_DIR_STOP    = 0,
    FMCC_DIR_FORWARD = 1,
    FMCC_DIR_REVERSE = 2
} fmcc_dir_t;

/* ── Per-motor pin descriptor ───────────────────────────────────────────── */
typedef struct {
    fmcc_pin_t pin_fwd;   /**< A-IA / B-IA: HIGH → forward  */
    fmcc_pin_t pin_rev;   /**< A-IB / B-IB: HIGH → reverse  */
    uint8_t    present;   /**< 1 if this slot is wired       */
} fmcc_motor_desc_t;

/* ── Configuration (fill before calling fmcc_init) ─────────────────────── */
typedef struct {
    fmcc_motor_desc_t motors[FMCC_MOTOR_MAX];
} fmcc_config_t;

/** Zero-initialiser for fmcc_config_t — all motors absent, pins NONE. */
#define FMCC_CONFIG_INIT \
    { { {FMCC_PIN_NONE, FMCC_PIN_NONE, 0}, \
        {FMCC_PIN_NONE, FMCC_PIN_NONE, 0}, \
        {FMCC_PIN_NONE, FMCC_PIN_NONE, 0}, \
        {FMCC_PIN_NONE, FMCC_PIN_NONE, 0} } }

/* ── Runtime chassis handle ─────────────────────────────────────────────── */
typedef struct {
    fmcc_config_t cfg;
    fmcc_dir_t    current_dir[FMCC_MOTOR_MAX];
} fmcc_chassis_t;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Configuration helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Register one motor slot in a config struct.
 * @param cfg      Pointer to an fmcc_config_t previously zero-inited.
 * @param id       Motor slot (FMCC_MOTOR_FL … FMCC_MOTOR_RR).
 * @param pin_fwd  GPIO pin connected to L9110S A-IA / B-IA.
 * @param pin_rev  GPIO pin connected to L9110S A-IB / B-IB.
 */
void fmcc_motor_set(fmcc_config_t  *cfg,
                    fmcc_motor_id_t id,
                    fmcc_pin_t      pin_fwd,
                    fmcc_pin_t      pin_rev);

/* ═══════════════════════════════════════════════════════════════════════════
 *  Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief  Initialise chassis: set up HAL and configure all registered pins.
 * @return 0 on success, negative on error.
 */
int  fmcc_init(fmcc_chassis_t *chassis, const fmcc_config_t *cfg);

/**
 * @brief  Stop all motors and release HAL resources.
 */
void fmcc_deinit(fmcc_chassis_t *chassis);

/* ═══════════════════════════════════════════════════════════════════════════
 *  Low-level single-motor control
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Drive one motor in a direction (STOP / FORWARD / REVERSE).
 */
void fmcc_motor_drive(fmcc_chassis_t *chassis,
                      fmcc_motor_id_t id,
                      fmcc_dir_t      dir);

/* ═══════════════════════════════════════════════════════════════════════════
 *  High-level movement patterns
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Stop all motors immediately. */
void fmcc_stop(fmcc_chassis_t *chassis);

/** Move forward for @p duration_ms milliseconds, then stop. 0 = continuous. */
void fmcc_forward(fmcc_chassis_t *chassis, uint32_t duration_ms);

/** Move backward for @p duration_ms milliseconds, then stop. 0 = continuous. */
void fmcc_backward(fmcc_chassis_t *chassis, uint32_t duration_ms);

/** Pivot right (left motors forward, right motors reverse). */
void fmcc_turn_right(fmcc_chassis_t *chassis, uint32_t duration_ms);

/** Pivot left (right motors forward, left motors reverse). */
void fmcc_turn_left(fmcc_chassis_t *chassis, uint32_t duration_ms);

/** Curve forward-right: left side full, right side stopped. */
void fmcc_curve_forward_right(fmcc_chassis_t *chassis, uint32_t duration_ms);

/** Curve forward-left: right side full, left side stopped. */
void fmcc_curve_forward_left(fmcc_chassis_t *chassis, uint32_t duration_ms);

/** Curve backward-right: left side reverse, right side stopped. */
void fmcc_curve_backward_right(fmcc_chassis_t *chassis, uint32_t duration_ms);

/** Curve backward-left: right side reverse, left side stopped. */
void fmcc_curve_backward_left(fmcc_chassis_t *chassis, uint32_t duration_ms);

/**
 * @brief Execute an arbitrary per-motor direction pattern.
 *
 * @param dirs  Array of exactly FMCC_MOTOR_MAX directions, indexed by
 *              fmcc_motor_id_t.  Absent motors are silently ignored.
 */
void fmcc_pattern(fmcc_chassis_t  *chassis,
                  const fmcc_dir_t dirs[FMCC_MOTOR_MAX],
                  uint32_t         duration_ms);

#ifdef __cplusplus
}
#endif
#endif /* FOUR_MOTOR_CHASSIS_H */

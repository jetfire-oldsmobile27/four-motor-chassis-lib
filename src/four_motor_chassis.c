/**
 * @file four_motor_chassis.c
 * @brief four-motor-chassis-control — C99 core implementation
 */

#include "four_motor_chassis.h"
#include <string.h>

/* ── internal helpers ───────────────────────────────────────────────────── */

static void _drive_raw(const fmcc_motor_desc_t *m, fmcc_dir_t dir)
{
    if (!m->present) return;
    switch (dir) {
        case FMCC_DIR_FORWARD:
            fmcc_hal_pin_write(m->pin_fwd, 1);
            fmcc_hal_pin_write(m->pin_rev, 0);
            break;
        case FMCC_DIR_REVERSE:
            fmcc_hal_pin_write(m->pin_fwd, 0);
            fmcc_hal_pin_write(m->pin_rev, 1);
            break;
        default: /* STOP */
            fmcc_hal_pin_write(m->pin_fwd, 0);
            fmcc_hal_pin_write(m->pin_rev, 0);
            break;
    }
}

static void _apply_dirs(fmcc_chassis_t *ch, const fmcc_dir_t dirs[FMCC_MOTOR_MAX])
{
    for (int i = 0; i < FMCC_MOTOR_MAX; i++) {
        _drive_raw(&ch->cfg.motors[i], dirs[i]);
        ch->current_dir[i] = dirs[i];
    }
}

static void _timed(fmcc_chassis_t *ch,
                   const fmcc_dir_t dirs[FMCC_MOTOR_MAX],
                   uint32_t         duration_ms)
{
    _apply_dirs(ch, dirs);
    if (duration_ms > 0) {
        fmcc_hal_delay_ms(duration_ms);
        fmcc_stop(ch);
    }
}

/* ── config ─────────────────────────────────────────────────────────────── */

void fmcc_motor_set(fmcc_config_t  *cfg,
                    fmcc_motor_id_t id,
                    fmcc_pin_t      pin_fwd,
                    fmcc_pin_t      pin_rev)
{
    if (!cfg || id >= FMCC_MOTOR_MAX) return;
    cfg->motors[id].pin_fwd = pin_fwd;
    cfg->motors[id].pin_rev = pin_rev;
    cfg->motors[id].present = 1;
}

/* ── lifecycle ───────────────────────────────────────────────────────────── */

int fmcc_init(fmcc_chassis_t *chassis, const fmcc_config_t *cfg)
{
    if (!chassis || !cfg) return -1;
    memcpy(&chassis->cfg, cfg, sizeof(fmcc_config_t));
    for (int i = 0; i < FMCC_MOTOR_MAX; i++)
        chassis->current_dir[i] = FMCC_DIR_STOP;

    int r = fmcc_hal_init();
    if (r < 0) return r;

    for (int i = 0; i < FMCC_MOTOR_MAX; i++) {
        fmcc_motor_desc_t *m = &chassis->cfg.motors[i];
        if (!m->present) continue;
        if ((r = fmcc_hal_pin_setup(m->pin_fwd)) < 0) return r;
        if ((r = fmcc_hal_pin_setup(m->pin_rev)) < 0) return r;
    }
    return 0;
}

void fmcc_deinit(fmcc_chassis_t *chassis)
{
    if (!chassis) return;
    fmcc_stop(chassis);
    fmcc_hal_deinit();
}

/* ── single-motor ────────────────────────────────────────────────────────── */

void fmcc_motor_drive(fmcc_chassis_t *chassis,
                      fmcc_motor_id_t id,
                      fmcc_dir_t      dir)
{
    if (!chassis || id >= FMCC_MOTOR_MAX) return;
    _drive_raw(&chassis->cfg.motors[id], dir);
    chassis->current_dir[id] = dir;
}

/* ── movement patterns ───────────────────────────────────────────────────── */

void fmcc_stop(fmcc_chassis_t *chassis)
{
    if (!chassis) return;
    fmcc_dir_t dirs[FMCC_MOTOR_MAX] = {
        FMCC_DIR_STOP, FMCC_DIR_STOP, FMCC_DIR_STOP, FMCC_DIR_STOP
    };
    _apply_dirs(chassis, dirs);
}

void fmcc_forward(fmcc_chassis_t *chassis, uint32_t duration_ms)
{
    fmcc_dir_t dirs[FMCC_MOTOR_MAX] = {
        FMCC_DIR_FORWARD, FMCC_DIR_FORWARD,
        FMCC_DIR_FORWARD, FMCC_DIR_FORWARD
    };
    _timed(chassis, dirs, duration_ms);
}

void fmcc_backward(fmcc_chassis_t *chassis, uint32_t duration_ms)
{
    fmcc_dir_t dirs[FMCC_MOTOR_MAX] = {
        FMCC_DIR_REVERSE, FMCC_DIR_REVERSE,
        FMCC_DIR_REVERSE, FMCC_DIR_REVERSE
    };
    _timed(chassis, dirs, duration_ms);
}

void fmcc_turn_right(fmcc_chassis_t *chassis, uint32_t duration_ms)
{
    /* left side forward, right side reverse → pivot right in place */
    fmcc_dir_t dirs[FMCC_MOTOR_MAX] = {
        FMCC_DIR_FORWARD,  /* FL */
        FMCC_DIR_REVERSE,  /* FR */
        FMCC_DIR_FORWARD,  /* RL */
        FMCC_DIR_REVERSE   /* RR */
    };
    _timed(chassis, dirs, duration_ms);
}

void fmcc_turn_left(fmcc_chassis_t *chassis, uint32_t duration_ms)
{
    fmcc_dir_t dirs[FMCC_MOTOR_MAX] = {
        FMCC_DIR_REVERSE,  /* FL */
        FMCC_DIR_FORWARD,  /* FR */
        FMCC_DIR_REVERSE,  /* RL */
        FMCC_DIR_FORWARD   /* RR */
    };
    _timed(chassis, dirs, duration_ms);
}

void fmcc_curve_forward_right(fmcc_chassis_t *chassis, uint32_t duration_ms)
{
    fmcc_dir_t dirs[FMCC_MOTOR_MAX] = {
        FMCC_DIR_FORWARD, FMCC_DIR_STOP,
        FMCC_DIR_FORWARD, FMCC_DIR_STOP
    };
    _timed(chassis, dirs, duration_ms);
}

void fmcc_curve_forward_left(fmcc_chassis_t *chassis, uint32_t duration_ms)
{
    fmcc_dir_t dirs[FMCC_MOTOR_MAX] = {
        FMCC_DIR_STOP, FMCC_DIR_FORWARD,
        FMCC_DIR_STOP, FMCC_DIR_FORWARD
    };
    _timed(chassis, dirs, duration_ms);
}

void fmcc_curve_backward_right(fmcc_chassis_t *chassis, uint32_t duration_ms)
{
    fmcc_dir_t dirs[FMCC_MOTOR_MAX] = {
        FMCC_DIR_REVERSE, FMCC_DIR_STOP,
        FMCC_DIR_REVERSE, FMCC_DIR_STOP
    };
    _timed(chassis, dirs, duration_ms);
}

void fmcc_curve_backward_left(fmcc_chassis_t *chassis, uint32_t duration_ms)
{
    fmcc_dir_t dirs[FMCC_MOTOR_MAX] = {
        FMCC_DIR_STOP, FMCC_DIR_REVERSE,
        FMCC_DIR_STOP, FMCC_DIR_REVERSE
    };
    _timed(chassis, dirs, duration_ms);
}

void fmcc_pattern(fmcc_chassis_t  *chassis,
                  const fmcc_dir_t dirs[FMCC_MOTOR_MAX],
                  uint32_t         duration_ms)
{
    if (!chassis || !dirs) return;
    _timed(chassis, dirs, duration_ms);
}

/**
 * @file fmcc_hal_gpiod.c
 * @brief HAL backend — libgpiod v1.x / v2.x  (Orange Pi, Raspberry Pi, …)
 *
 * No root required — add a udev rule so your user owns /dev/gpiochipN:
 *
 *   /etc/udev/rules.d/60-gpiod.rules:
 *     SUBSYSTEM=="gpio", KERNEL=="gpiochip*", GROUP="gpio", MODE="0660"
 *   sudo groupadd -f gpio && sudo usermod -aG gpio $USER
 *   (re-login or: newgrp gpio)
 *
 * libgpiod version is auto-detected via GPIOD_API_VERSION defined in gpiod.h:
 *   v1.x → GPIOD_API_VERSION < 0x020000  (or macro absent)
 *   v2.x → GPIOD_API_VERSION >= 0x020000
 *
 * You can also force a version:
 *   -DFMCC_GPIOD_V2   force v2 API
 *   -DFMCC_GPIOD_V1   force v1 API
 *
 * Chip path (default /dev/gpiochip0):
 *   cmake: -DFMCC_GPIOD_CHIP=gpiochip1
 *   gcc:   -DFMCC_GPIOD_CHIP='"gpiochip1"'
 *
 * Check chips and lines:
 *   gpiodetect
 *   gpioinfo /dev/gpiochip0
 */

#if defined(FMCC_USE_GPIOD)

#include "fmcc_hal.h"
#include <gpiod.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

/* ── chip path ──────────────────────────────────────────────────────────── */
/* CMake passes the chip name (e.g. gpiochip0); we prepend /dev/ at runtime */
#ifndef FMCC_GPIOD_CHIP
#  define FMCC_GPIOD_CHIP "gpiochip0"
#endif

#define MAX_PIN_NUMBER 512

/* ── version detection ──────────────────────────────────────────────────── */
/*
 * gpiod.h v2 defines GPIOD_API_VERSION as 0x020000 or higher.
 * gpiod.h v1 does not define it at all.
 */
#if !defined(FMCC_GPIOD_V1) && !defined(FMCC_GPIOD_V2)
#  if defined(GPIOD_API_VERSION) && (GPIOD_API_VERSION >= 0x020000)
#    define FMCC_GPIOD_V2
#  else
#    define FMCC_GPIOD_V1
#  endif
#endif

/* ════════════════════════════════════════════════════════════════════════
 *  libgpiod v1.x
 * ════════════════════════════════════════════════════════════════════════ */
#if defined(FMCC_GPIOD_V1)

static struct gpiod_chip *_chip = NULL;
static struct gpiod_line *_lines[MAX_PIN_NUMBER];

int fmcc_hal_init(void)
{
    memset(_lines, 0, sizeof(_lines));
    _chip = gpiod_chip_open_by_name(FMCC_GPIOD_CHIP);
    if (!_chip) {
        fprintf(stderr, "[HAL/gpiod1] cannot open %s: %s\n",
                FMCC_GPIOD_CHIP, strerror(errno));
        return -errno;
    }
    fprintf(stderr, "[HAL/gpiod1] chip %s opened\n", FMCC_GPIOD_CHIP);
    return 0;
}

void fmcc_hal_deinit(void)
{
    if (!_chip) return;
    for (int i = 0; i < MAX_PIN_NUMBER; i++) {
        if (_lines[i]) { gpiod_line_release(_lines[i]); _lines[i] = NULL; }
    }
    gpiod_chip_close(_chip);
    _chip = NULL;
}

int fmcc_hal_pin_setup(fmcc_pin_t pin)
{
    if (pin < 0 || pin >= MAX_PIN_NUMBER) {
        fprintf(stderr, "[HAL/gpiod1] pin %d out of range\n", pin);
        return -EINVAL;
    }
    if (!_chip) { fprintf(stderr, "[HAL/gpiod1] chip not open\n"); return -ENODEV; }

    struct gpiod_line *line = gpiod_chip_get_line(_chip, (unsigned)pin);
    if (!line) {
        fprintf(stderr, "[HAL/gpiod1] get_line(%d): %s\n", pin, strerror(errno));
        return -errno;
    }
    if (gpiod_line_request_output(line, "fmcc", 0) < 0) {
        fprintf(stderr, "[HAL/gpiod1] request_output(%d): %s\n", pin, strerror(errno));
        return -errno;
    }
    _lines[pin] = line;
    fprintf(stderr, "[HAL/gpiod1] pin %d ready\n", pin);
    return 0;
}

void fmcc_hal_pin_write(fmcc_pin_t pin, int value)
{
    if (pin < 0 || pin >= MAX_PIN_NUMBER || !_lines[pin]) return;
    gpiod_line_set_value(_lines[pin], value ? 1 : 0);
}

/* ════════════════════════════════════════════════════════════════════════
 *  libgpiod v2.x
 *
 *  Key API changes vs v1:
 *    gpiod_chip_open_by_name(name)  → gpiod_chip_open(path)  e.g. /dev/gpiochip0
 *    gpiod_chip_get_line()          → removed; use line_config + chip_request_lines
 *    gpiod_line_request_output()    → gpiod_line_settings + gpiod_line_config
 *    gpiod_line_set_value()         → gpiod_line_request_set_value()
 *    gpiod_line_release()           → gpiod_line_request_release()
 * ════════════════════════════════════════════════════════════════════════ */
#elif defined(FMCC_GPIOD_V2)

static struct gpiod_chip *_chip = NULL;

typedef struct {
    struct gpiod_line_request *req;
    unsigned int               offset;
} pin_req_t;

static pin_req_t _reqs[MAX_PIN_NUMBER];

/* Build full device path from chip name, e.g. "gpiochip1" → "/dev/gpiochip1" */
static void _chip_path(char *buf, size_t sz)
{
    /* If user already gave a full path, use it as-is */
    if (FMCC_GPIOD_CHIP[0] == '/')
        snprintf(buf, sz, "%s", FMCC_GPIOD_CHIP);
    else
        snprintf(buf, sz, "/dev/%s", FMCC_GPIOD_CHIP);
}

int fmcc_hal_init(void)
{
    memset(_reqs, 0, sizeof(_reqs));

    char path[64];
    _chip_path(path, sizeof(path));

    _chip = gpiod_chip_open(path);
    if (!_chip) {
        fprintf(stderr, "[HAL/gpiod2] cannot open %s: %s\n", path, strerror(errno));
        return -errno;
    }
    fprintf(stderr, "[HAL/gpiod2] chip %s opened\n", path);
    return 0;
}

void fmcc_hal_deinit(void)
{
    if (!_chip) return;
    for (int i = 0; i < MAX_PIN_NUMBER; i++) {
        if (_reqs[i].req) {
            gpiod_line_request_release(_reqs[i].req);
            _reqs[i].req = NULL;
        }
    }
    gpiod_chip_close(_chip);
    _chip = NULL;
}

int fmcc_hal_pin_setup(fmcc_pin_t pin)
{
    if (pin < 0 || pin >= MAX_PIN_NUMBER) {
        fprintf(stderr, "[HAL/gpiod2] pin %d out of range\n", pin);
        return -EINVAL;
    }
    if (!_chip) { fprintf(stderr, "[HAL/gpiod2] chip not open\n"); return -ENODEV; }

    /* 1. Line settings: output, initial LOW */
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    if (!settings) return -ENOMEM;
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    /* 2. Line config */
    struct gpiod_line_config *lcfg = gpiod_line_config_new();
    if (!lcfg) { gpiod_line_settings_free(settings); return -ENOMEM; }

    unsigned int offset = (unsigned int)pin;
    int r = gpiod_line_config_add_line_settings(lcfg, &offset, 1, settings);
    gpiod_line_settings_free(settings);
    if (r < 0) { gpiod_line_config_free(lcfg); return -errno; }

    /* 3. Request config (consumer name) */
    struct gpiod_request_config *rcfg = gpiod_request_config_new();
    if (rcfg) gpiod_request_config_set_consumer(rcfg, "fmcc");

    /* 4. Request lines */
    struct gpiod_line_request *req = gpiod_chip_request_lines(_chip, rcfg, lcfg);
    if (rcfg) gpiod_request_config_free(rcfg);
    gpiod_line_config_free(lcfg);

    if (!req) {
        fprintf(stderr, "[HAL/gpiod2] request line %d failed: %s\n",
                pin, strerror(errno));
        return -errno;
    }

    _reqs[pin].req    = req;
    _reqs[pin].offset = offset;
    fprintf(stderr, "[HAL/gpiod2] pin %d ready\n", pin);
    return 0;
}

void fmcc_hal_pin_write(fmcc_pin_t pin, int value)
{
    if (pin < 0 || pin >= MAX_PIN_NUMBER || !_reqs[pin].req) return;
    enum gpiod_line_value v =
        value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    gpiod_line_request_set_value(_reqs[pin].req, _reqs[pin].offset, v);
}

#endif /* FMCC_GPIOD_V1 / FMCC_GPIOD_V2 */

/* ── delay — shared ─────────────────────────────────────────────────────── */
void fmcc_hal_delay_ms(unsigned int ms)
{
    struct timespec ts = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

#endif /* FMCC_USE_GPIOD */
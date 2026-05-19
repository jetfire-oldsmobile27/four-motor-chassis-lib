/**
 * @file fmcc_hal.h
 * @brief Hardware Abstraction Layer for four-motor-chassis-control
 *
 * Supported backends (pass -D flag to compiler or cmake):
 *
 *   FMCC_USE_GPIOD    — libgpiod  ✓ recommended, no root required
 *   FMCC_USE_PIGPIO   — pigpio      (Raspberry Pi, needs root)
 *   FMCC_USE_WIRINGOP — wiringOP    (Orange Pi)
 *   (none)            — sysfs       fallback, needs root/udev, deprecated
 */
#ifndef FMCC_HAL_H
#define FMCC_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Platform detection ─────────────────────────────────────────────────── */
#if defined(ARDUINO)
#   define FMCC_PLATFORM_ARDUINO
#elif defined(__linux__)
#   define FMCC_PLATFORM_LINUX
#   if defined(FMCC_USE_PIGPIO)
#       define FMCC_LINUX_PIGPIO
#   elif defined(FMCC_USE_WIRINGOP)
#       define FMCC_LINUX_WIRINGOP
#   elif defined(FMCC_USE_GPIOD)
#       define FMCC_LINUX_GPIOD       /* libgpiod — implemented in fmcc_hal_gpiod.c */
#   else
#       define FMCC_LINUX_SYSFS       /* portable sysfs fallback */
#   endif
#else
#   error "Unsupported platform. Define ARDUINO or compile on Linux."
#endif

/* ── Pin type ───────────────────────────────────────────────────────────── */
typedef int fmcc_pin_t;
#define FMCC_PIN_NONE (-1)

/* ── HAL API ────────────────────────────────────────────────────────────── */
int  fmcc_hal_init(void);
void fmcc_hal_deinit(void);
int  fmcc_hal_pin_setup(fmcc_pin_t pin);
void fmcc_hal_pin_write(fmcc_pin_t pin, int value);
void fmcc_hal_delay_ms(unsigned int ms);

#ifdef __cplusplus
}
#endif
#endif /* FMCC_HAL_H */
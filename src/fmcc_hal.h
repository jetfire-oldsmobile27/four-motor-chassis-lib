/**
 * @file fmcc_hal.h
 * @brief Hardware Abstraction Layer for four-motor-chassis-control
 *
 * Platform-specific GPIO bindings.
 * Supported: Arduino, Raspberry Pi (pigpio/sysfs), Orange Pi (wiringOP/sysfs).
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
#   else
#       define FMCC_LINUX_SYSFS   /* default: portable sysfs */
#   endif
#else
#   error "Unsupported platform. Define ARDUINO or compile on Linux."
#endif

/* ── Pin type ───────────────────────────────────────────────────────────── */

typedef int fmcc_pin_t;
#define FMCC_PIN_NONE (-1)

/* ── HAL function prototypes (implemented per-platform in fmcc_hal.c) ───── */

/**
 * @brief  One-time platform initialisation (call before any pin ops).
 * @return 0 on success, negative errno on failure.
 */
int  fmcc_hal_init(void);

/**
 * @brief  Release platform resources.
 */
void fmcc_hal_deinit(void);

/**
 * @brief  Configure pin as digital output and drive it LOW.
 */
int  fmcc_hal_pin_setup(fmcc_pin_t pin);

/**
 * @brief  Write digital HIGH (1) or LOW (0) to pin.
 */
void fmcc_hal_pin_write(fmcc_pin_t pin, int value);

/**
 * @brief  Millisecond delay (busy-wait on bare metal, nanosleep on Linux).
 */
void fmcc_hal_delay_ms(unsigned int ms);

#ifdef __cplusplus
}
#endif
#endif /* FMCC_HAL_H */

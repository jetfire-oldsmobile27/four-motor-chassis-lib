/**
 * @file fmcc_hal.c
 * @brief HAL implementations: Arduino / pigpio / wiringOP / sysfs
 *
 * NOTE: libgpiod backend is implemented in fmcc_hal_gpiod.c — not here.
 *       This file intentionally has no code for FMCC_LINUX_GPIOD.
 */

#include "fmcc_hal.h"

/* ══════════════════════════════════════════════════════════════════════════
 *  ARDUINO
 * ══════════════════════════════════════════════════════════════════════════ */
#if defined(FMCC_PLATFORM_ARDUINO)

#include <Arduino.h>

int  fmcc_hal_init(void)   { return 0; }
void fmcc_hal_deinit(void) {}

int fmcc_hal_pin_setup(fmcc_pin_t pin)
{
    if (pin == FMCC_PIN_NONE) return -1;
    pinMode((uint8_t)pin, OUTPUT);
    digitalWrite((uint8_t)pin, LOW);
    return 0;
}

void fmcc_hal_pin_write(fmcc_pin_t pin, int value)
{
    if (pin != FMCC_PIN_NONE)
        digitalWrite((uint8_t)pin, value ? HIGH : LOW);
}

void fmcc_hal_delay_ms(unsigned int ms) { delay(ms); }

/* ══════════════════════════════════════════════════════════════════════════
 *  LINUX — pigpio
 * ══════════════════════════════════════════════════════════════════════════ */
#elif defined(FMCC_LINUX_PIGPIO)

#include <pigpio.h>
#include <time.h>

int fmcc_hal_init(void)    { return gpioInitialise() < 0 ? -1 : 0; }
void fmcc_hal_deinit(void) { gpioTerminate(); }

int fmcc_hal_pin_setup(fmcc_pin_t pin)
{
    if (pin == FMCC_PIN_NONE) return -1;
    if (gpioSetMode((unsigned)pin, PI_OUTPUT) != 0) return -2;
    gpioWrite((unsigned)pin, 0);
    return 0;
}

void fmcc_hal_pin_write(fmcc_pin_t pin, int value)
{
    if (pin != FMCC_PIN_NONE)
        gpioWrite((unsigned)pin, value ? 1 : 0);
}

void fmcc_hal_delay_ms(unsigned int ms)
{
    struct timespec ts = { (time_t)(ms/1000), (long)(ms%1000)*1000000L };
    nanosleep(&ts, NULL);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  LINUX — wiringOP
 * ══════════════════════════════════════════════════════════════════════════ */
#elif defined(FMCC_LINUX_WIRINGOP)

#include <wiringPi.h>
#include <time.h>

int fmcc_hal_init(void)    { return wiringPiSetupGpio() < 0 ? -1 : 0; }
void fmcc_hal_deinit(void) {}

int fmcc_hal_pin_setup(fmcc_pin_t pin)
{
    if (pin == FMCC_PIN_NONE) return -1;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    return 0;
}

void fmcc_hal_pin_write(fmcc_pin_t pin, int value)
{
    if (pin != FMCC_PIN_NONE)
        digitalWrite(pin, value ? HIGH : LOW);
}

void fmcc_hal_delay_ms(unsigned int ms)
{
    struct timespec ts = { (time_t)(ms/1000), (long)(ms%1000)*1000000L };
    nanosleep(&ts, NULL);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  LINUX — libgpiod
 *
 *  Implementation lives entirely in fmcc_hal_gpiod.c.
 *  This branch is intentionally empty — it just satisfies the
 *  preprocessor chain so we don't fall through to #error.
 * ══════════════════════════════════════════════════════════════════════════ */
#elif defined(FMCC_LINUX_GPIOD)

/* nothing — see fmcc_hal_gpiod.c */

/* ══════════════════════════════════════════════════════════════════════════
 *  LINUX — sysfs  (portable, no extra libs, needs root or udev rules)
 * ══════════════════════════════════════════════════════════════════════════ */
#elif defined(FMCC_LINUX_SYSFS)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define SYSFS_GPIO_BASE "/sys/class/gpio"
#define MAX_PIN_NUMBER  512

typedef struct {
    int value_fd;
    int we_exported;
} pin_state_t;

static pin_state_t _pins[MAX_PIN_NUMBER];
static int _hal_ready = 0;

static void _sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static int _write_file(const char *path, const char *data)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "[HAL] open(%s): %s\n", path, strerror(errno));
        return -errno;
    }
    ssize_t n = write(fd, data, strlen(data));
    int saved = errno;
    close(fd);
    if (n < 0) {
        fprintf(stderr, "[HAL] write(%s, \"%s\"): %s\n", path, data, strerror(saved));
        return -saved;
    }
    return 0;
}

static int _export_pin(fmcc_pin_t pin)
{
    char dir_path[128];
    snprintf(dir_path, sizeof(dir_path), SYSFS_GPIO_BASE "/gpio%d", pin);
    if (access(dir_path, F_OK) == 0) { _pins[pin].we_exported = 0; return 0; }

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", pin);
    int r = _write_file(SYSFS_GPIO_BASE "/export", buf);
    if (r < 0) return r;

    for (int i = 0; i < 20; i++) {
        _sleep_ms(10);
        if (access(dir_path, F_OK) == 0) { _pins[pin].we_exported = 1; return 0; }
    }
    fprintf(stderr, "[HAL] gpio%d: sysfs entry never appeared\n", pin);
    return -ETIMEDOUT;
}

int fmcc_hal_init(void)
{
    for (int i = 0; i < MAX_PIN_NUMBER; i++) {
        _pins[i].value_fd = -1;
        _pins[i].we_exported = 0;
    }
    _hal_ready = 1;
    return 0;
}

void fmcc_hal_deinit(void)
{
    if (!_hal_ready) return;
    for (int i = 0; i < MAX_PIN_NUMBER; i++) {
        if (_pins[i].value_fd >= 0) { close(_pins[i].value_fd); _pins[i].value_fd = -1; }
    }
    int ufd = open(SYSFS_GPIO_BASE "/unexport", O_WRONLY);
    if (ufd >= 0) {
        for (int i = 0; i < MAX_PIN_NUMBER; i++) {
            if (_pins[i].we_exported) {
                char buf[16];
                int n = snprintf(buf, sizeof(buf), "%d", i);
                write(ufd, buf, (size_t)n);
                _pins[i].we_exported = 0;
            }
        }
        close(ufd);
    }
    _hal_ready = 0;
}

int fmcc_hal_pin_setup(fmcc_pin_t pin)
{
    if (pin < 0 || pin >= MAX_PIN_NUMBER) return -EINVAL;
    int r = _export_pin(pin);
    if (r < 0) return r;

    char path[128];
    snprintf(path, sizeof(path), SYSFS_GPIO_BASE "/gpio%d/direction", pin);
    r = _write_file(path, "out");
    if (r < 0) return r;

    snprintf(path, sizeof(path), SYSFS_GPIO_BASE "/gpio%d/value", pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -errno;
    write(fd, "0", 1);
    _pins[pin].value_fd = fd;
    return 0;
}

void fmcc_hal_pin_write(fmcc_pin_t pin, int value)
{
    if (pin < 0 || pin >= MAX_PIN_NUMBER) return;
    int fd = _pins[pin].value_fd;
    if (fd < 0) return;
    lseek(fd, 0, SEEK_SET);
    write(fd, value ? "1" : "0", 1);
}

void fmcc_hal_delay_ms(unsigned int ms)
{
    struct timespec ts = { (time_t)(ms/1000), (long)(ms%1000)*1000000L };
    nanosleep(&ts, NULL);
}

#else
#  error "No platform selected. Define FMCC_PLATFORM_ARDUINO or compile on Linux."
#endif /* platform */
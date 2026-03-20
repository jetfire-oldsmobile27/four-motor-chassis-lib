/**
 * @file fmcc_hal.c
 * @brief HAL implementations: Arduino / pigpio / wiringOP / sysfs
 *
 * Compile only one branch by defining the appropriate macro before including
 * this translation unit (or via CMake / build flags).
 */

#include "fmcc_hal.h"

/* ══════════════════════════════════════════════════════════════════════════
 *  ARDUINO
 * ══════════════════════════════════════════════════════════════════════════ */
#if defined(FMCC_PLATFORM_ARDUINO)

#include <Arduino.h>

int fmcc_hal_init(void)  { return 0; }
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

void fmcc_hal_delay_ms(unsigned int ms)
{
    delay(ms);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  LINUX — pigpio  (Raspberry Pi preferred)
 * ══════════════════════════════════════════════════════════════════════════ */
#elif defined(FMCC_LINUX_PIGPIO)

#include <pigpio.h>
#include <time.h>

int fmcc_hal_init(void)
{
    return gpioInitialise() < 0 ? -1 : 0;
}

void fmcc_hal_deinit(void)
{
    gpioTerminate();
}

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
    struct timespec ts = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  LINUX — wiringOP  (Orange Pi)
 * ══════════════════════════════════════════════════════════════════════════ */
#elif defined(FMCC_LINUX_WIRINGOP)

#include <wiringPi.h>
#include <time.h>

int fmcc_hal_init(void)
{
    return wiringPiSetupGpio() < 0 ? -1 : 0;   /* BCM numbering */
}

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
    struct timespec ts = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  LINUX — sysfs  (portable fallback, no root needed with udev rules)
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
#define MAX_PINS        64

static int _exported[MAX_PINS];

static int sysfs_export(fmcc_pin_t pin)
{
    char path[128];
    snprintf(path, sizeof(path), SYSFS_GPIO_BASE "/gpio%d", pin);
    if (access(path, F_OK) == 0) return 0; /* already exported */

    int fd = open(SYSFS_GPIO_BASE "/export", O_WRONLY);
    if (fd < 0) return -errno;
    char buf[8];
    int n = snprintf(buf, sizeof(buf), "%d", pin);
    if (write(fd, buf, (size_t)n) < 0) { close(fd); return -errno; }
    close(fd);
    return 0;
}

static int sysfs_set_direction(fmcc_pin_t pin, const char *dir)
{
    char path[128];
    snprintf(path, sizeof(path), SYSFS_GPIO_BASE "/gpio%d/direction", pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -errno;
    if (write(fd, dir, strlen(dir)) < 0) { close(fd); return -errno; }
    close(fd);
    return 0;
}

int fmcc_hal_init(void)
{
    memset(_exported, 0, sizeof(_exported));
    return 0;
}

void fmcc_hal_deinit(void)
{
    int fd = open(SYSFS_GPIO_BASE "/unexport", O_WRONLY);
    if (fd < 0) return;
    for (int i = 0; i < MAX_PINS; i++) {
        if (_exported[i]) {
            char buf[8];
            int n = snprintf(buf, sizeof(buf), "%d", i);
            write(fd, buf, (size_t)n);
        }
    }
    close(fd);
}

int fmcc_hal_pin_setup(fmcc_pin_t pin)
{
    if (pin < 0 || pin >= MAX_PINS) return -1;
    int r = sysfs_export(pin);
    if (r < 0) return r;
    /* small delay for the kernel to create the direction file */
    struct timespec ts = {0, 50000000L};
    nanosleep(&ts, NULL);
    r = sysfs_set_direction(pin, "out");
    if (r < 0) return r;
    _exported[pin] = 1;
    fmcc_hal_pin_write(pin, 0);
    return 0;
}

void fmcc_hal_pin_write(fmcc_pin_t pin, int value)
{
    if (pin < 0 || pin >= MAX_PINS) return;
    char path[128];
    snprintf(path, sizeof(path), SYSFS_GPIO_BASE "/gpio%d/value", pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return;
    write(fd, value ? "1" : "0", 1);
    close(fd);
}

void fmcc_hal_delay_ms(unsigned int ms)
{
    struct timespec ts = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

#endif /* platform */

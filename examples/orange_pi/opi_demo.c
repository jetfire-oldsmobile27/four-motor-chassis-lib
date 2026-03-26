/**
 * @file opi_demo.c
 * @brief Orange Pi example — wiringOP or sysfs backend, pure C99 API
 *
 * Build with wiringOP:
 *   gcc -std=c99 -DFMCC_USE_WIRINGOP \
 *       opi_demo.c ../src/four_motor_chassis.c ../src/fmcc_hal.c \
 *       -lwiringPi -o opi_demo
 *
 * Build with portable sysfs (no extra lib, needs udev/root):
 *   gcc -std=c99 \
 *       opi_demo.c ../src/four_motor_chassis.c ../src/fmcc_hal.c \
 *       -o opi_demo
 *
 * Pin numbering: BCM-equivalent GPIO numbers for your OPi variant.
 * Adjust the defines below to match your board's header.
 */

#include "../src/four_motor_chassis.h"
#include <stdio.h>

/* Orange Pi Zero 2W example — adjust to your board */

/* FL (motor 1) — прямой */
#define FL_FWD  256
#define FL_REV  257

/* FR (motor 2) — ЗЕРКАЛЬНЫЙ: меняем fwd↔rev */
#define FR_FWD  259   
#define FR_REV  258   

/* RL (motor 3) — прямой */
#define RL_FWD  260
#define RL_REV  76

/* RR (motor 4) — прямой */
#define RR_FWD  270
#define RR_REV  271

int main(void)
{
    fmcc_config_t cfg = FMCC_CONFIG_INIT;
    fmcc_motor_set(&cfg, FMCC_MOTOR_FL, FL_FWD, FL_REV);
    fmcc_motor_set(&cfg, FMCC_MOTOR_FR, FR_FWD, FR_REV);
    fmcc_motor_set(&cfg, FMCC_MOTOR_RL, RL_FWD, RL_REV);
    fmcc_motor_set(&cfg, FMCC_MOTOR_RR, RR_FWD, RR_REV);

    fmcc_chassis_t chassis;
    if (fmcc_init(&chassis, &cfg) != 0) {
        fprintf(stderr, "fmcc_init failed\n");
        return 1;
    }

    printf("Forward 1s\n");  fmcc_forward (&chassis, 1000);
    printf("Left  0.5s\n");  fmcc_turn_left(&chassis, 500);
    printf("Back  1s\n");    fmcc_backward(&chassis, 1000);
    printf("Stop\n");        fmcc_stop    (&chassis);

    /* Two-wheel demo: only front motors present, rear slots absent */
    printf("\n-- 2-wheel mode --\n");
    fmcc_config_t cfg2 = FMCC_CONFIG_INIT;
    fmcc_motor_set(&cfg2, FMCC_MOTOR_FL, FL_FWD, FL_REV);
    fmcc_motor_set(&cfg2, FMCC_MOTOR_FR, FR_FWD, FR_REV);
    /* RL / RR deliberately omitted — library handles absent slots silently */

    fmcc_chassis_t chassis2;
    fmcc_init(&chassis2, &cfg2);
    fmcc_forward(&chassis2, 800);
    fmcc_stop   (&chassis2);
    fmcc_deinit (&chassis2);

    fmcc_deinit(&chassis);
    return 0;
}

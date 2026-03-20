/**
 * @file RobotCar_C99.ino
 * @brief Arduino example — pure C99 API (no C++ wrapper)
 *
 * Useful when you want maximum predictability or are mixing with
 * other C-only libraries.
 */

#include <four_motor_chassis.h>

static fmcc_chassis_t chassis;

void setup()
{
    Serial.begin(115200);

    fmcc_config_t cfg = FMCC_CONFIG_INIT;
    fmcc_motor_set(&cfg, FMCC_MOTOR_FL, 2, 3);
    fmcc_motor_set(&cfg, FMCC_MOTOR_FR, 4, 5);
    fmcc_motor_set(&cfg, FMCC_MOTOR_RL, 6, 7);
    fmcc_motor_set(&cfg, FMCC_MOTOR_RR, 8, 9);

    if (fmcc_init(&chassis, &cfg) != 0) {
        Serial.println("ERROR: fmcc_init failed");
        while (1) {}
    }
    Serial.println("Chassis ready (C99 mode).");
}

void loop()
{
    fmcc_forward (&chassis, 500);
    fmcc_turn_right(&chassis, 300);
    fmcc_backward(&chassis, 400);
    fmcc_stop    (&chassis);

    delay(2000);
}

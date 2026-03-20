/**
 * @file RobotCar.ino
 * @brief Arduino example — fluent C++ Builder API
 *
 * Wiring (L9110S dual-channel board × 2):
 *   Board A (front)   Board B (rear)
 *   A-IA → D2         A-IA → D6
 *   A-IB → D3         A-IB → D7
 *   B-IA → D4         B-IA → D8
 *   B-IB → D5         B-IB → D9
 */

#include <FourMotorChassis.hpp>

using namespace fmcc;

ChassisController *robot = nullptr;

void setup()
{
    Serial.begin(115200);

    /* ── Builder pattern: configure then build ── */
    robot = new ChassisController(
        ChassisBuilder()
            .frontLeft (2, 3)   /* A-IA, A-IB on board A */
            .frontRight(4, 5)   /* B-IA, B-IB on board A */
            .rearLeft  (6, 7)   /* A-IA, A-IB on board B */
            .rearRight (8, 9)   /* B-IA, B-IB on board B */
            .build()
    );

    Serial.println("Chassis ready.");
}

void loop()
{
    /* ── Fluent chained movement sequence ── */
    robot->forward(500)          // 500 ms ahead
         .turnRight(300)         // 300 ms pivot
         .forward(500)
         .turnLeft(300)
         .backward(400)
         .stop();

    /* ── Custom pattern: crab drift ── */
    fmcc_dir_t crab[FMCC_MOTOR_MAX] = {
        FMCC_DIR_FORWARD,   /* FL */
        FMCC_DIR_REVERSE,   /* FR */
        FMCC_DIR_REVERSE,   /* RL */
        FMCC_DIR_FORWARD    /* RR */
    };
    robot->pattern(crab, 400);

    delay(2000);  // pause between demo cycles
}

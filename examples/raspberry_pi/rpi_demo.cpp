/**
 * @file rpi_demo.cpp
 * @brief Raspberry Pi example — pigpio backend, C++ Builder API
 *
 * Build:
 *   g++ -std=c++17 -DFMCC_USE_PIGPIO \
 *       rpi_demo.cpp ../src/four_motor_chassis.c ../src/fmcc_hal.c \
 *       -lpigpio -lrt -o rpi_demo
 *
 * Run as root (pigpio requirement):
 *   sudo ./rpi_demo
 *
 * GPIO BCM numbering (physical header pin in parentheses):
 *   FL_FWD=17(11)  FL_REV=27(13)
 *   FR_FWD=22(15)  FR_REV=23(16)
 *   RL_FWD=24(18)  RL_REV=25(22)
 *   RR_FWD=5 (29)  RR_REV=6 (31)
 */

#include "../src/FourMotorChassis.hpp"
#include <iostream>

int main()
{
    try {
        auto robot = fmcc::ChassisBuilder()
            .frontLeft (17, 27)
            .frontRight(22, 23)
            .rearLeft  (24, 25)
            .rearRight ( 5,  6)
            .build();

        std::cout << "Forward 1s\n";
        robot.forward(1000);

        std::cout << "Pivot right 0.5s\n";
        robot.turnRight(500);

        std::cout << "Backward 1s\n";
        robot.backward(1000);

        std::cout << "Stop\n";
        robot.stop();

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

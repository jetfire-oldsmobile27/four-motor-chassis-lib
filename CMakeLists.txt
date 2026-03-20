cmake_minimum_required(VERSION 3.16)
project(four_motor_chassis VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD   99)
set(CMAKE_CXX_STANDARD 17)

# ── Platform backend selection ────────────────────────────────────────────
# Pass -DFMCC_BACKEND=pigpio | wiringop | sysfs   (default: sysfs)
set(FMCC_BACKEND "sysfs" CACHE STRING "GPIO backend: pigpio | wiringop | sysfs")

# ── Core library ─────────────────────────────────────────────────────────
add_library(fmcc STATIC
    src/fmcc_hal.c
    src/four_motor_chassis.c
)

target_include_directories(fmcc PUBLIC src)

if(FMCC_BACKEND STREQUAL "pigpio")
    target_compile_definitions(fmcc PUBLIC FMCC_USE_PIGPIO)
    target_link_libraries(fmcc PUBLIC pigpio rt)
elseif(FMCC_BACKEND STREQUAL "wiringop")
    target_compile_definitions(fmcc PUBLIC FMCC_USE_WIRINGOP)
    target_link_libraries(fmcc PUBLIC wiringPi)
else()
    message(STATUS "fmcc: using sysfs GPIO backend")
    # no extra link needed
endif()

# ── Examples ─────────────────────────────────────────────────────────────
add_executable(rpi_demo  examples/raspberry_pi/rpi_demo.cpp)
target_link_libraries(rpi_demo PRIVATE fmcc)

add_executable(opi_demo  examples/orange_pi/opi_demo.c)
target_link_libraries(opi_demo PRIVATE fmcc)

# ── Install rules ─────────────────────────────────────────────────────────
install(TARGETS fmcc ARCHIVE DESTINATION lib)
install(FILES
    src/four_motor_chassis.h
    src/FourMotorChassis.hpp
    src/fmcc_hal.h
    DESTINATION include/fmcc
)

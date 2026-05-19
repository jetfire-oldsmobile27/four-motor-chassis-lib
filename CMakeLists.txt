cmake_minimum_required(VERSION 3.16)
project(four_motor_chassis VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD   99)
set(CMAKE_CXX_STANDARD 17)

# ── Backend selection ─────────────────────────────────────────────────────
# -DFMCC_BACKEND=gpiod | pigpio | wiringop | sysfs   (default: gpiod)
set(FMCC_BACKEND "gpiod" CACHE STRING
    "GPIO backend: gpiod | pigpio | wiringop | sysfs")

set(FMCC_GPIOD_CHIP "gpiochip0" CACHE STRING
    "libgpiod chip device name, e.g. gpiochip0 or gpiochip1")

# ── Core library ──────────────────────────────────────────────────────────
add_library(fmcc STATIC
    src/fmcc_hal.c
    src/fmcc_hal_gpiod.c
    src/four_motor_chassis.c
)
target_include_directories(fmcc PUBLIC src)

# ── Backend flags & libs ──────────────────────────────────────────────────
if(FMCC_BACKEND STREQUAL "pigpio")
    message(STATUS "fmcc: backend = pigpio")
    target_compile_definitions(fmcc PUBLIC FMCC_USE_PIGPIO)
    target_link_libraries(fmcc PUBLIC pigpio rt)

elseif(FMCC_BACKEND STREQUAL "wiringop")
    message(STATUS "fmcc: backend = wiringOP")
    target_compile_definitions(fmcc PUBLIC FMCC_USE_WIRINGOP)
    target_link_libraries(fmcc PUBLIC wiringPi)

elseif(FMCC_BACKEND STREQUAL "gpiod")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GPIOD REQUIRED libgpiod)

    message(STATUS "fmcc: libgpiod version = ${GPIOD_VERSION}")

    string(REGEX MATCH "^([0-9]+)" _GPIOD_MAJOR "${GPIOD_VERSION}")

    if(_GPIOD_MAJOR GREATER_EQUAL 2)
        message(STATUS "fmcc: backend = libgpiod v2")
        target_compile_definitions(fmcc PUBLIC FMCC_GPIOD_V2)
    else()
        message(STATUS "fmcc: backend = libgpiod v1")
        target_compile_definitions(fmcc PUBLIC FMCC_GPIOD_V1)
    endif()

    target_compile_definitions(fmcc PUBLIC
        FMCC_USE_GPIOD
        FMCC_GPIOD_CHIP="${FMCC_GPIOD_CHIP}"
    )
    target_include_directories(fmcc PUBLIC ${GPIOD_INCLUDE_DIRS})
    target_link_libraries(fmcc PUBLIC ${GPIOD_LIBRARIES})

else()
    message(STATUS "fmcc: backend = sysfs (deprecated, may need root)")
endif()

# ── Examples ──────────────────────────────────────────────────────────────

add_executable(opi_demo examples/orange_pi/opi_demo.c)
target_link_libraries(opi_demo PRIVATE fmcc)

find_library(PIGPIO_LIB NAMES pigpio)
if(PIGPIO_LIB)
    add_executable(rpi_demo examples/raspberry_pi/rpi_demo.cpp)
    target_compile_definitions(rpi_demo PRIVATE FMCC_USE_PIGPIO)
    target_link_libraries(rpi_demo PRIVATE fmcc ${PIGPIO_LIB} rt)
    message(STATUS "rpi_demo: built (pigpio = ${PIGPIO_LIB})")
else()
    message(STATUS "rpi_demo: SKIPPED (pigpio not found)")
endif()

# ── Install ───────────────────────────────────────────────────────────────
install(TARGETS fmcc ARCHIVE DESTINATION lib)
install(FILES
    src/four_motor_chassis.h
    src/FourMotorChassis.hpp
    src/fmcc_hal.h
    DESTINATION include/fmcc
)
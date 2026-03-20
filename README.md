# four-motor-chassis-control

> L9110S 4-pin oriented, but can work with fewer than 4 servos.  
> Modern, simple to use and stable.

Cross-platform C/C++ library for controlling up to **4 DC motors** via the
**L9110S** dual-channel H-bridge.  
Works on **Arduino**, **Raspberry Pi** (pigpio), and **Orange Pi** (wiringOP / sysfs).

---

## Wiring

Each L9110S channel requires **two GPIO pins**:

```
A-IA → forward pin   HIGH = spin forward, LOW = coast
A-IB → reverse pin   HIGH = spin reverse, LOW = coast
```

Motor slot assignment:

```
┌──────────┬──────────┐
│  FL (0)  │  FR (1)  │   front of robot
├──────────┼──────────┤
│  RL (2)  │  RR (3)  │
└──────────┴──────────┘
```

You don't have to wire all four slots — absent motors are silently skipped.

---

## Quick start

### C++ Builder (recommended)

```cpp
#include <FourMotorChassis.hpp>
using namespace fmcc;

auto robot = ChassisBuilder()
    .frontLeft (2, 3)
    .frontRight(4, 5)
    .rearLeft  (6, 7)
    .rearRight (8, 9)
    .build();

// Fluent movement chain
robot.forward(500)      // 500 ms
     .turnRight(300)
     .backward(400)
     .stop();
```

### Pure C99

```c
#include <four_motor_chassis.h>

fmcc_config_t cfg = FMCC_CONFIG_INIT;
fmcc_motor_set(&cfg, FMCC_MOTOR_FL, 2, 3);
fmcc_motor_set(&cfg, FMCC_MOTOR_FR, 4, 5);

fmcc_chassis_t chassis;
fmcc_init(&chassis, &cfg);

fmcc_forward (&chassis, 500);
fmcc_turn_right(&chassis, 300);
fmcc_stop    (&chassis);
fmcc_deinit  (&chassis);
```

---

## Movement patterns

| Method / function              | Behaviour                              |
|-------------------------------|----------------------------------------|
| `forward(ms)`                 | All motors forward                     |
| `backward(ms)`                | All motors reverse                     |
| `turnRight(ms)`               | Left fwd, right rev — pivot in place   |
| `turnLeft(ms)`                | Right fwd, left rev — pivot in place   |
| `curveForwardRight(ms)`       | Left side only — arc right             |
| `curveForwardLeft(ms)`        | Right side only — arc left             |
| `curveBackwardRight(ms)`      | Left side reverse — arc back-right     |
| `curveBackwardLeft(ms)`       | Right side reverse — arc back-left     |
| `stop()`                      | All motors off                         |
| `pattern(dirs[], ms)`         | Custom per-motor direction array       |
| `drive(motor_id, dir)`        | Single motor control                   |

Pass `ms = 0` (or omit) to run continuously until `stop()` is called.

---

## Platform build

### Arduino

Copy the `src/` folder and `library.properties` into your Arduino libraries
directory as `FourMotorChassis/`, or use the Arduino Library Manager.

### Raspberry Pi (pigpio)

```bash
cmake -B build -DFMCC_BACKEND=pigpio
cmake --build build
sudo ./build/rpi_demo
```

### Orange Pi (wiringOP)

```bash
cmake -B build -DFMCC_BACKEND=wiringop
cmake --build build
./build/opi_demo
```

### Any Linux — portable sysfs (no extra library)

```bash
cmake -B build          # sysfs is default
cmake --build build
sudo ./build/opi_demo   # sudo needed for /sys/class/gpio export
```

---

## Architecture

```
┌────────────────────────────────────┐
│  FourMotorChassis.hpp              │  C++17 ChassisBuilder + ChassisController
│  (fluent Builder, RAII wrapper)    │
└────────────────┬───────────────────┘
                 │ wraps
┌────────────────▼───────────────────┐
│  four_motor_chassis.h / .c         │  C99 public API
│  (movement patterns, config)       │
└────────────────┬───────────────────┘
                 │ calls
┌────────────────▼───────────────────┐
│  fmcc_hal.h / fmcc_hal.c           │  HAL (pin_setup, pin_write, delay_ms)
│  Arduino | pigpio | wiringOP | sysfs│
└────────────────────────────────────┘
```

---

# Selected Implementation Aspects

## Finite State Machine (STM) and Mapping to GPIOs

Full matrix of all possible switching states

|                    | Sailing Int. Ind. | Sailing Coast Ind. | Driving Ind. | Anchoring Ind. | Disabled Ind. | Side & Stern Light | Masthead Light | Allround Green Light | Allround Red 1 Light | Allround Red 2 Light | Allround White Light |
|:-------------------|:-----------------:|:------------------:|:------------:|:--------------:|:-------------:|:------------------:|:--------------:|:--------------------:|:--------------------:|:--------------------:|:--------------------:|
| All Off Btn.       |                   |                    |              |                |               |                    |                |                      |                      |                      |                      |
| Sailing Int. Btn.  |         ✔         |                    |              |                |               |         ✔          |                |                      |                      |                      |                      |
| Sailing Coast Btn. |                   |         ✔          |              |                |               |         ✔          |                |          ✔           |          ✔           |                      |                      |
| Driving Btn.       |                   |                    |      ✔       |                |               |         ✔          |       ✔        |                      |                      |                      |                      |
| Anchoring Btn.     |                   |                    |              |       ✔        |               |                    |                |                      |                      |                      |          ✔           |
| Disabled  Btn.     |                   |                    |              |                |       ✔       |                    |                |                      |          ✔           |          ✔           |                      |

Reduced matrix which groups identical columns (row-wise identical).
This mostly affects indicator lights and navigation lights which are jointly switched.

|                    | Sailing Int. Ind. | Sailing Coast Ind. & Allround Green Light | Driving Ind. & Masthead Light | Anchoring Ind. & Allround White Light | Disabled Ind. & Allround Red 2 Light | Side & Stern Light | Allround Red 1 Light |
|:-------------------|:-----------------:|:-----------------------------------------:|:-----------------------------:|:-------------------------------------:|:------------------------------------:|:------------------:|:--------------------:|
| All Off Btn.       |                   |                                           |                               |                                       |                                      |                    |                      |
| Sailing Int. Btn.  |         ✔         |                                           |                               |                                       |                                      |         ✔          |                      |
| Sailing Coast Btn. |                   |                     ✔                     |                               |                                       |                                      |         ✔          |          ✔           |
| Driving Btn.       |                   |                                           |               ✔               |                                       |                                      |         ✔          |                      |
| Anchoring Btn.     |                   |                                           |                               |                   ✔                   |                                      |                    |                      |
| Disabled Btn.      |                   |                                           |                               |                                       |                  ✔                   |                    |          ✔           |

### Full-Fledged Realization

We configure the GPIOs of the ESP32-H2 as follows

| Pin # | Pin Name | Dual Purpose             | Power Domain  | Low Power | Usage | Eval Board            |
|------:|:---------|:-------------------------|:--------------|:---------:|:-----:|:----------------------|
|     3 | GPIO 0   |                          | VDDPST1       |           |   O   |                       |
|     4 | GPIO 1   |                          | VDDPST1       |           |   O   |                       |
|     5 | GPIO 2   | JTAG                     | VDDPST1       |           |   O   |                       |
|     6 | GPIO 3   | JTAG                     | VDDPST1       |           |   O   |                       |
|     7 | GPIO 4   | JTAG Clock               | VDDPST1       |           |   O   |                       |
|     8 | GPIO 5   | JTAG                     | VDDPST1       |           |   O   |                       |
|    10 | GPIO 8   | Strapping (Boot)         | VDDPST1       |     ✔     |       | RGB LED               |
|    11 | GPIO 9   | Strapping (Reset)        | VDDPST1       |     ✔     |       | SW 1                  |
|    12 | GPIO 10  |                          | VDDPST1       |     ✔     |   I   |                       |
|    13 | GPIO 11  |                          | VDDPST1       |     ✔     |   I   |                       |
|    14 | GPIO 12  |                          | VDDA_PMU/VBAT |     ✔     |   I   |                       |
|    15 | GPIO 13  | XTAL_32K_P               | VDDA_PMU/VBAT |     ✔     |       | Crystal               |
|    16 | GPIO 14  | XTAL_32K_N               | VDDA_PMU/VBAT |     ✔     |       | Crystal               |
|    21 | GPIO 22  |                          | VDDPST2       |           |   O   |                       |
|    22 | GPIO 23  | UART RX                  | VDDPST2       |           |       | USB-to-UART (CP2102N) |
|    23 | GPIO 24  | UART TX                  | VDDPST2       |           |       | USB-to-UART (CP2102N) |
|    24 | GPIO 25  | Strapping (JTAG on Boot) | VDDPST2       |           |       |                       |
|    25 | GPIO 26  | USB D-                   | VDDPST2       |           |       | USB                   |
|    27 | GPIO 27  | USB D+                   | VDDPST2       |           |       | USB                   |

Summary: We use

- GPIO 0-5+22 as outputs (we sacrifice JTAG) and
- GPIO 10-12 as inputs

Normally, GPIO 4 (JTAG clock) is pulled up during boot and hence a connected light will light up during boot.
See [Espressif: ESP32-H2 — Datasheet, table 2.1, "Pin Overview", footnote 4](https://documentation.espressif.com/esp32-h2_datasheet_en.pdf):

> Depends on the value of EFUSE_DIS_PAD_JTAG
> - 0 — WPU is enabled (WPU = weak-pull up, internal 45kΩ resistor)
> - 1 — pin floating

We burn the eFuse `EFUSE_DIS_PAD_JTAG`, which make GPIO 4 floating during boot.
However, this irrevocably disables JTAG forever.

We keep

- all strapping pins,
- the external LP crystal,
- UART, and
- USB

intact.

We map the six input buttons to three GPIs (GPIO 10-12) and decouple them with diodes.
We don't mind that this implies that we cannot distinguish multiple button events from single events.
Pressing a single button triggers a rising flank at one or several GPIs.

Mapping of 6 push buttons to 3 GPIs

| Button             | GPI 12 | GPI 11 | GPI 10 |
|:-------------------|:------:|:------:|:------:|
| All Off Btn.       |        |        |   ✔    |
| Sailing Int. Btn.  |        |   ✔    |        |
| Sailing Coast Btn. |        |   ✔    |   ✔    |
| Driving Btn.       |   ✔    |        |        |
| Anchoring Btn.     |   ✔    |        |   ✔    |
| Disabled Btn.      |   ✔    |   ✔    |        |

The mapping of the seven GPOs to the lights is straight forward

| Light                                     | GPO    |
|:------------------------------------------|:-------|
| Sailing Int. Ind.                         | GPO 0  |
| Sailing Coast Ind. & Allround Green Light | GPO 1  |
| Driving Ind. & Masthead Light             | GPO 2  |
| Anchoring Ind. & Allround White Light     | GPO 3  |
| Disabled Ind. & Allround Red 2 Light      | GPO 4  |
| Side & Stern Light                        | GPO 5  |
| Allround Red 1 Light                      | GPO 22 |

### Simplified Realization

We configure the GPIOs of the ESP32-H2 as follows

| Pin # | Pin Name | Dual Purpose             | Low Power | Usage | Eval Board            |
|------:|:---------|:-------------------------|:---------:|:-----:|:----------------------|
|     3 | GPIO 0   |                          |           |   I   |                       |
|     4 | GPIO 1   |                          |           |   O   |                       |
|     5 | GPIO 2   | JTAG                     |           |   O   |                       |
|     6 | GPIO 3   | JTAG                     |           |   O   |                       |
|     7 | GPIO 4   | JTAG Clock               |           |       |                       |
|     8 | GPIO 5   | JTAG                     |           |   O   |                       |
|    10 | GPIO 8   | Strapping (Boot)         |     ✔     |       | RGB LED               |
|    11 | GPIO 9   | Strapping (Reset)        |     ✔     |       | SW 1                  |
|    12 | GPIO 10  |                          |     ✔     |   I   |                       |
|    13 | GPIO 11  |                          |     ✔     |   I   |                       |
|    14 | GPIO 12  |                          |     ✔     |   I   |                       |
|    15 | GPIO 13  | XTAL_32K_P               |     ✔     |       | Crystal               |
|    16 | GPIO 14  | XTAL_32K_N               |     ✔     |       | Crystal               |
|    21 | GPIO 22  |                          |           |       |                       |
|    22 | GPIO 23  | UART RX                  |           |       | USB-to-UART (CP2102N) |
|    23 | GPIO 24  | UART TX                  |           |       | USB-to-UART (CP2102N) |
|    24 | GPIO 25  | Strapping (JTAG on Boot) |           |       |                       |
|    25 | GPIO 26  | USB D-                   |           |       | USB                   |
|    27 | GPIO 27  | USB D+                   |           |       | USB                   |

Summary: We use

- GPIO 1-3+5 as outputs (we sacrifice JTAG) and
- GPIO 0+10-12 as inputs

We do not use GPIO 4 (JTAG clock) as the clock signal is pulled up during boot and hence a connected light would
light up during boot.
See [Espressif: ESP32-H2 — Datasheet, table 2.1, "Pin Overview", footnote 4](https://documentation.espressif.com/esp32-h2_datasheet_en.pdf):

> Depends on the value of EFUSE_DIS_PAD_JTAG
> - 0 — WPU is enabled (WPU = weak-pull up, internal 45kΩ resistor)
> - 1 — pin floating

If burned the eFuse `EFUSE_DIS_PAD_JTAG`, then GPIO 4 would be floating during boot, but JTAG irrevocably disabled.


We keep

- all strapping pins,
- the external LP crystal,
- UART, and
- USB

intact.

Mapping of 4 push buttons to 4 GPIs

| Button         | GPI    |
|:---------------|:-------|
| All Off Btn.   | GPI 0  |
| Sailing Btn.   | GPI 10 |
| Driving Btn.   | GPI 11 |
| Anchoring Btn. | GPI 12 |

The mapping of the four GPOs to the lights is straight forward

| Light                                  | GPO   |
|:---------------------------------------|:------|
| Sailing Ind.                           | GPO 1 |
| Driving Ind. & Masthead Light          | GPO 2 |
| Anchoring Ind. & Allround White Light  | GPO 3 |
| Side & Stern Light                     | GPO 5 |


## Power Consumption

Navlico tries to consume as little power as possible.
The implemented techniques are based on
- [Espressif: ESP-IDF Programming Guide - API Reference - System API - Power Management](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/power_management.html)
- [Espressif: ESP-IDF Programming Guide - API Guide - Low Power Modes](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/low-power-mode/index.html)

### Configuration

Run `idf.py menuconfig`:
```
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_48=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_96 is not set

CONFIG_PM_ENABLE=y
CONFIG_PM_SLEEP_FUNC_IN_IRAM=y
CONFIG_PM_DFS_INIT_AUTO=y
CONFIG_PM_SLP_IRAM_OPT=y
CONFIG_PM_LIGHTSLEEP_RTC_OSC_CAL_INTERVAL=1
CONFIG_PM_SLP_IRAM_OPT=y
CONFIG_PM_RTOS_IDLE_OPT=y

CONFIG_PM_SLP_DISABLE_GPIO=y

CONFIG_RTC_CLK_SRC_INT_RC=y

CONFIG_FREERTOS_USE_TICKLESS_IDLE=y
```

To check if the following options may help
 - `CONFIG_PM_LIGHTSLEEP_RTC_OSC_CAL_INTERVAL`: Increase to reduce calibration frequency until system becomes unstable
 - `FREERTOS_IN_IRAM`: As `CONFIG_PM_SLP_IRAM_OPT` helps, this should, too.
 - `ESP_SLEEP_POWER_DOWN_FLASH`: see "ESP-IDF Programming Guide/API Reference/System API/Sleep Modes/Power-down of Flash"
 - `RTC_CLK_SRC` and `RTC_CLK_SRC_EXT_CRYS`:
    - The board has an external 32k crystal
    - See https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig-reference.html#config-rtc-clk-src
    - "Internal 150kHz oscillator" option provides lowest deep sleep current consumption,
      and does not require extra external components.
      However, frequency stability with respect to temperature is poor,
      so time may drift in deep/light sleep modes.


### Programming Strategies

- To enable [Light-sleep] mode, users need to set the `light_sleep_enable` field to true in `esp_pm_config_t` when configuring DFS.
  For more details, please refer to DFS Configuration.
- Users can switch to Deep-sleep mode by calling `esp_deep_sleep_start()` interface.
- Call `esp_light_sleep_start()` and `esp_deep_sleep_start()` when possible
- Register ISR on special GPIOs which also work in Deep Sleep
- When the peripheral power domain is powered down during sleep, both the IO_MUX and GPIO modules are inactive, meaning the chip pins' state is not maintained by these modules.
  To preserve the state of an IO during sleep, it's essential to call `gpio_hold_dis()` and `gpio_hold_en()` before and after configuring the GPIO state.
  This action ensures that the IO configuration is latched and prevents the IO from becoming floating while in sleep mode.
  https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/system/power_management.html#light-sleep-peripheral-power-down
- Pin Hold Feature
  [Espressif: ESP32-H2 - Technical Reference Manual, Sec. 6.9](https://documentation.espressif.com/esp32-h2_technical_reference_manual_en.pdf)
  Each GPIO pin (including the LP pins: GPIO8 ~ GPIO14) has an individual hold function controlled by an LP register.
  When the pin is set to hold, the state is latched at that moment and will not change no matter how the internal
  signals change or how the IO MUX/GPIO configuration is modified.
  Users can use the hold function for the pins to retain the pin state through a core reset triggered by watchdog
  time-out or Deep-sleep events.
  To use this feature, follow the steps below:
  - Digital pins (GPIO0 ~ GPIO5, GPIO22 ~ GPIO27):
    - To maintain pin input/output status in Deep-sleep mode, users can set `LP_AON_GPIO_HOLD0_REG[n]` to 1 before
      powering down.
      To disable the hold function after the chip is woken up, users can set `LP_AON_GPIO_HOLD0_REG[n]` to 0.
    - Or users can set `PMU_TIE_HIGH_HP_PAD_HOLD_ALL` to maintain the input/output status of all digital pins,
      and set `PMU_TIE_LOW_HP_PAD_HOLD_ALL` to disable the hold function of all digital pins.
  - LP pins (GPIO8 ~ GPIO14):
    - The input and output values of LP GPIO pins are controlled by `LP_AON_GPIO_HOLD0_REG[n]`,
      `PMU_TIE_HIGH_LP_PAD_HOLD_ALL`, and `PMU_TIE_LOW_LP_PAD_HOLD_ALL`.
      Users can set `LP_AON_GPIO_HOLD0_REG[n]` to 1 to hold the value of GPIOn, or set `LP_AON_GPIO_HOLD0_REG[n]` to 0
      to disable the hold function of GPIOn.
    - Or users can set `PMU_TIE_HIGH_LP_PAD_HOLD_ALL` to hold the values of all LP pins, and set
      `PMU_TIE_LOW_LP_PAD_HOLD_ALL` to disable the hold function of all LP pins.
    - When the LP pin is held in the input state, it can serve as a wake-up source to wake up the chip from Deep-sleep
      mode.

## Power Domains

[Espressif: ESP32-H2 — Technical Reference Manual, Sec. 6.11.1](https://documentation.espressif.com/esp32-h2_technical_reference_manual_en.pdf):
> 6.11.2 Power Supply Management
> Each ESP32-H2 pin is connected to one of the three different power domains.
> - VDDPST1: the input power supply for some digital GPIOs and some LP GPIOs
> - VDDPST2: the input power supply for some digital GPIOs
> - VDDA_PMU/VBAT: the input power supply for GPIO12, XTAL_32K_P and XTAL_32K_N

# Hardware

[Espressif: ESP32-H2 — Datasheet, sec. 2.2, "Pin Overview"](https://documentation.espressif.com/esp32-h2_datasheet_en.pdf)
> Default drive strength for GPIO26 and GPIO27 is 40 mA, and 20 mA for the other GPIOs.

| Parameter | Description                      |            Min | Typ |            Max | Unit |
|:----------|:---------------------------------|---------------:|----:|---------------:|:-----|
| C(in)     | Input capacitance                |              — |   2 |              — | pF   |
| U(ih)     | High-level input voltage         | 0.75×Vdd = 2.5 |   — |  Vdd+0.3 = 3.6 | V    |
| U(il)     | Low-level input voltage          |           -0.3 |   — | 0.25×Vdd = 0.8 | V    |
| I(ih)     | High-level input current         |              — |   — |             50 | nA   |
| I(il)     | Low-level input current          |              — |   — |             50 | nA   |
| U(oh)     | High-level output voltage        |  0.8×Vdd = 2.6 |   — |              — | V    |
| U(ol)     | Low-level output voltage         |              — |   — |  0.1×Vdd = 0.4 | V    |
| I(oh)     | High-level output source current |                |  40 |                | mA   |
| I(ol)     | Low-level input sink current     |                |  28 |                | mA   |
| R(pu)     | Internal weak pull-up resistor   |              — |  45 |              — | kΩ   |
| R(pd)     | Internal weak pull-down resistor |              — |  45 |              — | kΩ   |

**Calculation for Input Pins**

```
 Vcc -- R1 -- S ---+----+-- In
                   |    |
                  R2    C
                   |    |
                  GND  GND
```

_(Backward) Calculation:_

- R1 + R2 = 5V/500µA = 10kΩ
- R2 / (R1 + R2) = 3V/5V = 0.6
- --> R1 = 4kΩ, R2 = 6kΩ
- --> R1 = 4.7kΩ, R2 = mid(10kΩ, 22kΩ) = 6.9kΩ, R2' = mid(10kΩ, 22kΩ, 45kΩ) = 5.9kΩ

_(Forward) Calculation:_

- Rges = 4.7kΩ + 6.9kΩ = 11.6kΩ, Rges' = 4.7kΩ + 5.9kΩ = 10.6kΩ
- Iges = 5V / 11.6kΩ = 431µA, Iges' = 5V / 10.6kΩ = 472µA
- U(ih) = 5V × 6.9kΩ / 11.6kΩ = 2.9V, U(ih)' = 5V × 5.9kΩ / 10.6kΩ = 2.78V 

Aus https://www.mikrocontroller.net/articles/Entprellung

> Ein Taster prellt üblicherweise bis zu etwa 10 ms.
> Zur Sicherheit kann bei der Berechnung des Widerstandes eine Prellzeit von 20 ms angenommen werden.

**Calculation for Output Pins**

_(Backward) Calculation:_

- Uyellow = 1.8V, Ured = 1.6V
- Iled = 10mA
- Ryellow = (5V-1.8V)/10mA = 320Ω,  Ryellow = (5V-1.6V)/10mA = 340Ω
- --> R = 330Ω

- Igs = 100nA
- Iout,max = 40mA
- Iout = 1mA
- Rout = 3.3V / 1mA = 3.3kΩ
# Software Implementation — Power Management for Low Power Consumption

## Power Consumption

Navlico tries to consume as little power as possible.
The implemented techniques are based on
- [Espressif: ESP-IDF Programming Guide - API Reference - System API - Power Management](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/power_management.html)
- [Espressif: ESP-IDF Programming Guide - API Guide - Low Power Modes](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/low-power-mode/index.html)

## Framework Configuration

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


## Programming Strategies

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

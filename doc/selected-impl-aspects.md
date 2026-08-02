# Selected Implementation Aspects

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
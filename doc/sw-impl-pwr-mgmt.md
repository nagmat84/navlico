# Software Implementation Strategies for Low Power Consumption

Navlico tries to consume as little power as possible.
The implemented techniques are based on
- [Espressif: ESP-IDF Programming Guide - “API Reference › System API › Power Management”](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/power_management.html)
- [Espressif: ESP-IDF Programming Guide - “API Guide › Low Power Modes”](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/low-power-mode/index.html)

## Automatic Power Management

Navlico uses power management with dynamic frequency selection and automatic sleep.

### Relevant Build Configuration

- **`CONFIG_PM_ENABLE=y`:** Support for power management
- **`CONFIG_FREERTOS_USE_TICKLESS_IDLE=y`:** Allow tickless idling; necessary to support automatic light sleep
- **`CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_96=y`:** Set to 96 MHz (instead of less power-consuming 48 MHz) due to [ESP-IDF bug #19001](https://github.com/espressif/esp-idf/issues/19001)
- **`CONFIG_PM_DFS_INIT_AUTO=n`:** Disable dynamic frequency scaling (DFS) at startup due to bug [ESP-IDF bug #19001](https://github.com/espressif/esp-idf/issues/19001)

### Relevant Ramifications on Source Code

`app_main()` calls `setup_power_management()` at the earliest possibility
which uses `esp_pm_configure()` to enable
- DFS between `CONFIG_NAVLICO_PM_MIN_FREQ` and `CONFIG_NAVLICO_PM_MAX_FREQ` as well as
- automatic light sleep

`setup_power_management()` also calls `esp_sleep_set_console_uart_handling_mode()`
to ensure that all log messages are flushed via UART before entering sleep mode.

We do use neither `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_48` nor `CONFIG_PM_DFS_INIT_AUTO`.
This would

> Enable dynamic frequency scaling (DFS) at startup.
> The startup code configures dynamic frequency scaling.
> Max CPU frequency is set to `DEFAULT_CPU_FREQ_MHZ` setting, min frequency is set to XTAL frequency.

(see [Espressif: Configuration Options Reference – `CONFIG_PM_DFS_INIT_AUTO`](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/kconfig-reference.html#config-pm-dfs-init-auto))

However, if we did so and called `esp_pm_configure()`, then the framework miscalculates the APB frequency
due to [ESP-IDF bug #19001](https://github.com/espressif/esp-idf/issues/19001)

## Automatic Deep Sleep

The built-in power management is extended (through using callbacks) to not only go into light sleep but even deep sleep,
if the [Navlico’s FSM](./fsm.md) is in “OFF” state.

### Relevant Build Configuration

- **`CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP=y`:** Reduce wake-up time from deep sleep via skipping validation
- **`CONFIG_LIBC_TIME_SYSCALL_USE_RTC_HRT=y`:** Let time of day continue during deep sleep; required to log time spent in deep sleep
- **`CONFIG_RTC_CLK_SRC_INT_RC=y`:** Use internal RC oscillator for RTC in deep-sleep (instead of external 36 kHz crystal)
- **`CONFIG_PM_LIGHT_SLEEP_CALLBACKS=y`:** Enable light-sleep callbacks; required to redirect µC into deep-sleep instead

The first three options are copied from Espressif’s code example “deep\_sleep”.

### Relevant Ramifications on Source Code

`setup_power_management()` calls `esp_pm_light_sleep_register_cbs()` to install the callback `navlico_pm_try_deep_sleep()`.
`esp_pm_light_sleep_register_cbs()` requires `CONFIG_PM_LIGHT_SLEEP_CALLBACKS=y`.
The framework calls `navlico_pm_try_deep_sleep()` when it attempts to go into light sleep.
`navlico_pm_try_deep_sleep()` checks if [Navlico’s FSM](./fsm.md) is in “OFF” state
and – if that is the case – goes into deep sleep instead.

## Prefer Using RAM over Flash

Navlico is a simple program which doesn’t require much RAM for volatile runtime information.
This allows us to place more code and read-only constants (mostly string constants) in RAM instead of the flash.
This provides us with two benefits:
- The program can remain in light-sleep longer
- The µC can disable the flash during light sleep (see [sec. “Disabling Hardware During Light Sleep”](#disabling-hardware-during-light-sleep))
Disabling the flash is only possible if all code and data which is relevant for wake-up resides in RAM.

To check how much RAM is allocated by instructions (IRAM) and static DATA (DRAM) use `idf.py size`.

### Relevant Build Configuration

- **`CONFIG_PM_SLEEP_FUNC_IN_IRAM=y`:** Place Power Management module functions in RAM
- **`CONFIG_PM_SLP_IRAM_OPT=y`:** Places 2.1 KB of instructions into IRAM and extends light sleep by 310 µs (see [Espressif: Configuration Options Reference](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/kconfig-reference.html#config-pm-slp-iram-opt)).
- **`CONFIG_PM_RTOS_IDLE_OPT=y`:** Places 180 B of instructions into IRAM and extends light sleep by 20 µs (see [Espressif: Configuration Options Reference](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/kconfig-reference.html#config-pm-rtos-idle-opt)).
- **`CONFIG_FREERTOS_IN_IRAM=y`:** Place FreeRTOS function in IRAM. 
- **`CONFIG_GPIO_CTRL_FUNC_IN_IRAM=y`:** Place GPIO control functions into IRAM.

### Relevant Ramifications on Source Code

In order to stay in light sleep as long as possible and to disable the flash in light sleep, Navlico places
- the power management code in RAM
- the interrupt-service routine (ISR) `handle_navlico_fsm_gpio_interrupt()` which reacts to button presses in RAM

Navlico allocates this ISR with `ESP_INTR_FLAG_IRAM` and the ISR must therefore be IRAM-save.
This implies that all instruction and data which the `handle_navlico_fsm_gpio_interrupt()` accesses must be placed in RAM, incl. the function it calls.
`handle_navlico_fsm_gpio_interrupt()` calls
 - `disable_navlico_fsm_gpio_interrupts()` to prevent subsequent interrupts
   which in turn calls `gpio_intr_disable` and `gpio_set_intr_type` and requires `CONFIG_GPIO_CTRL_FUNC_IN_IRAM=y`
 - `vTaskNotifyGiveFromISR` to notify the FSM task about the interrupt and requires `CONFIG_FREERTOS_IN_IRAM=y`

See [Espressif: API Guides – “Memory Types › How to Place Code in IRAM”](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-guides/memory-types.html#how-to-place-code-in-iram) for general information on the topic.

**Coding Pitfall**

Always allocate string constants which must be place in DRAM as arrays like this:
```
static const DRAM_ATTR char MY_STRING[] = "xxx";
```
This ensures that the actual string constant, i.e. the array, is placed in RAM.
Allocating string constants as pointers is **wrong**:
```
static DRAM_ATTR char const * const MY_STRING = "xxx"
```
This only places the pointer itself in RAM, but the string which the pointer points to, may still end up in flash memory.

## Disabling Hardware During Light Sleep

To reduce power consumption, Navlico explicitly disables the following hardware during light sleep:
- GPIOs (except those required for wake-up)
- Flash memory

### Relevant Build Configuration

- ***`CONFIG_PM_SLP_DISABLE_GPIO=y`:*** Disable all GPIO when chip at sleep
- ***`CONFIG_ESP_SLEEP_POWER_DOWN_FLASH=y`:*** see "ESP-IDF Programming Guide/API Reference/System API/Sleep Modes/Power-down of Flash"

### Relevant Ramifications on Source Code

During light sleep some GPIOs act as active outputs or as wake-up sources.
Those GPIOs must remain powered.
Navlico calls
- calls `gpio_sleep_sel_dis` on the input and output pins such that they don't lose their configuration during sleep
  (see [Espressif: Configuration Options Reference – `CONFIG_PM_SLP_DISABLE_GPIO`](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/kconfig-reference.html#config-pm-slp-disable-gpio))
- calls `esp_sleep_pd_config` to keep the power domain controlled by XPD TOP on during light sleep
  (see [Espressif: ESP32-H2 Datsheet, sec. 2.2](https://espressif.com/sites/default/files/documentation/esp32-h2_datasheet_en.pdf))

The PWM output (LEDC) needs continue output during light sleep:
- `ledc_clk_cfg_t` selects the internal 8 MHz RC oscillator (`RC_FAST_CLK`) as source clock which is light-sleep compatible
- `ledc_channel_config_t::sleep_mode` selects `LEDC_SLEEP_MODE_KEEP_ALIVE` to keep output running during light sleep

See [Espressif: API Reference – LED Control (LEDC)](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/peripherals/ledc.html).

Navlico does not use the Pin Hold Feature.
The Pin Hold Feature does _not_ allow to hold the pull-up/-down configuration during light sleep (despite many online sources claiming the opposite).
The Pin Hold Feature only latches the recent configuration and input reading into an internal register,
but it does not help to keep the GPIO configuration during light sleep.
The hardware uses external pull-up/-downs.

## Future: RTC Clock Source

The RTC can use different clock sources.
At the moment, the RTC follows the default

- **Internal 150 kHz RC oscillator** (`CONFIG_RTC_CLK_SRC_INT_RC=y`, default):  
  This option provides the lowes deep sleep current consumption and does not require extra external components.
  However, frequency stability with respect to temperature is poor, so time may drift in deep/light sleep modes.
- **External 32 kHz crystal** (`CONFIG_RTC_CLK_SRC_EXT_CRYS`):  
  This option provides higher frequency stability but requires more power.
  The evaluation board provides an external crystal.

See [Espressif: Configuration Options Reference – `CONFIG_RTC_CLK_SRC` ](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig-reference.html#config-rtc-clk-src).

Using an external crystal with higher stability and less drift
may allow to increase `CONFIG_PM_LIGHTSLEEP_RTC_OSC_CAL_INTERVAL` to values greater than 1
which in turn could lower the power consumption again.

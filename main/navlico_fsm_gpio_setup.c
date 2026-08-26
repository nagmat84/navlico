/// \file main.c
/// Implements the setup routines for the GPIOs of Navlico's Finite State Machine (FSM).

#include "navlico_fsm.h"
#include "sdkconfig.h"
#include <esp_log.h>
#include <esp_sleep.h>

#if CONFIG_NAVLICO_HAS_VERBOSE_OUTPUT
void static dump_navlico_fsm_io_configuration( void ) {
	static uint64_t mask = 0ULL;
	if ( mask == 0ULL ) {
		for ( navlico_fsm_gpio_id_t g = 0; g < GPIO_COUNT; ++g )
			mask |= GPIO_MASK( navlico_fsm_gpios[g].num );
	}
	esp_log_level_t const level = esp_log_level_get( NAVLICO_FSM_TAG );
	if ( level == ESP_LOG_DEBUG || level == ESP_LOG_VERBOSE )
		gpio_dump_io_configuration( stdout, mask );
}
#else
void static dump_navlico_fsm_io_configuration( void ) {}
#endif

/**
 * Configures a single GPIO
 *
 * @internal This function explicitly sets the pin direction and pull mode during (light) sleep via
 * `gpio_sleep_set_direction()` and `gpio_sleep_set_pull_mode()`.
 * Navlico does not use `gpio_sleep_sel_dis()` for each pin, because its **effect does not last.**
 * Navlico uses the build options `CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND` and `ESP_SLEEP_FLASH_LEAKAGE_WORKAROUND`
 * as they mitigate some nasty effects (see documentation of those build options) and they are enabled by default
 * anyway.
 * However, `CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND` overacts and comes with a stupid restriction:
 * Each time Naviclo re-configures which GPIOs should act as wake-up sources with `gpio_wakeup_disable` or
 * `gpio_wakeup_enable` the framework also calls `gpio_sleep_sel_en()` and hence `gpio_sleep_sel_dis()` becomes
 * useless.
 * (IMHO, `gpio_wakeup_disable` shouldn't internally call `gpio_sleep_sel_en()`
 * no matter whether `CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND` is or is not set.)
 * `CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND` enables the same code paths as the build option `CONFIG_PM_SLP_DISABLE_GPIO`.
 * For this build option the documentation
 * [Espressif: ](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/kconfig-reference.html#config-pm-slp-disable-gpio)
 * states
 *
 * > `CONFIG_PM_SLP_DISABLE_GPIO`
 * >
 * > If you want to specifically use some pins normally as chip wakes when chip sleeps,
 * > you can call `gpio_sleep_sel_dis` to disable this feature on those pins.
 * > You can also keep this feature on and call `gpio_sleep_set_direction` and `gpio_sleep_set_pull_mode`
 * > to have a different GPIO configuration at sleep.
 *
 * However, as stated above `gpio_sleep_sel_dis` has no lasting effect.
 * Hence, using `gpio_sleep_set_direction` and `gpio_sleep_set_pull_mode` are the only reliable option.
 * While Navlico _doesn't_ want a _different_ GPIO configuration, Navilco still uses  `gpio_sleep_set_direction` and
 * `gpio_sleep_set_pull_mode` to have the _same_ configuration during light sleep.
 *
 * @param gpio_def The GPIO definition (contains GPIO number and configuration)
 */
void static setup_navlico_fsm_gpio_function( navlico_fsm_gpio_t const * const gpio_def ) {
	ESP_ERROR_CHECK( gpio_set_direction( gpio_def->num, gpio_def->mode ) );
	ESP_ERROR_CHECK( gpio_sleep_set_direction( gpio_def->num, gpio_def->mode ) );
	ESP_ERROR_CHECK( gpio_set_pull_mode( gpio_def->num, GPIO_FLOATING ) );
	ESP_ERROR_CHECK( gpio_sleep_set_pull_mode( gpio_def->num, GPIO_FLOATING ) );
	ESP_ERROR_CHECK( gpio_intr_disable( gpio_def->num ) );
	ESP_ERROR_CHECK( gpio_set_intr_type( gpio_def->num, GPIO_INTR_DISABLE ) );
}

/**
 * Configures the GPIO Functions
 *
 * The code relies on external pull-down resistors.
 * The ESP32-H2 lacks the `RTC_PERIPH` power domain and hence does not provide the special functions `rtc_gpio_...`.
 * The ESP32-H2 supports the HOLD function but this only latches the most recently read value form the input pin into
 * an internal register and isolates the GPIO peripheral from the pin.
 * The HOLD function does not actively pull down the input pin and any noise will trigger an immediate wake-up.
 */
void static setup_navlico_fsm_gpio_functions( void ) {
	dump_navlico_fsm_io_configuration();
	for ( navlico_fsm_gpio_id_t g = 0; g < GPIO_COUNT; ++g )
		setup_navlico_fsm_gpio_function( &navlico_fsm_gpios[g] );
	dump_navlico_fsm_io_configuration();
}

/**
 * Sets the wake-up sources.
 *
 * Enables all but `ignored_gpio` as a wake-up source.
 * Some buttons and navigational lights share a combined input/output GPIO.
 * When the FSM is in a state which drives such a GPIO, then that GPIO must not be set as a wake-up source as the
 * wake-up source would immediately trigger.
 *
 * @param ignored_gpio The GPIO which shall not be enabled as a wake-up source
 */
void set_navlico_fsm_gpio_wakeup( navlico_fsm_gpio_t const * const ignored_gpio, bool const prepare_for_deep_sleep ) {
	if ( prepare_for_deep_sleep ) {
		ESP_LOGD( NAVLICO_FSM_TAG, "Enabling EXT1 wake-up" );
		for ( navlico_fsm_button_id_t btn = 0; btn < BTN_COUNT; ++btn ) {
			navlico_fsm_gpio_t const * const gpio = navlico_fsm_buttons[btn].gpio;
			if ( !esp_sleep_is_valid_wakeup_gpio( gpio->num ) ) continue;
			if ( gpio == ignored_gpio ) {
				esp_sleep_disable_ext1_wakeup_io( gpio->num );
			} else {
				esp_sleep_enable_ext1_wakeup_io(
					GPIO_MASK( gpio->num ), gpio->active_level ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ANY_LOW
				);
			}
		}
	} else {
		ESP_LOGD( NAVLICO_FSM_TAG, "Enabling GPIO wake-up" );
		for ( navlico_fsm_button_id_t btn = 0; btn < BTN_COUNT; ++btn ) {
			navlico_fsm_gpio_t const * const gpio = navlico_fsm_buttons[btn].gpio;
			if ( gpio == ignored_gpio ) {
				gpio_wakeup_disable( gpio->num );
			} else {
				gpio_wakeup_enable( gpio->num, gpio->active_level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL );
			}
		}
		ESP_ERROR_CHECK( esp_sleep_enable_gpio_wakeup() );
	}
}

/**
 * Set up the GPIOs for Navlico's FSM.
 */
void setup_navlico_fsm_gpio( void ) {
	ESP_LOGD( NAVLICO_FSM_TAG, "Setting up GPIOs ..." );
	setup_navlico_fsm_gpio_functions();
	ESP_LOGD( NAVLICO_FSM_TAG, "GPIOs set up" );
}

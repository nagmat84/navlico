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
		for ( navlico_fsm_gpio_tag_t g = 0; g < GPIO_COUNT; ++g )
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
 * @param gpio_def The GPIO definition (contains GPIO number and configuration)
 */
void static setup_navlico_fsm_gpio_function( navlico_fsm_gpio_t const * const gpio_def ) {
	ESP_ERROR_CHECK( gpio_set_direction( gpio_def->num, gpio_def->mode ) );
	ESP_ERROR_CHECK( gpio_set_pull_mode( gpio_def->num, GPIO_FLOATING ) );
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
	for ( navlico_fsm_gpio_tag_t g = 0; g < GPIO_COUNT; ++g )
		setup_navlico_fsm_gpio_function( &navlico_fsm_gpios[g] );
	dump_navlico_fsm_io_configuration();
}

/**
 * Configures the power management for the pins
 *
 * This function calls `gpio_sleep_sel_dis` on the GPIOs.
 * Without `gpio_sleep_sel_dis`, the GPIOs would be isolated and lose the configuration
 * when the controller goes to light sleep.
 * For the output pins, the external MOSFET would slowly discharge each output pin as they are not actively driven.
 * For the input pins, the internal comparator wouldn't be power and the input pins not register any input.
 */
void static setup_navlico_fsm_gpio_power_mgmt( void ) {
	// See https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/kconfig-reference.html#config-pm-slp-disable-gpio
	//
	// `CONFIG_PM_SLP_DISABLE_GPIO` is set to `y` to disable all GPIOs during light sleep.
	//
	// you can call 'gpio_sleep_sel_dis' to disable this feature on those pins.
	// You can also keep this feature on and call 'gpio_sleep_set_direction' and 'gpio_sleep_set_pull_mode'
	ESP_LOGD( NAVLICO_FSM_TAG, "Ensure the GPIOs keep configuration in light sleep" );
	for ( navlico_fsm_gpio_tag_t g = 0; g < GPIO_COUNT; ++g )
		ESP_ERROR_CHECK( gpio_sleep_sel_dis( navlico_fsm_gpios[g].num ) );

	// See Datasheet Sec. 2.2
	// Digital pins (GPIO0 ~ GPIO5, GPIO22 ~ GPIO27):
	// are unable to work in Deep-sleep mode, but can work in Light-sleep mode
	// only if the power domain controlled by the XPD TOP does not power off.
	ESP_LOGD( NAVLICO_FSM_TAG, "Ensure the GPIOs remain powered in light sleep" );
	ESP_ERROR_CHECK( esp_sleep_pd_config( ESP_PD_DOMAIN_TOP, ESP_PD_OPTION_ON ) );
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
void enable_navlico_fsm_gpio_wakeup( navlico_fsm_gpio_t const * const ignored_gpio ) {
	ESP_LOGD( NAVLICO_FSM_TAG, "Enabling GPIO wake-up" );
	for ( navlico_fsm_button_tag_t btn = 0; btn < BTN_COUNT; ++btn ) {
		navlico_fsm_gpio_t const * const gpio = navlico_fsm_buttons[btn].gpio;
		if ( gpio == ignored_gpio ) continue;;
		gpio_wakeup_enable( gpio->num, gpio->active_level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL );
	}
	ESP_ERROR_CHECK( esp_sleep_enable_gpio_wakeup() );

	ESP_LOGD( NAVLICO_FSM_TAG, "Enabling EXT1 wake-up" );
	for ( navlico_fsm_button_tag_t btn = 0; btn < BTN_COUNT; ++btn ) {
		navlico_fsm_gpio_t const * const gpio = navlico_fsm_buttons[btn].gpio;
		if ( gpio == ignored_gpio || !esp_sleep_is_valid_wakeup_gpio( gpio->num ) ) continue;
		esp_sleep_enable_ext1_wakeup_io(
			GPIO_MASK( gpio->num ), gpio->active_level ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ANY_LOW
		);
	}
}

void disable_navlico_fsm_gpio_wakeup( void ) {
	for ( navlico_fsm_button_tag_t btn = 0; btn < BTN_COUNT; ++btn ) {
		navlico_fsm_gpio_t const * const gpio = navlico_fsm_buttons[btn].gpio;
		gpio_wakeup_disable( gpio->num );
	}
	esp_sleep_disable_wakeup_source( ESP_SLEEP_WAKEUP_GPIO );
	esp_sleep_disable_ext1_wakeup_io( 0 );
}

/**
 * Set up the GPIOs for Navlico's FSM.
 */
void setup_navlico_fsm_gpio( void ) {
	ESP_LOGD( NAVLICO_FSM_TAG, "Setting up GPIOs ..." );
	setup_navlico_fsm_gpio_functions();
	setup_navlico_fsm_gpio_power_mgmt();
	ESP_LOGD( NAVLICO_FSM_TAG, "GPIOs set up" );
}

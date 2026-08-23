/// \file main.c
/// Implements the setup routines for the GPIOs of Navlico's Finite State Machine (FSM).

#include "navlico_fsm.h"
#include "sdkconfig.h"
#include <esp_log.h>
#include <esp_sleep.h>

#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
void static dump_navlico_fsm_io_configuration( void ) {
	static uint64_t mask = 0ULL;
	if ( mask == 0ULL ) {
		for ( navlico_fsm_button_t btn = 0; btn < BTN_COUNT; ++btn )
			mask |= GPIO_MASK( navlico_fsm_buttons[btn].gpio_num );
		for ( navlico_fsm_indicator_t ind = 0; ind < IND_COUNT; ++ind )
			mask |= GPIO_MASK( navlico_fsm_indicators[ind].gpio_num );
		for ( navlico_fsm_light_t light = 0; light < LIGHT_COUNT; ++light )
			mask |= GPIO_MASK( navlico_fsm_lights[light].gpio_num );
	}
	if ( esp_log_level_get( NAVLICO_FSM_TAG ) == ESP_LOG_VERBOSE )
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
void static setup_navlico_fsm_gpio_function( navlico_fsm_gpio_definition_t const * const gpio_def ) {
	ESP_ERROR_CHECK( gpio_set_direction( gpio_def->gpio_num, gpio_def->gpio_mode ) );
	ESP_ERROR_CHECK( gpio_set_pull_mode( gpio_def->gpio_num, GPIO_FLOATING ) );
	ESP_ERROR_CHECK( gpio_intr_disable( gpio_def->gpio_num ) );
	ESP_ERROR_CHECK( gpio_set_intr_type( gpio_def->gpio_num, GPIO_INTR_DISABLE ) );
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
	for ( navlico_fsm_button_t btn = 0; btn < BTN_COUNT; ++btn )
		setup_navlico_fsm_gpio_function( &navlico_fsm_buttons[btn] );
	for ( navlico_fsm_indicator_t ind = 0; ind < IND_COUNT; ++ind )
		setup_navlico_fsm_gpio_function( &navlico_fsm_indicators[ind] );
	for ( navlico_fsm_light_t light = 0; light < LIGHT_COUNT; ++light )
		setup_navlico_fsm_gpio_function( &navlico_fsm_lights[light] );
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
	for ( navlico_fsm_button_t btn = 0; btn < BTN_COUNT; ++btn )
		ESP_ERROR_CHECK( gpio_sleep_sel_dis( navlico_fsm_buttons[btn].gpio_num ) );
	for ( navlico_fsm_indicator_t ind = 0; ind < IND_COUNT; ++ind )
		ESP_ERROR_CHECK( gpio_sleep_sel_dis( navlico_fsm_indicators[ind].gpio_num ) );
	for ( navlico_fsm_light_t light = 0; light < LIGHT_COUNT; ++light )
		ESP_ERROR_CHECK( gpio_sleep_sel_dis( navlico_fsm_lights[light].gpio_num ) );

	// See Datasheet Sec. 2.2
	// Digital pins (GPIO0 ~ GPIO5, GPIO22 ~ GPIO27):
	// are unable to work in Deep-sleep mode, but can work in Light-sleep mode
	// only if the power domain controlled by the XPD TOP does not power off.
	ESP_LOGD( NAVLICO_FSM_TAG, "Ensure the GPIOs remain powered in light sleep" );
	ESP_ERROR_CHECK( esp_sleep_pd_config( ESP_PD_DOMAIN_TOP, ESP_PD_OPTION_ON ) );
}

/**
 * Configures necessary wake-up sources.
 */
void static setup_navlico_fsm_gpio_wakeup( void ) {
	static uint64_t GPIO_DEEP_SLEEP_WAKEUP_BUTTONS_MASK;
	GPIO_DEEP_SLEEP_WAKEUP_BUTTONS_MASK =
		GPIO_MASK( navlico_fsm_buttons[ SAILING_BTN ].gpio_num ) |
		GPIO_MASK( navlico_fsm_buttons[ DRIVING_BTN ].gpio_num ) |
		GPIO_MASK( navlico_fsm_buttons[ ANCHORING_BTN ].gpio_num ) |
	#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
		GPIO_MASK( navlico_fsm_buttons[ SAILING_COAST_BTN ].gpio_num ) |
		GPIO_MASK( navlico_fsm_buttons[ DISABLED_BTN ].gpio_num ) |
	#endif
		0LL;

	ESP_LOGD( NAVLICO_FSM_TAG, "Enabling GPIO wake-up" );
	for ( navlico_fsm_button_t btn = 0; btn < BTN_COUNT; ++btn )
		ESP_ERROR_CHECK( gpio_wakeup_enable(
			navlico_fsm_buttons[btn].gpio_num,
			navlico_fsm_buttons[btn].active_level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL
		) );
	ESP_ERROR_CHECK( esp_sleep_enable_gpio_wakeup() );
	ESP_LOGD( NAVLICO_FSM_TAG, "Enabling EXT1 wake-up" );
	ESP_ERROR_CHECK( esp_sleep_enable_ext1_wakeup_io( GPIO_DEEP_SLEEP_WAKEUP_BUTTONS_MASK, ESP_EXT1_WAKEUP_ANY_LOW ) );
}

/**
 * Set up the GPIOs for Navlico's FSM.
 */
void setup_navlico_fsm_gpio( void ) {
	setup_navlico_fsm_gpio_functions();
	setup_navlico_fsm_gpio_power_mgmt();
	setup_navlico_fsm_gpio_wakeup();
}

/// \file main.c
/// Implements the setup routines for the GPIOs of Navlico's Finite State Machine (FSM).

#include "navlico_fsm.h"
#include "navlico_gpio_defs.h"
#include "sdkconfig.h"
#include <esp_log.h>
#include <esp_sleep.h>

#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
void static dump_navlico_fsm_io_configuration( void ) {
	if ( esp_log_level_get( NAVLICO_FSM_TAG ) == ESP_LOG_VERBOSE )
		gpio_dump_io_configuration(
			stdout,
			GPIO_INPUT_ONLY_MASK | GPIO_COMBINED_IO_OPEN_DRAIN_MASK | GPIO_OUTPUT_ONLY_PUSH_PULL_MASK | GPIO_OUTPUT_ONLY_OPEN_DRAIN_MASK
		);
}
#else
void static dump_navlico_fsm_io_configuration( void ) {}
#endif

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
	gpio_config_t config = {
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,  // only enable interrupts _after_ the ISR has been set up, keep interrupts off for now
		.hys_ctrl_mode = GPIO_HYS_SOFT_ENABLE
	};

	ESP_LOGI( NAVLICO_FSM_TAG, "Setting up input-only GPIOs");
	config.pin_bit_mask = GPIO_INPUT_ONLY_MASK;
	config.mode = GPIO_MODE_INPUT;
	ESP_ERROR_CHECK( gpio_config( &config ) );

	ESP_LOGI( NAVLICO_FSM_TAG, "Setting up combined input/output pins in open-drain mode");
	config.pin_bit_mask = GPIO_COMBINED_IO_OPEN_DRAIN_MASK;
	config.mode = GPIO_MODE_INPUT_OUTPUT_OD;
	ESP_ERROR_CHECK( gpio_config( &config ) );

	ESP_LOGI( NAVLICO_FSM_TAG, "Setting up output-only pins in push-pull mode");
	config.pin_bit_mask = GPIO_OUTPUT_ONLY_PUSH_PULL_MASK;
	config.mode = GPIO_MODE_OUTPUT;
	ESP_ERROR_CHECK( gpio_config( &config ) );

	ESP_LOGI( NAVLICO_FSM_TAG, "Setting up output-only pins in open-drain mode");
	config.pin_bit_mask = GPIO_OUTPUT_ONLY_OPEN_DRAIN_MASK;
	config.mode = GPIO_MODE_OUTPUT_OD;
	ESP_ERROR_CHECK( gpio_config( &config ) );

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
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_OFF_BUTTON ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_SAILING_BUTTON ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_DRIVING_BUTTON ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_ANCHORING_BUTTON ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_SAILING_INDICATOR ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_DRIVING_INDICATOR ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_ANCHORING_INDICATOR ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_SIDE_N_STERN_LIGHT ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_MASTHEAD_LIGHT ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_ALLROUND_WHITE_LIGHT ) );
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_SAILING_COAST_BUTTON ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_DISABLED_BUTTON ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_SAILING_COAST_INDICATOR ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_DISABLED_INDICATOR ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_ALLROUND_GREEN_LIGHT ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_ALLROUND_RED_1_LIGHT ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_ALLROUND_RED_2_LIGHT ) );
#endif

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
	ESP_LOGD( NAVLICO_FSM_TAG, "Enabling GPIO wake-up" );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_OFF_BUTTON, GPIO_INTR_LOW_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_SAILING_BUTTON, GPIO_INTR_LOW_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_DRIVING_BUTTON, GPIO_INTR_LOW_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_ANCHORING_BUTTON, GPIO_INTR_LOW_LEVEL ) );
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_SAILING_COAST_BUTTON, GPIO_INTR_LOW_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_DISABLED_BUTTON, GPIO_INTR_LOW_LEVEL ) );
#endif
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

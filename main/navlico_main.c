/** \file navlico_main.c
 * The main file of Navlico.
 */

#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/_timeval.h>

#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_defs.h"
#include "common_defs.h"

/**
 * Enum to define the operational state
 *
 * The operational state directly corresponds to the most recently pressed button and active indicator light.
 */
typedef enum navlico_operational_state_t_impl {
	OFF = 0,      ///< The user has pressed the OFF button, the operational state is OFF
	SAILING = 1,  ///< The user has pressed the SAILING button, the operational state is SAILING
	DRIVING = 2,  ///< The user has pressed the DRIVING button, the operational state is DRIVING
	ANCHORING = 3 ///< The user has pressed the ANCHORING button, the operational state is ANCHORING
} navlico_operational_state_t;

#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
/// Timestamp when program entered deep sleep for the last time
RTC_DATA_ATTR static struct timeval deep_sleep_enter_time;
/// Timestamp when program entered light sleep for the last time
RTC_DATA_ATTR static struct timeval light_sleep_enter_time;
#endif

/// The active operational state
RTC_DATA_ATTR static navlico_operational_state_t operational_state;

/**
 * Amount of ticks to wait for input buttons to stabilize and become released again
 *
 * TODO: Don't use a fix time period, but actively monitor and wait until user has released buttons again
 */
static constexpr TickType_t delayTicks = 50;

/**
 * Logs the duration since last deep sleep.
 *
 * The function only logs a message if the log level is DEBUG or higher.
 * This functions is a no-op if build configuration disables debug logs.
 */
void log_deep_sleep_duration( void ) {
#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
	struct timeval now;
	gettimeofday( &now, nullptr );
	long long const sleep_time_ms = (now.tv_sec - deep_sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - deep_sleep_enter_time.tv_usec) / 1000;
	ESP_LOGD( LOG_TAG, "Spend %lldms in deep sleep", sleep_time_ms );
#endif
}

/**
 * Logs the duration since last light sleep.
 *
 * The function only logs a message if the log level is DEBUG or higher.
 * This functions is a no-op if build configuration disables debug logs.
 */
void log_light_sleep_duration( void ) {
#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
	struct timeval now;
	gettimeofday( &now, nullptr );
	long long const sleep_time_ms = (now.tv_sec - light_sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - light_sleep_enter_time.tv_usec) / 1000;
	ESP_LOGD( LOG_TAG, "Spend %lldms in light sleep", sleep_time_ms );
#endif
}

/**
 * Sets up (Enables) Power Management
 *
 * Sets a lower minimum CPU frequency (`CONFIG_NAVLICO_MIN_FREQ`) and enables support for automatic light sleep.
 *
 * Also see [Espressif: API Guides - Low Power Modes - DFS Configuration](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-guides/low-power-mode/low-power-mode-soc.html#dfs-configuration).
 */
void setup_power_management( void ) {
	ESP_LOGI( LOG_TAG, "Enabling DFS with %u MHz as minimum frequency", CONFIG_NAVLICO_MIN_FREQ );
	const esp_pm_config_t pm_config = {
		.max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
		.min_freq_mhz = CONFIG_NAVLICO_MIN_FREQ,
		.light_sleep_enable = true
	};
	ESP_ERROR_CHECK( esp_pm_configure( &pm_config ) );

#if CONFIG_LOG_DEFAULT_LEVEL_NONE && !LOG_MAXIMUM_LEVEL_DEBUG && !LOG_MAXIMUM_LEVEL_VERBOSE
	esp_sleep_set_console_uart_handling_mode( ESP_SLEEP_ALWAYS_DISCARD_UART );
#else
	esp_sleep_set_console_uart_handling_mode( ESP_SLEEP_ALWAYS_FLUSH_UART );
#endif
}

/**
 * Configures the Input Pins
 *
 * The code relies on external pull-down resistors.
 *
 * The ESP32-H2 lacks the `RTC_PERIPH` power domain and hence does not provide the special functions `rtc_gpio_...`.
 * The ESP32-H2 supports the HOLD function but this only latches the most recently read value form the input pin into
 * an internal register and isolates the GPIO peripheral from the pin.
 * The HOLD function does not actively pull down the input pin and any noise will trigger an immediate wake-up.
 *
 * As a pre-cautionary action, this function calls `gpio_sleep_sel_dis` on the input pins.
 * Without `gpio_sleep_sel_dis` and if `CONFIG_PM_SLP_DISABLE_GPIO` was set, the GPIOs would lose their input function
 * when the controller goes to light sleep.
 */
void setup_input_pins( void ) {
#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
	if ( esp_log_level_get( LOG_TAG ) == ESP_LOG_VERBOSE )
		gpio_dump_io_configuration( stdout, GPIO_ALL_BUTTONS_MASK );
#endif

	const gpio_config_t config = {
		.pin_bit_mask = GPIO_ALL_BUTTONS_MASK,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
	};
	ESP_LOGI( LOG_TAG, "Setting up input pins");
	ESP_ERROR_CHECK( gpio_config( &config ) );

	// See https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/kconfig-reference.html#config-pm-slp-disable-gpio
	//
	// `CONFIG_PM_SLP_DISABLE_GPIO` is set to `y` to disable all GPIOs during light sleep.
	//
	// you can call 'gpio_sleep_sel_dis' to disable this feature on those pins.
	// You can also keep this feature on and call 'gpio_sleep_set_direction' and 'gpio_sleep_set_pull_mode'
	gpio_sleep_sel_dis( GPIO_OFF_BUTTON );
	gpio_sleep_sel_dis( GPIO_SAILING_BUTTON );
	gpio_sleep_sel_dis( GPIO_DRIVING_BUTTON );
	gpio_sleep_sel_dis( GPIO_ANCHORING_BUTTON );

#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
	if ( esp_log_level_get( LOG_TAG ) == ESP_LOG_VERBOSE )
		gpio_dump_io_configuration( stdout, GPIO_ALL_BUTTONS_MASK );
#endif
}

/**
 * Configures the Output Pins
 *
 * As a pre-cautionary action, this function calls `gpio_sleep_sel_dis` on the output pins.
 * Without `gpio_sleep_sel_dis` and if `CONFIG_PM_SLP_DISABLE_GPIO` was set, the GPIOs would be isolated when the
 * controller goes to light sleep and the external MOSFET would slowly discharge each output pin as they are not
 * actively driven.
 */
void setup_output_pins( void ) {
#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
	if ( esp_log_level_get( LOG_TAG ) == ESP_LOG_VERBOSE )
		gpio_dump_io_configuration( stdout, GPIO_ALL_INDICATORS_MASK | GPIO_ALL_LIGHTS_MASK );
#endif

	const gpio_config_t config = {
		.pin_bit_mask = GPIO_ALL_INDICATORS_MASK | GPIO_ALL_LIGHTS_MASK,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	ESP_LOGI( LOG_TAG, "Setting up output pins");
	ESP_ERROR_CHECK( gpio_config( &config ) );

	// See https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/kconfig-reference.html#config-pm-slp-disable-gpio
	//
	// `CONFIG_PM_SLP_DISABLE_GPIO` is set to `y` to disable all GPIOs during light sleep.
	//
	// you can call 'gpio_sleep_sel_dis' to disable this feature on those pins.
	// You can also keep this feature on and call 'gpio_sleep_set_direction' and 'gpio_sleep_set_pull_mode'
	gpio_sleep_sel_dis( GPIO_SAILING_INDICATOR );
	gpio_sleep_sel_dis( GPIO_DRIVING_INDICATOR );
	gpio_sleep_sel_dis( GPIO_ANCHORING_INDICATOR );
	gpio_sleep_sel_dis( GPIO_SIDE_N_STERN_LIGHT );
	gpio_sleep_sel_dis( GPIO_MASTHEAD_LIGHT );
	gpio_sleep_sel_dis( GPIO_ALLROUND_WHITE_LIGHT );

#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
	if ( esp_log_level_get( LOG_TAG ) == ESP_LOG_VERBOSE )
		gpio_dump_io_configuration( stdout, GPIO_ALL_INDICATORS_MASK | GPIO_ALL_LIGHTS_MASK );
#endif
}

/**
 * Reads the input pins and returns the currently or most recently pressed button.
 *
 * This function is called whenever the inputs should be handled:
 *  - after cold boot
 *  - after waking-up from deep sleep
 *  - after waking-up from light sleep
 *
 *  @return The currently or most recently pressed button.
 */
navlico_operational_state_t read_input_pins( void ) {
	uint32_t const wakeup_causes = esp_sleep_get_wakeup_causes();
	ESP_LOGI( LOG_TAG, "Reading input pins (wakeup_causes = 0x%.8" PRIx32 ")", wakeup_causes );

	if ( wakeup_causes & BIT( ESP_SLEEP_WAKEUP_EXT1 ) ) {
		ESP_LOGI( LOG_TAG, "Woke up from deep sleep" );
		uint64_t const wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
		if ( GPIO_SAILING_BUTTON_MASK & wakeup_pin_mask )
			return SAILING;
		if ( GPIO_DRIVING_BUTTON_MASK & wakeup_pin_mask )
			return DRIVING;
		if ( GPIO_ANCHORING_BUTTON_MASK & wakeup_pin_mask )
			return ANCHORING;
		ESP_LOGE( LOG_TAG, "Unable to determine GPIO which caused wake-up from deep sleep (pin mask = 0x%.16" PRIx64 ")", wakeup_pin_mask );
		return OFF;
	}

	if ( wakeup_causes & BIT( ESP_SLEEP_WAKEUP_GPIO ) ) {
		int offButtonLevel = 0;
		int sailingButtonLevel = 0;
		int drivingButtonLevel = 0;
		int anchoringButtonLevel = 0;
		ESP_LOGI(LOG_TAG, "Woke up from light sleep");
		// Repeated readings to debounce
		for ( int i = 0; i < CONFIG_NAVLICO_DEBOUNCE_PROBES; ++i) {
			offButtonLevel += gpio_get_level( GPIO_OFF_BUTTON );
			sailingButtonLevel += gpio_get_level( GPIO_SAILING_BUTTON );
			drivingButtonLevel += gpio_get_level( GPIO_DRIVING_BUTTON );
			anchoringButtonLevel += gpio_get_level( GPIO_ANCHORING_BUTTON );
			vTaskDelay( CONFIG_NAVLICO_DEBOUNCE_DELAY_MS / portTICK_PERIOD_MS );
		}
		ESP_LOGD( LOG_TAG,
			  "Input pins have been read (offButtonLevel = %d, sailingButtonLevel = %d, drivingButtonLevel = %d, anchoringButtonLevel = %d)",
			  offButtonLevel, sailingButtonLevel, drivingButtonLevel, anchoringButtonLevel );
		if ( offButtonLevel > CONFIG_NAVLICO_DEBOUNCE_PROBES / 2 )
			return OFF;
		if ( sailingButtonLevel > CONFIG_NAVLICO_DEBOUNCE_PROBES / 2 )
			return SAILING;
		if ( drivingButtonLevel > CONFIG_NAVLICO_DEBOUNCE_PROBES / 2 )
			return DRIVING;
		if ( anchoringButtonLevel > CONFIG_NAVLICO_DEBOUNCE_PROBES / 2 )
			return ANCHORING;
		ESP_LOGE( LOG_TAG, "Unable to determine GPIO which caused wake-up from light sleep" );
		return OFF;
	}

	ESP_LOGI( LOG_TAG, "Came out of cold boot; simulating off button had been pressed" );
	return OFF;
}

/**
 * Writes the output pins according to the current operational state.
 *
 * This function uses the currently stored operational state in #operational_state to set the output pins.
 */
void write_output_pins( void ) {
	switch ( operational_state ) {
		case OFF:
			ESP_LOGI( LOG_TAG, "New navigation light state: OFF" );
			gpio_set_level( GPIO_SAILING_INDICATOR, 0 );
			gpio_set_level( GPIO_DRIVING_INDICATOR, 0 );
			gpio_set_level( GPIO_ANCHORING_INDICATOR, 0 );
			gpio_set_level(GPIO_SIDE_N_STERN_LIGHT, 0 );
			gpio_set_level(GPIO_MASTHEAD_LIGHT, 0 );
			gpio_set_level(GPIO_ALLROUND_WHITE_LIGHT, 0 );
			break;
		case SAILING:
			ESP_LOGI( LOG_TAG, "New navigation light state: SAILING" );
			gpio_set_level( GPIO_SAILING_INDICATOR, 1 );
			gpio_set_level( GPIO_DRIVING_INDICATOR, 0 );
			gpio_set_level( GPIO_ANCHORING_INDICATOR, 0 );
			gpio_set_level(GPIO_SIDE_N_STERN_LIGHT, 1 );
			gpio_set_level(GPIO_MASTHEAD_LIGHT, 0 );
			gpio_set_level(GPIO_ALLROUND_WHITE_LIGHT, 0 );
			break;
		case DRIVING:
			ESP_LOGI( LOG_TAG, "New navigation light state: DRIVING" );
			gpio_set_level( GPIO_SAILING_INDICATOR, 0 );
			gpio_set_level( GPIO_DRIVING_INDICATOR, 1 );
			gpio_set_level( GPIO_ANCHORING_INDICATOR, 0 );
			gpio_set_level(GPIO_SIDE_N_STERN_LIGHT, 1 );
			gpio_set_level(GPIO_MASTHEAD_LIGHT, 1 );
			gpio_set_level(GPIO_ALLROUND_WHITE_LIGHT, 0 );
			break;
		case ANCHORING:
			ESP_LOGI( LOG_TAG, "New navigation light state: ANCHORING" );
			gpio_set_level( GPIO_SAILING_INDICATOR, 0 );
			gpio_set_level( GPIO_DRIVING_INDICATOR, 0 );
			gpio_set_level( GPIO_ANCHORING_INDICATOR, 1 );
			gpio_set_level(GPIO_SIDE_N_STERN_LIGHT, 0 );
			gpio_set_level(GPIO_MASTHEAD_LIGHT, 0 );
			gpio_set_level(GPIO_ALLROUND_WHITE_LIGHT, 1 );
			break;
	}
}

/**
 * Attempts to go into deep sleep.
 *
 * This function is a wrapper around `esp_deep_sleep_try_to_start()`.
 * As a preliminary step, the function ensures that the GPIOs are set as a wake-up source, but nothing else.
 *
 * @return Result of the underlying `esp_deep_sleep_try_to_start()`.
 */
esp_err_t sleep_deeply( void ) {
	ESP_LOGD( LOG_TAG, "Enabling EXT1 wake-up on input pins for buttons" );
	ESP_ERROR_CHECK( esp_sleep_disable_wakeup_source( ESP_SLEEP_WAKEUP_ALL ) );
	ESP_ERROR_CHECK( esp_sleep_enable_ext1_wakeup_io( GPIO_WAKEUP_BUTTONS_MASK, ESP_EXT1_WAKEUP_ANY_HIGH ) );
	ESP_LOGI( LOG_TAG, "Going to deep sleep ..." );
#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
	gettimeofday( &deep_sleep_enter_time, nullptr );
#endif
	return esp_deep_sleep_try_to_start();
}

/**
 * Attempts to go into deep sleep.
 *
 * This function is a wrapper around `esp_light_sleep_start()`.
 * As a preliminary step, the function ensures that the GPIOs are set as a wake-up source, but nothing else.
 *
 * @return Result of the underlying `esp_light_sleep_start()`.
 */
esp_err_t sleep_lightly( void ) {
	ESP_LOGD( LOG_TAG, "Enabling GPIO wake-up on input pins for buttons" );
	ESP_ERROR_CHECK( esp_sleep_disable_wakeup_source( ESP_SLEEP_WAKEUP_ALL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_OFF_BUTTON , GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_SAILING_BUTTON , GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_DRIVING_BUTTON , GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_ANCHORING_BUTTON , GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( esp_sleep_enable_gpio_wakeup() );
	ESP_LOGD( LOG_TAG, "Ensure the GPIO outputs remain powered in light sleep" );
	// See Datasheet Sec. 2.2
	// Digital pins (GPIO0 ~ GPIO5, GPIO22 ~ GPIO27):
	// are unable to work in Deep-sleep mode, but can work in Light-sleep mode
	// only if the power domain controlled by the XPD TOP does not power off.
	ESP_ERROR_CHECK( esp_sleep_pd_config( ESP_PD_DOMAIN_TOP, ESP_PD_OPTION_ON ) );
	ESP_LOGI( LOG_TAG, "Going to light sleep ..." );
#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
	gettimeofday( &light_sleep_enter_time, nullptr );
#endif
	return esp_light_sleep_start();
}

/**
 * Attempts to go into light or deep sleep depending on the current operational state.
 *
 * If the current operational state if `OFF`,
 * this function attempts to go into deep sleep by calling ::sleep_deeply().
 * Else, this function attempts to go into light sleep by calling ::sleep_lightly().
 *
 * @return Result of the underlying ::sleep_deeply() or ::sleep_lightly().
 */
esp_err_t hibernate( void ) {
	esp_err_t const sleep_error = operational_state == OFF ? sleep_deeply() : sleep_lightly();
	switch ( sleep_error ) {
		case ESP_ERR_SLEEP_REJECT:
			ESP_LOGE( LOG_TAG, "%s sleep rejected as wake-up source already set", operational_state == OFF ? "Deep" : "Light" );
			return sleep_error;
		case ESP_ERR_SLEEP_TOO_SHORT_SLEEP_DURATION:
			ESP_LOGE( LOG_TAG, "%s sleep rejected as period would be too short", operational_state == OFF ? "Deep" : "Light" );
			return sleep_error;
		case ESP_OK:
			log_light_sleep_duration();
			return ESP_OK;
		default:
			ESP_LOGE( LOG_TAG, "%s sleep failed with unknown reason", operational_state == OFF ? "Deep" : "Light" );
			return sleep_error;
	}
}

/**
 * The main task of Navlico
 */
void app_main(void) {
	setup_power_management();
	setup_input_pins();
	setup_output_pins();

	log_deep_sleep_duration();
	do {
		operational_state = read_input_pins();
		write_output_pins();
		// Wait a little until the buttons become released, otherwise hibernation will fail
		// TODO: Don't wait a fixed amount of time, but actively check and wait until user releases buttons again
		vTaskDelay( delayTicks );
	} while ( hibernate() == ESP_OK );
}

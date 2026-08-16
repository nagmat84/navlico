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

typedef enum navigation_lights_state_t_impl {
	OFF = 0,
	SAILING = 1,
	DRIVING = 2,
	ANCHORING = 3
} navigation_lights_state_t;

#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
RTC_DATA_ATTR static struct timeval deep_sleep_enter_time;
RTC_DATA_ATTR static struct timeval light_sleep_enter_time;
#endif

RTC_DATA_ATTR static navigation_lights_state_t navigation_light_state;

static constexpr TickType_t delayTicks = 50;

//static TaskHandle_t main_task_handle = nullptr;

void log_deep_sleep_duration( void ) {
#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
	struct timeval now;
	gettimeofday( &now, nullptr );
	long long const sleep_time_ms = (now.tv_sec - deep_sleep_enter_time.tv_sec) * 1000 + (now.tv_usec - deep_sleep_enter_time.tv_usec) / 1000;
	ESP_LOGD( LOG_TAG, "Spend %lldms in deep sleep", sleep_time_ms );
#endif
}

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
 * Sets a lower minimum CPU frequency (`CONFIG_NAVLICO_MIN_FREQ`) and enables support for light sleep.
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
 * The ESP32-H2 lacks the `RTC_PERIPH` power domain and does not provide the special functions `rtc_gpio_...`.
 * Only the regular `gpio_...` functions are available.
 * See `components/esp_hw_support/include/esp_sleep.h` and
 * [Espressif: ESP32-H2 - Technical Reference Manual, Sec. 6.11.1](https://documentation.espressif.com/esp32-h2_technical_reference_manual_en.pdf)
 * for available power domains.
 * This implies that input pins only support the HOLD function which latches the last input state before the
 * controller goes into deep sleep. See [Espressif: ESP32-H2 - Technical Reference Manual, Sec. 6.9](https://documentation.espressif.com/esp32-h2_technical_reference_manual_en.pdf)
 * > Each GPIO pin (including the LP pins: GPIO8 ~ GPIO14) has an individual hold function controlled by an LP register.
 * > When the pin is set to hold, the state is latched at that moment and will not change
 * > no matter how the internal signals change or how the IO MUX/GPIO configuration is modified.
 * > Users can use the hold function for the pins to retain the pin state through a core reset
 * > triggered by watchdog time-out or Deep-sleep events.
 * If the controller supported the `RTC_PERIPH` domain, then we had to call
 * `esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF)` explicitly in order to power down the that domain
 * during deep sleep despite setting the internal pull-down and latching the last state.
 */
void setup_input_pins( void ) {
#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
	if ( esp_log_level_get( LOG_TAG ) == ESP_LOG_VERBOSE )
		gpio_dump_io_configuration( stdout, GPIO_ALL_BUTTONS_MASK );
#endif

	/* We enable the internal pull-downs on the input pins such that the ESP-IDF framework latches (aka hold) the
	 * last known state of the input pins before deep sleep.
	 * Note that we explicitly disabled the `RTC_PERIPH` power domain also for chips which support them.
	 * Hence, the internal pull-down will always lose their function in deep sleep for any kind of chip.
	 * An external pull-down is always required.
	 * Setting the internal pull-down is merely a workaround to latch/hold the last-known good state as the ESP-IDF
	 * framework has no dedicated API call for that.
	 */
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

/*void handle_gpio_interrupt( void* ) {
	vTaskResume( main_task_handle );
}

void setup_isr( void ) {
	main_task_handle = xTaskGetCurrentTaskHandle();
	gpio_isr_register( handle_gpio_interrupt, nullptr, ESP_INTR_FLAG_HIGH | ESP_INTR_FLAG_SHARED, nullptr );
}*/

navigation_lights_state_t read_input_pins( void ) {
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

void write_output_pins( void ) {
	switch ( navigation_light_state ) {
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
 * Updates the navigation light state by reading the GPIO input and setting GPIO output accordingly
 *
 * This function is called whenever the inputs should be handled:
 *  - after cold boot
 *  - after waking-up from deep sleep
 *  - after waking-up from light sleep
 *  - after the interrupt-service routine (ISR) released the main task from suspension
 */
void update_state( void ) {
	ESP_LOGD( LOG_TAG, "Updating state" );
	navigation_light_state = read_input_pins();
	write_output_pins();
}

bool sleep_deeply( void ) {
	ESP_LOGD( LOG_TAG, "Enabling EXT1 wake-up on input pins for buttons" );
	ESP_ERROR_CHECK( esp_sleep_disable_wakeup_source( ESP_SLEEP_WAKEUP_ALL ) );
	ESP_ERROR_CHECK( esp_sleep_enable_ext1_wakeup_io( GPIO_WAKEUP_BUTTONS_MASK, ESP_EXT1_WAKEUP_ANY_HIGH ) );
	ESP_LOGI( LOG_TAG, "Going to deep sleep ..." );
#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
	gettimeofday( &deep_sleep_enter_time, nullptr );
#endif
	return esp_deep_sleep_try_to_start();
}

bool sleep_lightly( void ) {
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

bool hibernate( void ) {
	esp_err_t const sleep_error = navigation_light_state == OFF ? sleep_deeply() : sleep_lightly();
	switch ( sleep_error ) {
		case ESP_ERR_SLEEP_REJECT:
			ESP_LOGE( LOG_TAG, "%s sleep rejected as wake-up source already set", navigation_light_state == OFF ? "Deep" : "Light" );
			return false;
		case ESP_ERR_SLEEP_TOO_SHORT_SLEEP_DURATION:
			ESP_LOGE( LOG_TAG, "%s sleep rejected as period would be too short", navigation_light_state == OFF ? "Deep" : "Light" );
			return false;
		case ESP_OK:
			log_light_sleep_duration();
			return true;
		default:
			ESP_LOGE( LOG_TAG, "%s sleep failed with unknown reason", navigation_light_state == OFF ? "Deep" : "Light" );
			return false;
	}
}


/**
 * The main task of Navlico
 */
void app_main(void) {
	setup_power_management();
	setup_input_pins();
	setup_output_pins();
	//setup_isr();

	log_deep_sleep_duration();
	do {
		update_state();
		// Wait a little until the buttons become released, otherwise hibernation will fail
		// TODO: Don't wait a fixed amount of time, but actively check and wait until user releases buttons again
		vTaskDelay( delayTicks );
	} while ( hibernate() );
}

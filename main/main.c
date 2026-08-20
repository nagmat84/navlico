/** \file main.c
 * The main file of Navlico.
 */

#include "navlico_fsm.h"
#include "sdkconfig.h"
#include <esp_attr.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <sys/time.h>

static char const * const NAVLICO_MAIN_TAG = "navlico_main";

#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
/// Timestamp when program entered deep sleep for the last time
RTC_DATA_ATTR static struct timeval deep_sleep_enter_time;
#endif

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
	ESP_LOGD( NAVLICO_MAIN_TAG, "Spend %lldms in deep sleep", sleep_time_ms );
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
	ESP_LOGI( NAVLICO_MAIN_TAG, "Enabling DFS between %u MHz and %u MHz", CONFIG_NAVLICO_PM_MIN_FREQ, CONFIG_NAVLICO_PM_MAX_FREQ );
	const esp_pm_config_t pm_config = {
		.max_freq_mhz = CONFIG_NAVLICO_PM_MAX_FREQ,
		.min_freq_mhz = CONFIG_NAVLICO_PM_MIN_FREQ,
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
 * Attempts to go into deep sleep.
 *
 * This function is a wrapper around `esp_deep_sleep_try_to_start()`.
 * As a preliminary step, the function ensures that the GPIOs are set as a wake-up source, but nothing else.
 *
 * @return Result of the underlying `esp_deep_sleep_try_to_start()`.
 */
esp_err_t sleep_deeply( void ) {
	ESP_LOGI( NAVLICO_MAIN_TAG, "Going to deep sleep ..." );
#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
	gettimeofday( &deep_sleep_enter_time, nullptr );
#endif
	return esp_deep_sleep_try_to_start();
}

/**
 * The main task of Navlico
 */
void app_main(void) {
	setup_power_management();
	log_deep_sleep_duration();

	xTaskCreate( navlico_fsm_task, NAVLICO_FSM_TAG, CONFIG_MAIN_TASK_STACK_SIZE, nullptr, 6, nullptr );
	while( true ) {
		// Let the higher prioritized task run
		vTaskDelay( 0 );
		// If we come back here, all higher prioritized tasks have been either yielded or suspended
		if ( get_navlico_fsm_state() == OFF ) {
			// `navlico_fsm_task` is in OFF state
			// We go to deep sleep.
			// We assume that `navlico_fsm_task` has set up proper wake-up sources.
			sleep_deeply();
		}
		// `navlico_fsm_task` has only yielded, but not suspended
		// Maybe it is just waiting for a stable read on the input buttons (debouncing).
		// We do the same and just sleep
		vTaskDelay( pdMS_TO_TICKS( 20 ) );
	}
}

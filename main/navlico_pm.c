/// \file navlico_pm.c
/// Implements power management for Navlico.
///
/// When the built-in power management of the ESP IDF framework attempts to go into (ligh) sleep mode,
/// Navlico's implementation may alternatively go into deep sleep, if Navlico's FSM is in OFF state.
///
/// If built with a log level greater or equal to debugging this implementation also logs the sleep time.

#include "navlico_fsm.h"
#include "navlico_pm.h"
#include "sdkconfig.h"
#include <esp_attr.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <sys/time.h>

static char const * const NAVLICO_PM_TAG = "navlico_pm";

#if CONFIG_NAVLICO_HAS_SLEEP_TIMES

RTC_RODATA_ATTR static char * const NAVLICO_PM_LIGHT_STR = "light";
RTC_RODATA_ATTR static char * const NAVLICO_PM_DEEP_STR = "deep";
/// Timestamp when program entered deep sleep for the last time
RTC_DATA_ATTR static struct timeval navlico_pm_deep_sleep_enter_time;

IRAM_ATTR static esp_err_t log_navlico_pm_enter_sleep( int64_t const sleep_time_us, void* ) {
	ESP_EARLY_LOGI( NAVLICO_PM_TAG, "Going to sleep for approximately %" PRId64 " ms ...", sleep_time_us );
	return ESP_OK;
}

IRAM_ATTR static esp_err_t log_navlico_pm_exit_sleep( int64_t const sleep_time_us, void *arg ) {
	ESP_EARLY_LOGI( NAVLICO_PM_TAG, "Spent %" PRId64 " ms in %s sleep", sleep_time_us, arg );
	return ESP_OK;
}

void log_navlico_pm_time_since_deep_sleep( void ) {
	struct timeval now;
	gettimeofday( &now, nullptr );
	int64_t const deep_sleep_time_us =
		(now.tv_sec - navlico_pm_deep_sleep_enter_time.tv_sec) * 1000 * 1000 +
		(now.tv_usec - navlico_pm_deep_sleep_enter_time.tv_usec);
	log_navlico_pm_exit_sleep( deep_sleep_time_us, NAVLICO_PM_DEEP_STR );
}
#endif

/**
 * Attempts to go into deep sleep.
 *
 * This function is a wrapper around `esp_deep_sleep_try_to_start()`.
 * As a preliminary step, the function ensures that the GPIOs are set as a wake-up source, but nothing else.
 *
 * @return Result of the underlying `esp_deep_sleep_try_to_start()`.
 */
IRAM_ATTR esp_err_t navlico_pm_try_deep_sleep( int64_t const sleep_time_us, void *arg ) {
#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
	gettimeofday( &navlico_pm_deep_sleep_enter_time, nullptr );
#endif
	if ( sleep_time_us < CONFIG_NAVLICO_DEEP_SLEEP_THRESHOLD_MS * 1000 ) {
		ESP_EARLY_LOGI( NAVLICO_PM_TAG, "Expected sleep time of %" PRId64 " ms too short for deep sleep", sleep_time_us / 1000 );
		return ESP_OK;
	}
	if ( get_navlico_fsm_state() != OFF ) {
		ESP_EARLY_LOGI( NAVLICO_PM_TAG, "Navlico FSM is not in OFF state; deep sleep not possible" );
		return ESP_OK;
	}
	// `esp_pm_configure(const void*)` enables the timer as a wake-up source with 0µs.
	// Only `vApplicationSleep(TickType_t)` re-enables the timer with the correct sleep time
	// _after_ all pre-sleep callbacks have been executed.
	// This means at this point the timer is enabled as a wake-up source with the wake-up time in the past.
	// We must explicitly disable the timer as otherwise deep sleep will fail.
	esp_sleep_disable_wakeup_source( ESP_SLEEP_WAKEUP_TIMER );
	ESP_EARLY_LOGI( NAVLICO_PM_TAG, "Navlico FSM is entering deep sleep ..." );
	esp_err_t const res = esp_deep_sleep_try_to_start();
	if ( res == ESP_ERR_SLEEP_REJECT ) {
		ESP_EARLY_LOGE( NAVLICO_PM_TAG, "Deep sleep rejected as wake-up source already triggered" );
	}
	return res;
}

/**
 * Sets up (Enables) Power Management
 *
 * Sets a lower minimum CPU frequency (`CONFIG_NAVLICO_MIN_FREQ`) and enables support for automatic light sleep.
 *
 * Also see [Espressif: API Guides - Low Power Modes - DFS Configuration](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-guides/low-power-mode/low-power-mode-soc.html#dfs-configuration).
 */
void setup_power_management( void ) {
	ESP_LOGI( NAVLICO_PM_TAG, "Enabling DFS between %u MHz and %u MHz", CONFIG_NAVLICO_PM_MIN_FREQ, CONFIG_NAVLICO_PM_MAX_FREQ );
	const esp_pm_config_t pm_config = {
		.max_freq_mhz = CONFIG_NAVLICO_PM_MAX_FREQ,
		.min_freq_mhz = CONFIG_NAVLICO_PM_MIN_FREQ,
		.light_sleep_enable = true
	};
	ESP_ERROR_CHECK( esp_pm_configure( &pm_config ) );

#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
	esp_pm_sleep_cbs_register_config_t pm_cb_log_config = {
		.enter_cb = log_navlico_pm_enter_sleep,
		.exit_cb = log_navlico_pm_exit_sleep,
		.enter_cb_user_arg = nullptr,
		.exit_cb_user_arg = NAVLICO_PM_LIGHT_STR,
		.enter_cb_prior = 0,
		.exit_cb_prior = 0
	};
	ESP_ERROR_CHECK( esp_pm_light_sleep_register_cbs( &pm_cb_log_config ) );
#endif
	esp_pm_sleep_cbs_register_config_t pm_cb_deep_sleep_config = {
		.enter_cb = navlico_pm_try_deep_sleep,
		.exit_cb = nullptr,
		.enter_cb_user_arg = nullptr,
		.exit_cb_user_arg = nullptr,
		.enter_cb_prior = UINT32_MAX, // redirection to deep sleep must be executed as the last callback as nothing will be executed after going into deep sleep sucessfully
		.exit_cb_prior = 0
	};
	ESP_ERROR_CHECK( esp_pm_light_sleep_register_cbs( &pm_cb_deep_sleep_config ) );

#if CONFIG_LOG_DEFAULT_LEVEL_NONE && !LOG_MAXIMUM_LEVEL_DEBUG && !LOG_MAXIMUM_LEVEL_VERBOSE
	esp_sleep_set_console_uart_handling_mode( ESP_SLEEP_ALWAYS_DISCARD_UART );
#else
	esp_sleep_set_console_uart_handling_mode( ESP_SLEEP_ALWAYS_FLUSH_UART );
#endif
}

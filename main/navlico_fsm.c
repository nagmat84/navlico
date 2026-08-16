/// \file main.c
/// Implements the Finite State Machine (FSM) handle the operational state and the associated GPIOs.

#include "navlico_fsm.h"
#include "navlico_gpio_defs.h"
#include "sdkconfig.h"
#include <esp_attr.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <unistd.h>

char const * const NAVLICO_FSM_TAG = "navlico_fsm";

#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
/// Timestamp when program entered light sleep for the last time
RTC_DATA_ATTR static struct timeval navlico_fsm_yield_enter_time;
#endif

/// The active operational state
RTC_DATA_ATTR static navlico_fsm_state_t navlico_fsm_state = UNDEFINED;

/**
 * Logs the duration since last light sleep.
 *
 * The function only logs a message if the log level is DEBUG or higher.
 * This functions is a no-op if build configuration disables debug logs.
 *
 * TODO: Don't let this individual component decide about sleep mode. This should be moved to the idle task.
 */
void log_navlico_fsm_yield_duration( void ) {
#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
	struct timeval now;
	gettimeofday( &now, nullptr );
	long long const sleep_time_ms =
		(now.tv_sec - navlico_fsm_yield_enter_time.tv_sec) * 1000 +
		(now.tv_usec - navlico_fsm_yield_enter_time.tv_usec) / 1000;
	ESP_LOGD( NAVLICO_FSM_TAG, "Spend %lldms in light sleep", sleep_time_ms );
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
void static setup_navlico_fsm_input_pins( void ) {
#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
	if ( esp_log_level_get( NAVLICO_FSM_TAG ) == ESP_LOG_VERBOSE )
		gpio_dump_io_configuration( stdout, GPIO_ALL_BUTTONS_MASK );
#endif

	const gpio_config_t config = {
		.pin_bit_mask = GPIO_ALL_BUTTONS_MASK,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
	};
	ESP_LOGI( NAVLICO_FSM_TAG, "Setting up input pins");
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
	if ( esp_log_level_get( NAVLICO_FSM_TAG ) == ESP_LOG_VERBOSE )
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
void static setup_navlico_fsm_output_pins( void ) {
#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
	if ( esp_log_level_get( NAVLICO_FSM_TAG ) == ESP_LOG_VERBOSE )
		gpio_dump_io_configuration( stdout, GPIO_ALL_INDICATORS_MASK | GPIO_ALL_LIGHTS_MASK );
#endif

	const gpio_config_t config = {
		.pin_bit_mask = GPIO_ALL_INDICATORS_MASK | GPIO_ALL_LIGHTS_MASK,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	ESP_LOGI( NAVLICO_FSM_TAG, "Setting up output pins");
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
	if ( esp_log_level_get( NAVLICO_FSM_TAG ) == ESP_LOG_VERBOSE )
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

/**
 * Reads the input pins and returns the currently or most recently pressed button.
 *
 * This function is called whenever the inputs should be handled:
 *  - after cold boot
 *  - after waking-up from deep sleep
 *  - after waking-up from light sleep
 *  - during normal runtime
 *  - after the interrupt-service routine (ISR) released the main task from suspension
 *
 * @param firstRun If `true`, the function does not read the current level of the input pins,
 * but reads the input level which has been latched upon boot.
 * After booting from deep-sleep users may already have released the buttons again, hence reading the current input
 * level won't give the desired result.
 * If `false`, the function reads the current level of the input pins.
 * @return The currently or most recently pressed button.
 */
navlico_fsm_state_t static read_navlico_fsm_input_pins( bool const firstRun ) {
	// A button typical bounces between 0.1ms and 10ms while being pressed down.
	// Source: https://www.mikrocontroller.net/articles/Entprellung
	// After 20ms even the worst button should have stabilized.
	// Hence, this function initially waits for 20ms and then takes 5 readings at 2ms intervals, i.e. 5 readings between
	// 20ms and 30ms.
	// Professional typists achieve at most 120 words (à 5 letters) per minutes.
	// This yields 60s/(120*5) = 100ms per keystroke.
	// Hence, 30ms < 100ms is still short enough.
	// Source: https://en.wikipedia.org/wiki/Words_per_minute
	static constexpr uint_fast8_t debounceProbes = 5;
	static constexpr useconds_t initialDebounceDelay = 20000;
	static constexpr useconds_t inbetweenDebounceDelay = 2000;

	if ( firstRun ) {
		uint32_t const wakeup_causes = esp_sleep_get_wakeup_causes();
		ESP_LOGI( NAVLICO_FSM_TAG, "Reading input pins (wakeup_causes = 0x%.8" PRIx32 ")", wakeup_causes );

		if ( wakeup_causes & BIT( ESP_SLEEP_WAKEUP_EXT1 ) ) {
			ESP_LOGI( NAVLICO_FSM_TAG, "Woke up from deep sleep" );
			uint64_t const wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
			if ( GPIO_SAILING_BUTTON_MASK & wakeup_pin_mask )
				return SAILING;
			if ( GPIO_DRIVING_BUTTON_MASK & wakeup_pin_mask )
				return DRIVING;
			if ( GPIO_ANCHORING_BUTTON_MASK & wakeup_pin_mask )
				return ANCHORING;
			ESP_LOGE( NAVLICO_FSM_TAG, "Unable to determine GPIO which caused wake-up from deep sleep (pin mask = 0x%.16" PRIx64 ")", wakeup_pin_mask );
			return UNDEFINED;
		}

		ESP_LOGI( NAVLICO_FSM_TAG, "Came out of cold boot; simulating off button had been pressed" );
		return OFF;
	}

	uint_fast8_t offButtonLevel = 0;
	uint_fast8_t sailingButtonLevel = 0;
	uint_fast8_t drivingButtonLevel = 0;
	uint_fast8_t anchoringButtonLevel = 0;
	ESP_LOGI(NAVLICO_FSM_TAG, "Woke up from light sleep or invoked from runtime context switch");
	// Repeated readings to debounce
	usleep( initialDebounceDelay );
	for ( uint_fast8_t i = 0; i < debounceProbes; ++i) {
		offButtonLevel += gpio_get_level( GPIO_OFF_BUTTON );
		sailingButtonLevel += gpio_get_level( GPIO_SAILING_BUTTON );
		drivingButtonLevel += gpio_get_level( GPIO_DRIVING_BUTTON );
		anchoringButtonLevel += gpio_get_level( GPIO_ANCHORING_BUTTON );
		usleep( inbetweenDebounceDelay );
	}
	ESP_LOGD( NAVLICO_FSM_TAG,
	          "Input pins have been read "
	          "(offButtonLevel = %" PRIuFAST8 ", sailingButtonLevel = %" PRIuFAST8
	          ", drivingButtonLevel = %" PRIuFAST8 ", anchoringButtonLevel = %" PRIuFAST8 ")",
	          offButtonLevel, sailingButtonLevel, drivingButtonLevel, anchoringButtonLevel );
	if ( offButtonLevel > debounceProbes / 2 )
		return OFF;
	if ( sailingButtonLevel > debounceProbes / 2 )
		return SAILING;
	if ( drivingButtonLevel > debounceProbes / 2 )
		return DRIVING;
	if ( anchoringButtonLevel > debounceProbes / 2 )
		return ANCHORING;
	ESP_LOGE( NAVLICO_FSM_TAG, "Unable to determine active input GPIO" );
	return UNDEFINED;
}

/**
 * Waits until all input pins have become idle
 */
void static wait_for_navlico_fsm_idle_input( void ) {
	while ( gpio_get_level( GPIO_OFF_BUTTON ) == 1 ||
	        gpio_get_level( GPIO_SAILING_BUTTON ) == 1 ||
	        gpio_get_level( GPIO_DRIVING_BUTTON ) == 1 ||
	        gpio_get_level( GPIO_ANCHORING_BUTTON ) == 1 ) {
		vTaskDelay( pdMS_TO_TICKS( 10 ) );
	}
}

/**
 * Writes the output pins according to the current operational state.
 *
 * This function uses the currently stored operational state in #operational_state to set the output pins.
 */
void static write_navlico_fsm_output_pins( navlico_fsm_state_t const state ) {
	switch ( state ) {
		case UNDEFINED:
			// TODO: We should do something else here and conspicuously indicate this error condition instead of just pretending to be in the "OFF" state.
		case OFF:
			ESP_LOGI( NAVLICO_FSM_TAG, "New navigation light state: OFF" );
			gpio_set_level( GPIO_SAILING_INDICATOR, 0 );
			gpio_set_level( GPIO_DRIVING_INDICATOR, 0 );
			gpio_set_level( GPIO_ANCHORING_INDICATOR, 0 );
			gpio_set_level(GPIO_SIDE_N_STERN_LIGHT, 0 );
			gpio_set_level(GPIO_MASTHEAD_LIGHT, 0 );
			gpio_set_level(GPIO_ALLROUND_WHITE_LIGHT, 0 );
			break;
		case SAILING:
			ESP_LOGI( NAVLICO_FSM_TAG, "New navigation light state: SAILING" );
			gpio_set_level( GPIO_SAILING_INDICATOR, 1 );
			gpio_set_level( GPIO_DRIVING_INDICATOR, 0 );
			gpio_set_level( GPIO_ANCHORING_INDICATOR, 0 );
			gpio_set_level(GPIO_SIDE_N_STERN_LIGHT, 1 );
			gpio_set_level(GPIO_MASTHEAD_LIGHT, 0 );
			gpio_set_level(GPIO_ALLROUND_WHITE_LIGHT, 0 );
			break;
		case DRIVING:
			ESP_LOGI( NAVLICO_FSM_TAG, "New navigation light state: DRIVING" );
			gpio_set_level( GPIO_SAILING_INDICATOR, 0 );
			gpio_set_level( GPIO_DRIVING_INDICATOR, 1 );
			gpio_set_level( GPIO_ANCHORING_INDICATOR, 0 );
			gpio_set_level(GPIO_SIDE_N_STERN_LIGHT, 1 );
			gpio_set_level(GPIO_MASTHEAD_LIGHT, 1 );
			gpio_set_level(GPIO_ALLROUND_WHITE_LIGHT, 0 );
			break;
		case ANCHORING:
			ESP_LOGI( NAVLICO_FSM_TAG, "New navigation light state: ANCHORING" );
			gpio_set_level( GPIO_SAILING_INDICATOR, 0 );
			gpio_set_level( GPIO_DRIVING_INDICATOR, 0 );
			gpio_set_level( GPIO_ANCHORING_INDICATOR, 1 );
			gpio_set_level(GPIO_SIDE_N_STERN_LIGHT, 0 );
			gpio_set_level(GPIO_MASTHEAD_LIGHT, 0 );
			gpio_set_level(GPIO_ALLROUND_WHITE_LIGHT, 1 );
			break;
	}
}

esp_err_t static suspend_navlico_fsm( void ) {
	ESP_LOGD( NAVLICO_FSM_TAG, "Enabling EXT1 wake-up on input pins for buttons" );
	ESP_ERROR_CHECK( esp_sleep_disable_wakeup_source( ESP_SLEEP_WAKEUP_ALL ) );
	ESP_ERROR_CHECK( esp_sleep_enable_ext1_wakeup_io( GPIO_WAKEUP_BUTTONS_MASK, ESP_EXT1_WAKEUP_ANY_HIGH ) );
	ESP_LOGI( NAVLICO_FSM_TAG, "Suspending ..." );
	vTaskSuspend( nullptr );
	return ESP_OK;
}

/**
 * Attempts to go into deep sleep.
 *
 * This function is a wrapper around `esp_light_sleep_start()`.
 * As a preliminary step, the function ensures that the GPIOs are set as a wake-up source, but nothing else.
 *
 * @return Result of the underlying `esp_light_sleep_start()`.
 */
esp_err_t static sleep_navlico_fsm_lightly( void ) {
	ESP_LOGD( NAVLICO_FSM_TAG, "Enabling GPIO wake-up on input pins for buttons" );
	ESP_ERROR_CHECK( esp_sleep_disable_wakeup_source( ESP_SLEEP_WAKEUP_ALL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_OFF_BUTTON , GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_SAILING_BUTTON , GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_DRIVING_BUTTON , GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_ANCHORING_BUTTON , GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( esp_sleep_enable_gpio_wakeup() );
	ESP_LOGD( NAVLICO_FSM_TAG, "Ensure the GPIO outputs remain powered in light sleep" );
	// See Datasheet Sec. 2.2
	// Digital pins (GPIO0 ~ GPIO5, GPIO22 ~ GPIO27):
	// are unable to work in Deep-sleep mode, but can work in Light-sleep mode
	// only if the power domain controlled by the XPD TOP does not power off.
	ESP_ERROR_CHECK( esp_sleep_pd_config( ESP_PD_DOMAIN_TOP, ESP_PD_OPTION_ON ) );
	ESP_LOGI( NAVLICO_FSM_TAG, "Going to light sleep ..." );
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
esp_err_t static yield_navlico_fsm( void ) {
#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
	gettimeofday( &navlico_fsm_yield_enter_time, nullptr );
#endif
	esp_err_t const sleep_error = navlico_fsm_state == OFF ? suspend_navlico_fsm() : sleep_navlico_fsm_lightly();
	switch ( sleep_error ) {
		case ESP_ERR_SLEEP_REJECT:
			ESP_LOGE( NAVLICO_FSM_TAG, "Light sleep rejected as wake-up source already set" );
			return sleep_error;
		case ESP_ERR_SLEEP_TOO_SHORT_SLEEP_DURATION:
			ESP_LOGE( NAVLICO_FSM_TAG, "Light sleep rejected as period would be too short" );
			return sleep_error;
		case ESP_OK:
			log_navlico_fsm_yield_duration();
			return ESP_OK;
		default:
			ESP_LOGE( NAVLICO_FSM_TAG, "Yield (light sleep or suspending) failed with unknown reason" );
			return sleep_error;
	}
}

/**
 * Returns the current operational state of the FSM:
 *
 * The returned operational state equals `UNDEFINED` if
 * - the task has never read the inputs and set the state (initial state), or
 * - the task is currently in the middle of updating the state, but has not yet reached a consistent state again (transitional state)
 *
 * @return The current operational state of the FSM.
 */
navlico_fsm_state_t get_navlico_fsm_state( void ) {
	return navlico_fsm_state;
}

/**
 * Updates the state of the new FSM based on the input readings and sets the outputs accordingly.
 *
 * This functions sets the temporarily sets the state of the FMS to `UNDEFINED` while it reads the input pins and
 * sets the output pins accordingly.
 * This is a safety precaution in case another task calls get_navlico_fsm_state() asynchronously and concurrently
 * while this function is in the middle of updating the output pins to indicate that the there is no consistent state yet.
 *
 * @param firstRun Passed on to read_input_pins(bool) and determines how read_input_pins(bool) determines the new state.
 */
void update_navlico_fsm_state( bool firstRun ) {
	navlico_fsm_state = UNDEFINED;
	navlico_fsm_state_t const new_state = read_navlico_fsm_input_pins( firstRun );
	write_navlico_fsm_output_pins( new_state );
	wait_for_navlico_fsm_idle_input();
	navlico_fsm_state = new_state;
}

void navlico_fsm_task( void* ) {
	setup_navlico_fsm_input_pins();
	setup_navlico_fsm_output_pins();
	//setup_isr();

	// First run action
	update_navlico_fsm_state( true );

	// If `yield` suspends ourselves, the loop is not executed, as `yield` does not return.
	// Resuming from suspension will enter `navlico_fsm_task` from the start.
	// Only if `yield` goes into light sleep, `yield` returns and the loop is executed.
	while ( yield_navlico_fsm() == ESP_OK ) {
		update_navlico_fsm_state( false );
	}
}

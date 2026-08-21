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
#include <unistd.h>

char const * const NAVLICO_FSM_TAG = "navlico_fsm";

/// The active operational state
RTC_DATA_ATTR static navlico_fsm_state_t navlico_fsm_state = UNDEFINED;

/// The handle for the Navlico FSM Task
static TaskHandle_t navlico_fsm_task_handle;

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
		.intr_type = GPIO_INTR_DISABLE,  // only enable interrupts _after_ the ISR has been set up, keep interrupts off for now
		.hys_ctrl_mode = GPIO_HYS_SOFT_ENABLE
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
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_SAILING_INDICATOR ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_DRIVING_INDICATOR ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_ANCHORING_INDICATOR ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_SIDE_N_STERN_LIGHT ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_MASTHEAD_LIGHT ) );
	ESP_ERROR_CHECK( gpio_sleep_sel_dis( GPIO_ALLROUND_WHITE_LIGHT ) );

#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
	if ( esp_log_level_get( NAVLICO_FSM_TAG ) == ESP_LOG_VERBOSE )
		gpio_dump_io_configuration( stdout, GPIO_ALL_INDICATORS_MASK | GPIO_ALL_LIGHTS_MASK );
#endif
}

/**
 * Enables interrupts from the GPIO peripheral
 *
 * This function enables interrupts at their source, i.e. at the GPIO peripheral.
 * The function assumes that the interrupt is already (or still) allocated and the ISR installed.
 *
 * @internal The interrupt must trigger upon a high input level, a rising edge is not sufficient.
 * During (light) sleep a rising edge is not detected and the ISR will never be called.
 * `gpio_wakeup_enable only` only accepts the two level types for a reason:
 * Light-sleep GPIO wake on the normal digital pins is level-only by design.
 * The digital edge-detect logic isn't clocked while the core is down,
 * so there's no edge detector alive to catch the transition in the first place.
 * Only a level comparator is watching, which is why `HIGH_LEVEL` works and `POSEDGE` just never fires.
 * Only the pins which sit in the LP/RTC IO domain stay powered through light sleep and keeps a real edge detector.
 * The detector latches the rising edge and holds it until the CPU is back up.
 * So the LP/RTC pins are the only place you get true edge semantics across sleep.
 * See https://www.reddit.com/r/esp32/comments/1vtfldn/comment/p51c27o/
 */
void static enable_navlico_fsm_gpio_interrupts( void ) {
	ESP_ERROR_CHECK( gpio_set_intr_type( GPIO_OFF_BUTTON, GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_set_intr_type( GPIO_SAILING_BUTTON, GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_set_intr_type( GPIO_DRIVING_BUTTON, GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_set_intr_type( GPIO_ANCHORING_BUTTON, GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_intr_enable( GPIO_OFF_BUTTON ) );
	ESP_ERROR_CHECK( gpio_intr_enable( GPIO_SAILING_BUTTON ) );
	ESP_ERROR_CHECK( gpio_intr_enable( GPIO_DRIVING_BUTTON ) );
	ESP_ERROR_CHECK( gpio_intr_enable( GPIO_ANCHORING_BUTTON ) );
}

/**
 * Disables interrupts from the GPIO peripheral
 *
 * This function disables interrupts at their source, i.e. at the GPIO peripheral.
 * The function keeps the interrupt allocation and the ISR untouched.
 *
 * @internal This function must be placed in RAM as the ISR handle_navlico_fsm_gpio_interrupt(void) calls this function
 * to temporarily disable subsequent interrupts while the first is still handled.
 * An ISR can only call code from RAM.
 */
void static IRAM_ATTR disable_navlico_fsm_gpio_interrupts( void ) {
	ESP_ERROR_CHECK( gpio_intr_disable( GPIO_OFF_BUTTON ) );
	ESP_ERROR_CHECK( gpio_intr_disable( GPIO_SAILING_BUTTON ) );
	ESP_ERROR_CHECK( gpio_intr_disable( GPIO_DRIVING_BUTTON ) );
	ESP_ERROR_CHECK( gpio_intr_disable( GPIO_ANCHORING_BUTTON ) );
	ESP_ERROR_CHECK( gpio_set_intr_type( GPIO_OFF_BUTTON, GPIO_INTR_DISABLE ) );
	ESP_ERROR_CHECK( gpio_set_intr_type( GPIO_SAILING_BUTTON, GPIO_INTR_DISABLE ) );
	ESP_ERROR_CHECK( gpio_set_intr_type( GPIO_DRIVING_BUTTON, GPIO_INTR_DISABLE ) );
	ESP_ERROR_CHECK( gpio_set_intr_type( GPIO_ANCHORING_BUTTON, GPIO_INTR_DISABLE ) );
}

/**
 * The interrupt-service routine which notifies this task upon a GPIO interrupt.
 *
 *
 *
 * @internal An ISR can only code (and data) which resides in RAM as flash access (and SPI) is potentially disabled.
 * See:
 * - https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/system/intr_alloc.html#iram-safe-interrupt-handlers
 * - https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-guides/memory-types.html#when-to-place-code-in-iram
 * Hence, `vTaskNotifyGiveFromISR` and `vPortYieldFromISR` must be placed in IRAM, too.
 * This means `CONFIG_FREERTOS_IN_IRAM=y` must be set.
 */
void static IRAM_ATTR handle_navlico_fsm_gpio_interrupt( void* ) {
	disable_navlico_fsm_gpio_interrupts();
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	vTaskNotifyGiveFromISR( navlico_fsm_task_handle, &xHigherPriorityTaskWoken );
	if ( xHigherPriorityTaskWoken == pdTRUE ) {
		vPortYieldFromISR();
	}
}

/**
 * Registers the interrupt-service routine (ISR) for GPIO
 *
 * @internal This function registers the ISR with `ESP_INTR_FLAG_IRAM` to mark it as IRAM-safe, see
 * [Espressif: ESP-IDF Programming Guide - System API - Interrupt Allocation](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/system/intr_alloc.html#iram-safe-interrupt-handlers).
 * This means that handle_navlico_fsm_gpio_interrupt(void*) and all function it calls must be placed in IRAM.
 */
void static setup_navlico_fsm_isr( void ) {
	ESP_LOGD( NAVLICO_FSM_TAG, "Registering interrupt-service routine ..." );
#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
	if ( esp_log_level_get( NAVLICO_FSM_TAG ) == ESP_LOG_VERBOSE )
	esp_intr_dump( stdout );
#endif
	navlico_fsm_task_handle = xTaskGetCurrentTaskHandle();
	ESP_ERROR_CHECK( gpio_install_isr_service( ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_IRAM ) );
	ESP_ERROR_CHECK( gpio_isr_handler_add( GPIO_OFF_BUTTON, handle_navlico_fsm_gpio_interrupt, nullptr ) );
	ESP_ERROR_CHECK( gpio_isr_handler_add( GPIO_SAILING_BUTTON, handle_navlico_fsm_gpio_interrupt, nullptr ) );
	ESP_ERROR_CHECK( gpio_isr_handler_add( GPIO_DRIVING_BUTTON, handle_navlico_fsm_gpio_interrupt, nullptr ) );
	ESP_ERROR_CHECK( gpio_isr_handler_add( GPIO_ANCHORING_BUTTON, handle_navlico_fsm_gpio_interrupt, nullptr ) );
	ESP_LOGD( NAVLICO_FSM_TAG, "Interrupt-service routine registered" );
#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
	if ( esp_log_level_get( NAVLICO_FSM_TAG ) == ESP_LOG_VERBOSE )
		esp_intr_dump( stdout );
#endif
}

/**
 * Reads the input pins and returns the currently or most recently pressed button.
 *
 * This function is called whenever the inputs should be handled:
 *  - after cold boot
 *  - after waking-up from deep sleep
 *
 * @return The button which was pressed to trigger the wake-up from deep sleep
 */
navlico_fsm_state_t static read_navlico_fsm_input_pins_after_start() {
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

	ESP_LOGI( NAVLICO_FSM_TAG, "Came out of cold boot; simulating OFF button had been pressed" );
	return OFF;
}

/**
 * Reads the input pins and returns the currently or most recently pressed button.
 *
 * This function is called whenever the inputs should be handled:
 *  - after waking up from light sleep
 *  - during normal runtime
 *  - after the interrupt-service routine (ISR) notified this task
 *
 * @return The currently or most recently pressed button.
 */
navlico_fsm_state_t static read_navlico_fsm_input_pins() {
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

	uint_fast8_t offButtonLevel = 0;
	uint_fast8_t sailingButtonLevel = 0;
	uint_fast8_t drivingButtonLevel = 0;
	uint_fast8_t anchoringButtonLevel = 0;
	ESP_LOGI( NAVLICO_FSM_TAG, "Reading input pins" );
	// Repeated readings to debounce
	usleep( initialDebounceDelay );
	for ( uint_fast8_t i = 0; i < debounceProbes; ++i ) {
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

/**
 * Configures necessary wake-up sources.
 */
void static setup_navlico_fsm_wakeup_sources( void ) {
	ESP_LOGD( NAVLICO_FSM_TAG, "Enabling GPIO wake-up on input pins for buttons" );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_OFF_BUTTON, GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_SAILING_BUTTON, GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_DRIVING_BUTTON, GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( gpio_wakeup_enable( GPIO_ANCHORING_BUTTON, GPIO_INTR_HIGH_LEVEL ) );
	ESP_ERROR_CHECK( esp_sleep_enable_gpio_wakeup() );
	ESP_LOGD( NAVLICO_FSM_TAG, "Ensure the GPIO outputs remain powered in light sleep" );
	// See Datasheet Sec. 2.2
	// Digital pins (GPIO0 ~ GPIO5, GPIO22 ~ GPIO27):
	// are unable to work in Deep-sleep mode, but can work in Light-sleep mode
	// only if the power domain controlled by the XPD TOP does not power off.
	ESP_ERROR_CHECK( esp_sleep_pd_config( ESP_PD_DOMAIN_TOP, ESP_PD_OPTION_ON ) );
	ESP_LOGD( NAVLICO_FSM_TAG, "Enabling EXT1 wake-up on input pins for buttons" );
	ESP_ERROR_CHECK( esp_sleep_enable_ext1_wakeup_io( GPIO_WAKEUP_BUTTONS_MASK, ESP_EXT1_WAKEUP_ANY_HIGH ) );
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
 * Updates the state of the FSM based on the input readings and sets the outputs accordingly.
 *
 * This functions temporarily sets the state of the FMS to `UNDEFINED` while it reads the input pins and
 * sets the output pins accordingly.
 * This is a safety precaution in case another task calls get_navlico_fsm_state(void) asynchronously and concurrently
 * while this function is in the middle of updating the output pins to indicate that the there is no consistent state yet.
 *
 * @param firstRun If `true`, the function calls read_navlico_fsm_input_pins_after_start(void)
 * which reads the input level which has been latched upon boot.
 * After booting from deep-sleep users may already have released the buttons again, hence reading the current input
 * level won't give the desired result.
 * If `false`, the function calls read_navlico_fsm_input_pins(void) which reads the current level of the input pins.
 */
void update_navlico_fsm_state( bool firstRun ) {
	navlico_fsm_state = UNDEFINED;
	navlico_fsm_state_t const new_state = firstRun ?
		read_navlico_fsm_input_pins_after_start() :
		read_navlico_fsm_input_pins();
	write_navlico_fsm_output_pins( new_state );
	wait_for_navlico_fsm_idle_input();
	navlico_fsm_state = new_state;
}

/**
 * Entry point of the Navlico FSM Task.
 *
 * Contains the main loop of the Navlico FSM Task.
 * This function never returns and is supposed to be called via `xTaskCreate`.
 */
void navlico_fsm_task( void* ) {
	setup_navlico_fsm_input_pins();
	setup_navlico_fsm_output_pins();
	setup_navlico_fsm_wakeup_sources();
	setup_navlico_fsm_isr();

	// Update (initialize) state after boot (either cold boot or wake-up from deep sleep)
	update_navlico_fsm_state( true );

	// ReSharper disable once CppDFAEndlessLoop
	while ( true ) {
		// We have to (re-)enable the interrupts each time as the ISR disables the interrupts
		// before it notifies the task to avoid interim interrupts piling up
		// while the first interrupt is still being handled.
		enable_navlico_fsm_gpio_interrupts();
		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		update_navlico_fsm_state( false );
	}
}

/// \file main.c
/// Implements the Finite State Machine (FSM) handle the operational state and the associated GPIOs.
///
/// In order to keep this file clean and focused on the actual FSM some aspects are moved out to dedicated source files:
/// - `navlico_fsm_gpio_setup.c`: contains the (lengthy) boilerplate code to correctly set up the GPIOs
/// - `navlico_fsm_intr.c`: contains the interrupt-related code

#include "navlico_fsm.h"
#include "sdkconfig.h"
#include <esp_attr.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unistd.h>

const char NAVLICO_FSM_TAG[] = "navlico_fsm";

/// The active operational state
static RTC_DATA_ATTR navlico_fsm_state_t navlico_fsm_state = INVALID_STATE;

// Defined in `navlicao_fsm_gpio_setup.c`
void setup_navlico_fsm_gpio();
// Defined in `navlicao_fsm_intr.c`
void setup_navlico_fsm_isr( void );
void enable_navlico_fsm_gpio_interrupts( void );

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
		for ( navlico_fsm_state_t s = 0; s < STATE_COUNT; ++s ) {
			if ( GPIO_MASK( navlico_fsm_buttons[navlico_fsm_states[s].button].gpio_num ) & wakeup_pin_mask )
				return s;
		}
		ESP_LOGE( NAVLICO_FSM_TAG, "Unable to determine GPIO which caused wake-up from deep sleep (pin mask = 0x%.16" PRIx64 ")", wakeup_pin_mask );
		return INVALID_STATE;
	}

	ESP_LOGI( NAVLICO_FSM_TAG, "Came out of cold boot; simulating OFF button had been pressed" );
	return OFF_STATE;
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

	uint_fast8_t buttonLevels[ BTN_COUNT ];
	ESP_LOGI( NAVLICO_FSM_TAG, "Reading input pins" );
	// Repeated readings to debounce
	usleep( initialDebounceDelay );
	for ( uint_fast8_t i = 0; i < debounceProbes; ++i ) {
		for ( navlico_fsm_button_t btn = 0; btn < BTN_COUNT; ++btn ) {
			buttonLevels[btn] += ( gpio_get_level( navlico_fsm_buttons[btn].gpio_num ) == navlico_fsm_buttons[btn].active_level );
		}
		usleep( inbetweenDebounceDelay );
	}
	for ( navlico_fsm_state_t s = 0; s < STATE_COUNT; ++s ) {
		if ( buttonLevels[ navlico_fsm_states[s].button ] > debounceProbes / 2 )
			return s;
	}
	ESP_LOGE( NAVLICO_FSM_TAG, "Unable to determine active input GPIO" );
	return INVALID_STATE;
}

/**
 * Checks whether any of the button inputs is active
 *
 * @return True, if any of the button inputs is active; false otherwise
 */
bool static has_navlico_fsm_active_input( void ) {
	for ( navlico_fsm_button_t btn = 0; btn < BTN_COUNT; ++btn ) {
		if ( gpio_get_level( navlico_fsm_buttons[btn].gpio_num ) == navlico_fsm_buttons[btn].active_level )
			return true;
	}
	return false;
}

/**
 * Waits until all input pins have become idle
 */
void static wait_for_navlico_fsm_idle_input( void ) {
	while ( has_navlico_fsm_active_input() ) {
		vTaskDelay( pdMS_TO_TICKS( 10 ) );
	}
}

/**
 * Writes the output pins according to the current operational state.
 *
 * This function uses the currently stored operational state in #operational_state to set the output pins.
 */
void static write_navlico_fsm_output_pins( navlico_fsm_state_t const state ) {
	// Deactivate all indicator and lights
	for ( navlico_fsm_indicator_t i = 0; i < IND_COUNT; ++i )
		gpio_set_level( navlico_fsm_indicators[i].gpio_num, 1 - navlico_fsm_indicators[i].active_level );
	for ( navlico_fsm_light_t l = 0; l < LIGHT_COUNT; ++l )
		gpio_set_level( navlico_fsm_lights[l].gpio_num, 1 - navlico_fsm_lights[l].active_level );

	// TODO: We should do something else here and conspicuously indicate this error condition instead of just pretending to be in the "OFF" state.
	if ( state == INVALID_STATE )
		return;

	navlico_fsm_indicator_t const i = navlico_fsm_states[ state ].indicator;
	navlico_fsm_light_t const l0 = navlico_fsm_states[ state ].lights[0];
	navlico_fsm_light_t const l1 = navlico_fsm_states[ state ].lights[1];

	if ( i != INVALID_IND )
		gpio_set_level( navlico_fsm_indicators[i].gpio_num, navlico_fsm_indicators[i].active_level );
	if ( l0 != INVALID_LIGHT )
		gpio_set_level( navlico_fsm_lights[l0].gpio_num, navlico_fsm_lights[l0].active_level );
	if ( l1 != INVALID_LIGHT )
		gpio_set_level( navlico_fsm_lights[l1].gpio_num, navlico_fsm_lights[l1].active_level );
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
void static update_navlico_fsm_state( bool const firstRun ) {
	navlico_fsm_state = INVALID_STATE;
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
	setup_navlico_fsm_gpio();
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

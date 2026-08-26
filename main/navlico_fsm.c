/// \file main.c
/// Implements the Finite State Machine (FSM) handle the operational state and the associated GPIOs.
///
/// In order to keep this file clean and focused on the actual FSM some aspects are moved out to dedicated source files:
/// - `navlico_fsm_gpio_defs.c`: contains the static definitions of GPIOs, buttons, indicators and lights
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
#include <driver/rtc_io.h>

const char NAVLICO_FSM_TAG[] = "navlico_fsm";

/// The active operational state
static RTC_DATA_ATTR navlico_fsm_state_t const * navlico_fsm_state = nullptr;

// Defined in `navlicao_fsm_gpio_setup.c`
void setup_navlico_fsm_gpio();
void set_navlico_fsm_gpio_wakeup( navlico_fsm_gpio_t const * ignored_gpio, bool prepare_for_deep_sleep );
// Defined in `navlicao_fsm_intr.c`
void setup_navlico_fsm_isr( void );
void enable_navlico_fsm_gpio_interrupts( navlico_fsm_gpio_t const * ignored_gpio );

/**
 * Reads the input pins and returns the currently or most recently pressed button.
 *
 * This function is called whenever the inputs should be handled:
 *  - after cold boot
 *  - after waking-up from deep sleep
 *
 * @return The button which was pressed to trigger the wake-up from deep sleep
 */
static navlico_fsm_button_t const * read_navlico_fsm_input_pins_after_start() {
	uint32_t const wakeup_causes = esp_sleep_get_wakeup_causes();
	ESP_LOGI( NAVLICO_FSM_TAG, "Reading input pins (wakeup_causes = 0x%.8" PRIx32 ")", wakeup_causes );

	if ( wakeup_causes & BIT( ESP_SLEEP_WAKEUP_EXT1 ) ) {
		ESP_LOGI( NAVLICO_FSM_TAG, "Woke up from deep sleep" );
		uint64_t const wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
		for ( navlico_fsm_button_tag_t b = 0; b < BTN_COUNT; ++b ) {
			if ( GPIO_MASK( navlico_fsm_buttons[b].gpio->num ) & wakeup_pin_mask )
				return &navlico_fsm_buttons[b];
		}
		ESP_LOGE( NAVLICO_FSM_TAG, "Unable to determine GPIO which caused wake-up from deep sleep (pin mask = 0x%.16" PRIx64 ")", wakeup_pin_mask );
		return nullptr;
	}

	ESP_LOGI( NAVLICO_FSM_TAG, "Came out of cold boot; simulating OFF button had been pressed" );
	return &navlico_fsm_buttons[OFF_BTN];
}

/**
 * Reads the input pins and returns the currently pressed button.
 *
 * When finding the currently pressed button, this function skips `ignored_gpio`.
 *
 * This function is called whenever the inputs should be handled:
 *  - after waking up from light sleep
 *  - during normal runtime
 *  - after the interrupt-service routine (ISR) notified this task
 *
 * @param ignored_gpio GPIO to ignore when finding the pressed button
 * @return The currently or most recently pressed button.
 */
static navlico_fsm_button_t const * read_navlico_fsm_input_pins( navlico_fsm_gpio_t const * ignored_gpio ) {
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

	uint_fast8_t btn_levels[ BTN_COUNT ];
	for ( navlico_fsm_button_tag_t b = 0; b < BTN_COUNT; ++b )
		btn_levels[b] = 0;
	ESP_LOGI( NAVLICO_FSM_TAG, "Reading input pins" );
	// Repeated readings to debounce
	usleep( initialDebounceDelay );
	for ( uint_fast8_t i = 0; i < debounceProbes; ++i ) {
		for ( navlico_fsm_button_tag_t b = 0; b < BTN_COUNT; ++b ) {
			navlico_fsm_gpio_t const * gpio = navlico_fsm_buttons[b].gpio;
			if ( gpio == ignored_gpio ) continue;
			btn_levels[b] +=
				gpio_get_level( gpio->num ) == gpio->active_level;
		}
		usleep( inbetweenDebounceDelay );
	}
#ifdef CONFIG_NAVLICO_HAS_VERBOSE_OUTPUT
	ESP_LOGD( NAVLICO_FSM_TAG, "│ GPIO # │ GPIO Label │ Button # │ Button Label  │ Level │ Active │ Ignored │" );
	ESP_LOGD( NAVLICO_FSM_TAG, "├────────┼────────────┼──────────┼───────────────┼───────┼────────┼─────────┤" );
	for ( navlico_fsm_button_tag_t b = 0; b < BTN_COUNT; ++b ) {
		navlico_fsm_gpio_t const * const gpio = navlico_fsm_buttons[b].gpio;
		ESP_LOGD(
			NAVLICO_FSM_TAG,
			"│ %6.1d │ %-10.10s │ %8.1" PRIdFAST8 " │ %-13.13s │   %1.1d   │    %c   │    %c    │",
			gpio->num,
			gpio->label,
			b,
			navlico_fsm_buttons[b].label,
			btn_levels[b],
			btn_levels[b] > debounceProbes / 2 ? 'x' : ' ',
			gpio == ignored_gpio ? 'x' : ' '
		);
	}
#endif
	for ( navlico_fsm_button_tag_t b = 0; b < BTN_COUNT; ++b ) {
		// We take the first button for which more than half of the probes indicated an active GPIO
		if ( btn_levels[b] > debounceProbes / 2 )
			return &navlico_fsm_buttons[b];
	}
	ESP_LOGE( NAVLICO_FSM_TAG, "Unable to determine active input GPIO" );
	return nullptr;
}

/**
 * Checks whether any of the button inputs but the ignored one is active
 *
 * @param ignored_gpio The GPIO which shall be ignored when determining whether any button is active
 * @return True, if any of the button inputs is active; false otherwise
 */
bool static has_navlico_fsm_active_input( navlico_fsm_gpio_t const * ignored_gpio ) {
	for ( navlico_fsm_button_tag_t btn = 0; btn < BTN_COUNT; ++btn ) {
		if ( navlico_fsm_buttons[btn].gpio == ignored_gpio ) continue;
		if ( gpio_get_level( navlico_fsm_buttons[btn].gpio->num ) == navlico_fsm_buttons[btn].gpio->active_level )
			return true;
	}
	return false;
}

#if CONFIG_NAVLICO_HAS_VERBOSE_OUTPUT
void static dump_navlico_fsm_input_state( navlico_fsm_gpio_t const * ignored_gpio ) {
	ESP_LOGD( NAVLICO_FSM_TAG, "Dumping input state ... " );
	ESP_LOGD( NAVLICO_FSM_TAG, "│ GPIO # │ GPIO Label │ Button # │ Button Label  │ Level │ Active │ Ignored │" );
	ESP_LOGD( NAVLICO_FSM_TAG, "├────────┼────────────┼──────────┼───────────────┼───────┼────────┼─────────┤" );
	for ( navlico_fsm_button_tag_t btn = 0; btn < BTN_COUNT; ++btn ) {
		navlico_fsm_gpio_t const * const gpio = navlico_fsm_buttons[btn].gpio;
		int const level = gpio_get_level( gpio->num );
		ESP_LOGD(
			NAVLICO_FSM_TAG,
			"│ %6.1d │ %-10.10s │ %8.1" PRIdFAST8 " │ %-13.13s │   %1.1d   │    %c   │    %c    │",
			gpio->num,
			gpio->label,
			btn,
			navlico_fsm_buttons[btn].label,
			level,
			level == gpio->active_level ? 'x' : ' ',
			gpio == ignored_gpio ? 'x' : ' '
		);
	}
	ESP_LOGD( NAVLICO_FSM_TAG, "Dumping input state ... finished" );
}
#endif

/**
 * Waits until all input pins but the ignored one have become idle
 *
 * @param ignored_gpio The GPIO which shall be ignored when waiting for all pins to become idle
 */
void static wait_for_navlico_fsm_idle_input( navlico_fsm_gpio_t const * ignored_gpio ) {
#if CONFIG_NAVLICO_HAS_VERBOSE_OUTPUT
	unsigned int counter = 0;
	while ( has_navlico_fsm_active_input( ignored_gpio ) ) {
		vTaskDelay( pdMS_TO_TICKS( 10 ) );
		if ( counter % 200 == 0 ) {
			dump_navlico_fsm_input_state( ignored_gpio );
			counter = 1;
		} else {
			counter++;
		}
	}
#else
	while ( has_navlico_fsm_active_input() ) {
		vTaskDelay( pdMS_TO_TICKS( 10 ) );
	}
#endif
}

/**
 * Writes the output pins according to the current operational state.
 *
 * This function uses the currently stored operational state in #operational_state to set the output pins.
 */
void static write_navlico_fsm_output_pins( navlico_fsm_state_t const * const state ) {
	ESP_LOGI( NAVLICO_FSM_TAG, "Writing output pins" );
	// Deactivate all indicator and lights
	for ( navlico_fsm_indicator_tag_t i = 0; i < IND_COUNT; ++i )
		ESP_ERROR_CHECK( gpio_set_level( navlico_fsm_indicators[i].gpio->num, 1 - navlico_fsm_indicators[i].gpio->active_level ) );
	for ( navlico_fsm_light_tag_t l = 0; l < LIGHT_COUNT; ++l )
		ESP_ERROR_CHECK( gpio_set_level( navlico_fsm_lights[l].gpio->num, 1 - navlico_fsm_lights[l].gpio->active_level ) );

	// TODO: We should do something else here and conspicuously indicate this error condition instead of just pretending to be in the "OFF" state.
	if ( state == nullptr )
		return;

#if CONFIG_NAVLICO_HAS_VERBOSE_OUTPUT
	uint64_t const mask =
		( state->indicator ? GPIO_MASK( state->indicator->gpio->num ) : 0ULL ) |
		( state->lights[0] ? GPIO_MASK( state->lights[0]->gpio->num ) : 0ULL ) |
		( state->lights[1] ? GPIO_MASK( state->lights[1]->gpio->num ) : 0ULL );
	esp_log_level_t const level = esp_log_level_get( NAVLICO_FSM_TAG );
	if ( level == ESP_LOG_DEBUG || level == ESP_LOG_VERBOSE )
		gpio_dump_io_configuration( stdout, mask );
#endif

	if ( state->indicator ) {
		navlico_fsm_gpio_t const * const gpio = state->indicator->gpio;
		ESP_LOGD(
			NAVLICO_FSM_TAG, "Setting indictor %d (\"%s\") on GPIO %d (\"%s\") to level %d",
			state->indicator->tag, state->indicator->label, gpio->num, gpio->label, gpio->active_level
		);
		if ( rtc_gpio_is_valid_gpio( gpio->num ) ) {
			ESP_ERROR_CHECK( rtc_gpio_hold_dis( gpio->num ) );
			ESP_ERROR_CHECK( rtc_gpio_deinit( gpio->num ) );
		}
		ESP_ERROR_CHECK( gpio_set_level( gpio->num, gpio->active_level ) );
	}
	if ( state->lights[0] ) {
		navlico_fsm_gpio_t const * const gpio = state->lights[0]->gpio;
		ESP_LOGD(
			NAVLICO_FSM_TAG, "Setting light %d (\"%s\") on GPIO %d (\"%s\") to level %d",
			state->lights[0]->tag, state->lights[0]->label, gpio->num, gpio->label, gpio->active_level
		);
		if ( rtc_gpio_is_valid_gpio( gpio->num ) ) {
			ESP_ERROR_CHECK( rtc_gpio_hold_dis( gpio->num ) );
			ESP_ERROR_CHECK( rtc_gpio_deinit( gpio->num ) );
		}
		ESP_ERROR_CHECK( gpio_set_level( gpio->num, gpio->active_level ) );
	}
	if ( state->lights[1] ) {
		navlico_fsm_gpio_t const * const gpio = state->lights[1]->gpio;
		ESP_LOGD(
			NAVLICO_FSM_TAG, "Setting light %d (\"%s\") on GPIO %d (\"%s\") to level %d",
			state->lights[1]->tag, state->lights[1]->label, gpio->num, gpio->label, gpio->active_level
		);
		if ( rtc_gpio_is_valid_gpio( gpio->num ) ) {
			ESP_ERROR_CHECK( rtc_gpio_hold_dis( gpio->num ) );
			ESP_ERROR_CHECK( rtc_gpio_deinit( gpio->num ) );
		}
		ESP_ERROR_CHECK( gpio_set_level( gpio->num, gpio->active_level ) );
	}
}

/**
 * Returns whether the FSM is ready for deep sleep
 *
 * @return True if the FSM is ready for deep sleep
 */
bool is_navlico_fsm_deep_sleep_ready( void ) {
	return navlico_fsm_state == &navlico_fsm_states[OFF_STATE];
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
	navlico_fsm_state_t const * const prev_state = navlico_fsm_state;
	navlico_fsm_state = nullptr;
	navlico_fsm_button_t const * const button = firstRun ?
		read_navlico_fsm_input_pins_after_start() :
		read_navlico_fsm_input_pins( prev_state ? prev_state->button->gpio : nullptr );
	navlico_fsm_state_t const * const new_state = button ? button->state : nullptr;
	write_navlico_fsm_output_pins( new_state );
	ESP_LOGI( NAVLICO_FSM_TAG, "New state is: %s", new_state->label );
	wait_for_navlico_fsm_idle_input( new_state->button->gpio );
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
		// Some buttons and navigational lights share a combined input/output line as peers.
		// When the FSM is in a state which drives such a GPIO,
		// then that GPIO must not be ignored as a wake-up and interrupt source
		// as the wake-up source or interrupt would immediately trigger.
		navlico_fsm_gpio_t const * const ignored_gpio = navlico_fsm_state ? navlico_fsm_state->button->gpio : nullptr;
		// We have to (re-)enable the interrupts each time as the ISR disables the interrupts
		// before it notifies the task to avoid interim interrupts piling up
		// while the first interrupt is still being handled.
		enable_navlico_fsm_gpio_interrupts( ignored_gpio );
		// The wake-up source are not disabled, but we must (re-)set them as the ignored button may have changed.
		set_navlico_fsm_gpio_wakeup( ignored_gpio, is_navlico_fsm_deep_sleep_ready() );
		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
		update_navlico_fsm_state( false );
	}
}

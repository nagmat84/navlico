/// \file navlico_fsm.h
/// General declarations for Navlico's Finite-State Machine.

#ifndef NAVLICO_NAVLICO_FSM_H
#define NAVLICO_NAVLICO_FSM_H

#include "sdkconfig.h"
#include <driver/gpio.h>

#define GPIO_MASK( gpio_num ) ( 1ULL << gpio_num )

extern const char NAVLICO_FSM_TAG[];

/**
 * The operational state
 *
 * The operational state directly corresponds to the most recently pressed button and active indicator light.
 *
 * @internal The ordering of the enum must be kept in sync with the definition of the array navlico_fsm_states
 */
typedef enum navlico_fsm_state_t_impl : uint_fast8_t {
	OFF_STATE,           ///< The user has pressed the OFF button, the operational state is OFF
	SAILING_STATE,       ///< The user has pressed the SAILING button, the operational state is SAILING
	DRIVING_STATE,       ///< The user has pressed the DRIVING button, the operational state is DRIVING
	ANCHORING_STATE,     ///< The user has pressed the ANCHORING button, the operational state is ANCHORING
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	SAILING_COAST_STATE, ///< The user has pressed the SAILING COAST button, the operational state is SAILING COAST
	DISABLED_STATE,      ///< The user has pressed the DISABLED button, the operational state is DISABLED
#endif
	STATE_COUNT,
	INVALID_STATE = STATE_COUNT
} navlico_fsm_state_t;

/**
 * The buttons
 *
 * A button directly corresponds to the selected operational state.
 *
 * @internal The ordering of the enum must be kept in sync with the definition of the array navlico_fsm_buttons.
 */
typedef enum navlico_fsm_button_t_impl : uint_fast8_t {
	OFF_BTN,
	SAILING_BTN,
	DRIVING_BTN,
	ANCHORING_BTN,
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	SAILING_COAST_BTN,
	DISABLED_BTN,
#endif
	BTN_COUNT,
	INVALID_BTN = BTN_COUNT
} navlico_fsm_button_t;

/**
 * The indicator lights
 *
 * An indicator light directly corresponds to the active operational state.
 * There is no indicator light for the "OFF" state.
 *
 * @internal The ordering of the enum must be kept in sync with the definition of the array navlico_fsm_indicators.
 */
typedef enum navlico_fsm_indicator_t_impl : uint_fast8_t {
	SAILING_IND,
	DRIVING_IND,
	ANCHORING_IND,
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	SAILING_COAST_IND,
	DISABLED_IND,
#endif
	IND_COUNT,
	INVALID_IND = IND_COUNT
} navlico_fsm_indicator_t;

/**
 * The navigational lights
 *
 * Zero, one or several navigational lights are active depending on the operational state.
 *
 * @internal The ordering of the enum must be kept in sync with the definition of the array navlico_fsm_lights.
 */
typedef enum navlico_fsm_light_t_impl : uint_fast8_t {
	SIDE_N_STERN_LIGHT,
	MASTHEAD_LIGHT,
	ALLROUND_WHITE_LIGHT,
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	ALLROUND_GREEN_LIGHT,
	ALLROUND_RED_1_LIGHT,
	ALLROUND_RED_2_LIGHT,
#endif
	LIGHT_COUNT,
	INVALID_LIGHT = LIGHT_COUNT
} navlico_fsm_light_t;

/**
 * Defines the configuration of a single GPIO
 */
typedef struct navlico_fsm_gpio_definition_t_impl navlico_fsm_gpio_definition_t;

/**
 * Internal definition of navlico_fsm_gpio_definition_t
 *
 * @see navlico_fsm_gpio_definition_t
 */
struct navlico_fsm_gpio_definition_t_impl {
	gpio_num_t gpio_num; ///< The number of GPIO
	gpio_mode_t gpio_mode; ///< The operational mode of the GPIO: either `GPIO_MODE_INPUT`, `GPIO_MODE_INPUT_OUTPUT_OD`, `GPIO_MODE_OUTPUT` or `GPIO_MODE_OUTPUT_OD`.
	uint32_t active_level; ///< Determines whether the pin is active high or active low: 1 = active high, 0 = active low
	navlico_fsm_gpio_definition_t const * peer; ///< Points to the peer pin for a combined input/output pin, i.e. if `gpio_mode == GPIO_MODE_INPUT_OUTPUT_OD`; `nullptr` otherwise
};

/**
 * Defines an operational state
 *
 * A state is defined by
 * - the button which triggered the state
 * - the indicator which signals the state
 * - a set of navigational lights which are active.
 */
typedef struct navlico_fsm_state_definition_t_impl {
	navlico_fsm_button_t button;             ///< The button which triggered the state
	navlico_fsm_indicator_t indicator;       ///< The indicator which signals the state
	/**
	 * The navigational lights which are active in the state
	 *
	 * The maximum number of lights which can be simultaneously be active for a single state is statically limited to 2.
	 * Unused array entries are set to `INVALID_LIGHT`.
	 */
	navlico_fsm_light_t lights[2];
} navlico_fsm_state_definition_t;

/**
 * Defines the pin configuration for the buttons
 *
 * All buttons are active low.
 * The operational mode is either `GPIO_MODE_INPUT` or `GPIO_MODE_INPUT_OUTPUT_OD` if the button shares a signal line
 * with a navigational light.
 *
 * @internal The ordering of this array must be kept in sync with the definition of the enum navlico_fsm_button_t.
 */
extern const navlico_fsm_gpio_definition_t navlico_fsm_buttons[ BTN_COUNT ];

/**
 * Defines the pin configuration for the indicators
 *
 * All indicators are active high.
 * The operational mode is always `GPIO_MODE_OUTPUT` (i.e. push-pull mode) due to optional PWM dimming of the indicators.
 * Indicators use low-side switching via an NMOS.
 * Pins for indicators never share a signal line with a peer as otherwise PWM dimming wouldn't be possible.
 *
 * @internal The ordering of this array must be kept in sync with the definition of the enum navlico_fsm_indicator_t.
 */
extern const navlico_fsm_gpio_definition_t navlico_fsm_indicators[ IND_COUNT ];

/**
 * Defines the pin configuration for the navigational lights.
 *
 * All lights are active low.
 * The operational mode is either `GPIO_MODE_OUTPUT_OD` or `GPIO_MODE_INPUT_OUTPUT_OD` if the navigational light shares
 * a signal line with a button.
 * Navigational lights use high-side switching via a PMOS.
 *
 * @internal The ordering of this array must be kept in sync with the definition of the enum navlico_fsm_light_t.
 */
extern const navlico_fsm_gpio_definition_t navlico_fsm_lights[ LIGHT_COUNT ];

/**
 * Defines the states.
 *
 * @internal The ordering of this array must be kept in sync with the definition of the enum navlico_fsm_state_t.
 */
extern const navlico_fsm_state_definition_t navlico_fsm_states[ STATE_COUNT ];

navlico_fsm_state_t get_navlico_fsm_state( void );

void navlico_fsm_task( void *args );

#endif //NAVLICO_NAVLICO_FSM_H

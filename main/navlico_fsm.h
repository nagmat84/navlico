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
typedef enum navlico_fsm_state_id_t : uint_fast8_t {
	OFF_STATE,           ///< The user has pressed the OFF button, the operational state is OFF
	SAILING_STATE,       ///< The user has pressed the SAILING button, the operational state is SAILING
	DRIVING_STATE,       ///< The user has pressed the DRIVING button, the operational state is DRIVING
	ANCHORING_STATE,     ///< The user has pressed the ANCHORING button, the operational state is ANCHORING
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	SAILING_COAST_STATE, ///< The user has pressed the SAILING COAST button, the operational state is SAILING COAST
	DISABLED_STATE,      ///< The user has pressed the DISABLED button, the operational state is DISABLED
#endif
	STATE_COUNT
} navlico_fsm_state_id_t;

typedef enum navlico_fsm_gpio_id_t : uint_fast8_t {
	GPIO_0,
	GPIO_1,
	GPIO_2,
	GPIO_3,
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	GPIO_4,
#endif
	GPIO_5,
	GPIO_10,
	GPIO_11,
	GPIO_12,
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	GPIO_13,
	GPIO_14,
	GPIO_22,
	GPIO_25,
#endif
	GPIO_COUNT
} navlico_fsm_gpio_id_t;

/**
 * The buttons
 *
 * A button directly corresponds to the selected operational state.
 *
 * @internal The ordering of the enum must be kept in sync with the definition of the array navlico_fsm_buttons.
 */
typedef enum navlico_fsm_button_id_t : uint_fast8_t {
	OFF_BTN,
	SAILING_BTN,
	DRIVING_BTN,
	ANCHORING_BTN,
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	SAILING_COAST_BTN,
	DISABLED_BTN,
#endif
	BTN_COUNT
} navlico_fsm_button_id_t;

/**
 * The indicator lights
 *
 * An indicator light directly corresponds to the active operational state.
 * There is no indicator light for the "OFF" state.
 *
 * @internal The ordering of the enum must be kept in sync with the definition of the array navlico_fsm_indicators.
 */
typedef enum navlico_fsm_indicator_id_t : uint_fast8_t {
	SAILING_IND,
	DRIVING_IND,
	ANCHORING_IND,
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	SAILING_COAST_IND,
	DISABLED_IND,
#endif
	IND_COUNT
} navlico_fsm_indicator_id_t;

/**
 * The navigational lights
 *
 * Zero, one or several navigational lights are active depending on the operational state.
 *
 * @internal The ordering of the enum must be kept in sync with the definition of the array navlico_fsm_lights.
 */
typedef enum navlico_fsm_light_id_t : uint_fast8_t {
	SIDE_N_STERN_LIGHT,
	MASTHEAD_LIGHT,
	ALLROUND_WHITE_LIGHT,
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	ALLROUND_GREEN_LIGHT,
	ALLROUND_RED_1_LIGHT,
	ALLROUND_RED_2_LIGHT,
#endif
	LIGHT_COUNT
} navlico_fsm_light_id_t;

typedef struct navlico_fsm_gpio_t navlico_fsm_gpio_t;
typedef struct navlico_fsm_button_t navlico_fsm_button_t;
typedef struct navlico_fsm_indicator_t navlico_fsm_indicator_t;
typedef struct navlico_fsm_light_t navlico_fsm_light_t;
typedef struct navlico_fsm_state_t navlico_fsm_state_t;

/**
 * Defines the configuration of a single GPIO
 *
 * A GPIO is defined by its
 * - id
 * - (pin) number
 * - mode
 * - whether it is active-high or active-low
 */
struct navlico_fsm_gpio_t {
	navlico_fsm_gpio_id_t id; ///< The id of the GPIO definition
	gpio_num_t num; ///< The number of GPIO
	gpio_mode_t mode; ///< The operational mode of the GPIO: either `GPIO_MODE_INPUT`, `GPIO_MODE_INPUT_OUTPUT_OD`, `GPIO_MODE_OUTPUT` or `GPIO_MODE_OUTPUT_OD`.
	uint32_t active_level; ///< Determines whether the pin is active high or active low: 1 = active high, 0 = active low
};

/**
 * Defines a button
 *
 * The associated GPIO definition of a button
 * - is always active low
 * - has a mode which is either `GPIO_MODE_INPUT` or `GPIO_MODE_INPUT_OUTPUT_OD` if the button shares a signal line
 *   with a navigational light.
 */
struct navlico_fsm_button_t {
	navlico_fsm_button_id_t id; ///< The id of the button definition
	char const * const label; ///< A printable name
	navlico_fsm_gpio_t const * gpio; ///< The associated GPIO configuration
	navlico_fsm_state_t const * state; ///< The state which this button triggers
};

/**
 * Defines an indicator
 *
 * The associated GPIO definition of an indicator
 * - is always active high
 * - has mode `GPIO_MODE_OUTPUT` (i.e. push-pull mode) due to optional PWM dimming
 */
struct navlico_fsm_indicator_t {
	navlico_fsm_indicator_id_t id; ///< The id of the indicator definition
	char const * const label; ///< A printable name
	navlico_fsm_gpio_t const * gpio; ///< The associated GPIO configuration
	navlico_fsm_state_t const * state; ///< The state which this indicator signals
};

/**
 * Defines a navigational light
 *
 * The associated GPIO definition of a navigational light
 * - is always active low
 * - has a mode which is either `GPIO_MODE_OUTPUT_OD` or `GPIO_MODE_INPUT_OUTPUT_OD` if the navigational light shares
 *   a signal line with a button.
 */
struct navlico_fsm_light_t {
	navlico_fsm_light_id_t id; ///< The id of the light definition
	char const * const label; ///< A printable name
	navlico_fsm_gpio_t const * gpio; ///< The associated GPIO configuration
};

/**
 * Defines an operational state
 *
 * A state is defined by
 * - the button which triggered the state
 * - the indicator which signals the state
 * - a set of navigational lights which are active.
 */
struct navlico_fsm_state_t {
	navlico_fsm_state_id_t id; ///< The id of the state definition
	char const * const label; ///< A printable name
	navlico_fsm_button_t const * button; ///< The button which triggered the state
	navlico_fsm_indicator_t const * indicator; ///< The indicator which signals the state
	/**
	 * The navigational lights which are active in the state
	 *
	 * The maximum number of lights which can be simultaneously be active for a single state is statically limited to 2.
	 * Unused array entries are set to `nulltptr`.
	 */
	navlico_fsm_light_t const * lights[2];
};

/**
 * Defines the GPIO configuration
 */
extern const navlico_fsm_gpio_t navlico_fsm_gpios[ GPIO_COUNT ];

/**
 * Defines the buttons
 *
 * All buttons are active low.
 * The mode is either `GPIO_MODE_INPUT` or `GPIO_MODE_INPUT_OUTPUT_OD` if the button shares a signal line
 * with a navigational light.
 *
 * @internal The ordering of this array must be kept in sync with the definition of the enum navlico_fsm_button_t.
 */
extern const navlico_fsm_button_t navlico_fsm_buttons[ BTN_COUNT ];

/**
 * Defines the indicators
 *
 * All indicators are active high.
 * The mode is always `GPIO_MODE_OUTPUT` (i.e. push-pull mode) due to optional PWM dimming of the indicators.
 * Indicators use low-side switching via an NMOS.
 * Pins for indicators never share a signal line as otherwise PWM dimming wouldn't be possible.
 *
 * @internal The ordering of this array must be kept in sync with the definition of the enum navlico_fsm_indicator_t.
 */
extern const navlico_fsm_indicator_t navlico_fsm_indicators[ IND_COUNT ];

/**
 * Defines the navigational lights.
 *
 * All lights are active low.
 * The mode is either `GPIO_MODE_OUTPUT_OD` or `GPIO_MODE_INPUT_OUTPUT_OD` if the navigational light shares
 * a signal line with a button.
 * Navigational lights use high-side switching via a PMOS.
 *
 * @internal The ordering of this array must be kept in sync with the definition of the enum navlico_fsm_light_t.
 */
extern const navlico_fsm_light_t navlico_fsm_lights[ LIGHT_COUNT ];

/**
 * Defines the states.
 *
 * @internal The ordering of this array must be kept in sync with the definition of the enum navlico_fsm_state_t.
 */
extern const navlico_fsm_state_t navlico_fsm_states[ STATE_COUNT ];

bool is_navlico_fsm_deep_sleep_ready( void );

void navlico_fsm_task( void *args );

#endif //NAVLICO_NAVLICO_FSM_H

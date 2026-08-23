/// \file navlico_gpio_defs.h
/// Declares GPIO pin assignments for Navlico's Finite-State Machine (FSM).

#ifndef NAVLICO_GPIO_DEFS_H
#define NAVLICO_GPIO_DEFS_H

#include <driver/gpio.h>

#define GPIO_MASK( gpio_num ) ( 1ULL << gpio_num )

// Buttons

static constexpr gpio_num_t GPIO_OFF_BUTTON = GPIO_NUM_0;            // input-only
static constexpr gpio_num_t GPIO_SAILING_BUTTON = GPIO_NUM_10;       // input-only
static constexpr gpio_num_t GPIO_DRIVING_BUTTON = GPIO_NUM_11;       // combined input/output in open-drain mode (peer: MASTHEAD_LIGHT)
static constexpr gpio_num_t GPIO_ANCHORING_BUTTON = GPIO_NUM_12;     // combined input/output in open-drain mode (peer: ALLROUND_WHITE_LIGHT)
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
static constexpr gpio_num_t GPIO_SAILING_COAST_BUTTON = GPIO_NUM_13; // combined input/output in open-drain mode (peer: ALLROUND_GREEN_LIGHT)
static constexpr gpio_num_t GPIO_DISABLED_BUTTON = GPIO_NUM_14;      // combined input/output in open-drain mode (peer: ALLROUND_RED_2_LIGHT)
#endif

// Indicator Lights (all output-only in push-pull mode for PWM dimming)

static constexpr gpio_num_t GPIO_SAILING_INDICATOR = GPIO_NUM_1;       // output-only in push-pull mode
static constexpr gpio_num_t GPIO_DRIVING_INDICATOR = GPIO_NUM_2;       // output-only in push-pull mode
static constexpr gpio_num_t GPIO_ANCHORING_INDICATOR = GPIO_NUM_3;     // output-only in push-pull mode
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
static constexpr gpio_num_t GPIO_SAILING_COAST_INDICATOR = GPIO_NUM_4; // output-only in push-pull mode
static constexpr gpio_num_t GPIO_DISABLED_INDICATOR = GPIO_NUM_22;     // output-only in push-pull mode
#endif

// Navigational Lights

static constexpr gpio_num_t GPIO_SIDE_N_STERN_LIGHT = GPIO_NUM_5;    // output-only in open-drain mode
static constexpr gpio_num_t GPIO_MASTHEAD_LIGHT = GPIO_NUM_11;       // combined input/output in open-drain mode (peer: DRIVING_BUTTON)
static constexpr gpio_num_t GPIO_ALLROUND_WHITE_LIGHT = GPIO_NUM_12; // combined input/output in open-drain mode (peer: ANCHORING_BUTTON)
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
static constexpr gpio_num_t GPIO_ALLROUND_GREEN_LIGHT = GPIO_NUM_13; // combined input/output in open-drain mode (peer: SAILING_COAST_BUTTON)
static constexpr gpio_num_t GPIO_ALLROUND_RED_1_LIGHT = GPIO_NUM_25;
static constexpr gpio_num_t GPIO_ALLROUND_RED_2_LIGHT = GPIO_NUM_14; // combined input/output in open-drain mode (peer: DISABLED_BUTTON)
#endif

// Masks

static constexpr uint64_t GPIO_INPUT_ONLY_MASK =
	GPIO_MASK( GPIO_OFF_BUTTON ) |
	GPIO_MASK( GPIO_SAILING_BUTTON );

static constexpr uint64_t GPIO_COMBINED_IO_OPEN_DRAIN_MASK =
	GPIO_MASK( GPIO_MASTHEAD_LIGHT ) |
	GPIO_MASK( GPIO_ALLROUND_WHITE_LIGHT ) |
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	GPIO_MASK( GPIO_ALLROUND_GREEN_LIGHT ) |
	GPIO_MASK( GPIO_ALLROUND_RED_2_LIGHT ) |
#endif
	0LL;

static constexpr uint64_t GPIO_OUTPUT_ONLY_PUSH_PULL_MASK =
	GPIO_MASK( GPIO_SAILING_INDICATOR ) |
	GPIO_MASK( GPIO_DRIVING_INDICATOR ) |
	GPIO_MASK( GPIO_ANCHORING_INDICATOR ) |
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	GPIO_MASK( GPIO_SAILING_COAST_INDICATOR ) |
	GPIO_MASK( GPIO_DISABLED_INDICATOR ) |
#endif
	0LL;

static constexpr uint64_t GPIO_OUTPUT_ONLY_OPEN_DRAIN_MASK =
	GPIO_MASK( GPIO_SIDE_N_STERN_LIGHT ) |
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	GPIO_MASK( GPIO_ALLROUND_RED_1_LIGHT ) |
#endif
	0LL;

static constexpr uint64_t GPIO_DEEP_SLEEP_WAKEUP_BUTTONS_MASK =
	GPIO_MASK( GPIO_SAILING_BUTTON ) |
	GPIO_MASK( GPIO_DRIVING_BUTTON ) |
	GPIO_MASK( GPIO_ANCHORING_BUTTON ) |
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	GPIO_MASK( GPIO_SAILING_COAST_BUTTON ) |
	GPIO_MASK( GPIO_DISABLED_BUTTON ) |
#endif
	0LL;

#endif //NAVLICO_GPIO_DEFS_H

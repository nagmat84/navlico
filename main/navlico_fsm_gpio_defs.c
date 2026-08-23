/// \file main.c
/// Defines GPIO functions of Navlico's Finite State Machine (FSM).

#include "navlico_fsm.h"
#include "sdkconfig.h"
#include <esp_attr.h>

const navlico_fsm_gpio_t navlico_fsm_gpios[ GPIO_COUNT ];
const navlico_fsm_button_t navlico_fsm_buttons[ BTN_COUNT ];
const navlico_fsm_indicator_t navlico_fsm_indicators[ IND_COUNT ];
const navlico_fsm_light_t navlico_fsm_lights[ LIGHT_COUNT ];
const navlico_fsm_state_t navlico_fsm_states[ STATE_COUNT ];

const DRAM_ATTR navlico_fsm_gpio_t navlico_fsm_gpios[ GPIO_COUNT ] = {
	{ .tag = GPIO_0,  .num = GPIO_NUM_0,  .mode = GPIO_MODE_INPUT,           .active_level = 0 },
	{ .tag = GPIO_1,  .num = GPIO_NUM_1,  .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
	{ .tag = GPIO_2,  .num = GPIO_NUM_2,  .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
	{ .tag = GPIO_3,  .num = GPIO_NUM_3,  .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .tag = GPIO_4,  .num = GPIO_NUM_4,  .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
#endif
	{ .tag = GPIO_5,  .num = GPIO_NUM_5,  .mode = GPIO_MODE_OUTPUT_OD,       .active_level = 0 },
	{ .tag = GPIO_10, .num = GPIO_NUM_10, .mode = GPIO_MODE_INPUT,           .active_level = 0 },
	{ .tag = GPIO_11, .num = GPIO_NUM_11, .mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	{ .tag = GPIO_12, .num = GPIO_NUM_12, .mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .tag = GPIO_13, .num = GPIO_NUM_13, .mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	{ .tag = GPIO_14, .num = GPIO_NUM_14, .mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	{ .tag = GPIO_22, .num = GPIO_NUM_22, .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
	{ .tag = GPIO_25, .num = GPIO_NUM_25, .mode = GPIO_MODE_OUTPUT_OD,       .active_level = 0 }
#endif
};

const DRAM_ATTR navlico_fsm_button_t navlico_fsm_buttons[ BTN_COUNT ] = {
	{ .tag = OFF_BTN,           .gpio = &navlico_fsm_gpios[GPIO_0],  .state = &navlico_fsm_states[OFF_STATE] },
	{ .tag = SAILING_BTN,       .gpio = &navlico_fsm_gpios[GPIO_10], .state = &navlico_fsm_states[SAILING_STATE] },
	{ .tag = DRIVING_BTN,       .gpio = &navlico_fsm_gpios[GPIO_11], .state = &navlico_fsm_states[DRIVING_STATE] },
	{ .tag = ANCHORING_BTN,     .gpio = &navlico_fsm_gpios[GPIO_12], .state = &navlico_fsm_states[ANCHORING_STATE] },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .tag = SAILING_COAST_BTN, .gpio = &navlico_fsm_gpios[GPIO_13], .state = &navlico_fsm_states[SAILING_COAST_STATE] },
	{ .tag = DISABLED_BTN,      .gpio = &navlico_fsm_gpios[GPIO_14], .state = &navlico_fsm_states[DISABLED_STATE] },
#endif
};

const DRAM_ATTR navlico_fsm_indicator_t navlico_fsm_indicators[ IND_COUNT ] = {
	{ .tag = SAILING_IND,       .gpio = &navlico_fsm_gpios[GPIO_1],  .state = &navlico_fsm_states[SAILING_STATE] },
	{ .tag = DRIVING_IND,       .gpio = &navlico_fsm_gpios[GPIO_2],  .state = &navlico_fsm_states[DRIVING_STATE] },
	{ .tag = ANCHORING_IND,     .gpio = &navlico_fsm_gpios[GPIO_3],  .state = &navlico_fsm_states[ANCHORING_STATE] },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .tag = SAILING_COAST_IND, .gpio = &navlico_fsm_gpios[GPIO_4],  .state = &navlico_fsm_states[SAILING_COAST_STATE] },
	{ .tag = DISABLED_IND,      .gpio = &navlico_fsm_gpios[GPIO_22], .state = &navlico_fsm_states[DISABLED_STATE] },
#endif
};

const DRAM_ATTR navlico_fsm_light_t navlico_fsm_lights[ LIGHT_COUNT ] = {
	{ .tag = SIDE_N_STERN_LIGHT,   .gpio = &navlico_fsm_gpios[GPIO_5] },
	{ .tag = MASTHEAD_LIGHT,       .gpio = &navlico_fsm_gpios[GPIO_11] },
	{ .tag = ALLROUND_WHITE_LIGHT, .gpio = &navlico_fsm_gpios[GPIO_12] },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .tag = ALLROUND_GREEN_LIGHT, .gpio = &navlico_fsm_gpios[GPIO_13] },
	{ .tag = ALLROUND_RED_1_LIGHT, .gpio = &navlico_fsm_gpios[GPIO_25] },
	{ .tag = ALLROUND_RED_2_LIGHT, .gpio = &navlico_fsm_gpios[GPIO_14] },
#endif
};

const DRAM_ATTR navlico_fsm_state_t navlico_fsm_states[ STATE_COUNT ] = { {
	.tag = OFF_STATE,
	.button = &navlico_fsm_buttons[OFF_BTN],
	.indicator = nullptr,
	.lights = { nullptr, nullptr }
}, {
	.tag = SAILING_STATE,
	.button = &navlico_fsm_buttons[SAILING_BTN],
	.indicator = &navlico_fsm_indicators[SAILING_IND],
	.lights = { &navlico_fsm_lights[SIDE_N_STERN_LIGHT], nullptr }
}, {
	.tag = DRIVING_STATE,
	.button = &navlico_fsm_buttons[DRIVING_BTN],
	.indicator = &navlico_fsm_indicators[DRIVING_IND],
	.lights = { &navlico_fsm_lights[SIDE_N_STERN_LIGHT], &navlico_fsm_lights[MASTHEAD_LIGHT] }
}, {
	.tag = ANCHORING_STATE,
	.button = &navlico_fsm_buttons[ANCHORING_BTN],
	.indicator = &navlico_fsm_indicators[ANCHORING_IND],
	.lights = { &navlico_fsm_lights[ALLROUND_WHITE_LIGHT], nullptr }
},
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
{
	.tag = SAILING_COAST_STATE,
	.button = &navlico_fsm_buttons[SAILING_COAST_BTN],
	.indicator = &navlico_fsm_indicators[SAILING_COAST_IND],
	.lights = { &navlico_fsm_lights[ALLROUND_GREEN_LIGHT], &navlico_fsm_lights[ALLROUND_RED_1_LIGHT] }
}, {
	.tag = DISABLED_STATE,
	.button = &navlico_fsm_buttons[DISABLED_BTN],
	.indicator = &navlico_fsm_indicators[DISABLED_IND],
	.lights = { &navlico_fsm_lights[ALLROUND_RED_1_LIGHT], &navlico_fsm_lights[ALLROUND_RED_2_LIGHT] }
},
#endif
};

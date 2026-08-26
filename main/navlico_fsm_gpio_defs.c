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

static constexpr DRAM_ATTR char OFF_LABEL[] = "Off";
static constexpr DRAM_ATTR char SAILING_LABEL[] = "Sailing";
static constexpr DRAM_ATTR char DRIVING_LABEL[] = "Driving";
static constexpr DRAM_ATTR char ANCHORING_LABEL[] = "Anchoring";
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
static constexpr DRAM_ATTR char SAILING_COAST_LABEL[] = "Sailing Coast";
static constexpr DRAM_ATTR char DISABLED_LABEL[] = "Disabled";
#endif

static constexpr DRAM_ATTR char SIDE_N_STERN_LABEL[] = "Side & Stern";
static constexpr DRAM_ATTR char MASTHEAD_LABEL[] = "Masthead";
static constexpr DRAM_ATTR char ALLROUND_WHITE_LABEL[] = "Allround White";
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
static constexpr DRAM_ATTR char ALLROUND_GREEN_LABEL[] = "Allround Green";
static constexpr DRAM_ATTR char ALLROUND_RED_1_LABEL[] = "Allround Red 1";
static constexpr DRAM_ATTR char ALLROUND_RED_2_LABEL[] = "Allround Red 2";
#endif

const DRAM_ATTR navlico_fsm_gpio_t navlico_fsm_gpios[ GPIO_COUNT ] = {
	{ .id = GPIO_0,  .num = GPIO_NUM_0,  .mode = GPIO_MODE_INPUT,           .active_level = 0 },
	{ .id = GPIO_1,  .num = GPIO_NUM_1,  .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
	{ .id = GPIO_2,  .num = GPIO_NUM_2,  .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
	{ .id = GPIO_3,  .num = GPIO_NUM_3,  .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .id = GPIO_4,  .num = GPIO_NUM_4,  .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
#endif
	{ .id = GPIO_5,  .num = GPIO_NUM_5,  .mode = GPIO_MODE_OUTPUT_OD,       .active_level = 0 },
	{ .id = GPIO_10, .num = GPIO_NUM_10, .mode = GPIO_MODE_INPUT,           .active_level = 0 },
	{ .id = GPIO_11, .num = GPIO_NUM_11, .mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	{ .id = GPIO_12, .num = GPIO_NUM_12, .mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .id = GPIO_13, .num = GPIO_NUM_13, .mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	{ .id = GPIO_14, .num = GPIO_NUM_14, .mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	{ .id = GPIO_22, .num = GPIO_NUM_22, .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
	{ .id = GPIO_25, .num = GPIO_NUM_25, .mode = GPIO_MODE_OUTPUT_OD,       .active_level = 0 }
#endif
};

const DRAM_ATTR navlico_fsm_button_t navlico_fsm_buttons[ BTN_COUNT ] = {
	{ .id = OFF_BTN,           .label = OFF_LABEL,           .gpio = &navlico_fsm_gpios[GPIO_0],  .state = &navlico_fsm_states[OFF_STATE] },
	{ .id = SAILING_BTN,       .label = SAILING_LABEL,       .gpio = &navlico_fsm_gpios[GPIO_10], .state = &navlico_fsm_states[SAILING_STATE] },
	{ .id = DRIVING_BTN,       .label = DRIVING_LABEL,       .gpio = &navlico_fsm_gpios[GPIO_11], .state = &navlico_fsm_states[DRIVING_STATE] },
	{ .id = ANCHORING_BTN,     .label = ANCHORING_LABEL,     .gpio = &navlico_fsm_gpios[GPIO_12], .state = &navlico_fsm_states[ANCHORING_STATE] },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .id = SAILING_COAST_BTN, .label = SAILING_COAST_LABEL, .gpio = &navlico_fsm_gpios[GPIO_13], .state = &navlico_fsm_states[SAILING_COAST_STATE] },
	{ .id = DISABLED_BTN,      .label = DISABLED_LABEL,      .gpio = &navlico_fsm_gpios[GPIO_14], .state = &navlico_fsm_states[DISABLED_STATE] },
#endif
};

const DRAM_ATTR navlico_fsm_indicator_t navlico_fsm_indicators[ IND_COUNT ] = {
	{ .id = SAILING_IND,       .label = SAILING_LABEL,       .gpio = &navlico_fsm_gpios[GPIO_1],  .state = &navlico_fsm_states[SAILING_STATE] },
	{ .id = DRIVING_IND,       .label = DRIVING_LABEL,       .gpio = &navlico_fsm_gpios[GPIO_2],  .state = &navlico_fsm_states[DRIVING_STATE] },
	{ .id = ANCHORING_IND,     .label = ANCHORING_LABEL,     .gpio = &navlico_fsm_gpios[GPIO_3],  .state = &navlico_fsm_states[ANCHORING_STATE] },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .id = SAILING_COAST_IND, .label = SAILING_COAST_LABEL, .gpio = &navlico_fsm_gpios[GPIO_4],  .state = &navlico_fsm_states[SAILING_COAST_STATE] },
	{ .id = DISABLED_IND,      .label = DISABLED_LABEL,      .gpio = &navlico_fsm_gpios[GPIO_22], .state = &navlico_fsm_states[DISABLED_STATE] },
#endif
};

const DRAM_ATTR navlico_fsm_light_t navlico_fsm_lights[ LIGHT_COUNT ] = {
	{ .id = SIDE_N_STERN_LIGHT,   .label = SIDE_N_STERN_LABEL,   .gpio = &navlico_fsm_gpios[GPIO_5] },
	{ .id = MASTHEAD_LIGHT,       .label = MASTHEAD_LABEL,       .gpio = &navlico_fsm_gpios[GPIO_11] },
	{ .id = ALLROUND_WHITE_LIGHT, .label = ALLROUND_WHITE_LABEL, .gpio = &navlico_fsm_gpios[GPIO_12] },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .id = ALLROUND_GREEN_LIGHT, .label = ALLROUND_GREEN_LABEL, .gpio = &navlico_fsm_gpios[GPIO_13] },
	{ .id = ALLROUND_RED_1_LIGHT, .label = ALLROUND_RED_1_LABEL, .gpio = &navlico_fsm_gpios[GPIO_25] },
	{ .id = ALLROUND_RED_2_LIGHT, .label = ALLROUND_RED_2_LABEL, .gpio = &navlico_fsm_gpios[GPIO_14] },
#endif
};

const DRAM_ATTR navlico_fsm_state_t navlico_fsm_states[ STATE_COUNT ] = { {
	.id = OFF_STATE,
	.label = OFF_LABEL,
	.button = &navlico_fsm_buttons[OFF_BTN],
	.indicator = nullptr,
	.lights = { nullptr, nullptr }
}, {
	.id = SAILING_STATE,
	.label = SAILING_LABEL,
	.button = &navlico_fsm_buttons[SAILING_BTN],
	.indicator = &navlico_fsm_indicators[SAILING_IND],
	.lights = { &navlico_fsm_lights[SIDE_N_STERN_LIGHT], nullptr }
}, {
	.id = DRIVING_STATE,
	.label = DRIVING_LABEL,
	.button = &navlico_fsm_buttons[DRIVING_BTN],
	.indicator = &navlico_fsm_indicators[DRIVING_IND],
	.lights = { &navlico_fsm_lights[SIDE_N_STERN_LIGHT], &navlico_fsm_lights[MASTHEAD_LIGHT] }
}, {
	.id = ANCHORING_STATE,
	.label = ANCHORING_LABEL,
	.button = &navlico_fsm_buttons[ANCHORING_BTN],
	.indicator = &navlico_fsm_indicators[ANCHORING_IND],
	.lights = { &navlico_fsm_lights[ALLROUND_WHITE_LIGHT], nullptr }
},
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
{
	.id = SAILING_COAST_STATE,
	.label = SAILING_COAST_LABEL,
	.button = &navlico_fsm_buttons[SAILING_COAST_BTN],
	.indicator = &navlico_fsm_indicators[SAILING_COAST_IND],
	.lights = { &navlico_fsm_lights[ALLROUND_GREEN_LIGHT], &navlico_fsm_lights[ALLROUND_RED_1_LIGHT] }
}, {
	.id = DISABLED_STATE,
	.label = DISABLED_LABEL,
	.button = &navlico_fsm_buttons[DISABLED_BTN],
	.indicator = &navlico_fsm_indicators[DISABLED_IND],
	.lights = { &navlico_fsm_lights[ALLROUND_RED_1_LIGHT], &navlico_fsm_lights[ALLROUND_RED_2_LIGHT] }
},
#endif
};

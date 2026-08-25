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

#if CONFIG_NAVLICO_HAS_VERBOSE_OUTPUT
#define STR_OR_NULL( str ) str
#else
#define STR_OR_NULL( str ) nullptr
#endif

#ifdef CONFIG_NAVLICO_HAS_VERBOSE_OUTPUT
static constexpr DRAM_ATTR char GPIO_0_LABEL[] = "GPIO 0";
static constexpr DRAM_ATTR char GPIO_1_LABEL[] = "GPIO 1";
static constexpr DRAM_ATTR char GPIO_2_LABEL[] = "MTMS";
static constexpr DRAM_ATTR char GPIO_3_LABEL[] = "MTDO";
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
static constexpr DRAM_ATTR char GPIO_4_LABEL[] = "MTCK";
#endif
static constexpr DRAM_ATTR char GPIO_5_LABEL[] = "MTDI";
static constexpr DRAM_ATTR char GPIO_10_LABEL[] = "GPIO 10";
static constexpr DRAM_ATTR char GPIO_11_LABEL[] = "GPIO 11";
static constexpr DRAM_ATTR char GPIO_12_LABEL[] = "GPIO 12";
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
static constexpr DRAM_ATTR char GPIO_13_LABEL[] = "XTAL_32K_P";
static constexpr DRAM_ATTR char GPIO_14_LABEL[] = "XTAL_32K_N";
static constexpr DRAM_ATTR char GPIO_22_LABEL[] = "GPIO 22";
static constexpr DRAM_ATTR char GPIO_25_LABEL[] = "GPIO 25";
#endif
#endif

static constexpr DRAM_ATTR char OFF_LABEL[] = "Off";
static constexpr DRAM_ATTR char SAILING_LABEL[] = "Sailing";
static constexpr DRAM_ATTR char DRIVING_LABEL[] = "Driving";
static constexpr DRAM_ATTR char ANCHORING_LABEL[] = "Anchoring";
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
static constexpr DRAM_ATTR char SAILING_COAST_LABEL[] = "Sailing Coast";
static constexpr DRAM_ATTR char DISABLED_LABEL[] = "Disabled";
#endif

static constexpr DRAM_ATTR char SIDE_N_STERN_LABEL[] = "Off";
static constexpr DRAM_ATTR char MASTHEAD_LABEL[] = "Masthead";
static constexpr DRAM_ATTR char ALLROUND_WHITE_LABEL[] = "Allround White";
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
static constexpr DRAM_ATTR char ALLROUND_GREEN_LABEL[] = "Allround Green";
static constexpr DRAM_ATTR char ALLROUND_RED_1_LABEL[] = "Allround Red 1";
static constexpr DRAM_ATTR char ALLROUND_RED_2_LABEL[] = "Allround Red 2";
#endif

const DRAM_ATTR navlico_fsm_gpio_t navlico_fsm_gpios[ GPIO_COUNT ] = {
	{ .tag = GPIO_0,  .label = STR_OR_NULL( GPIO_0_LABEL ),  .num = GPIO_NUM_0,  .mode = GPIO_MODE_INPUT,           .active_level = 0 },
	{ .tag = GPIO_1,  .label = STR_OR_NULL( GPIO_1_LABEL ),  .num = GPIO_NUM_1,  .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
	{ .tag = GPIO_2,  .label = STR_OR_NULL( GPIO_2_LABEL ),  .num = GPIO_NUM_2,  .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
	{ .tag = GPIO_3,  .label = STR_OR_NULL( GPIO_3_LABEL ),  .num = GPIO_NUM_3,  .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .tag = GPIO_4,  .label = STR_OR_NULL( GPIO_4_LABEL ),  .num = GPIO_NUM_4,  .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
#endif
	{ .tag = GPIO_5,  .label = STR_OR_NULL( GPIO_5_LABEL ),  .num = GPIO_NUM_5,  .mode = GPIO_MODE_OUTPUT_OD,       .active_level = 0 },
	{ .tag = GPIO_10, .label = STR_OR_NULL( GPIO_10_LABEL ), .num = GPIO_NUM_10, .mode = GPIO_MODE_INPUT,           .active_level = 0 },
	{ .tag = GPIO_11, .label = STR_OR_NULL( GPIO_11_LABEL ), .num = GPIO_NUM_11, .mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	{ .tag = GPIO_12, .label = STR_OR_NULL( GPIO_12_LABEL ), .num = GPIO_NUM_12, .mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .tag = GPIO_13, .label = STR_OR_NULL( GPIO_13_LABEL ), .num = GPIO_NUM_13, .mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	{ .tag = GPIO_14, .label = STR_OR_NULL( GPIO_14_LABEL ), .num = GPIO_NUM_14, .mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	{ .tag = GPIO_22, .label = STR_OR_NULL( GPIO_22_LABEL ), .num = GPIO_NUM_22, .mode = GPIO_MODE_OUTPUT,          .active_level = 1 },
	{ .tag = GPIO_25, .label = STR_OR_NULL( GPIO_25_LABEL ), .num = GPIO_NUM_25, .mode = GPIO_MODE_OUTPUT_OD,       .active_level = 0 }
#endif
};

const DRAM_ATTR navlico_fsm_button_t navlico_fsm_buttons[ BTN_COUNT ] = {
	{ .tag = OFF_BTN,           .label = OFF_LABEL,           .gpio = &navlico_fsm_gpios[GPIO_0],  .state = &navlico_fsm_states[OFF_STATE] },
	{ .tag = SAILING_BTN,       .label = SAILING_LABEL,       .gpio = &navlico_fsm_gpios[GPIO_10], .state = &navlico_fsm_states[SAILING_STATE] },
	{ .tag = DRIVING_BTN,       .label = DRIVING_LABEL,       .gpio = &navlico_fsm_gpios[GPIO_11], .state = &navlico_fsm_states[DRIVING_STATE] },
	{ .tag = ANCHORING_BTN,     .label = ANCHORING_LABEL,     .gpio = &navlico_fsm_gpios[GPIO_12], .state = &navlico_fsm_states[ANCHORING_STATE] },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .tag = SAILING_COAST_BTN, .label = SAILING_COAST_LABEL, .gpio = &navlico_fsm_gpios[GPIO_13], .state = &navlico_fsm_states[SAILING_COAST_STATE] },
	{ .tag = DISABLED_BTN,      .label = DISABLED_LABEL,      .gpio = &navlico_fsm_gpios[GPIO_14], .state = &navlico_fsm_states[DISABLED_STATE] },
#endif
};

const DRAM_ATTR navlico_fsm_indicator_t navlico_fsm_indicators[ IND_COUNT ] = {
	{ .tag = SAILING_IND,       .label = SAILING_LABEL,       .gpio = &navlico_fsm_gpios[GPIO_1],  .state = &navlico_fsm_states[SAILING_STATE] },
	{ .tag = DRIVING_IND,       .label = DRIVING_LABEL,       .gpio = &navlico_fsm_gpios[GPIO_2],  .state = &navlico_fsm_states[DRIVING_STATE] },
	{ .tag = ANCHORING_IND,     .label = ANCHORING_LABEL,     .gpio = &navlico_fsm_gpios[GPIO_3],  .state = &navlico_fsm_states[ANCHORING_STATE] },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .tag = SAILING_COAST_IND, .label = SAILING_COAST_LABEL, .gpio = &navlico_fsm_gpios[GPIO_4],  .state = &navlico_fsm_states[SAILING_COAST_STATE] },
	{ .tag = DISABLED_IND,      .label = DISABLED_LABEL,      .gpio = &navlico_fsm_gpios[GPIO_22], .state = &navlico_fsm_states[DISABLED_STATE] },
#endif
};

const DRAM_ATTR navlico_fsm_light_t navlico_fsm_lights[ LIGHT_COUNT ] = {
	{ .tag = SIDE_N_STERN_LIGHT,   .label = SIDE_N_STERN_LABEL,   .gpio = &navlico_fsm_gpios[GPIO_5] },
	{ .tag = MASTHEAD_LIGHT,       .label = MASTHEAD_LABEL,       .gpio = &navlico_fsm_gpios[GPIO_11] },
	{ .tag = ALLROUND_WHITE_LIGHT, .label = ALLROUND_WHITE_LABEL, .gpio = &navlico_fsm_gpios[GPIO_12] },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	{ .tag = ALLROUND_GREEN_LIGHT, .label = ALLROUND_GREEN_LABEL, .gpio = &navlico_fsm_gpios[GPIO_13] },
	{ .tag = ALLROUND_RED_1_LIGHT, .label = ALLROUND_RED_1_LABEL, .gpio = &navlico_fsm_gpios[GPIO_25] },
	{ .tag = ALLROUND_RED_2_LIGHT, .label = ALLROUND_RED_2_LABEL, .gpio = &navlico_fsm_gpios[GPIO_14] },
#endif
};

const DRAM_ATTR navlico_fsm_state_t navlico_fsm_states[ STATE_COUNT ] = { {
	.tag = OFF_STATE,
	.label = OFF_LABEL,
	.button = &navlico_fsm_buttons[OFF_BTN],
	.indicator = nullptr,
	.lights = { nullptr, nullptr }
}, {
	.tag = SAILING_STATE,
	.label = SAILING_LABEL,
	.button = &navlico_fsm_buttons[SAILING_BTN],
	.indicator = &navlico_fsm_indicators[SAILING_IND],
	.lights = { &navlico_fsm_lights[SIDE_N_STERN_LIGHT], nullptr }
}, {
	.tag = DRIVING_STATE,
	.label = DRIVING_LABEL,
	.button = &navlico_fsm_buttons[DRIVING_BTN],
	.indicator = &navlico_fsm_indicators[DRIVING_IND],
	.lights = { &navlico_fsm_lights[SIDE_N_STERN_LIGHT], &navlico_fsm_lights[MASTHEAD_LIGHT] }
}, {
	.tag = ANCHORING_STATE,
	.label = ANCHORING_LABEL,
	.button = &navlico_fsm_buttons[ANCHORING_BTN],
	.indicator = &navlico_fsm_indicators[ANCHORING_IND],
	.lights = { &navlico_fsm_lights[ALLROUND_WHITE_LIGHT], nullptr }
},
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
{
	.tag = SAILING_COAST_STATE,
	.label = SAILING_COAST_LABEL,
	.button = &navlico_fsm_buttons[SAILING_COAST_BTN],
	.indicator = &navlico_fsm_indicators[SAILING_COAST_IND],
	.lights = { &navlico_fsm_lights[ALLROUND_GREEN_LIGHT], &navlico_fsm_lights[ALLROUND_RED_1_LIGHT] }
}, {
	.tag = DISABLED_STATE,
	.label = DISABLED_LABEL,
	.button = &navlico_fsm_buttons[DISABLED_BTN],
	.indicator = &navlico_fsm_indicators[DISABLED_IND],
	.lights = { &navlico_fsm_lights[ALLROUND_RED_1_LIGHT], &navlico_fsm_lights[ALLROUND_RED_2_LIGHT] }
},
#endif
};

#undef STR_OR_NULL

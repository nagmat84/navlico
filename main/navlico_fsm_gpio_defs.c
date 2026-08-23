/// \file main.c
/// Defines GPIO functions of Navlico's Finite State Machine (FSM).

#include "navlico_fsm.h"
#include "sdkconfig.h"
#include <esp_attr.h>

const DRAM_ATTR navlico_fsm_gpio_definition_t navlico_fsm_buttons[ BTN_COUNT ] = {
	/* OFF_BTN */{ .gpio_num = GPIO_NUM_0, .gpio_mode = GPIO_MODE_INPUT, .active_level = 0 },
	/* SAILING_BTN */{ .gpio_num = GPIO_NUM_10, .gpio_mode = GPIO_MODE_INPUT, .active_level = 0 },
	/* DRIVING_BTN */{ .gpio_num = GPIO_NUM_11, .gpio_mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	/* ANCHORING_BTN */{ .gpio_num = GPIO_NUM_12, .gpio_mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	/* SAILING_COAST_BTN */{ .gpio_num = GPIO_NUM_13, .gpio_mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	/* DISABLED_BTN */{ .gpio_num = GPIO_NUM_14, .gpio_mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
#endif
};

const DRAM_ATTR navlico_fsm_gpio_definition_t navlico_fsm_indicators[ IND_COUNT ] = {
	/* SAILING_IND */{ .gpio_num = GPIO_NUM_1, .gpio_mode = GPIO_MODE_OUTPUT, .active_level = 1 },
	/* DRIVING_IND */{ .gpio_num = GPIO_NUM_2, .gpio_mode = GPIO_MODE_OUTPUT, .active_level = 1 },
	/* ANCHORING_IND */{ .gpio_num = GPIO_NUM_3, .gpio_mode = GPIO_MODE_OUTPUT, .active_level = 1 },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	/* SAILING_COAST_IND */{ .gpio_num = GPIO_NUM_4, .gpio_mode = GPIO_MODE_OUTPUT, .active_level = 1 },
	/* DISABLED_IND */{ .gpio_num = GPIO_NUM_22, .gpio_mode = GPIO_MODE_OUTPUT, .active_level = 1 },
#endif
};

const DRAM_ATTR navlico_fsm_gpio_definition_t navlico_fsm_lights[ LIGHT_COUNT ] = {
	/* SIDE_N_STERN_LIGHT */{ .gpio_num = GPIO_NUM_5, .gpio_mode = GPIO_MODE_OUTPUT_OD, .active_level = 0 },
	/* MASTHEAD_LIGHT */{ .gpio_num = GPIO_NUM_11, .gpio_mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	/* ALLROUND_WHITE_LIGHT */{ .gpio_num = GPIO_NUM_12, .gpio_mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	/* ALLROUND_GREEN_LIGHT */{ .gpio_num = GPIO_NUM_13, .gpio_mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
	/* ALLROUND_RED_1_LIGHT */{ .gpio_num = GPIO_NUM_25, .gpio_mode = GPIO_MODE_OUTPUT_OD, .active_level = 0 },
	/* ALLROUND_RED_2_LIGHT */{ .gpio_num = GPIO_NUM_14, .gpio_mode = GPIO_MODE_INPUT_OUTPUT_OD, .active_level = 0 },
#endif
};

const DRAM_ATTR navlico_fsm_state_definition_t navlico_fsm_states[ STATE_COUNT ] = {
	/* OFF_STATE */{ .button = OFF_BTN, .indicator = INVALID_IND, .lights = {
		INVALID_LIGHT, INVALID_LIGHT
	} },
	/* SAILING_STATE */{ .button = SAILING_BTN, .indicator = SAILING_IND, .lights = {
		SIDE_N_STERN_LIGHT, INVALID_LIGHT
	} },
	/* DRIVING_STATE */{ .button = DRIVING_BTN, .indicator = DRIVING_IND, .lights = {
		SIDE_N_STERN_LIGHT, MASTHEAD_LIGHT
	} },
	/* ANCHORING_STATE */{ .button = ANCHORING_BTN, .indicator = ANCHORING_IND, .lights = {
		ALLROUND_WHITE_LIGHT, INVALID_LIGHT
	} },
#ifdef CONFIG_NAVLICO_VARIANT_FULL_FLEDGED
	/* SAILING_COAST_STATE */{ .button = SAILING_COAST_BTN, .indicator = SAILING_COAST_IND, .lights = {
		ALLROUND_GREEN_LIGHT, ALLROUND_RED_1_LIGHT
	} },
	/* DISABLED_STATE */{ .button = DISABLED_BTN, .indicator = DISABLED_IND, .lights = {
		ALLROUND_RED_1_LIGHT, ALLROUND_RED_2_LIGHT
	} },
#endif
};

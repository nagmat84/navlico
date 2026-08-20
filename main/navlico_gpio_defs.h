/// \file navlico_gpio_defs.h
/// Defines GPIO pin assignments for Navlico's Finite-State Machine (FSM).

#ifndef NAVLICO_GPIO_DEFS_H
#define NAVLICO_GPIO_DEFS_H

#include <driver/gpio.h>

static constexpr gpio_num_t GPIO_OFF_BUTTON = GPIO_NUM_0;
static constexpr gpio_num_t GPIO_SAILING_BUTTON = GPIO_NUM_10;
static constexpr gpio_num_t GPIO_DRIVING_BUTTON = GPIO_NUM_11;
static constexpr gpio_num_t GPIO_ANCHORING_BUTTON = GPIO_NUM_12;

static constexpr gpio_num_t GPIO_SAILING_INDICATOR = GPIO_NUM_1;
static constexpr gpio_num_t GPIO_DRIVING_INDICATOR = GPIO_NUM_2;
static constexpr gpio_num_t GPIO_ANCHORING_INDICATOR = GPIO_NUM_3;

// GPIO 4 skipped on purpose as it is pull-up during boot and hence a connected light would go on during boot
static constexpr gpio_num_t GPIO_SIDE_N_STERN_LIGHT = GPIO_NUM_5;
static constexpr gpio_num_t GPIO_MASTHEAD_LIGHT = GPIO_DRIVING_INDICATOR;
static constexpr gpio_num_t GPIO_ALLROUND_WHITE_LIGHT = GPIO_ANCHORING_INDICATOR;

static constexpr uint64_t GPIO_OFF_BUTTON_MASK = 1ULL << GPIO_OFF_BUTTON;
static constexpr uint64_t GPIO_SAILING_BUTTON_MASK = 1ULL << GPIO_SAILING_BUTTON;
static constexpr uint64_t GPIO_DRIVING_BUTTON_MASK = 1ULL << GPIO_DRIVING_BUTTON;
static constexpr uint64_t GPIO_ANCHORING_BUTTON_MASK = 1ULL << GPIO_ANCHORING_BUTTON;

static constexpr uint64_t GPIO_SAILING_INDICATOR_MASK = 1ULL << GPIO_SAILING_INDICATOR;
static constexpr uint64_t GPIO_DRIVING_INDICATOR_MASK = 1ULL << GPIO_DRIVING_INDICATOR;
static constexpr uint64_t GPIO_ANCHORING_INDICATOR_MASK = 1ULL << GPIO_ANCHORING_INDICATOR;

static constexpr uint64_t GPIO_SIDE_N_STERN_LIGHT_MASK = 1ULL << GPIO_SIDE_N_STERN_LIGHT;
static constexpr uint64_t GPIO_MASTHEAD_LIGHT_MASK = 1ULL << GPIO_MASTHEAD_LIGHT;
static constexpr uint64_t GPIO_ALLROUND_WHITE_LIGHT_MASK = 1ULL << GPIO_ALLROUND_WHITE_LIGHT;

static constexpr uint64_t GPIO_WAKEUP_BUTTONS_MASK =
    GPIO_SAILING_BUTTON_MASK |
    GPIO_DRIVING_BUTTON_MASK |
    GPIO_ANCHORING_BUTTON_MASK;

static constexpr uint64_t GPIO_ALL_BUTTONS_MASK =
    GPIO_OFF_BUTTON_MASK |
    GPIO_WAKEUP_BUTTONS_MASK;

static constexpr uint64_t GPIO_ALL_INDICATORS_MASK =
    GPIO_SAILING_INDICATOR_MASK |
    GPIO_DRIVING_INDICATOR_MASK |
    GPIO_ANCHORING_INDICATOR_MASK;

static constexpr uint64_t GPIO_ALL_LIGHTS_MASK =
    GPIO_SIDE_N_STERN_LIGHT_MASK |
    GPIO_MASTHEAD_LIGHT_MASK |
    GPIO_ALLROUND_WHITE_LIGHT_MASK;

#endif //NAVLICO_GPIO_DEFS_H

/// \file main.c
/// The main file of Navlico.

#include "navlico_fsm.h"
#include "navlico_pm.h"
#include "sdkconfig.h"
#include <freertos/FreeRTOS.h>

/**
 * The main task of Navlico
 */
void app_main(void) {
	setup_power_management();
#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
	log_navlico_pm_time_since_deep_sleep();
#endif
	xTaskCreate( navlico_fsm_task, NAVLICO_FSM_TAG, CONFIG_MAIN_TASK_STACK_SIZE, nullptr, 6, nullptr );
}

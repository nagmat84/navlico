/** \file navlico_main.c
 * The main file of Navlico.
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

/**
 * Identifier to tag log messages.
 */
static const char *TAG = "navlico";

/**
 * The main task of Navlico
 * At the moment the main doesn't do anything but suspend itself.
 */
void app_main(void) {
	while (1) {
		ESP_LOGI( TAG, "Suspending main task" );
		vTaskSuspend( nullptr );
	}
}

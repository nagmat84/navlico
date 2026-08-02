/** \file navlico_main.c
 * The main file of Navlico.
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_pm.h"
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
	// Enable Support for Light Sleep Mode
	// See https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/low-power-mode/low-power-mode-soc.html#dfs-configuration
	// TODO: Check if `.min_freq_mhz` can even lowered further; the documentation sayys it can be the XTAL frequency divided by an integer
	const esp_pm_config_t pm_config = {
		.max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, // 48 Mhz
		.min_freq_mhz = CONFIG_XTAL_FREQ, // 32 MHz
		.light_sleep_enable = true
	};
	ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

	while (1) {
		ESP_LOGI( TAG, "Suspending main task" );
		vTaskSuspend( nullptr );
	}
}

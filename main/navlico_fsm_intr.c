/// \file main.c
/// Implements the interrupt-related routines for Navlico's Finite State Machine (FSM).

#include "navlico_fsm.h"
#include "sdkconfig.h"
#include <esp_attr.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

/// The handle for the Navlico FSM Task
static DRAM_ATTR TaskHandle_t navlico_fsm_task_handle;

/**
 * Enables interrupts from the GPIO peripheral
 *
 * This function enables interrupts at their source, i.e. at the GPIO peripheral.
 * The function assumes that the interrupt is already (or still) allocated and the ISR installed.
 *
 * Enables the interrupt for all but `ignored_gpio`.
 * Some buttons and navigational lights share a combined input/output GPIO.
 * When the FSM is in a state which drives such a GPIO, then that GPIO must not be set as an interrupt source as the
 * interrupt would immediately trigger.
 *
 * @internal The interrupt must trigger upon a high input level, a rising edge is not sufficient.
 * During (light) sleep a rising edge is not detected and the ISR will never be called.
 * `gpio_wakeup_enable only` only accepts the two level types for a reason:
 * Light-sleep GPIO wake on the normal digital pins is level-only by design.
 * The digital edge-detect logic isn't clocked while the core is down,
 * so there's no edge detector alive to catch the transition in the first place.
 * Only a level comparator is watching, which is why `HIGH_LEVEL` works and `POSEDGE` just never fires.
 * Only the pins which sit in the LP/RTC IO domain stay powered through light sleep and keeps a real edge detector.
 * The detector latches the rising edge and holds it until the CPU is back up.
 * So the LP/RTC pins are the only place you get true edge semantics across sleep.
 * See https://www.reddit.com/r/esp32/comments/1vtfldn/comment/p51c27o/
 *
 * @param ignored_gpio The GPIO which shall not be enabled as a wake-up source
 */
void enable_navlico_fsm_gpio_interrupts( navlico_fsm_gpio_t const * const ignored_gpio ) {
	for ( navlico_fsm_button_id_t btn = 0; btn < BTN_COUNT; ++btn ) {
		navlico_fsm_gpio_t const * const gpio = navlico_fsm_buttons[btn].gpio;
		if ( gpio == ignored_gpio ) continue;
		ESP_ERROR_CHECK( gpio_set_intr_type(
				gpio->num, gpio->active_level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL
		) );
		ESP_ERROR_CHECK( gpio_intr_enable( gpio->num ) );
	}
}

/**
 * Disables interrupts from the GPIO peripheral
 *
 * This function disables interrupts at their source, i.e. at the GPIO peripheral.
 * The function keeps the interrupt allocation and the ISR untouched.
 *
 * @internal This function must be placed in RAM as the ISR handle_navlico_fsm_gpio_interrupt(void) calls this function
 * to temporarily disable subsequent interrupts while the first is still handled.
 * An ISR can only call code from RAM.
 */
void static IRAM_ATTR disable_navlico_fsm_gpio_interrupts( void ) {
	for ( navlico_fsm_button_id_t btn = 0; btn < BTN_COUNT; ++btn ) {
		navlico_fsm_gpio_t const * const gpio = navlico_fsm_buttons[btn].gpio;
		ESP_ERROR_CHECK( gpio_intr_disable( gpio->num ) );
		ESP_ERROR_CHECK( gpio_set_intr_type( gpio->num, GPIO_INTR_DISABLE ) );
	}
}

/**
 * The interrupt-service routine which notifies this task upon a GPIO interrupt.
 *
 * @internal An IRAM-safe ISR can only code (and data) which resides in RAM as flash access (and SPI) is potentially disabled.
 * See:
 * - https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/system/intr_alloc.html#iram-safe-interrupt-handlers
 * - https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-guides/memory-types.html#when-to-place-code-in-iram
 * Hence, `vTaskNotifyGiveFromISR` and `vPortYieldFromISR` must be placed in IRAM, too.
 * This means `CONFIG_FREERTOS_IN_IRAM=y` and `CONFIG_GPIO_CTRL_FUNC_IN_IRAM=y` must be set.
 */
void static IRAM_ATTR handle_navlico_fsm_gpio_interrupt( void* ) {
	disable_navlico_fsm_gpio_interrupts();
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	vTaskNotifyGiveFromISR( navlico_fsm_task_handle, &xHigherPriorityTaskWoken );
	if ( xHigherPriorityTaskWoken == pdTRUE ) {
		vPortYieldFromISR();
	}
}

#if CONFIG_LOG_DEFAULT_LEVEL_VERBOSE || LOG_MAXIMUM_LEVEL_VERBOSE
void static dump_navlico_fsm_isr_config( void ) {
	if ( esp_log_level_get( NAVLICO_FSM_TAG ) == ESP_LOG_VERBOSE )
		esp_intr_dump( stdout );
}
#else
void static dump_navlico_fsm_isr_config( void ) {}
#endif

/**
 * Registers the interrupt-service routine (ISR) for GPIO
 *
 * @internal This function registers the ISR with `ESP_INTR_FLAG_IRAM` to mark it as IRAM-safe, see
 * [Espressif: ESP-IDF Programming Guide - System API - Interrupt Allocation](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-reference/system/intr_alloc.html#iram-safe-interrupt-handlers).
 * This means that handle_navlico_fsm_gpio_interrupt(void*) and all function it calls must be placed in IRAM.
 */
void setup_navlico_fsm_isr( void ) {
	ESP_LOGD( NAVLICO_FSM_TAG, "Registering interrupt-service routine ..." );
	dump_navlico_fsm_isr_config();
	navlico_fsm_task_handle = xTaskGetCurrentTaskHandle();
	ESP_ERROR_CHECK( gpio_install_isr_service( ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_IRAM ) );
	for ( navlico_fsm_button_id_t btn = 0; btn < BTN_COUNT; ++btn ) {
		navlico_fsm_gpio_t const * const gpio = navlico_fsm_buttons[btn].gpio;
		ESP_ERROR_CHECK( gpio_isr_handler_add(
			gpio->num, handle_navlico_fsm_gpio_interrupt, nullptr
		) );
	}
	ESP_LOGD( NAVLICO_FSM_TAG, "Interrupt-service routine registered" );
	dump_navlico_fsm_isr_config();
}

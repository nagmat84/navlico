/// \file navlico_pm.h
/// Declarations for power management for Navlico.

#ifndef NAVLICO_NAVLICO_PM_H
#define NAVLICO_NAVLICO_PM_H

#include "sdkconfig.h"

/**
 * Sets up (Enables) Power Management
 *
 * Sets a lower minimum CPU frequency (`CONFIG_NAVLICO_MIN_FREQ`) and enables support for automatic light sleep.
 *
 * Also see [Espressif: API Guides - Low Power Modes - DFS Configuration](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32h2/api-guides/low-power-mode/low-power-mode-soc.html#dfs-configuration).
 */
void setup_power_management( void );

#if CONFIG_NAVLICO_HAS_SLEEP_TIMES
/**
 * Logs the duration of the last sleep period.
 *
 * The function only logs a message if the log level is DEBUG or higher and `CONFIG_NAVLICO_HAS_SLEEP_TIMES` is set.s
 * This functions is a no-op if build configuration disables debug logs.
 */
void log_navlico_pm_time_since_deep_sleep( void );
#endif

#endif //NAVLICO_NAVLICO_PM_H

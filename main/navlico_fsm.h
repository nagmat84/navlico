/// \file navlico_fsm.h
/// General definitions for Navlico's Finite-State Machine.
///
/// In order to keep this file succinct and clear the extensive GPIO assignments are in the seperate header file navlico_gpio_defs.h.

#ifndef NAVLICO_NAVLICO_FSM_H
#define NAVLICO_NAVLICO_FSM_H

extern char const * const NAVLICO_FSM_TAG;

/**
 * Enum to define the operational state
 *
 * The operational state directly corresponds to the most recently pressed button and active indicator light.
 */
typedef enum navlico_fsm_state_t_impl {
    UNDEFINED = 0,
    OFF = 1,      ///< The user has pressed the OFF button, the operational state is OFF
    SAILING = 2,  ///< The user has pressed the SAILING button, the operational state is SAILING
    DRIVING = 3,  ///< The user has pressed the DRIVING button, the operational state is DRIVING
    ANCHORING = 4 ///< The user has pressed the ANCHORING button, the operational state is ANCHORING
} navlico_fsm_state_t;

navlico_fsm_state_t get_navlico_fsm_state( void );

void navlico_fsm_task( void *args );

#endif //NAVLICO_NAVLICO_FSM_H

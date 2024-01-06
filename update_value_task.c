#include "TM4C123GH6PM.h"

#include "FreeRTOS.h"
#include "task.h"
#include "list.h"

#include "update_value_task.h"

#include "bsp.h"

void update_value_task(void * pvParameters) {

    start_timers();  // start timers for HC-SR04 trigger/measurement

    for (;;) {
        uint32_t edgeNotifType;
        uint32_t pulse_start;

        // want to detect just the rising edge
        // otherwise, interrupt is outpacing this task
        // outpacing will place the wrong value in the TAR register
        do {
	        xTaskNotifyWait(
                    0,  // clear nothing on entry
                    RISING_EDGE_NOTIF | FALLING_EDGE_NOTIF,  // clear both
                    &edgeNotifType,
                    portMAX_DELAY  // change to limit to detect errors
            );
        } while (edgeNotifType & FALLING_EDGE_NOTIF);
        // rising edge time now stored in TIMER0->TAR register
        pulse_start = TIMER0->TAR;

        // wait on falling edge notification
        xTaskNotifyWait(
                0,  // clear nothing on entry
                RISING_EDGE_NOTIF | FALLING_EDGE_NOTIF,  // clear both
                &edgeNotifType,
                portMAX_DELAY  // change to limit to detect errors
        );
        // want to detect just the falling edge
        // otherwise, interrupt is outpacing this task
        // outpacing will place the wrong value in the TAR register
        while (edgeNotifType & RISING_EDGE_NOTIF) {
            // reassign with updated rising edge time value
            pulse_start = TIMER0->TAR;
            xTaskNotifyWait(
                    0,  // clear nothing on entry
                    RISING_EDGE_NOTIF | FALLING_EDGE_NOTIF,  // clear both
                    &edgeNotifType,
                    portMAX_DELAY  // change to limit to detect errors
            );

        }
        uint32_t cycles = TIMER0->TAR - pulse_start;

        // uses float in formula for precision, converts back to int
        uint32_t millimeters = (uint32_t)
                ((cycles * MACH1) / (SYSCLOCK_PER_MS * 2.0));

	    set_display_number(millimeters);
	}
}


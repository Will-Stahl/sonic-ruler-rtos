#include "FreeRTOS.h"
#include "task.h"

#include "bsp.h"

#include "run_display_task.h"

void run_display_task(void *pvParameters) {
// 4dig7seg API
uint16_t digit_place = 0;

// FreeRTOS API
// initialize period anchor variable
TickType_t xLastWakeTime = xTaskGetTickCount();
// period to be faster than human eye can see
// period of 1 tick at 1000 HZ tick rate is good
const TickType_t xDisplayPeriod = pdMS_TO_TICKS( 1 );

    for (;;) {
        render_digit(digit_place);
        digit_place = (digit_place + 1) % NUM_DIGITS;

        xTaskDelayUntil(&xLastWakeTime, xDisplayPeriod);
    }
}

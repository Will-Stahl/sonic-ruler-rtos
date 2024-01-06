#include "bsp.h"
#include "TM4C123GH6PM.h"

#include "FreeRTOS.h"
#include "task.h"
#include "list.h"

#include "run_display_task.h"
#include "update_value_task.h"

TaskHandle_t xUpdateValTask;

void prvSetupHardware();

/**
 * main.c
 */
int main(void)
{
#if (__ARM_FP != 0) /* if VFP available... */
    /* make sure that the FPU is enabled by setting CP10 & CP11 Full Access */
    SCB->CPACR |= ((3UL << 20U)|(3UL << 22U));
#endif

    prvSetupHardware();

    // adjust stack depths as needed
    xTaskCreate(update_value_task, "Update Value", 1024, NULL, 2,
                &xUpdateValTask);
    // run display must run at higher priority than others
    xTaskCreate(run_display_task, "Run Display", 1024, NULL, 3, NULL);
    vTaskStartScheduler();
    // TODO: use watermarks to determine stack usage
    // convert to TivaWare library
    // take averages to control noise
    for (;;) {}
	return 0;
}

void prvSetupHardware() {
    // enable GPIO for display
    io_init_7seg_4dig();

    // enable pwm and interrupt for HC-SR04
    io_init_hcsr04();
    // do not start timers yet, we can afford potential redundancy in the task
}

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
    /* If configCHECK_FOR_STACK_OVERFLOW is set to either 1 or 2 then this
    function will automatically get called if a task overflows its stack.
    Taken from demo, perhaps implement better error handling */
    ( void ) pxTask;
    ( void ) pcTaskName;
    for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    /* If configUSE_MALLOC_FAILED_HOOK is set to 1 then this function will
    be called automatically if a call to pvPortMalloc() fails.  pvPortMalloc()
    is called automatically when a task, queue or semaphore is created.
    Copied from demo, should upgrade from plain DOS. */
    for( ;; );
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task.
Copied from a demo */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* copied from a FreeRTOS demo, like an OS counterpart to assert_failed */
void vMainAssertCalled( const char *pcFileName, uint32_t ulLineNumber )
{
volatile BaseType_t xSetToNonZeroToStepOutOfLoop = 0;

    taskENTER_CRITICAL();
    while( xSetToNonZeroToStepOutOfLoop == 0 )
    {
        /* Use the variables to prevent compiler warnings and in an attempt to
        ensure they can be viewed in the debugger.  If the variables get
        optimised away then set copy their values to file scope or globals then
        view the variables they are copied to. */
        ( void ) pcFileName;
        ( void ) ulLineNumber;
    }
}

/* Board Support Package */
#include "TM4C123GH6PM.h"

#include "FreeRTOS.h"
#include "task.h"

#include "bsp.h"

extern TaskHandle_t xUpdateValTask;

uint8_t curr_display[] = {0, 0, 0, 0};
const uint8_t numbers[] = {SEG0, SEG1, SEG2, SEG3, SEG4, SEG5, SEG6, SEG7,
                            SEG8, SEG9 };

void io_init_7seg_4dig() {
    // enable Run mode for GPIO A, B, E
    SYSCTL->RCGCGPIO  |= ENABLE_PORTA | ENABLE_PORTE | ENABLE_PORTB;
    // enable AHB for GPIO A, B, E
    SYSCTL->GPIOHBCTL |= ENABLE_PORTA | ENABLE_PORTE | ENABLE_PORTB;

    /* make sure the Run Mode and AHB-enable take effects
     * before accessing the peripherals
     */
    uint32_t delay = 10;
    while (delay--);

    // pins 2-7 as output on portA
    GPIOA_AHB->DIR |= P2 | P3 | P4 | P5 | P6 | P7;
    GPIOA_AHB->DEN |= P2 | P3 | P4 | P5 | P6 | P7;

    // pins 0-1 as output on portE
    GPIOE_AHB->DIR |= P0 | P1;
    GPIOE_AHB->DEN |= P0 | P1;

    // pins 0-3 as output on portB
    GPIOB_AHB->DIR |= P0 | P1 | P2 | P3;
    GPIOB_AHB->DEN |= P0 | P1 | P2 | P3;

    // initialize digit output to high so it does not behave as cathode
    GPIOB_AHB->DATA_Bits[(P0 | P1 | P2 | P3)] = P0 | P1 | P2 | P3;
}

void render_digit(uint16_t place) {
    // clear all segments first so it doesn't render out of position
    GPIOE_AHB->DATA_Bits[EMASK] = 0;
    GPIOA_AHB->DATA_Bits[AMASK] = 0;

    // clear placeth bit to make it cathode
    uint16_t mask = (1u << place);
    mask = (~mask) & (P0 | P1 | P2 | P3);
    GPIOB_AHB->DATA_Bits[(P0 | P1 | P2 | P3)] = mask;

    GPIOE_AHB->DATA_Bits[EMASK] = curr_display[place];
    GPIOA_AHB->DATA_Bits[AMASK] = curr_display[place];
}

/*
 * explicitly set what will be displayed on 4-digit, 7-segment display
 * in render_digit()
 * lower index is less significant digit
 */
void set_display(uint8_t segment_digit_codes[]) {
    for (int i = 0; i < NUM_DIGITS; i++) {
        curr_display[i] = segment_digit_codes[i];
    }
}

/*
 * converts number to segment digit codes and uses set_display
 */
void set_display_number(uint32_t display_number) {

    uint8_t digits[NUM_DIGITS];
    for (int i = 0; i < NUM_DIGITS; i++) {
        digits[i] = display_number % 10;
        display_number = display_number / 10;
    }

    // don't display leading 0s
    uint8_t to_display[NUM_DIGITS] = {0};
    to_display[0] = numbers[digits[0]];  // always display least sig digit
    if (digits[NUM_DIGITS-1]) {
        to_display[NUM_DIGITS-1] = numbers[digits[NUM_DIGITS-1]];
    }
    for (int i = NUM_DIGITS - 2; i > 0; i--) {
        if (to_display[i+1] || digits[i]) {
            to_display[i] = numbers[digits[i]];
        } else {
            to_display[i] = 0;
        }
    }
    set_display(to_display);

}

/*
 * End of display support
 * Begin HC-SR04 support
 */

/*
 * initialize timers 0A, 0B on same period
 * 0B generates trigger pulse to HC-SR04
 * 0A detects rising and falling edge of HC-SR04 echo pulse
 * 0A enables ISR for rising and falling edges
 */
void io_init_hcsr04() {
    // TIMER0 init
    SYSCTL->RCGCTIMER = 1u;  // clock enable to TIMER0
    TIMER0->CTL |= 0;  // TAEN disable timer A, B during config
    TIMER0->CFG |= 0x04u;  // 16-bit
    // TAMR capture mode, TACMR edge-time mode, //TACDIR count up
    TIMER0->TAMR |= 0x3u | P2 | P4;
    TIMER0->TBMR &= ~P2;  // TBAMS PWM
    TIMER0->TBMR |= 0x2 | P3;  // TBMR periodic mode, TBCMR to 'edge-time'
    TIMER0->ICR |= P2;  // CAECINT clear event capture flag
    TIMER0->CTL |= (0x3u << 2);  // TAEVENT both edge event detection
    TIMER0->TAV = 0;  // ensure timers 0
    TIMER0->TBV = 0;
    TIMER0->IMR |= (P0 << 2);  // CAEIM capture mode interrupt enable
    uint32_t period = 2 * SYS_CLOCK_HZ / 10;
    TIMER0->TAILR = period;
    TIMER0->TBILR = period;  // 0.2s
    TIMER0->TBMATCHR = period - (SYS_CLOCK_HZ / 100000);  // 10us
    TIMER0->TBPMR = (period - (SYS_CLOCK_HZ / 100000)) >> 16;
    TIMER0->TAPR = (2 * SYS_CLOCK_HZ / 10) >> 16;  // pr match regs 23:16
    TIMER0->TBPR = (2 * SYS_CLOCK_HZ / 10) >> 16;

    // SONIC DIST SENSOR INIT
    GPIOB_AHB->DEN |= P7 | P6;
    GPIOB_AHB->DIR |= P7;  // PB7 output
    GPIOB_AHB->AFSEL |= P7 | P6;  // alternate func
    // GPIOPCTL use T0CCP0/1 (timer 0 periph)
    GPIOB_AHB->PCTL &= ~(0xFFu << 24u);  // clear for sure
    GPIOB_AHB->PCTL |= (0x77 << 24u);  // tie PB6/7 to timers

    // Timer 0A interrupt- vec#: 35, interrupt#: 19, vec offset: 0x0000.008C
    NVIC->ISER[0] |= (1u << 19);
    // set Timer 0A interrupt priority to lower logically than config
    // using core_cm4.h
    __NVIC_SetPriority(19, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);
}

/*
 * start timers 0A and 0B at same time, AFTER initialization
 */
void start_timers() {
    TIMER0->CTL |= P0 | (1u << 8);
}

void Timer0A_IRQHandler(void) {
    TIMER0->ICR |= (1u << 2);  // interrupt status clear

    // initialization assumes falling edge
    uint32_t edgeTypeNotif = FALLING_EDGE_NOTIF;

    if (GPIOB_AHB->DATA & P6) {  // if rising edge, reassign notif value
        edgeTypeNotif = RISING_EDGE_NOTIF;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xTaskNotifyFromISR(xUpdateValTask,
                       edgeTypeNotif,
                       eSetBits,  // detect task moving too slow
                       &xHigherPriorityTaskWoken);

    // may wake update value task while run display task is asleep
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

__attribute__((naked)) void assert_failed (char const *file, int line) {
    /* TBD: damage control */
    NVIC_SystemReset(); /* reset the system */
}

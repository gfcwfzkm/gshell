/**
 * Basic example on how the gshell library can be used in your project
 * This example shows both the (optional) static commands as well as the dynamically
 * added commands. The example aims at the newer AVRs / XMEGAs microcontrollers by
 * (Atmel) Microchip, well as the GD32V RISC-V microcontroller by GigaDevices.
 * 
 * Any UART library (or similar) can be used to use the shell library, but for this
 * demonstration I assume my own written libraries:
 * For the new AVR generation: https://github.com/gfcwfzkm/newAVRSeries_USART
 * For the classic XMEGAs: https://github.com/gfcwfzkm/xmegaUSART
 * For the GD32VF103: https://github.com/gfcwfzkm/gfc32vf103_uart
 */
#ifdef DEMO_CODE
#ifdef AVR
#include <avr/io.h>
#include <avr/interrupt.h>
#include "uart.h"	
#else
#include "gd32vf103.h"
#include "gfc32vf103_uart.h"
#endif

#include <inttypes.h>
#include "gshell.h"

/* Buffer Size for the UART FIFO library: */
#define UART_BUF_SIZE	128

BUF_UART_t uartHnd;
uint8_t receiveBuf[UART_BUF_SIZE];
uint8_t transmitBuf[UART_BUF_SIZE];

void mcu_init(void);		// Initialise the peripherals of the MCU
void processShell(void);	// Check for new characters and run the shell
void putCharacter(char ch);	// Print a character
uint32_t optionalMSTick(void);	// Optional function to get the millis tick
int main(void)
{
	/* Initialise the MCU's peripherals first */
	mcu_init();

	/* Initialise the shell by passing over the output-character function and
	 * optionally the function to get the millis tick */
	gshell_init(&putCharacter, &optionalMSTick);
	/* Example initialisation without millis tick:
	 * gshell_init(&putCharacter, NULL); */
	gshell_setPromt(1);

	while(1)
	{
		/* Loop forever and process the inputs */
		processShell();
	}
}

/* The whole mcu init is not nessesairly part of the gshell library, but
 * without UART you can't really use the shell much, right? */
void mcu_init(void)
{
#ifdef AVR
	// TODO: Actually test the code on a AVR dev board first
	#error "AVR Demonstration not fully written!"
#else
	/* RISC-V GD32VF103 initialisation! Lets use USART1 for this: */
	/* Enable the required peripheral clocks */
	rcu_periph_clock_enable(RCU_GPIOA);
	rcu_periph_clock_enable(RCU_USART1);

	/* Initalise the UART pins */
	gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_2MHZ, BIT(2));	// PortA 2 Tx
	gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_2MHZ, BIT(3));	// PortA 3 Rx

	/* Configure & enable interrupts for the USART peripheral */
	eclic_global_interrupt_enable();
	eclic_priority_group_set(ECLIC_PRIGROUP_LEVEL3_PRIO1);
	eclic_irq_enable(USART1_IRQn, 1, 0);

	/* Initialise the library: */
	uart_init(&uartHnd, USART1, 115200, SERIAL_8N1, 
		receiveBuf, UART_BUF_SIZE, transmitBuf, UART_BUF_SIZE);
#endif
}

void processShell()
{
	uint16_t data = uart_getc(&uartHnd);
    if ((data >> 8) == UART_DATA_AVAILABLE)
    {
        gshell_CharReceived((char)data);
    }
}
#endif
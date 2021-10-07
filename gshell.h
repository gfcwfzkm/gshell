/* gfcwfzkm's generic shell for embedded systems
 *
 * A small terminal and console logging library with small features added over the years.
 * Initialy designed for the Atmel AVR microcontrollers (in particular the larger xmega series),
 * it slowly has been adopted to target other and more generic microcontrollers as well (tested
 * on the STM32L1x, GD32VF103x and ESP32)
 *
 * Todo:
 * - Add file and line to the logging output (__FILE__ __LINE__)
 * - Add alternative timestamps (HH:MM:SS for example)
 *
 * Created: 18.03.2020 07:08:46
 *  Author: gfcwfzkm
 */ 

#ifndef GSHELL_H_
#define GSHELL_H_

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#ifdef AVR
	#include <avr/pgmspace.h>
	#define _GMEMX	__memx
#else
	#define _GMEMX
#endif

#ifndef G_XSTR
	#define G_XSTR(s)	(__extension__({static _GMEMX const char __fc[] = (s); &__fc[0];}))
#endif

#ifndef G_XARR
	#define G_XARR(X)		( ( const _GMEMX char[] ) { X } )
#endif

#define G_RX_BUFSIZE	120	// also used as temporary buffer size in vsprintf for AVR, making it 
							// 2 times G_RX_BUFSIZE on AVR systems
#define G_MAX_ARGS		16	// Amount of pointers to strings that are passed to your command function
							// as *argv[]


#define G_ESCAPE		"\x1b["
#define G_TEXTNORMAL	G_ESCAPE"0m"
#define G_TEXTBOLD		G_ESCAPE"1m"
#define G_TEXTUNDERL	G_ESCAPE"4m"
#define G_TEXTBLINK		G_ESCAPE"5m"
#define G_TEXTREVERSE	G_ESCAPE"7m"
#define G_CLEARLINE		G_ESCAPE"2K"
#define G_COLORRESET	G_ESCAPE"0m"
#define G_COLORRED		G_ESCAPE"31m"
#define G_COLORGREEN	G_ESCAPE"32m"
#define G_COLORYELLOW	G_ESCAPE"33m"

#define G_CRLF			"\r\n"
#define C_NEWLINE		'\n'
#define C_CARRET		'\r'
#define C_BACKSPCE1		'\b'
#define C_BACKSPCE2		0x7F
#define C_WITESPCE		' '
#define C_NULLCHAR		'\0'

/* Definition for each valid command */
typedef struct gshell_cmd {
	const _GMEMX char *cmdName;					// name of the command to look for
	void (*handler)(uint8_t argc, char *argv[]);// function pointer to the command
	const _GMEMX char *desc;					// Simple description of the command
	struct gshell_cmd *next;	// Don't assign this yourself - used for dynamic commands via gshell_register_cmd
} gshell_cmd_t;

/* Log levels for gs_log_f */
enum glog_level{
	GLOG_NORMAL	= 0,
	GLOG_INFO	= 1,
	GLOG_OK		= 2,
	GLOG_WARN	= 3,
	GLOG_ERROR	= 4,
	GLOG_FATAL	= 5
};

/* Static command list. Use as followed in a seperate file (eg. 'commands.c':
 *
 * #include "gshell.h"
 *
 * void cli_cmd_hello(uint8_t argc, char *argv[])
 * {
	 gshell_putString_f("Hello World!\r\n");
 * }
 *
 * static const gshell_cmd_t shell_command_list[] = {
 *	 {G_XARR("hello"),	cli_cmd_hello,	G_XARR("Say Hello")}
 * // Simply add more commands here
 * };
 *
 * const gshell_cmd_t *const gshell_list_commands = shell_command_list;
 * const uint8_t gshell_list_num_commands = sizeof(shell_command_list) / sizeof(shell_command_list[0]);
 */
extern const gshell_cmd_t *const gshell_list_commands;
extern const uint8_t gshell_list_num_commands;

/* gshell_init
 * 
 * Initialise the console's internal variables and passes over the function to send
 * a character and to get a millisecond-timestamp for the logging functions.
 * If no millisecond-timestamp is wished, replace it with 'NULL' instead. */
void gshell_init(void (*put_char)(char), uint32_t (*get_msTimeStamp)(void));

/* gshell_register_cmd
 * 
 * Register a console command yourself by passing over the pointer of your
 * gshell_cmd_t command definition. */
uint8_t gshell_register_cmd(gshell_cmd_t *cmd);

/* Enables or disables the whole terminal & logging functions */
void gshell_setActive(uint8_t activeStatus);

/* Enables or disables console input */
void gshell_setPromt(uint8_t promtStatus);

/* gshell_CharReceived
 *
 * Called by the user when a character has been received so the shell can proces
 * the character. */
uint8_t gshell_CharReceived(char c);

/* gshell_putChar
 *
 * Directy calls the put_char function of the initialise function.
 * if the Terminal is not set active, no characters will be printed */
void gshell_putChar(char c);

/* gshell_putString
 * 
 * Sends a whole string up to the null character out.
 * Won't send anything if the shell isn't set active */
void gshell_putString(const char *str);

/* gshell_printf
 *
 * Basic printf functionality using the vsprintf function */
void gshell_printf(const char *str, ...);

/* Similar as gshell_putString but contents are expected to be in flash memory.
 * Use the gshell_putString_f macro to directly place the string into flash memory */
void gshell_putString_flash(const _GMEMX char *progmem_s);
#define gshell_putString_f(__f)	gshell_putString_flash(G_XSTR(__f))

/* Similar as gshell_printf but contents are expected to be in flash memory.
 * Use the gshell_printf_f macro to directly place the string into flash memory */
void gshell_printf_flash(const _GMEMX char *progmem_s, ...);
#define gshell_printf_f(__f, ...)	gshell_printf_flash(G_XSTR(__f),__VA_ARGS__);

/* gshell_log_flash
 *
 * Basic logging function that prints out the text together with the logging level
 * and optionally a milliseconds timestamp. Uses vsprintf, so additonal arguments and
 * variables can be printed similar like printf. Expects a const string pointer (to
 * flash for AVR devices) */
void gshell_log_flash(enum glog_level loglvl, const _GMEMX char *format, ...);
#define gs_log_f(__l,__f,...)	gshell_log_flash(__l,G_XSTR(__f), ##__VA_ARGS__)

#endif // GSHELL_H_

/**
 * @mainpage GSHELL - generic shell for embedded systems
 *
 * A small terminal and console logging library with small features added over the years.\n
 * Initialy designed for the Atmel AVR microcontrollers (in particular the larger xmega series),
 * it slowly has been adopted to target other and more generic microcontrollers (STM32L1x,
 * GD32VF103x, SAMD21G18J and ATxmega32A4U) and on the computer. (Created 18.03.2020) \n
 * Written by gfcwfzkm (github.com/gfcwfzkm, gfcwfzkm@protonmail.com)
 * 
 * \version 2.2
 * -Refined documentation, fixed minor bug
 *
 * \version 2.1
 * -Quotation-Marks detection and processing added (see \a G_ENABLE_SPECIALCMDSTR )
 *
 * \version 2.0
 * -Rework of the library, proper documentation
 *
 * \file gshell.h
 * \author gfcwfzkm
 * \date 08.10.2022
 * \copyright GNU Lesser General Public License v2.1
 *
 * \bug No known Bugs
 * \todo
 * - Add quotation-marks-functionality to the string-splitting / arguments parser
 * - Add basic cursor handling to the processShell-function
 */
/**
 * @file gshell.h
 * @brief Header-File of gshell library
 */
#ifndef GSHELL_H_
#define GSHELL_H_

/* --Includes-- */
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>


/****** USER CONFIGURATION STARTS HERE ******/
/**
 * @brief Enable static shell commands
 */
//#define ENABLE_STATIC_COMMANDS

/**
 * @brief Enable (/ Disable) echoing of the input back to the output
 */
#define G_ENABLE_ECHO

/**
 * @brief Use CR instead of LF to detect a new-line/enter keypress
 */
//#define G_CR_INSTEADOF_LF

/**
 * @brief Receive Buffer, also defines a AVR temporary buffer size (2x on AVR)
 */
#define G_RX_BUFSIZE	120

/**
 * @brief Amount of max. pointers passed as *argv[]
 */
#define G_MAX_ARGS		16

/**
 * @brief Enables specialised command string processing
 *
 * Allows ' and " characters to be used in order to not split
 * by spaces.
 */
#define G_ENABLE_SPECIALCMDSTR
/****** USER CONFIGURATION ENDS HERE ******/


/* If on a 8-bit AVR Microcontroller, store text data in flash memory */
#if defined(AVR) && !defined(__GNUG__)	// AVR + C Compiler (GCC)
	#include <avr/pgmspace.h>
	#define _GMEMX	__memx
	#define _PRGMX
#elif defined(AVR) && defined(__GNUG__)	// AVR + C++ Compiler (G++ workaround)
	#include <avr/pgmspace.h>
	#define _GMEMX
	#define _PRGMX PROGMEM
#else	// Non-AVR Platform
	#define _GMEMX
	#define _PRGMX
#endif

#ifndef G_XSTR
	/**
	 * @brief Store string (text array) in program memory and return a pointer from it
	 */
	#define G_XSTR(s)	(__extension__({static _GMEMX const char __fc[] _PRGMX = (s); &__fc[0];}))
#endif

#ifndef G_XARR
	/**
	 * @brief Store a Array in program memory
	 */
	#define G_XARR(X)		( ( const _GMEMX char[] _PRGMX ) { X } )
#endif

/* ANSI ESCAPE SEQUENCES TEXT FORMATTING */
#define G_ESCAPE		"\x1b["
#define G_TEXTNORMAL	G_ESCAPE"0m"
#define G_TEXTBOLD		G_ESCAPE"1m"
#define G_TEXTUNDERLINE	G_ESCAPE"4m"
#define G_TEXTBLINK		G_ESCAPE"5m"
#define G_TEXTNEGATIVE	G_ESCAPE"7m"
#define G_CLEARLINE		G_ESCAPE"2K"
/* ANSI ESCAPE SEQUENCES TEXT COLOR */
#define G_COLORRESET	G_ESCAPE"39m"
#define G_COLORRED		G_ESCAPE"31m"
#define G_COLORGREEN	G_ESCAPE"32m"
#define G_COLORYELLOW	G_ESCAPE"33m"
#define G_COLORBLUE		G_ESCAPE"34m"
#define G_COLORMAGENTA	G_ESCAPE"35m"
#define G_COLORCYAN		G_ESCAPE"36m"
#define G_COLORWHITE	G_ESCAPE"37m"
#define G_COLORCUSTOM(r,g,b) \
		G_ESCAPE"38m;2;"r";"g";"b


/* General Terminal Sequences */
#define G_CRLF			"\r\n"
#define C_NEWLINE		'\n'
#define C_CARRET		'\r'
#define C_BACKSPCE1		'\b'
#define C_BACKSPCE2		0x7F
#define C_WITESPCE		' '
#define C_NULLCHAR		'\0'

/**
 * @brief Shell command structure
 * 
 * Structure containing the command's name, a pointer to the function as well as a basic description.
 */
typedef struct gshell_cmd {
	const _GMEMX char *cmdName;						/**< String - command name (case sensitive) */
	uint8_t (*handler)(uint8_t argc, char *argv[]);	/**< function pointer to the command's funciton */
	const _GMEMX char *desc;						/**< String - basic, short description of the command */
	struct gshell_cmd *next;						/**< Don't assign this yourself! Used in dynamic command list */
} gshell_cmd_t;

/**
 * @brief logging level
 * 
 * Basic logging levels, highlighted by different colours and formats
 */
enum glog_level{
	GLOG_NORMAL	= 0,	/**< No Text, normal color */
	GLOG_INFO	= 1,	/**< 'INFO' Text, normal color */
	GLOG_OK		= 2,	/**< 'OK' Text, green color */
	GLOG_WARN	= 3,	/**< 'WARN' text, yellow color */
	GLOG_ERROR	= 4,	/**< 'ERROR' text, red color */
	GLOG_FATAL	= 5		/**< 'FATAL' text, red and blink */
};


#define GSHELL_CMDRET_MASK      0x7F
#define GSHELL_CMDRET_VAL(x)    (uint8_t)(x >> 8)
/**
 * @brief gshell-return status
 *
 * Information about buffer status or cmd function returns
 */
enum gshell_return{
	GSHELL_OK		= 0,	/**< Everything okay, nothing to report */
	GSHELL_INACTIVE,		/**< gshell inactive, character inputs ignored */
	GSHELL_BUFFULL,			/**< text input buffer full! */
	GSHELL_RUBBISH,			/**< Unrecognised data/command, discarded */
	GSHELL_CMDINV,			/**< Unrecognised command / command not found */
	GSHELL_ESCSEQ,			/**< Is processing a ANSI Escape Sequence */
	GSHELL_CMDRET	= 0x80	/**< Command returned Value, lower 7-bits contain command ID */
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
#ifdef ENABLE_STATIC_COMMANDS
extern const gshell_cmd_t *const gshell_list_commands;
extern const uint8_t gshell_list_num_commands;
#endif 


/**
 * @brief Initialise the shell
 * 
 * Initialise the console's internal variables and passes over the function to send
 * a character and to get a millisecond-timestamp for the logging functions.
 * If no millisecond-timestamp is wished, replace it with 'NULL' instead.
 * 
 * @param putchar			Function pointer to print a character
 * @param get_msTimeStamp	Function pointer to get the milliseconds timestamp as uint32_t
 */
int8_t gshell_init(void (*put_char)(char), uint32_t (*get_msTimeStamp)(void));

/**
 * @brief Register a command
 * 
 * Register a console command yourself by passing over the pointer of your
 * gshell_cmd_t command definition.
 *
 * @param cmd	Pointer to a initialised gshell_cmd_t struct
 * @return		Returns the amount of registered commands. If the value
 *				is negative, the command couldn't be added.
 */
int8_t gshell_register_cmd(gshell_cmd_t *cmd);

/**
 * @brief Returns the ID of a command
 *
 * Searches the internal command list for the command name passed
 * over to this function and returns the corresponding command ID
 * when it's found, otherwise returns -1
 *
 * @param cmd_name	Name of the command
 * @return			-1 if not found, non-negative ID otherwise
 */
int8_t gshell_getCmdIDbyName(const char* cmd_name);

/**
 * @brief Returns the ID of a command
 *
 * Searches the internal command list for the matching command
 * structure passed over to this function. Returns the command
 * ID when it's found, otherwise returns -1
 *
 * @param cmd		Pointer to the gshell_cmd_t command
 * @return			-1 if not found, non-negative ID otherwise
 */
int8_t gshell_getCmdIDbyStruct(gshell_cmd_t *cmd);

/**
 * @brief Set Shell Active
 *
 * Enables or disables the whole terminal and logging functionality
 * Enabled by default when initialising gshell (\a gshell_init )
 *
 * @param activeStatus	0 to disable, non-zero to enable the terminal
 */
void gshell_setActive(uint8_t activeStatus);

/**
 * @brief Enable or disable the promt
 *
 * Enables or disables the console promt and text input
 *
 * @param promtStatus	0 to disable, non-zero to enable the promt
 */
void gshell_setPromt(uint8_t promtStatus);

/**
 * @brief Character-received Callback Function
 *
 * Call this function with every character received by your system/peripheral or
 * driver to let gshell process the user data input
 *
 * @param c		8-bit Character received via UART/Terminal/etc...
 * @return		Lower 8-bit contain the enum gshell_return, upper 8-bit the
 *				return value of the executed command
 */
uint16_t gshell_processShell(char c);

/**
 * @brief Prints a single character
 *
 * Prints a single character, passes the character directly
 * to the driver function passed over during initialisation.
 * If the terminal isn't set 'active', nothing will be printed.
 *
 * @param c		8-bit Character to send
 */
void gshell_putChar(char c);

/**
 * @brief Sends a (S)RAM string
 *
 * Sends a string stored in volatile RAM/Memory.
 * If the terminal isn't set 'active', nothing will be printed.
 *
 * @param str	Pointer to array of characters, NULL-Character at the end
 */
void gshell_putStringRAM(const char *str);

/**
 * @brief Sends a flash string
 *
 * Sends a string stored in non-volatile flash memory. Use the macro
 * \a gshell_putString to automatically mark the text within there as
 * progmem. Otherwise use the \a G_XSTR macro.
 * If the terminal isn't set 'active', nothing will be printed.
 *
 * @param progmem_s	Pointer to the program memory holding the flash string
 */
void gshell_putString_flash(const _GMEMX char *progmem_s);

/**
 * @brief Makro for \a gshell_putString_flash
 * 
 * Stores the text automatically via a macro in flash memory for
 * the \a gshell_putString_flash function.
 * 
 * @param __f 	Text to send, automatically saved in flash
 */
#define gshell_putString(__f)	gshell_putString_flash(G_XSTR(__f))

/**
 * @brief Printf-functionality from program memory
 *
 * Basic (vs)printf functionality, where the main string is
 * stored in flash memory instead of RAM. Requires the string to be
 * put in the program memory if the macro \a gshell_printf isn't used.
 * If the terminal isn't set 'active', nothing will be printed.
 *
 * @param progmem_s	Pointer to the program memory holding the flash string
 * @param ...		Additional, optional printf-style arguments
 */
void gshell_printf_flash(const _GMEMX char *progmem_s, ...);

/**
 * @brief Printf-functionality & auto-flash string saving
 * 
 * Similar as \a gshell_printf_f but stores the text automatically
 * in the program memory.
 * 
 * @param __f		printf-formatting text to process
 * @param ...		Additional, optional printf-style arguments
 */
#define gshell_printf(__f, ...)	gshell_printf_flash(G_XSTR(__f),__VA_ARGS__);

/**
 * @brief Logging functionality
 *
 * Basic logging function that prints out the text together with the logging level
 * and optionally a milliseconds timestamp. Uses vsprintf, so additonal arguments and
 * variables can be printed similar like printf. Expects a const string pointer (to
 * flash memory).
 *
 * @param loglvl	glog_level logging level
 * @param logText	Program-memory-pointer to the logging text & printf-formatting 
 * @param ...		Additional, optional printf-style arguments
 */
void gshell_log_flash(enum glog_level loglvl, const _GMEMX char *logText, ...);

/**
 * @brief Logging macro
 * 
 * Basically \a gshell_log_flash but with a macro to store the (printf) text
 * in the flash memory & passing the flash memory pointer to the function 
 * \a gshell_log_flash.
 * 
 * @param __l		enum \a glog_level logging level
 * @param logText	Logging text & printf-formatting 
 * @param ...		Additional, optional printf-style arguments
 */
#define glog(__l,__f,...)		gshell_log_flash(__l,G_XSTR(__f), ##__VA_ARGS__)

/**
 * @brief Logging macro level NORMAL
 * 
 * Basically \a gshell_log_flash but with a macro to store the (printf) text
 * in the flash memory & passing the flash memory pointer to the function 
 * \a gshell_log_flash.
 * 
 * @param logText	Logging text & printf-formatting 
 * @param ...		Additional, optional printf-style arguments
 */
#define glog_norm(__f,...)		gshell_log_flash(GLOG_NORMAL,G_XSTR(__f), ##__VA_ARGS__)

/**
 * @brief Logging macro level INFO
 * 
 * Basically \a gshell_log_flash but with a macro to store the (printf) text
 * in the flash memory & passing the flash memory pointer to the function 
 * \a gshell_log_flash.
 * 
 * @param logText	Logging text & printf-formatting 
 * @param ...		Additional, optional printf-style arguments
 */
#define glog_info(__f,...)		gshell_log_flash(GLOG_INFO,G_XSTR(__f), ##__VA_ARGS__)

/**
 * @brief Logging macro level OK
 * 
 * Basically \a gshell_log_flash but with a macro to store the (printf) text
 * in the flash memory & passing the flash memory pointer to the function 
 * \a gshell_log_flash.
 * 
 * @param logText	Logging text & printf-formatting 
 * @param ...		Additional, optional printf-style arguments
 */
#define glog_ok(__f,...)		gshell_log_flash(GLOG_OK,G_XSTR(__f), ##__VA_ARGS__)

/**
 * @brief Logging macro level WARNING
 * 
 * Basically \a gshell_log_flash but with a macro to store the (printf) text
 * in the flash memory & passing the flash memory pointer to the function 
 * \a gshell_log_flash.
 * 
 * @param logText	Logging text & printf-formatting 
 * @param ...		Additional, optional printf-style arguments
 */
#define glog_warn(__f,...)		gshell_log_flash(GLOG_WARN,G_XSTR(__f), ##__VA_ARGS__)

/**
 * @brief Logging macro level ERROR
 * 
 * Basically \a gshell_log_flash but with a macro to store the (printf) text
 * in the flash memory & passing the flash memory pointer to the function 
 * \a gshell_log_flash.
 * 
 * @param logText	Logging text & printf-formatting 
 * @param ...		Additional, optional printf-style arguments
 */
#define glog_error(__f,...)		gshell_log_flash(GLOG_ERROR,G_XSTR(__f), ##__VA_ARGS__)

/**
 * @brief Logging macro level FATAL
 * 
 * Basically \a gshell_log_flash but with a macro to store the (printf) text
 * in the flash memory & passing the flash memory pointer to the function 
 * \a gshell_log_flash.
 * 
 * @param logText	Logging text & printf-formatting 
 * @param ...		Additional, optional printf-style arguments
 */
#define glog_fatal(__f,...)		gshell_log_flash(GLOG_FATAL,G_XSTR(__f), ##__VA_ARGS__)

/**
 * @brief Logging file/function/line
 *
 * Prints the exact file, function and line via the logging function.
 * The text is automatically stored in flash / program memory.
 * Expects a logging level as argument.
 *
 * @param __l		enum \a glog_level logging level
 */
#define glog_ffl(__l)			\
	gshell_log_flash(__l, G_XSTR("In ["__FILE__"], function [%s] line [%d]"), __FUNCTION__, __LINE__)



#endif // GSHELL_H_

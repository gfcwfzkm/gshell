/**
 * @file gshell.c
 * @brief gshell C file
 * @author gfcwfzkm
 * @version 2.2
 */

#include "gshell.h"

/* Strings used by the library are defined here */
#define _G_UNKCMD	"Unknown command: "
#define _G_HLPCMD	G_CRLF"Type 'help' to list all available commands"G_CRLF
#define _G_HLPDESC	"Lists all available commands"
#define _G_PROMT			G_CRLF G_TEXTBOLD"gshell> "G_TEXTNORMAL

/* For weird terminals that send \r instead of \n at enter-keypresses */
#ifdef G_CR_INSTEADOF_LF
#define _G_ENT_IGNORE	'\n'
#define _G_STR_PROCESS	" \r"
#define _G_STR_COMPSLIT	"\"'"
#define _G_ENT_PROCESS	'\r'
#else
#define _G_ENT_IGNORE	'\r'
#define _G_STR_PROCESS	" \n"
#define _G_STR_COMPSLIT	"\"'"
#define _G_ENT_PROCESS	'\n'
#endif

/* Escape Sequence Buffer Length */
#define ESCSEQ_BUFLEN	8

/* String-Functions for strings stored in flash memory */
#ifdef AVR
	#define _G_STRNLEN(key,length)		strnlen_PF((__uint24)(key),length)
	#define _G_STRNCMP(str,key,length)	strncmp_PF(str,(__uint24)(key),length)
#else
	#define _G_STRNLEN(key,length)		strnlen(key,length)
	#define _G_STRNCMP(str,key,length)	strncmp(str,key,length)
#endif

#define _G_MAXCMD	127	// Maximum of 127 commands allowed!

/* Internal Variable Structure */
static struct {
	void (*fp_putChar)(char);			/**< Functionspointer to send a char */
	uint32_t (*fp_msTimeStamp)(void);	/**< Functionspointer to get the milliseconds tick */
	uint8_t chain_len;					/**< length of the command struct chain */
	gshell_cmd_t *lastChain;			/**< Dynamic Command Chain, pointer to the last registered command */
	uint8_t rx_index;					/**< Receive Buffer Index */
	char rx_buf[G_RX_BUFSIZE];			/**< Receive Buffer, G_RX_BUFSIZE Bytes */
	char vsprintf_buf[G_RX_BUFSIZE];	/**< vsprintf buffer used in gshell_printf and glog functions */
	uint8_t isActive:1;					/**< Enable the whole shell, including any basic printing or reading */
	uint8_t promtEnabled:1;				/**< Enable the shell promt, controls input processing by the user */
	uint8_t helpCmdDescLength;			/**< Optimizing of the help function for faster yet nicer screen output */
	uint8_t helpCmdNameLength;			/**< Optimizing of the help function for faster yet nicer screen output */
#ifdef AVR
	char tempBuf[G_RX_BUFSIZE];
#endif
} sInternals = {0};


/* Internal 'help' command, to list all other commands
 * It is also the head/first element of the command list */
static uint8_t gshell_cmd_help(uint8_t argc, char *argv[]);
static gshell_cmd_t cmd_help = {
	G_XARR("help"),
	gshell_cmd_help,
	G_XARR(_G_HLPDESC),
	NULL
};

/* Logging Texts with additonal formatting, stored in the program flash */
static const _GMEMX char * const _GMEMX console_levels[6] =
{
	G_XARR("[      ] "),																/**< GLOG_NORMAL */
	G_XARR("["G_TEXTBOLD" INFO "G_TEXTNORMAL"] "),										/**< GLOG_INFO */
	G_XARR("["G_TEXTBOLD G_COLORGREEN"  OK  "G_COLORRESET G_TEXTNORMAL"] "),			/**< GLOG_OK */
	G_XARR("["G_TEXTBOLD G_COLORYELLOW" WARN "G_COLORRESET G_TEXTNORMAL"] "),			/**< GLOG_WARN */
	G_XARR("["G_TEXTBOLD G_COLORRED"ERROR!"G_COLORRESET G_TEXTNORMAL"] "),				/**< GLOG_ERROR */
	G_XARR("["G_TEXTBOLD G_TEXTBLINK G_COLORRED"PANIC!"G_COLORRESET G_TEXTNORMAL"] ")	/**< GLOG_FATAL */
};

/* Echoes back to the terminal / serial port
 * Filters out certain special characters
 * Can be disabled via the define G_ENABLE_ECHO */
static void _gshell_echo(char c)
{
#ifdef G_ENABLE_ECHO

	if (C_NEWLINE == c) // Echoing newline?
	{
		// Add carriage return to newline
		gshell_putChar(C_CARRET);
		gshell_putChar(C_NEWLINE);
	}
	else if ((c == C_BACKSPCE1) || (c == C_BACKSPCE2)) // Echoing backspace?
	{
		// remove previous character on screen
		gshell_putChar(C_BACKSPCE1);
		gshell_putChar(C_WITESPCE);
		gshell_putChar(C_BACKSPCE1);
	}
	else
	{
		gshell_putChar(c);
	}
#else
	// Supress compiler warning
	(void)(c);
#endif
}

/* Find the right gshell_cmd_t command structure, which name
 * matches the name passed over to this function. Also returns the ID
 * of said command if found!
 *
 * If no command has been found, it returns a null-pointer and pi8CmdID with -1 */
static const gshell_cmd_t *_gshellFindCmd(const char *name, int8_t *pi8CmdID)
{
	const gshell_cmd_t *command;
	uint8_t u8_cnt;
	
	/* First check the linked chain
	 * The first entry in the chain are always the internal commands, the
	 * last entry being sInternals.lastChain. With that the chain can be
	 * quicklychecked.
	*/
	for (u8_cnt = 0; u8_cnt < sInternals.chain_len; u8_cnt++)
	{
		// First entry always is cmd_help, from there follow the chain
		if (u8_cnt)
		{
			command = command->next;
		}
		else
		{
			command = &cmd_help;
		}
		// Check if 'name' matches the command struct's name
		if (_G_STRNCMP(name, command->cmdName, G_RX_BUFSIZE) == 0)
		{
			*pi8CmdID = u8_cnt;	// Command-ID
			return command;		// Pointer to Command Structure
		}
	}

	*pi8CmdID = u8_cnt;
#ifdef ENABLE_STATIC_COMMANDS
	/* Then check the command list */
	for (u8_cnt = 0; u8_cnt < gshell_list_num_commands; u8_cnt++)
	{
		command = &gshell_list_commands[u8_cnt];
		// Similar as before, search the name in the command struct list
		// and return it's ID and pointer when found
		if (_G_STRNCMP(name, command->cmdName, G_RX_BUFSIZE) == 0)
		{
			*pi8CmdCnt += u8_cnt;
			return command;
		}
	}
#endif 

	/* None of the commands matched? Return a NULL-pointer and
	 * the invalid command ID '-1'! */
	*pi8CmdID = -1;
	return NULL;
}

/* Checks if a character contains any character inside the string.
 * If yes, it returns that, otherwise it returns zero */
static char _gshell_CharCmpStr(const char cInput, const char *strCheck)
{
	while(*strCheck != '\0')
	{
		if (cInput != *strCheck)
		{
			strCheck++;
		}
		else
		{
			return *strCheck;
		}
	}
	return 0;
}

/* strtok replacement to support additional, special arguments
 *
 * In order to support string splitting with " or ' characters, a
 * extended strtok function has been coded. It behaves similarly as the
 * original function from string.h, but takes an additional string parameter.
 *
 * Otherwise arguments and return pointers are the same and it should
 * behave the same too.
 */
static char *_gshell_strtok(char *strInput, const char *delim, const char *special)
{
	static char *strProcess = NULL;	// Process string pointer
	char *strStart = NULL;			// Start pointer of a string to split
	static uint8_t bSpecialMode = 0;// Special Mode, enabled if a special character has been found

	// New string passed over?
	if (strInput != NULL)
	{
		strProcess = strInput;
		bSpecialMode = 0;
	}

	// Process-String-Pointer empty?
	if ((strProcess == NULL) || (*strProcess == '\0'))
	{
		return NULL;
	}

	// Work loop where the string is splitted
	do
	{
		// Set the starting pointer to a valid character (non-delim or special)
		if (strStart == NULL)
		{
			// Check if a normal split operation is detected
			if ((bSpecialMode == 0) && _gshell_CharCmpStr(*strProcess, delim))
			{
				*strProcess = '\0';
			}
			// Check if a special split operation is detected
			else if (_gshell_CharCmpStr(*strProcess, special))
			{
				*strProcess = '\0';

				if (bSpecialMode)
				{
					bSpecialMode = 0;
				}
				else
				{
					bSpecialMode = 1;
				}
			}
			else
			{
				// None of the other two? Character to set the start pointer on found!
				strStart = strProcess;
			}
		}
		else
		{
			// Check if a normal split operation is detected
			if ((bSpecialMode == 0) && _gshell_CharCmpStr(*strProcess, delim))
			{
				*strProcess = '\0';
				strProcess++;

				return strStart;
			}
			// Check if a special split operation is detected
			else if (_gshell_CharCmpStr(*strProcess, special))
			{
				*strProcess = '\0';
				strProcess++;

				if (bSpecialMode)
				{
					bSpecialMode = 0;
				}
				else
				{
					bSpecialMode = 1;
				}
				return strStart;
			}
		}
	}
	while(*++strProcess != '\0');
	// Finish if the work-string-pointer reaches the end
	// and return the last start pointer
	return strStart;
}

/* Processes the complete string, inputted by the user
 * Splits the string by spaces and searches for a matching command,
 * before calling it and passing over the arguments in a standard-c-style fashion.
 * If the command returns a value, it too is processed and returned back
 * for the user to handle the command's return value.
 *
 * Lower 8-bits is the enum 'gshell_return', upper 8-bits for the command return value. */
static int16_t _gshell_process()
{
	enum gshell_return eGshellPrc = GSHELL_OK;	// Function Status uppon exit
	uint8_t argc = 0;			// Classic C-Style argc to fill in
	char *argv[G_MAX_ARGS];		// Classic C-Style argv to fill in
	uint16_t u8CmdRet = 0;		// Return Value of the command
	int8_t i8CmdID = 0;			// ID of the command

	// No newline, no command to process!
	if (sInternals.rx_buf[sInternals.rx_index - 1] != _G_ENT_PROCESS)
	{
		return GSHELL_OK;
	}

	// Split by spaces into seperate strings, store the pointers in argv (command + arguments)
	// and increase argc
#ifdef G_ENABLE_SPECIALCMDSTR
	// Pay attention to special characters like " or ' and split accordingly
	char *pch = _gshell_strtok(sInternals.rx_buf, _G_STR_PROCESS, _G_STR_COMPSLIT);
	while (pch != NULL)
	{
		if (argc < G_MAX_ARGS)
		{
			argv[argc++] = pch;
			pch = _gshell_strtok(NULL, _G_STR_PROCESS, _G_STR_COMPSLIT);
		}
		else
		{
			break;
		}
	}
#else
	char *pch = strtok(sInternals.rx_buf, _G_STR_PROCESS);
    while (pch != NULL)
	{
		if (argc < G_MAX_ARGS)
		{
			argv[argc++] = pch;
			pch = strtok(NULL, _G_STR_PROCESS);
		}
		else
		{
			break;
		}
	}
#endif

	// Just making sure we have actually found *any* argument
	if (argc >= 1)
	{
		// Actual text has been received! Time to find the fitting command to it, else
		// print the error
		const gshell_cmd_t *command = _gshellFindCmd(argv[0], &i8CmdID);
		if (!command)
		{
			// command not found, return error
			gshell_putString(_G_UNKCMD);
			gshell_putStringRAM(argv[0]);
			gshell_putString(_G_HLPCMD);
			eGshellPrc = GSHELL_CMDINV;
		}
		else
		{
			// Command found, calling the function pointer with the command line arguments
			if (command->handler != NULL)
			{
				u8CmdRet = command->handler(argc, argv);
			}
		}
	}
	else
	{
		// User probably just spammed the enter key
		gshell_putString(_G_HLPCMD);
		eGshellPrc = GSHELL_RUBBISH;
	}

	/* Resetting the input buffer */
	memset(sInternals.rx_buf, 0, G_RX_BUFSIZE);
	sInternals.rx_index = 0;

	// If the shell promt is enabled, reprint it (showing the user that we're ready
	// for new commands)
	if (sInternals.promtEnabled)
	{
		gshell_putString(_G_PROMT);
	}

	// If the called command returned a value, return it on the upper half of the word,
	// and the command's ID on the lower half.
	if (u8CmdRet)
	{
		return (u8CmdRet << 8) | (GSHELL_CMDRET | i8CmdID);
	}
	else
	{
		return eGshellPrc;
	}
}

int8_t gshell_init(void (*put_char)(char), uint32_t (*get_msTimeStamp)(void))
{
	// Storing function pointers in the internal variable structure
	sInternals.fp_putChar = put_char;
	sInternals.fp_msTimeStamp = get_msTimeStamp;

	// Terminal turned on by default, prompt turned off by default
	sInternals.isActive = 1;
	sInternals.promtEnabled = 0;
	
#ifdef ENABLE_STATIC_COMMANDS
	// Make sure not too many static commands have been added
	if (gshell_list_num_commands == _G_MAXCMD)
	{

	}
	else if (gshell_list_num_commands > _G_MAXCMD)
	{
		return -1;
	}
	else
	{
		gshell_register_cmd(&cmd_help);
	}
#else
	// Register the default help command
	gshell_register_cmd(&cmd_help);
#endif
	
	// Resetting Input Buffer
	memset(sInternals.rx_buf, 0, G_RX_BUFSIZE);

	// New lines for good measure
	gshell_putString(G_CRLF G_CRLF);

	return 0;
}

int8_t gshell_register_cmd(gshell_cmd_t *cmd)
{
	// Check if max. amount of commands already have been reached
	uint8_t u8CheckCmdCnt = sInternals.chain_len;
#ifdef ENABLE_STATIC_COMMANDS
	u8CheckCmdCnt += gshell_list_num_commands;
#endif
	if (u8CheckCmdCnt >= _G_MAXCMD)
	{
		return -1;
	}

	// Check if the lastChain variable as been assigned -> set it in as first entry!
	if (sInternals.lastChain == NULL)
	{
		sInternals.lastChain = cmd;
		sInternals.chain_len++;
	}
	else
	{
		// If not, rebuild and extend the command chain, and increase the chain length counter
		sInternals.lastChain->next = cmd;
		sInternals.lastChain = cmd;
		sInternals.chain_len++;
	}

	// Help function needs to rebuild the "spaces" length for nice formatting
	sInternals.helpCmdDescLength = 0;
	sInternals.helpCmdNameLength = 0;

	// Return the length of the command chain
	return sInternals.chain_len - 1;
}

// Returns the command ID from the string, returns -1 if it hasn't been found
int8_t gshell_getCmdIDbyName(const char* cmd_name)
{
	int8_t cmdID = 0;
	_gshellFindCmd(cmd_name, &cmdID);
	return cmdID;
}

// Returns the command ID from the command structure pointer, returns -1 if it hasn't been found
int8_t gshell_getCmdIDbyStruct(gshell_cmd_t *cmd)
{
	int8_t cmdID;
	uint8_t u8_cnt;
	const gshell_cmd_t *command;

	// Similar as _gshellFindCmd, but comparing the pointer addresses instead of the
	// command's name
	for (u8_cnt = 0; u8_cnt < sInternals.chain_len; u8_cnt++)
	{
		/* First entry always is cmd_help, from there follow the chain */
		if (u8_cnt)
		{
			command = command->next;
		}
		else
		{
			command = &cmd_help;
		}

		if (command == cmd)	// Just compare the pointer addresses against eachother
		{
			return (int8_t)u8_cnt;
		}
	}

	cmdID = u8_cnt;
#ifdef ENABLE_STATIC_COMMANDS
	/* Then check the command list */
	for (u8_cnt = 0; u8_cnt < gshell_list_num_commands; u8_cnt++)
	{
		command = &gshell_list_commands[u8_cnt];
		if (command == cmd)	// Just compare the pointer addresses against eachother
		{
			return (int8_t)(u8_cnt + cmdID);
		}
	}
#else
	// Supress compiler warning
	(void)(cmdID);
#endif

	// If no ID has been returned by now, return -1
	return -1;
}

void gshell_setActive(uint8_t activeStatus)
{
	// Set active means all activities are enabled...
	if (activeStatus)
	{
		sInternals.isActive = 1;
	}
	else
	{
		//... or halted if disabled! No printing of any functions, input is ignored
		sInternals.isActive = 0;
	}
}

void gshell_setPromt(uint8_t promtStatus)
{
	if (promtStatus)
	{
		// Enabling the prompt? Check if it has already been enabled...
		if (sInternals.promtEnabled == 0)
		{
			// .. if no, print the prompt and enable it internally
			sInternals.promtEnabled = 1;
			gshell_putString(_G_PROMT);
		}
	}
	else
	{
		// If the promt was enabled before, erase it from the terminal screen
		if (sInternals.promtEnabled)
		{
			gshell_putString(G_CLEARLINE);
			gshell_putChar('\r');
		}
		sInternals.promtEnabled = 0;
	}
}

/*** WIP WIP WIP - Do not enable G_ENABLE_INESCAPES - WIP WIP WIP ***/
/**
 * @brief Enables incoming Escape-Sequence Processing
 */
//#define G_ENABLE_INESCAPES

uint16_t gshell_processShell(char c)
{
#ifdef G_ENABLE_INESCAPES
	static uint8_t u8EscapeSequenceCnt = 0;
	static char cEscapeSequenceBuf[ESCSEQ_BUFLEN];
	int32_t i32EscapeSequenceValue= 0;
#endif

	// Main function to process each incoming character!
	// CR/LF or \0 is ignored
	if ((c == _G_ENT_IGNORE) || (c == '\0'))
	{
		return GSHELL_OK;
	}
	else if ((sInternals.isActive == 0) || (sInternals.promtEnabled == 0))
	{
		// If the shell isn't even set active, avoid any further processing!
		return GSHELL_INACTIVE;
	}

#ifdef G_ENABLE_INESCAPES
	/* ANSI Escape Sequence check & processing */
	if ((u8EscapeSequenceCnt) || (c == 0x1B))
	{
		if (u8EscapeSequenceCnt)
		{
			if ((u8EscapeSequenceCnt == 1) && (c == '['))
			{
				u8EscapeSequenceCnt = 2;
			}
			else
			{
				// Letter, number or something else received?
				if (((c >=0x41) && (c <= 0x5A)) || ((c >= 0x61) && (c <= 0x7A)))
				{
					// Letter received! Finish sequence handling
					// and process it.

				}
				else if ((c >= 0x30) && (c <= 0x39))
				{
					// Number received!

				}
				else
				{
					// Something else received!
				}
			}
		}
		else
		{
			u8EscapeSequenceCnt = 1;
		}

		return GSHELL_OK;
	}
#endif

	/* Backspace handling */
	if ((c == C_BACKSPCE1) || (c == C_BACKSPCE2))
	{
		// Interpreting backspace, removing the character from the input buffer
		// and removing it also from the user's screen!

		if (sInternals.rx_index > 0)
		{
			sInternals.rx_buf[--sInternals.rx_index] = C_NULLCHAR;
			_gshell_echo(c);
		}
		return GSHELL_OK;
	}
	else if (sInternals.rx_index >= G_RX_BUFSIZE)
	{
		// Input buffer full? Stop processing and inform the user!
		return GSHELL_BUFFULL;
	}

	// Echo the (valid) character back to the terminal/user
#ifdef G_CR_INSTEADOF_LF
	// Some terminals send a CR instead of LF, handle things here-
	if (c == _G_ENT_PROCESS)
	{
		_gshell_echo(_G_ENT_IGNORE);
	}
	else
	{
		_gshell_echo(c);
	}
#else
	_gshell_echo(c);
#endif

	// Storing the received character, increading index
	sInternals.rx_buf[sInternals.rx_index++] = c;

	// Call the main processing function, return it's return-value
	return _gshell_process();
}

void gshell_putChar(char c)
{
	// If shell is not inactive, print character
	if (sInternals.isActive == 0)	return;
	sInternals.fp_putChar(c);
}

void gshell_putStringRAM(const char *str)
{
	// If shell is not inactive, print string
	if (sInternals.isActive == 0)	return;
	while (*str)
	{
		gshell_putChar(*str++);
	}
}

void gshell_putString_flash(const _GMEMX char *progmem_s)
{
	char character;

	// If shell is not inactive, print string from program memory
	if (sInternals.isActive == 0)	return;

	while ( (character = *progmem_s++) )
	{
		gshell_putChar(character);
	}
}

void gshell_printf_flash(const _GMEMX char *progmem_s, ...)
{
	va_list args;
	// If shell is not inactive, return directly back
	if (sInternals.isActive == 0)	return;
	
	// Printf with the main string stored in the program memory! First get the argument list
	va_start(args, progmem_s);
#ifdef AVR
	// Copy the program-memory string into the SRAM memory, store it in the
	// additional vsprintf buffer for further processing
	// Handle printf-processing via vsprintf
	strncpy_PF(sInternals.tempBuf, (__uint24)progmem_s, G_RX_BUFSIZE);
	vsprintf(sInternals.vsprintf_buf, sInternals.tempBuf, args);
#else
	// Handle printf-processing via vsprintf
	vsprintf(sInternals.vsprintf_buf, progmem_s, args);
#endif
	va_end(args);
	
	// Print the result of vsprintf back to the user
	gshell_putStringRAM(sInternals.vsprintf_buf);
}

void gshell_log_flash(enum glog_level loglvl, const _GMEMX char *logText, ...)
{
	va_list args;
	uint32_t timestamp = 0;

	// Shell not set active? Abort further processing!
	if (sInternals.isActive == 0)	return;

	// If the promt is enabled, it is likely also printed out -> erase it (and anything typed by the user)
	if (sInternals.promtEnabled)
	{
		gshell_putStringRAM(G_CLEARLINE);
		gshell_putChar(C_CARRET);
	}
	
	// Print the logging level
	gshell_putString_flash(console_levels[loglvl]);
	
	// If a timestamp function pointer has been given, call it to get the ms-Tick
	if (sInternals.fp_msTimeStamp != NULL)
	{
		// Print the msTick / Timestamp
		timestamp = sInternals.fp_msTimeStamp();
		gshell_printf_flash(G_XSTR("[%09u] "), timestamp);
	}

	// Similar "printf / vsprintf" processing as in gshell_printf_flash
#ifdef AVR
	strncpy_PF(sInternals.tempBuf, (__uint24)logText, G_RX_BUFSIZE);
	va_start(args, logText);
	vsprintf(sInternals.vsprintf_buf, sInternals.tempBuf, args);
    va_end(args);
#else
	va_start(args, logText);
	vsprintf(sInternals.vsprintf_buf, logText, args);
	va_end(args);
#endif

	// Print the result from vsprintf
	gshell_putStringRAM(sInternals.vsprintf_buf);

	if (sInternals.promtEnabled)
	{
		// If the promt was enabled before, reprint not only
		// the prompt itself, but also what the user has typed in!
		// Thus, restoring any command / typing flow of the user despite random logging
		gshell_putString(_G_PROMT);
		gshell_putStringRAM(sInternals.rx_buf);
	}
	else
	{
		gshell_putString(G_CRLF);
	}
}

/*****************************************************************************/
/*****************   DEFAULT COMMANDS INCLUDED WITH GSHELL *******************/
/*****************************************************************************/
static uint8_t gshell_cmd_help(uint8_t argc, char *argv[])
{
	const gshell_cmd_t *command;

	uint8_t longestCommand = sInternals.helpCmdNameLength;
	uint8_t longestDescription = sInternals.helpCmdDescLength;
	uint8_t tempLen = 0;
	uint8_t u8_cnt;

	// Supress 'unused parameter' warning:
	(void)(argc);
	(void)(argv);

	// If we haven't found the longest command and description yet, find it
	if ((longestCommand == 0) || (longestDescription == 0))
	{
		// Loop through every dynamic command!
		for (u8_cnt = 0; u8_cnt < sInternals.chain_len; u8_cnt++)
		{
			if (u8_cnt)
			{
				command = command->next;
			}
			else
			{
				command = &cmd_help;
			}

			// Get the length of the command name and description!
			//
			tempLen = _G_STRNLEN(command->cmdName, G_RX_BUFSIZE);
			if (tempLen > longestCommand)	longestCommand = tempLen;

			tempLen = _G_STRNLEN(command->desc, G_RX_BUFSIZE);
			if (tempLen > longestDescription)	longestDescription = tempLen;

		}
#ifdef ENABLE_STATIC_COMMANDS
		for (u8_cnt = 0; u8_cnt < gshell_list_num_commands; u8_cnt++)
		{
			command = &gshell_list_commands[u8_cnt];
			tempLen = _G_STRNLEN(command->cmdName, G_RX_BUFSIZE);
			if (tempLen > longestCommand)	longestCommand = tempLen;

			tempLen = _G_STRNLEN(command->desc, G_RX_BUFSIZE);
			if (tempLen > longestDescription)	longestDescription = tempLen;
		}
#endif
		sInternals.helpCmdDescLength = longestDescription;
		sInternals.helpCmdNameLength = longestCommand;
	}

	// Print the command, followed by some spacing, and finally the description
	// The amount of "spaces" between the command name and description is
	// based on the longest command name.
	for (u8_cnt = 0; u8_cnt < sInternals.chain_len; u8_cnt++)
	{
		// If u8Cnt is zero, start with the help command, otherwise with the dynamic command chain
		if (u8_cnt)
		{
			command = command->next;
		}
		else
		{
			command = &cmd_help;
		}

		// Some boundary checks in order to print long descriptions nicely:
		if (2+longestDescription+longestCommand >= G_RX_BUFSIZE)
		{
			gshell_putString("\r"G_TEXTBOLD);
			gshell_putString_flash(command->cmdName);
			gshell_putString(G_TEXTNORMAL":"G_CRLF"     ");
			gshell_putString_flash(command->desc);
			gshell_putString(G_CRLF);
		}
		else
		{
			// Command name + description not too large?
			// Print the spaces, then the description, then use
			// CR to return back to the start to print the command name
			// Go to the next line (LF) and repeat as long a there are commands
			for (uint8_t j = 0; j < (longestCommand+2); j++)
			{
				gshell_putChar(' ');
			}
			gshell_putString_flash(command->desc);
			gshell_putString("\r"G_TEXTBOLD);
			gshell_putString_flash(command->cmdName);
			gshell_putString(G_TEXTNORMAL":"G_CRLF);
		}
	}
#ifdef ENABLE_STATIC_COMMANDS
	// Same as with the dynamic commands above!
	for (u8_cnt = 0; u8_cnt < gshell_list_num_commands; u8_cnt++)
	{
		command = &gshell_list_commands[u8_cnt];
		if (2+longestDescription+longestCommand >= G_RX_BUFSIZE)
		{
			gshell_putString("\r"G_TEXTBOLD);
			gshell_putString_flash(command->cmdName);
			gshell_putString(G_TEXTNORMAL":"G_CRLF"     ");
			gshell_putString_flash(command->desc);
			gshell_putString(G_CRLF);
		}
		else
		{
			for (uint8_t j = 0; j < (longestCommand+2); j++)
			{
				gshell_putChar(' ');
			}
			gshell_putString_flash(command->desc);
			gshell_putString("\r"G_TEXTBOLD);
			gshell_putString_flash(command->cmdName);
			gshell_putString(G_TEXTNORMAL":"G_CRLF);
		}
	}
#endif
	return 0;
}

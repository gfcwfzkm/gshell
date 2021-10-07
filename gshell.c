/*
 * gshell.c
 *
 * Created: 18.03.2020 07:08:46
 *  Author: gfcwfzkm
 */ 

#include "gshell.h"

/* Let the Preprocessor deal with language. Change the stuff here to your desired language */
#define _G_UNKCMD	"Unknown command: "
#define _G_HLPCMD	G_CRLF"Type 'help' to list all available commands"G_CRLF
#define _G_HLPDESC	"Lists all available commands"
#define G_PROMT			G_CRLF G_TEXTBOLD"gshell> "G_TEXTNORMAL

#ifdef AVR
	#define _G_STRNLEN(key,length)		strnlen_PF((__uint24)(key),length)
	#define _G_STRNCMP(str,key,length)	strncmp_PF(str,(__uint24)(key),length)
#else
	#define _G_STRNLEN(key,length)		strnlen(key,length)
	#define _G_STRNCMP(str,key,length)	strncmp(str,key,length)
#endif

static struct {
    void (*put_char)(char);
	uint32_t (*get_msTimeStamp)(void);
	uint8_t chain_len;
	gshell_cmd_t *lastChain;
    uint8_t rx_index;
    char rx_buf[G_RX_BUFSIZE];
    char vspr_buf[G_RX_BUFSIZE];
	uint8_t argc;
	char *argv[G_MAX_ARGS];
    uint8_t isActive:1;
    uint8_t promtStatus:1;
#ifdef AVR
	char tempBuf[G_RX_BUFSIZE];
#endif
} g_shell = {0};

void gshell_cmd_help(uint8_t argc, char *argv[]);
static gshell_cmd_t cmd_help = {
	G_XARR("help"),
	gshell_cmd_help,
	G_XARR(_G_HLPDESC),
	NULL
};


static const _GMEMX char * const _GMEMX console_levels[6] =
{
	G_XARR("[      ] "),
	G_XARR("["G_TEXTBOLD" INFO "G_TEXTNORMAL"] "),
	G_XARR("["G_TEXTBOLD G_COLORGREEN"  OK  "G_COLORRESET G_TEXTNORMAL"] "),
	G_XARR("["G_TEXTBOLD G_COLORYELLOW" WARN "G_COLORRESET G_TEXTNORMAL"] "),
	G_XARR("["G_TEXTBOLD G_COLORRED"ERROR"G_COLORRESET G_TEXTNORMAL"] "),
	G_XARR("["G_TEXTBOLD G_TEXTBLINK G_COLORRED"PANIC!"G_COLORRESET G_TEXTNORMAL"] ")
};

static void _gshell_echo(char c)
{
	if (C_NEWLINE == c)
	{
		// Add carriage return to newline
		gshell_putChar(C_CARRET);
		gshell_putChar(C_NEWLINE);
	}
	else if ((c == C_BACKSPCE1) || (c == C_BACKSPCE2))
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
}

static const gshell_cmd_t *_gshellFindCmd(const char *name)
{
	const gshell_cmd_t *command;
	uint8_t u8_cnt;
	
	/* First check the linked chain
	 * The first entry in the chain is always the help command, the last entry
	 * being g_shell.lastChain. With that the chain can be quickly checked.     */
	for (u8_cnt = 0; u8_cnt < g_shell.chain_len; u8_cnt++)
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

		if (_G_STRNCMP(name, command->cmdName, G_RX_BUFSIZE) == 0)
		{
			return command;
		}
	}
	
	/* Then check the command list */
	for (uint8_t i = 0; i < gshell_list_num_commands; i++)
	{
		command = &gshell_list_commands[i];
		if (_G_STRNCMP(name, command->cmdName, G_RX_BUFSIZE) == 0)
		{
			return command;
		}
	}
	
	return NULL;
}

uint8_t _gshell_process()
{
	// No newline, no command to process!
	if (g_shell.rx_buf[g_shell.rx_index - 1] != C_NEWLINE)
	{
		return 0;
	}
	
	g_shell.argc = 0;

    char *pch = strtok(g_shell.rx_buf, " \n");
    while (pch != NULL)
	{
		if (g_shell.argc < G_MAX_ARGS)
		{
			g_shell.argv[g_shell.argc++] = pch;
			pch = strtok(NULL, " \n");
		}
		else
		{
			break;
		}
	}

	if (g_shell.argc >= 1)
	{
		// Actual text has been received! Time to find the fitting command to it, else
		// print the error
		const gshell_cmd_t *command = _gshellFindCmd(g_shell.argv[0]);
		if (!command)
		{
			gshell_putString_f(_G_UNKCMD);
			gshell_putString(g_shell.argv[0]);
			gshell_putString_f(_G_HLPCMD);
		}
		else
		{
			if (command->handler != NULL)	command->handler(g_shell.argc, g_shell.argv);
		}
	}
	else
	{
		// User probably just spammed the enter key
		gshell_putString_f(_G_HLPCMD);
	}

	/* Resetting Input Buffer */
	memset(g_shell.rx_buf, 0, G_RX_BUFSIZE);
	g_shell.rx_index = 0;

	gshell_putString_f(G_PROMT);
	return 0;
}

void gshell_init(void (*put_char)(char), uint32_t (*get_msTimeStamp)(void))
{
	g_shell.put_char = put_char;
	g_shell.get_msTimeStamp = get_msTimeStamp;
	g_shell.isActive = 1;
	
	gshell_register_cmd(&cmd_help);
	
	/* Resetting Input Buffer */
	memset(g_shell.rx_buf, 0, G_RX_BUFSIZE);

	gshell_putString_f(G_CRLF"\n");
}

uint8_t gshell_register_cmd(gshell_cmd_t *cmd)
{
	if ( (g_shell.lastChain == NULL) && (g_shell.chain_len == 0) )
	{
		g_shell.lastChain = cmd;
		g_shell.chain_len++;
	}
	else
	{
		g_shell.lastChain->next = cmd;
		g_shell.chain_len++;
	}
	return g_shell.chain_len;
}

void gshell_setActive(uint8_t activeStatus)
{
	if (activeStatus)
	{
		g_shell.isActive = 1;
	}
	else
	{
		g_shell.isActive = 0;
	}
}

void gshell_setPromt(uint8_t promtStatus)
{
	if (promtStatus)
	{
		if (g_shell.promtStatus == 0)
		{
			g_shell.promtStatus = 1;		
			gshell_putString_f(G_PROMT);
		}
	}
	else
	{
		g_shell.promtStatus = 0;
	}
}

uint8_t gshell_CharReceived(char c)
{
	if (c == C_CARRET)
	{
		return 0;
	}
	else if (g_shell.isActive == 0)
	{
		return 3;
	}

	if ((c == C_BACKSPCE1) || (c == C_BACKSPCE2))
	{
		if (g_shell.rx_index > 0)
		{
			g_shell.rx_buf[--g_shell.rx_index] = C_NULLCHAR;
			_gshell_echo(c);
		}
		return 0;
	}
	else if (g_shell.rx_index >= G_RX_BUFSIZE)
	{
		return 2;
	}

	_gshell_echo(c);
	g_shell.rx_buf[g_shell.rx_index++] = c;
	return _gshell_process();
}

void gshell_cmd_help(uint8_t argc, char *argv[])
{
	const gshell_cmd_t *command;
	uint8_t longestCommand = 0;
	uint8_t longestDescription = 0;
	uint8_t tempLen;
	uint8_t u8_cnt;
	
	for (u8_cnt = 0; u8_cnt < g_shell.chain_len; u8_cnt++)
	{
		if (u8_cnt)
		{
			command = command->next;
		}
		else
		{
			command = &cmd_help;
		}

		tempLen = _G_STRNLEN(command->cmdName, G_RX_BUFSIZE);
		if (tempLen > longestCommand)	longestCommand = tempLen;
		
		tempLen = _G_STRNLEN(command->desc, G_RX_BUFSIZE);
		if (tempLen > longestDescription)	longestDescription = tempLen;
		
	}
	
	for (u8_cnt = 0; u8_cnt < gshell_list_num_commands; u8_cnt++)
	{
		command = &gshell_list_commands[u8_cnt];
		tempLen = _G_STRNLEN(command->cmdName, G_RX_BUFSIZE);
		if (tempLen > longestCommand)	longestCommand = tempLen;
		
		tempLen = _G_STRNLEN(command->desc, G_RX_BUFSIZE);
		if (tempLen > longestDescription)	longestDescription = tempLen;
	}
	
	for (u8_cnt = 0; u8_cnt < g_shell.chain_len; u8_cnt++)
	{
		if (u8_cnt)
		{
			command = command->next;
		}
		else
		{
			command = &cmd_help;
		}
		
		if (2+longestDescription+longestCommand >= G_RX_BUFSIZE)
		{
			gshell_putString_f("\r"G_TEXTBOLD);
			gshell_putString_flash(command->cmdName);
			gshell_putString_f(G_TEXTNORMAL":"G_CRLF"     ");
			gshell_putString_flash(command->desc);
			gshell_putString_f(G_CRLF);
		}
		else
		{
			for (uint8_t j = 0; j < (longestCommand+2); j++)
			{
				gshell_putChar(' ');
			}
			gshell_putString_flash(command->desc);
			gshell_putString_f("\r"G_TEXTBOLD);
			gshell_putString_flash(command->cmdName);
			gshell_putString_f(G_TEXTNORMAL":"G_CRLF);
		}
	}
	
	for (u8_cnt = 0; u8_cnt < gshell_list_num_commands; u8_cnt++)
	{
		command = &gshell_list_commands[u8_cnt];
		if (2+longestDescription+longestCommand >= G_RX_BUFSIZE)
		{
			gshell_putString_f("\r"G_TEXTBOLD);
			gshell_putString_flash(command->cmdName);
			gshell_putString_f(G_TEXTNORMAL":"G_CRLF"     ");
			gshell_putString_flash(command->desc);
			gshell_putString_f(G_CRLF);
		}
		else
		{
			for (uint8_t j = 0; j < (longestCommand+2); j++)
			{
				gshell_putChar(' ');
			}
			gshell_putString_flash(command->desc);
			gshell_putString_f("\r"G_TEXTBOLD);
			gshell_putString_flash(command->cmdName);
			gshell_putString_f(G_TEXTNORMAL":"G_CRLF);	
		}
	}
	
}

void gshell_putChar(char c)
{
	if (g_shell.isActive == 0)	return;
	g_shell.put_char(c);
}

void gshell_putString(const char *str)
{
	if (g_shell.isActive == 0)	return;
	while (*str)
	{
		gshell_putChar(*str++);
	}
}

void gshell_printf(const char *str, ...)
{
	va_list args;
	
	if (g_shell.isActive == 0)	return;
	
	va_start(args, str);
	vsprintf(g_shell.vspr_buf, str, args);
	va_end(args);

	gshell_putString(g_shell.vspr_buf);
}

void gshell_putString_flash(const _GMEMX char *progmem_s)
{
	char character;

	if (g_shell.isActive == 0)	return;

	while ( (character = *progmem_s++) )
	{
		gshell_putChar(character);
	}
}

void gshell_printf_flash(const _GMEMX char *progmem_s, ...)
{
	va_list args;

	if (g_shell.isActive == 0)	return;
	
	va_start(args, progmem_s);
#ifdef AVR
	strncpy_PF(g_shell.tempBuf, (__uint24)progmem_s, G_RX_BUFSIZE);
	vsprintf(g_shell.vspr_buf, g_shell.tempBuf, args);
#else
	vsprintf(g_shell.vspr_buf, progmem_s, args);
#endif
	
	va_end(args);
	
	gshell_putString(g_shell.vspr_buf);
}

void gshell_log_flash(enum glog_level loglvl, const _GMEMX char *format, ...)
{
	va_list args;
	uint32_t timestamp = 0;

	if (g_shell.isActive == 0)	return;
	if (g_shell.promtStatus)
	{
        gshell_putString(G_CLEARLINE);
        gshell_putChar('\r');
	}
	
	gshell_putString_flash(console_levels[loglvl]);
	
	if (g_shell.get_msTimeStamp != NULL)
	{
		timestamp = g_shell.get_msTimeStamp();
		gshell_printf_flash(G_XSTR("[%09d] "), timestamp);
	}

#ifdef AVR
	strncpy_PF(g_shell.tempBuf, (__uint24)format, G_RX_BUFSIZE);
	va_start(args, format);
    vsprintf(g_shell.vspr_buf, g_shell.tempBuf, args);
    va_end(args);
#else
	va_start(args, format);
	vsprintf(g_shell.vspr_buf, format, args);
	va_end(args);
#endif

    gshell_putString(g_shell.vspr_buf);

	if (g_shell.promtStatus)
	{
		gshell_putString_f(G_PROMT);
		gshell_putString(g_shell.rx_buf);
	}
	else
	{
		gshell_putString_f(G_CRLF);
	}
}

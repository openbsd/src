
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	NWUtil.h
 * DESCRIPTION	:	Utility functions for NetWare implementation of Perl.
 * Author		:	HYAK, SGP
 * Date			:	January 2001.
 *
 */



#ifndef __NWUtil_H__
#define __NWUtil_H__


#include "stdio.h"
#include <stdlib.h>
#include "win32ish.h"		// For "BOOL", "TRUE" and "FALSE"


#ifdef MPK_ON
	#include <mpktypes.h>	
	#include <mpkapis.h>
#else
	#include <nwsemaph.h>
#endif	//MPK_ON


// Name of console command to invoke perl
#define PERL_COMMAND_NAME "perl"

// Name of console command to load an NLM
#define LOAD_COMMAND "load"


typedef struct tagCommandLineParser
{
	BOOL    m_noScreen;
	BOOL	m_AutoDestroy;
	BOOL    m_isValid;

	int	    m_argc;
	int     m_argv_len;
	
	#ifdef MPK_ON
		SEMAPHORE	m_qSemaphore;
	#else
		long        m_qSemaphore;
	#endif

	char*   m_redirInName;
	char*   m_redirOutName;
	char*   m_redirErrName;
	char*   m_redirBothName;
	char*   nextarg;
	char*   sSkippedToken;

	char**  m_argv;
	char**  new_argv;

}COMMANDLINEPARSER, *PCOMMANDLINEPARSER;



char* fnSkipWhite(char* cptr);
char* fnNwGetEnvironmentStr(char *name, char *defaultvalue);
char* fnSkipToken(char *s, char *r);
char* fnScanToken(char* x, char *r);
char* fnStashString(char *s, char *r, int length);
void fnAppendArgument(PCOMMANDLINEPARSER pclp, char * new_arg);
void fnDeleteArgument(PCOMMANDLINEPARSER pclp, int index);
void fnCommandLineParser(PCOMMANDLINEPARSER pclp, char * commandLine, BOOL preserveQuotes);
void fnSystemCommand (char** argv, int argc);
void fnInternalPerlLaunchHandler(char* cmdLine);
char* fnMy_MkTemp(char* templatestr);


/* NWDEFPERLROOT:
 *  This symbol contains the name of the starting default directory to search
 *  for scripts to run.
 */
#define NWDEFPERLROOT "sys:\\perl\\scripts"

/* NWDEFPERLTEMP:
 *  This symbol contains the name of the default temp files directory.
 */
#define NWDEFPERLTEMP "sys:\\perl\\temp"


#endif	// __NWUtil_H__



/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	NWPipe.h
 * DESCRIPTION	:	Functions to implement pipes on NetWare.
 * Author		:	HYAK
 * Date			:	January 2001.
 *
 */



#ifndef __NWPipe_H__
#define __NWPipe_H__


#include "stdio.h"
#include "nwutil.h"

#define MAX_PIPE_RECURSION 256


typedef struct tagTempPipeFile
{
	BOOL	m_mode;		//  FALSE - Read mode  ;  TRUE - Write mode
	BOOL	m_launchPerl;
	BOOL	m_doPerlGlob;

	int		m_argv_len;

	char *	m_fileName;
	char**	m_argv;
	char *	m_redirect;

	#ifdef MPK_ON
		SEMAPHORE	m_perlSynchSemaphore;
	#else
		long		m_perlSynchSemaphore;
	#endif

	FILE*	m_file;
	PCOMMANDLINEPARSER	m_pipeCommand;

} TEMPPIPEFILE, *PTEMPPIPEFILE;


void fnPipeFileClose(PTEMPPIPEFILE ptpf);
void fnPipeFileDoPerlLaunch(PTEMPPIPEFILE ptpf);
BOOL fnPipeFileMakeArgv(PTEMPPIPEFILE ptpf);
FILE* fnPipeFileOpen(PTEMPPIPEFILE ptpf, char* command, char* mode);
void fnTempPipeFileReleaseMemory(PTEMPPIPEFILE ptpf);


#endif	// __NWPipe_H__


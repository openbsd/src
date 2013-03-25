
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	NWPipe.c
 * DESCRIPTION	:	Functions to implement pipes on NetWare.
 * Author		:	HYAK
 * Date			:	January 2001.
 *
 */



#include <nwadv.h>
#include <nwdsdefs.h>

#include "win32ish.h"
#include "nwpipe.h"
#include "nwplglob.h"


// This was added since the compile failed saying "undefined P_WAIT"
// when USE_ITHREADS was commented in the makefile
#ifndef P_WAIT
#define	P_WAIT		0
#endif

#ifndef P_NOWAIT
#define	P_NOWAIT	1
#endif




/*============================================================================================

 Function		:	fnPipeFileMakeArgv

 Description	:	This function makes the argument array.

 Parameters 	:	ptpf	(IN)	-	Input structure.

 Returns		:	Boolean.

==============================================================================================*/

BOOL fnPipeFileMakeArgv(PTEMPPIPEFILE ptpf)
{
	int i=0, j=0;
	int dindex = 0;
	int sindex = 0;

	ptpf->m_argv_len = 0;


	// Below 2 is added for the following reason:
	//   - The first one is for an additional value that will be added through ptpf->m_redirect.
	//   - The second one is for a NULL termination of the array.
	//     This is required for spawnvp API that takes a NULL-terminated array as its 3rd parameter.
	//     If the array is NOT NULL-terminated, then the server abends at the spawnvp call !!
	ptpf->m_argv = (char **) malloc((ptpf->m_pipeCommand->m_argc + 2) * sizeof(char*));
	if (ptpf->m_argv == NULL)
		return FALSE;

	// For memory allocation it is just +1 since the last one is only for NULL-termination
	// and no memory is required to be allocated.
	for(i=0; i<(ptpf->m_pipeCommand->m_argc + 1); i++)
	{
		ptpf->m_argv[i] = (char *) malloc(MAX_DN_BYTES * sizeof(char));
		if (ptpf->m_argv[i] == NULL)
		{
			for(j=0; j<i; j++)
			{
				if(ptpf->m_argv[j])
				{
					free(ptpf->m_argv[j]);
					ptpf->m_argv[j] = NULL;
				}
			}
			free(ptpf->m_argv);
			ptpf->m_argv = NULL;

			return FALSE;
		}
	}

	// Copy over parsed items, removing "load" keyword if necessary.
	sindex = ((stricmp(ptpf->m_pipeCommand->m_argv[0], LOAD_COMMAND) == 0) ? 1 : 0);
	while (sindex < ptpf->m_pipeCommand->m_argc)
	{
		strcpy(ptpf->m_argv[dindex], ptpf->m_pipeCommand->m_argv[sindex]);
		dindex++;
		sindex++;
	}

	if (stricmp(ptpf->m_argv[0], PERL_COMMAND_NAME) == 0)	// If Perl is the first command.
	{
		ptpf->m_launchPerl = TRUE;

		#ifdef MPK_ON
			ptpf->m_perlSynchSemaphore = kSemaphoreAlloc((BYTE *)"pipeSemaphore", 0);
		#else
			ptpf->m_perlSynchSemaphore = OpenLocalSemaphore(0);
		#endif	//MPK_ON
	}
	else if (stricmp(ptpf->m_argv[0], (char *)"perlglob") == 0)
		ptpf->m_doPerlGlob = TRUE;


	// Create last argument, which will redirect to or from the temp file
	if (!ptpf->m_doPerlGlob || ptpf->m_mode)
	{
		if (!ptpf->m_mode)	// If read mode?
		{
			if (ptpf->m_launchPerl)
				strcpy(ptpf->m_redirect, (char *)">");
			else
				strcpy(ptpf->m_redirect, (char *)"(CLIB_OPT)/>");
		}
		else
		{
			if (ptpf->m_launchPerl)
				strcpy(ptpf->m_redirect, (char *)"<");
			else
				strcpy(ptpf->m_redirect, (char *)"(CLIB_OPT)/<");
		}
		strcat(ptpf->m_redirect, ptpf->m_fileName);

		if (ptpf->m_launchPerl)
		{
			char tbuf[15] = {'\0'};
			sprintf(tbuf, (char *)" -{%x", ptpf->m_perlSynchSemaphore);
			strcat(ptpf->m_redirect, tbuf);
		}

		strcpy(ptpf->m_argv[dindex], (char*) ptpf->m_redirect);
		dindex++;
	}

	if (dindex < (ptpf->m_pipeCommand->m_argc + 1))
	{
		if(ptpf->m_argv[dindex])
		{
			free(ptpf->m_argv[dindex]);
			ptpf->m_argv[dindex] = NULL;	// NULL termination - required for  spawnvp  call.
		}
	}

	ptpf->m_argv_len = dindex;		// Length of the argv array  OR  number of argv string values.
	ptpf->m_argv[ptpf->m_argv_len] = NULL;	// NULL termination - required for  spawnvp  call.


	return TRUE;
}


/*============================================================================================

 Function		:	fnPipeFileOpen

 Description	:	This function opens the pipe file.

 Parameters 	:	ptpf	(IN)	-	Input structure.
					command	(IN)	-	Input command string.
					mode	(IN)	-	Mode of opening.

 Returns		:	File pointer.

==============================================================================================*/

FILE* fnPipeFileOpen(PTEMPPIPEFILE ptpf, char* command, char* mode)
{
	int i=0, j=0;

	char tempName[_MAX_PATH] = {'\0'};


	ptpf->m_fileName = (char *) malloc(_MAX_PATH * sizeof(char));
	if(ptpf->m_fileName == NULL)
		return NULL;

	// The char array is emptied so that there is no junk characters.
	strncpy(ptpf->m_fileName, "", (_MAX_PATH * sizeof(char)));
	

	// Save off stuff
	//
	if(strchr(mode,'r') != 0)
		ptpf->m_mode = FALSE;	// Read mode
	else if(strchr(mode,'w') != 0)
		ptpf->m_mode = TRUE;	// Write mode
	else
	{
		if(ptpf->m_fileName != NULL)
		{
//			if (strlen(ptpf->m_fileName))
			if (ptpf->m_fileName)
				unlink(ptpf->m_fileName);

			free(ptpf->m_fileName);
			ptpf->m_fileName = NULL;
		}

		return NULL;
	}


	ptpf->m_pipeCommand = (PCOMMANDLINEPARSER) malloc(sizeof(COMMANDLINEPARSER));
	if (!ptpf->m_pipeCommand)
	{
//		if (strlen(ptpf->m_fileName))
		if (ptpf->m_fileName)
			unlink(ptpf->m_fileName);

		free(ptpf->m_fileName);
		ptpf->m_fileName = NULL;

		return NULL;
	}

	// Initialise the variables
	ptpf->m_pipeCommand->m_isValid = TRUE;

/****
// Commented since these are not being used.  Still retained here.
// To be removed once things are proved to be working fine to a good confident level,

	ptpf->m_pipeCommand->m_redirInName = NULL;
	ptpf->m_pipeCommand->m_redirOutName = NULL;
	ptpf->m_pipeCommand->m_redirErrName = NULL;
	ptpf->m_pipeCommand->m_redirBothName = NULL;
	ptpf->m_pipeCommand->nextarg = NULL;
****/

	ptpf->m_pipeCommand->sSkippedToken = NULL;
	ptpf->m_pipeCommand->m_argv = NULL;
	ptpf->m_pipeCommand->new_argv = NULL;

	#ifdef MPK_ON
		ptpf->m_pipeCommand->m_qSemaphore = NULL;
	#else
		ptpf->m_pipeCommand->m_qSemaphore = 0L;
	#endif	//MPK_ON

	ptpf->m_pipeCommand->m_noScreen = 0;
	ptpf->m_pipeCommand->m_AutoDestroy = 0;
	ptpf->m_pipeCommand->m_argc = 0;
	ptpf->m_pipeCommand->m_argv_len = 1;


	ptpf->m_pipeCommand->m_argv = (char **) malloc(ptpf->m_pipeCommand->m_argv_len * sizeof(char *));
	if (ptpf->m_pipeCommand->m_argv == NULL)
	{
		free(ptpf->m_pipeCommand);
		ptpf->m_pipeCommand = NULL;

//		if (strlen(ptpf->m_fileName))
		if (ptpf->m_fileName)
			unlink(ptpf->m_fileName);

		free(ptpf->m_fileName);
		ptpf->m_fileName = NULL;

		return NULL;
	}
	ptpf->m_pipeCommand->m_argv[0] = (char *) malloc(MAX_DN_BYTES * sizeof(char));
	if (ptpf->m_pipeCommand->m_argv[0] == NULL)
	{
		for(j=0; j<i; j++)
		{
			if(ptpf->m_pipeCommand->m_argv[j])
			{
				free(ptpf->m_pipeCommand->m_argv[j]);
				ptpf->m_pipeCommand->m_argv[j]=NULL;
			}
		}
		free(ptpf->m_pipeCommand->m_argv);
		ptpf->m_pipeCommand->m_argv=NULL;

		free(ptpf->m_pipeCommand);
		ptpf->m_pipeCommand = NULL;

//		if (strlen(ptpf->m_fileName))
		if (ptpf->m_fileName)
			unlink(ptpf->m_fileName);

		free(ptpf->m_fileName);
		ptpf->m_fileName = NULL;

		return NULL;
	}


	ptpf->m_redirect = (char *) malloc(MAX_DN_BYTES * sizeof(char));
	if (ptpf->m_redirect == NULL)
	{
		for(i=0; i<ptpf->m_pipeCommand->m_argv_len; i++)
		{
			if(ptpf->m_pipeCommand->m_argv[i] != NULL)
			{
				free(ptpf->m_pipeCommand->m_argv[i]);
				ptpf->m_pipeCommand->m_argv[i] = NULL;
			}
		}

		free(ptpf->m_pipeCommand->m_argv);
		ptpf->m_pipeCommand->m_argv = NULL;

		free(ptpf->m_pipeCommand);
		ptpf->m_pipeCommand = NULL;


//		if (strlen(ptpf->m_fileName))
		if (ptpf->m_fileName)
			unlink(ptpf->m_fileName);

		free(ptpf->m_fileName);
		ptpf->m_fileName = NULL;

		return NULL;
	}

	// The char array is emptied.
	// If it is not done so, then it could contain some junk values and the string length in that case
	// will not be zero.  This causes erroneous results in  fnPipeFileMakeArgv()  function
	// where  strlen(ptpf->m_redirect)  is used as a check for incrementing the parameter count and
	// it will wrongly get incremented in such cases.
	strncpy(ptpf->m_redirect, "", (MAX_DN_BYTES * sizeof(char)));

	// Parse the parameters.
	fnCommandLineParser(ptpf->m_pipeCommand, (char *)command, TRUE);
	if (!ptpf->m_pipeCommand->m_isValid)
	{
		fnTempPipeFileReleaseMemory(ptpf);
		return NULL;
	}


	// Create a temporary file name
	//
	strncpy ( tempName, fnNwGetEnvironmentStr((char *)"TEMP", NWDEFPERLTEMP), (_MAX_PATH - 20) );
	tempName[_MAX_PATH-20] = '\0';
	strcat(tempName, (char *)"\\plXXXXXX.tmp");
	if (!fnMy_MkTemp(tempName))
	{
		fnTempPipeFileReleaseMemory(ptpf);
		return NULL;
	}

	// create a temporary place-holder file
	fclose(fopen(tempName, (char *)"w"));
	strcpy(ptpf->m_fileName, tempName);


	// Make the argument array
	if(!fnPipeFileMakeArgv(ptpf))
	{
		fnTempPipeFileReleaseMemory(ptpf);

		// Release additional memory
		if(ptpf->m_argv != NULL)
		{
			for(i=0; i<ptpf->m_argv_len; i++)
			{
				if(ptpf->m_argv[i] != NULL)
				{
					free(ptpf->m_argv[i]);
					ptpf->m_argv[i] = NULL;
				}
			}

			free(ptpf->m_argv);
			ptpf->m_argv = NULL;
		}

		return NULL;
	}


	// Open the temp file in the appropriate way...
	//
	if (!ptpf->m_mode)	// If Read mode?
	{
		// we wish to spawn a command, intercept its output,
		// and then get that output
		//
		if (!ptpf->m_argv[0])
		{
			fnTempPipeFileReleaseMemory(ptpf);

			// Release additional memory
			if(ptpf->m_argv != NULL)
			{
				for(i=0; i<ptpf->m_argv_len; i++)
				{
					if(ptpf->m_argv[i] != NULL)
					{
						free(ptpf->m_argv[i]);
						ptpf->m_argv[i] = NULL;
					}
				}

				free(ptpf->m_argv);
				ptpf->m_argv = NULL;
			}

			return NULL;
		}

		if (ptpf->m_launchPerl)
			fnPipeFileDoPerlLaunch(ptpf);
		else
			if (ptpf->m_doPerlGlob)
				fnDoPerlGlob(ptpf->m_argv, ptpf->m_fileName);	// hack to do perl globbing
		else
			spawnvp(P_WAIT, ptpf->m_argv[0], ptpf->m_argv);

		ptpf->m_file = fopen (ptpf->m_fileName, (char *)"r");	// Get the Pipe file handle
	}
	else if (ptpf->m_mode)	// If Write mode?
	{
		// we wish to open the file for writing now and
		// do the command later
		//
		ptpf->m_file = fopen(ptpf->m_fileName, (char *)"w");
	}

	fnTempPipeFileReleaseMemory(ptpf);

	// Release additional memory
	if(ptpf->m_argv != NULL)
	{
		for(i=0; i<(ptpf->m_argv_len); i++)
		{
			if(ptpf->m_argv[i] != NULL)
			{
				free(ptpf->m_argv[i]);
				ptpf->m_argv[i] = NULL;
			}
		}

		free(ptpf->m_argv);
		ptpf->m_argv = NULL;
	}

		
	return ptpf->m_file;	// Return the Pipe file handle.
}


/*============================================================================================

 Function		:	fnPipeFileClose

 Description	:	This function closes the pipe file.

 Parameters 	:	ptpf	(IN)	-	Input structure.

 Returns		:	Nothing.

==============================================================================================*/

void fnPipeFileClose(PTEMPPIPEFILE ptpf)
{
	int i = 0;

	if (ptpf->m_mode)	// If Write mode?
	{
		// we wish to spawn a command using our temp file for
		// its input
		//
		if(ptpf->m_file != NULL)
		{
			fclose (ptpf->m_file);
			ptpf->m_file = NULL;
		}

		if (ptpf->m_launchPerl)
			fnPipeFileDoPerlLaunch(ptpf);
		else if (ptpf->m_argv)
			spawnvp(P_WAIT, ptpf->m_argv[0], ptpf->m_argv);
	}


	// Close the temporary Pipe File, if opened
	if (ptpf->m_file)
	{
		fclose(ptpf->m_file);
		ptpf->m_file = NULL;
	}
	// Delete the temporary Pipe Filename if still valid and free the memory associated with the file name.
	if(ptpf->m_fileName != NULL)
	{
//		if (strlen(ptpf->m_fileName))
		if (ptpf->m_fileName)
			unlink(ptpf->m_fileName);

		free(ptpf->m_fileName);
		ptpf->m_fileName = NULL;
	}

/**
	if(ptpf->m_argv != NULL)
	{
		for(i=0; i<(ptpf->m_argv_len); i++)
		{
			if(ptpf->m_argv[i] != NULL)
			{
				free(ptpf->m_argv[i]);
				ptpf->m_argv[i] = NULL;
			}
		}

		free(ptpf->m_argv);
		ptpf->m_argv = NULL;
	}
**/

	if (ptpf->m_perlSynchSemaphore)
	{
		#ifdef MPK_ON
			kSemaphoreFree(ptpf->m_perlSynchSemaphore);
		#else
			CloseLocalSemaphore(ptpf->m_perlSynchSemaphore);
		#endif	//MPK_ON
	}


	return;
}


/*============================================================================================

 Function		:	fnPipeFileDoPerlLaunch

 Description	:	This function launches Perl.

 Parameters 	:	ptpf	(IN)	-	Input structure.

 Returns		:	Nothing.

==============================================================================================*/

void fnPipeFileDoPerlLaunch(PTEMPPIPEFILE ptpf)
{
	char curdir[_MAX_PATH] = {'\0'};
	char* pcwd = NULL;

	int i=0;


	// save off the current working directory to restore later
	// this is just a hack! these problems of synchronization and
	// restoring calling context need a much better solution!
	pcwd = (char *)getcwd(curdir, sizeof(curdir)-1);
	fnSystemCommand(ptpf->m_argv, ptpf->m_argv_len);
	if (ptpf->m_perlSynchSemaphore)
	{
		#ifdef MPK_ON
			kSemaphoreWait(ptpf->m_perlSynchSemaphore);
		#else
			WaitOnLocalSemaphore(ptpf->m_perlSynchSemaphore);
		#endif	//MPK_ON
	}

	if (pcwd)
		chdir(pcwd);

	return;
}


/*============================================================================================

 Function		:	fnTempPipeFile

 Description	:	This function initialises the variables of the structure passed in.

 Parameters 	:	ptpf	(IN)	-	Input structure.

 Returns		:	Nothing.

==============================================================================================*/

void fnTempPipeFile(PTEMPPIPEFILE ptpf)
{
	ptpf->m_fileName = NULL;

	ptpf->m_mode = FALSE;	// Default mode = Read mode.
	ptpf->m_file = NULL;
	ptpf->m_pipeCommand = NULL;
	ptpf->m_argv = NULL;

	ptpf->m_redirect = NULL;

	ptpf->m_launchPerl = FALSE;
	ptpf->m_doPerlGlob = FALSE;

	#ifdef MPK_ON
		ptpf->m_perlSynchSemaphore = NULL;
	#else
		ptpf->m_perlSynchSemaphore = 0L;
	#endif

	ptpf->m_argv_len = 0;

	return;
}


/*============================================================================================

 Function		:	fnTempPipeFileReleaseMemory

 Description	:	This function frees the memory allocated to various buffers.

 Parameters 	:	ptpf	(IN)	-	Input structure.

 Returns		:	Nothing.

==============================================================================================*/

void fnTempPipeFileReleaseMemory(PTEMPPIPEFILE ptpf)
{
	int i=0;


	if (ptpf->m_pipeCommand)
	{
		if(ptpf->m_pipeCommand->m_argv != NULL)
		{
			for(i=0; i<ptpf->m_pipeCommand->m_argv_len; i++)
			{
				if(ptpf->m_pipeCommand->m_argv[i] != NULL)
				{
					free(ptpf->m_pipeCommand->m_argv[i]);
					ptpf->m_pipeCommand->m_argv[i] = NULL;
				}
			}

			free(ptpf->m_pipeCommand->m_argv);
			ptpf->m_pipeCommand->m_argv = NULL;
		}

		if(ptpf->m_pipeCommand->sSkippedToken != NULL)
		{
			free(ptpf->m_pipeCommand->sSkippedToken);
			ptpf->m_pipeCommand->sSkippedToken = NULL;
		}
/****
// Commented since these are not being used.  Still retained here.
// To be removed once things are proved to be working fine to a good confident level,

		if(ptpf->m_pipeCommand->nextarg)
		{
			free(ptpf->m_pipeCommand->nextarg);
			ptpf->m_pipeCommand->nextarg = NULL;
		}

		if(ptpf->m_pipeCommand->m_redirInName)
		{
			free(ptpf->m_pipeCommand->m_redirInName);
			ptpf->m_pipeCommand->m_redirInName = NULL;
		}
		if(ptpf->m_pipeCommand->m_redirOutName)
		{
			free(ptpf->m_pipeCommand->m_redirOutName);
			ptpf->m_pipeCommand->m_redirOutName = NULL;
		}
		if(ptpf->m_pipeCommand->m_redirErrName)
		{
			free(ptpf->m_pipeCommand->m_redirErrName);
			ptpf->m_pipeCommand->m_redirErrName = NULL;
		}
		if(ptpf->m_pipeCommand->m_redirBothName)
		{
			free(ptpf->m_pipeCommand->m_redirBothName);
			ptpf->m_pipeCommand->m_redirBothName = NULL;
		}
****/

		if(ptpf->m_pipeCommand != NULL)
		{
			free(ptpf->m_pipeCommand);
			ptpf->m_pipeCommand = NULL;
		}
	}

	if(ptpf->m_redirect != NULL)
	{
		free(ptpf->m_redirect);
		ptpf->m_redirect = NULL;
	}

	return;
}



/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	NWMain.c
 * DESCRIPTION	:	Main function, Commandline handlers and shutdown for NetWare implementation of Perl.
 * Author		:	HYAK, SGP
 * Date			:	January 2001.
 *
 */



#ifdef NLM
#define N_PLAT_NLM
#endif

#undef BYTE
#define BYTE char


#include <nwadv.h>
#include <signal.h>
#include <nwdsdefs.h>

#include "perl.h"
#include "nwutil.h"
#include "stdio.h"
#include "clibstuf.h"

#ifdef MPK_ON
	#include <mpktypes.h>
	#include <mpkapis.h>
#endif	//MPK_ON


// Thread group ID for this NLM. Set only by main when the NLM is initially loaded,
// so it should be okay for this to be global.
//
#ifdef MPK_ON
	THREAD	gThreadHandle;
#else
	int gThreadGroupID = -1;
#endif	//MPK_ON


// Global to kill all running scripts during NLM unload.
//
bool gKillAll = FALSE;


// Global structure needed by OS to register command parser.
// fnRegisterCommandLineHandler gets called only when the NLM is initially loaded,
// so it should be okay for this structure to be a global.
//
static struct commandParserStructure gCmdParser = {0,0,0};


// True if the command-line parsing procedure has been registered with the OS.
// Altered only during initial NLM loading or unloading so it should be okay as a global.
//
BOOL gCmdProcInit = FALSE;


// Array to hold the screen name for all new screens.
//
char sPerlScreenName[MAX_DN_BYTES * sizeof(char)] = {'\0'};


// Structure to pass data when spawning new threadgroups to run scripts.
//
typedef struct tagScriptData
{
	char *m_commandLine;
	BOOL m_fromConsole;
}ScriptData;


#define  CS_CMD_NOT_FOUND	-1		// Console command not found
#define  CS_CMD_FOUND		0		// Console command found

/**
  The stack size is make 256k from the earlier 64k since complex scripts (charnames.t and complex.t)
  were failing with the lower stack size. In fact, we tested with 128k and it also failed
  for the complexity of the script used. In case the complexity of a script is increased,
  then this might warrant an increase in the stack size. But instead of simply giving  a very large stack,
  a trade off was required and we stopped at 256k!
**/
#define PERL_COMMAND_STACK_SIZE (256*1024L)	// Stack size of thread that runs a perl script from command line

#define MAX_COMMAND_SIZE 512


#define kMaxValueLen 1024	// Size of the Environment variable value limited/truncated to 1024 characters.
#define kMaxVariableNameLen 256		// Size of the Environment variable name.


typedef void (*PFUSEACCURATECASEFORPATHS) (int);
typedef LONG (*PFGETFILESERVERMAJORVERSIONNUMBER) (void);
typedef void (*PFUCSTERMINATE) ();		// For ucs terminate.
typedef void (*PFUNAUGMENTASTERISK)(BOOL);		// For longfile support.
typedef int (*PFFSETMODE) (FILE *, char *);


// local function prototypes
//
void fnSigTermHandler(int sig);
void fnRegisterCommandLineHandler(void);
void fnLaunchPerl(void* context);
void fnSetUpEnvBlock(char*** penv);
void fnDestroyEnvBlock(char** env);
int fnFpSetMode(FILE* fp, int mode, int *err);

void fnGetPerlScreenName(char *sPerlScreenName);

void fnGetPerlScreenName(char *sPerlScreenName);
void fnSetupNamespace(void); 
char *getcwd(char [], int); 
void fnRunScript(ScriptData* psdata);
void nw_freeenviron();


/*============================================================================================

 Function		:	main

 Description	:	Called when the NLM is first loaded. Registers the command-line handler
								and then terminates-stay-resident.

 Parameters		:	argc	(IN)	-	No of  Input  strings.
								argv	(IN)	-	Array of  Input  strings.

 Returns		:	Nothing.

==============================================================================================*/

void main(int argc, char *argv[]) 
{
	char sysCmdLine[MAX_COMMAND_SIZE] = {'\0'};
	char cmdLineCopy[sizeof(PERL_COMMAND_NAME)+sizeof(sysCmdLine)+2] = {'\0'};

	ScriptData* psdata = NULL;


	// Keep this thread alive, since we use the thread group id of this thread to allocate memory on.
	// When we unload the NLM, clib will tear the thread down.
	//
	#ifdef MPK_ON
		gThreadHandle = kCurrentThread();
	#else
		gThreadGroupID = GetThreadGroupID ();
	#endif	//MPK_ON

	signal (SIGTERM, fnSigTermHandler);
	fnInitGpfGlobals();		// For importing the CLIB calls in place of the Watcom calls
	fnInitializeThreadInfo();


//	Ensure that we have a "temp" directory
	fnSetupNamespace();
	if (access(NWDEFPERLTEMP, 0) != 0)
		mkdir(NWDEFPERLTEMP);

	// Create the file NUL if not present. This is done only once per NLM load.
	// This is required for -e.
	// Earlier verions were creating temporary files (in perl.c file) for -e.
	// Now, the technique of creating temporary files are removed since they were
	// fragile or insecure or slow. It now uses the memory by setting
	// the BIT_BUCKET to "nul" on Win32, which is equivalent to /dev/nul of Unix.
	// Since there is no equivalent of /dev/nul on NetWare, the work-around is that
	// we create a file called "nul" and the BIT_BUCKET is set to "nul".
	// This makes sure that -e works on NetWare too without the creation of temporary files
	// in -e code in perl.c
	{
		char sNUL[MAX_DN_BYTES] = {'\0'};

		strcpy(sNUL, NWDEFPERLROOT);
		strcat(sNUL, "\\nwnul");
		if (access((const char *)sNUL, 0) != 0)
		{
			// The file, "nul" is not found and so create the file.
			FILE *fp = NULL;

			fp = fopen((const char *)sNUL, (const char *)"w");
			fclose(fp);
		}
	}

	fnRegisterCommandLineHandler();		// Register the command line handler
	SynchronizeStart();		// Restart the NLM startup process when using synchronization mode.

	fnGetPerlScreenName(sPerlScreenName);	// Get the screen name. Done only once per NLM load.


	// If the command line has two strings, then the first has to be "Perl" and the second is assumed
	// to be a script to be run. If only one string (i.e., Perl) is input, then there is nothing to do!
	//
	if ((argc > 1) && getcmd(sysCmdLine))
	{
		strcpy(cmdLineCopy, PERL_COMMAND_NAME);
		strcat(cmdLineCopy, (char *)" ");	// Space between the Perl Command and the input script name.
		strcat(cmdLineCopy, sysCmdLine);	// The command line parameters built into 

		// Create a safe copy of the command line and pass it to the
		// new thread for parsing. The new thread will be responsible
		// to delete it when it is finished with it.
		//
		psdata = (ScriptData *) malloc(sizeof(ScriptData));
		if (psdata)
		{
			psdata->m_commandLine = NULL;
			psdata->m_commandLine = (char *) malloc(MAX_DN_BYTES * sizeof(char));
			if(psdata->m_commandLine)
			{
				strcpy(psdata->m_commandLine, cmdLineCopy);
				psdata->m_fromConsole = TRUE;

				#ifdef MPK_ON
//					kStartThread((char *)"ConsoleHandlerThread", fnLaunchPerl, NULL, PERL_COMMAND_STACK_SIZE, (void *)psdata);
					// Establish a new thread within a new thread group.
					BeginThreadGroup(fnLaunchPerl, NULL, PERL_COMMAND_STACK_SIZE, (void*)psdata);
				#else
					// Start a new thread in its own thread group
					BeginThreadGroup(fnLaunchPerl, NULL, PERL_COMMAND_STACK_SIZE, (void*)psdata);
				#endif	//MPK_ON
			}
			else
			{
				free(psdata);
				psdata = NULL;
				return;
			}
		}
		else
			return;
	}


	// Keep this thread alive, since we use the thread group id of this thread to allocate memory on.
	// When we unload the NLM, clib will tear the thread down.
	//
	#ifdef MPK_ON
		kSuspendThread(gThreadHandle);
	#else
		SuspendThread(GetThreadID());
	#endif	//MPK_ON


	return;
}



/*============================================================================================

 Function		:	fnSigTermHandler

 Description	:	Called when the NLM is unloaded; used to unregister the	console command handler.

 Parameters		:	sig		(IN)

 Returns		:	Nothing.

==============================================================================================*/

void fnSigTermHandler(int sig)
{
	int k = 0;


	#ifdef MPK_ON
		kResumeThread(gThreadHandle);
	#endif	//MPK_ON

	// Unregister the command line handler.
	//
	if (gCmdProcInit)
	{
		UnRegisterConsoleCommand (&gCmdParser);
		gCmdProcInit = FALSE;
	}

	// Free the global environ buffer
	nw_freeenviron();

	// Kill running scripts.
	//
	if (!fnTerminateThreadInfo())
	{
		ConsolePrintf("Terminating Perl scripts...\n");
		gKillAll = TRUE;

		// fnTerminateThreadInfo will be run for 5 threads. If more threads/scripts are run,
		// then the NLM will unload without terminating the thread info and leaks more memory.
		// If this number is increased to reduce memory leaks, then it will unnecessarily take more time
		// to unload when there are a smaller no of threads. Since this is a rare case, the no is kept as 5.
		//
		while (!fnTerminateThreadInfo() && k < 5)
		{
			nw_sleep(1);
			k++;
		}
	}

	// Delete the file, "nul" if present since the NLM is unloaded.
	{
		char sNUL[MAX_DN_BYTES] = {'\0'};

		strcpy(sNUL, NWDEFPERLROOT);
		strcat(sNUL, "\\nwnul");
		if (access((const char *)sNUL, 0) == 0)
		{
			// The file, "nul" is found and so delete it.
			unlink((const char *)sNUL);
		}
	}
}



/*============================================================================================

 Function		:	fnCommandLineHandler

 Description	:	Gets called by OS when someone enters an unknown command at the system console,
					after this routine is registered by RegisterConsoleCommand.
					For the valid command we just spawn	a thread with enough stack space
					to actually run the script.

 Parameters		:	screenID	(IN)	-	id for the screen.
								cmdLine		(IN)	-	Command line string.

 Returns		:	Long.

==============================================================================================*/

LONG  fnCommandLineHandler (LONG screenID, BYTE * cmdLine)
{
	ScriptData* psdata=NULL;
	int OsThrdGrpID = -1;
	LONG retCode = CS_CMD_FOUND;
	char* cptr = NULL;


	#ifdef MPK_ON
		// Initialisation for MPK_ON
	#else
		OsThrdGrpID = -1;
	#endif	//MPK_ON


	#ifdef MPK_ON
		// For MPK_ON
	#else
		if (gThreadGroupID != -1)
			OsThrdGrpID = SetThreadGroupID (gThreadGroupID);
	#endif	//MPK_ON


	cptr = fnSkipWhite(cmdLine);	// Skip white spaces.
	if ((strnicmp(cptr, PERL_COMMAND_NAME, strlen(PERL_COMMAND_NAME)) == 0) &&
		 ((cptr[strlen(PERL_COMMAND_NAME)] == ' ') ||
		 (cptr[strlen(PERL_COMMAND_NAME)] == '\t') ||
		 (cptr[strlen(PERL_COMMAND_NAME)] == '\0')))
	{
		// Create a safe copy of the command line and pass it to the new thread for parsing.
		// The new thread will be responsible to delete it when it is finished with it.
		//
		psdata = (ScriptData *) malloc(sizeof(ScriptData));
		if (psdata)
		{
			psdata->m_commandLine = NULL;
			psdata->m_commandLine = (char *) malloc(MAX_DN_BYTES * sizeof(char));
			if(psdata->m_commandLine)
			{
				strcpy(psdata->m_commandLine, (char *)cmdLine);
				psdata->m_fromConsole = TRUE;

				#ifdef MPK_ON
//					kStartThread((char *)"ConsoleHandlerThread", fnLaunchPerl, NULL, PERL_COMMAND_STACK_SIZE, (void *)psdata);
					// Establish a new thread within a new thread group.
					BeginThreadGroup(fnLaunchPerl, NULL, PERL_COMMAND_STACK_SIZE, (void*)psdata);
				#else
					// Start a new thread in its own thread group
					BeginThreadGroup(fnLaunchPerl, NULL, PERL_COMMAND_STACK_SIZE, (void*)psdata);
				#endif	//MPK_ON
			}
			else
			{
				free(psdata);
				psdata = NULL;
				retCode = CS_CMD_NOT_FOUND;
			}
		}
		else
			retCode = CS_CMD_NOT_FOUND;
	}
	else
		retCode = CS_CMD_NOT_FOUND;


	#ifdef MPK_ON
		// For MPK_ON
	#else
		if (OsThrdGrpID != -1)
			SetThreadGroupID (OsThrdGrpID);
	#endif	//MPK_ON


	return retCode;
}



/*============================================================================================

 Function		:	fnRegisterCommandLineHandler

 Description	:	Registers the console command-line parsing function with the OS.

 Parameters		:	None.

 Returns		:	Nothing.

==============================================================================================*/

void fnRegisterCommandLineHandler(void)
{
	// Allocates resource tag for Console Command
	if ((gCmdParser.RTag =
		AllocateResourceTag (GetNLMHandle(), (char *)"Console Command", ConsoleCommandSignature)) != 0)
	{
		gCmdParser.parseRoutine = fnCommandLineHandler;		// Set the Console Command parsing routine.
		RegisterConsoleCommand (&gCmdParser);		// Registers the Console Command parsing function
		gCmdProcInit = TRUE;
	}

	return;
}



/*============================================================================================

 Function		:	fnSetupNamespace

 Description	:	Sets the name space of the current threadgroup to the long name space.

 Parameters		:	None.

 Returns		:	Nothing.

==============================================================================================*/

void fnSetupNamespace(void)
{
	SetCurrentNameSpace(NWOS2_NAME_SPACE);


	//LATER: call SetTargetNameSpace(NWOS2_NAME_SPACE)? Currently, if
	// I make this call, then CPerlExe::Rename fails in certain cases,
	// and it isn't clear why. Looks like a CLIB bug...
//	SetTargetNameSpace(NWOS2_NAME_SPACE); 

	//Uncommented that above call, retaining the comment so that it will be easy 
	//to revert back if there is any problem - sgp - 10th May 2000

	//Commented again, since Perl debugger had some problems because of
	//the above call - sgp - 20th June 2000

	{
		// if running on Moab, call UseAccurateCaseForPaths. This API
		// does bad things on 4.11 so we call only for Moab.
		PFGETFILESERVERMAJORVERSIONNUMBER pf_getfileservermajorversionnumber = NULL;
		pf_getfileservermajorversionnumber = (PFGETFILESERVERMAJORVERSIONNUMBER) 
		ImportSymbol(GetNLMHandle(), (char *)"GetFileServerMajorVersionNumber");
		if (pf_getfileservermajorversionnumber && ((*pf_getfileservermajorversionnumber)() > 4))
		{
			PFUSEACCURATECASEFORPATHS pf_useaccuratecaseforpaths = NULL;
			pf_useaccuratecaseforpaths = (PFUSEACCURATECASEFORPATHS) 
			ImportSymbol(GetNLMHandle(), (char *)"UseAccurateCaseForPaths");
			if (pf_useaccuratecaseforpaths)
				(*pf_useaccuratecaseforpaths)(TRUE);
			{
				PFUNAUGMENTASTERISK pf_unaugmentasterisk = NULL;
				pf_unaugmentasterisk = (PFUNAUGMENTASTERISK)
				ImportSymbol(GetNLMHandle(), (char *)"UnAugmentAsterisk");
				if (pf_unaugmentasterisk)
					(*pf_unaugmentasterisk)(TRUE);
			}
		}
	}

	return;
}



/*============================================================================================

 Function		:	fnLaunchPerl

 Description	:	Parse the command line into argc/argv style parameters and then run the script.

 Parameters		:	context	(IN)	-	void* that will be typecasted to ScriptDate structure.

 Returns		:	Nothing.

==============================================================================================*/

void fnLaunchPerl(void* context)
{
	char* defaultDir = NULL;
	char curdir[_MAX_PATH] = {'\0'};
	ScriptData* psdata = (ScriptData *) context;

	unsigned int moduleHandle = 0;
	int currentThreadGroupID = -1;

	#ifdef MPK_ON
		kExitNetWare();
	#endif	//MPK_ON

	errno = 0;

	if (psdata->m_fromConsole)
	{
		// get the default working directory name
		//
		defaultDir = fnNwGetEnvironmentStr("PERL_ROOT", NWDEFPERLROOT);
	}
	else
		defaultDir = getcwd(curdir, sizeof(curdir)-1);

	// set long name space
	//
	fnSetupNamespace();

	// make the working directory the current directory if from console
	//
	if (psdata->m_fromConsole)
		chdir(defaultDir);

	// run the script
	//
	fnRunScript(psdata);

	// May have to check this, I am blindly calling UCSTerminate, irrespective of
	// whether it is initialized or not
	// Copied from the previous Perl - sgp - 31st Oct 2000
	moduleHandle = FindNLMHandle("UCSCORE.NLM");
	if (moduleHandle)
	{
		PFUCSTERMINATE ucsterminate = (PFUCSTERMINATE)ImportSymbol(moduleHandle, "therealUCSTerminate");
		if (ucsterminate!=NULL)
			(*ucsterminate)();
	}

	if (psdata->m_fromConsole)
	{
		// change thread groups for the call to free the memory
		// allocated before the new thread group was started
		#ifdef MPK_ON
			// For MPK_ON
		#else
			if (gThreadGroupID != -1)
				currentThreadGroupID = SetThreadGroupID (gThreadGroupID);
		#endif	//MPK_ON
	}

	// Free memory
	if (psdata)
	{
		if(psdata->m_commandLine)
		{
			free(psdata->m_commandLine);
			psdata->m_commandLine = NULL;
		}

		free(psdata);
		psdata = NULL;
		context = NULL;
	}

	#ifdef MPK_ON
		// For MPK_ON
	#else
		if (currentThreadGroupID != -1)
			SetThreadGroupID (currentThreadGroupID);
	#endif	//MPK_ON

	#ifdef MPK_ON
//		kExitThread(NULL);
	#else
		// just let the thread terminate by falling off the end of the
		// function started by BeginThreadGroup
//		ExitThread(EXIT_THREAD, 0);
	#endif

	return;
}



/*============================================================================================

 Function		:	fnRunScript

 Description	:	Parses and runs a perl script.

 Parameters		:	psdata	(IN)	-	ScriptData structure.

 Returns		:	Nothing.

==============================================================================================*/

void fnRunScript(ScriptData* psdata)
{
	char **av=NULL;
	char **en=NULL;
	int exitstatus = 1;
	int i=0, j=0;
	int *dummy = 0;

	PCOMMANDLINEPARSER pclp = NULL;

	// Set up the environment block. This will only work on
	// on Moab; on 4.11 the environment block will be empty.
	char** env = NULL;

	BOOL use_system_console = TRUE;
	BOOL newscreen = FALSE;
	int newscreenhandle = 0;

	// redirect stdin or stdout and run the script
	FILE* redirOut = NULL;
	FILE* redirIn = NULL;
	FILE* redirErr = NULL;
	FILE* stderr_fp = NULL;

	int stdin_fd=-1, stdin_fd_dup=-1;
	int stdout_fd=-1, stdout_fd_dup=-1;
	int stderr_fd=-1, stderr_fd_dup=-1;


	// Main callback instance
	//
	if (fnRegisterWithThreadTable() == FALSE)
		return;

	// parse the command line into argc/argv style:
	// number of params and char array of params
	//
	pclp = (PCOMMANDLINEPARSER) malloc(sizeof(COMMANDLINEPARSER));
	if (!pclp)
	{
		fnUnregisterWithThreadTable();
		return;
	}

	// Initialise the variables
	pclp->m_isValid = TRUE;
	pclp->m_redirInName = NULL;
	pclp->m_redirOutName = NULL;
	pclp->m_redirErrName = NULL;
	pclp->m_redirBothName = NULL;
	pclp->nextarg = NULL;
	pclp->sSkippedToken = NULL;
	pclp->m_argv = NULL;
	pclp->new_argv = NULL;

	#ifdef MPK_ON
		pclp->m_qSemaphore = NULL;
	#else
		pclp->m_qSemaphore = 0L;
	#endif	//MPK_ON

	pclp->m_noScreen = 0;
	pclp->m_AutoDestroy = 0;
	pclp->m_argc = 0;
	pclp->m_argv_len = 1;

	// Allocate memory
	pclp->m_argv = (char **) malloc(pclp->m_argv_len * sizeof(char *));
	if (pclp->m_argv == NULL)
	{
		free(pclp);
		pclp = NULL;

		fnUnregisterWithThreadTable();
		return;
	}

	pclp->m_argv[0] = (char *) malloc(MAX_DN_BYTES * sizeof(char));
	if (pclp->m_argv[0] == NULL)
	{
		free(pclp->m_argv);
		pclp->m_argv=NULL;

		free(pclp);
		pclp = NULL;

		fnUnregisterWithThreadTable();
		return;
	}

	// Parse the command line
	fnCommandLineParser(pclp, (char *)psdata->m_commandLine, FALSE);
	if (!pclp->m_isValid)
	{
		if(pclp->m_argv)
		{
			for(i=0; i<pclp->m_argv_len; i++)
			{
				if(pclp->m_argv[i] != NULL)
				{
					free(pclp->m_argv[i]);
					pclp->m_argv[i] = NULL;
				}
			}

			free(pclp->m_argv);
			pclp->m_argv = NULL;
		}

		if(pclp->nextarg)
		{
			free(pclp->nextarg);
			pclp->nextarg = NULL;
		}
		if(pclp->sSkippedToken != NULL)
		{
			free(pclp->sSkippedToken);
			pclp->sSkippedToken = NULL;
		}

		if(pclp->m_redirInName)
		{
			free(pclp->m_redirInName);
			pclp->m_redirInName = NULL;
		}
		if(pclp->m_redirOutName)
		{
			free(pclp->m_redirOutName);
			pclp->m_redirOutName = NULL;
		}
		if(pclp->m_redirErrName)
		{
			free(pclp->m_redirErrName);
			pclp->m_redirErrName = NULL;
		}
		if(pclp->m_redirBothName)
		{
			free(pclp->m_redirBothName);
			pclp->m_redirBothName = NULL;
		}

		// Signal a semaphore, if indicated by "-{" option, to indicate that
		// the script has terminated and files are closed
		//
		if (pclp->m_qSemaphore != 0)
		{
			#ifdef MPK_ON
				kSemaphoreSignal(pclp->m_qSemaphore);
			#else
				SignalLocalSemaphore(pclp->m_qSemaphore);
			#endif	//MPK_ON
		}

		free(pclp);
		pclp = NULL;

		fnUnregisterWithThreadTable();
		return;
	}

	// Simulating a shell on NetWare can be difficult. If you don't
	// create a new screen for the script to run in, you can output to
	// the console but you can't get any input from the console. Therefore,
	// every invocation of perl potentially needs its own screen unless
	// you are running either "perl -h" or "perl -v" or you are redirecting
	// stdin from a file.
	//
	// So we need to create a new screen and set that screen as the current
	// screen when running any script launched from the console that is not
	// "perl -h" or "perl -v" and is not redirecting stdin from a file.
	//
	// But it would be a little weird if we didn't create a new screen only
	// in the case when redirecting stdin from a file; in only that case,
	// stdout would be the console instead of a new screen.
	//
	// There is also the issue of standard err. In short, we might as well
	// create a new screen no matter what is going on with redirection, just
	// for the sake of consistency.
	//
	// In summary, we should a create a new screen and make that screen the
	// current screen unless one of the following is true:
	//  * The command is "perl -h"
	//  * The command is "perl -v"
	//  * The script was launched by another perl script. In this case,
	//	  the screen belonging to the parent perl script should probably be
	//    the same screen for this process. And it will be if use BeginThread
	//    instead of BeginThreadGroup when launching Perl from within a Perl
	//    script.
	//
	// In those cases where we create a new screen we should probably also display
	// that screen.
	//

	use_system_console = pclp->m_noScreen  ||
				((pclp->m_argc == 2) && (strcmp(pclp->m_argv[1], (char *)"-h") == 0))  ||
				((pclp->m_argc == 2) && (strcmp(pclp->m_argv[1], (char *)"-v") == 0));

	newscreen = (!use_system_console) && psdata->m_fromConsole;

	if (newscreen)
	{
		newscreenhandle = CreateScreen(sPerlScreenName, 0);
		if (newscreenhandle)
			DisplayScreen(newscreenhandle);
	}
	else if (use_system_console)
	  CreateScreen((char *)"System Console", 0);

	if (pclp->m_redirInName)
	{
		if ((stdin_fd = fileno(stdin)) != -1)
		{
			stdin_fd_dup = dup(stdin_fd);
			if (stdin_fd_dup != -1)
			{
				redirIn = fdopen (stdin_fd_dup, (char const *)"r");
				if (redirIn)
					stdin = freopen (pclp->m_redirInName, (char const *)"r", redirIn);
				if (!stdin)
				{
					redirIn = NULL;
					// undo the redirect, if possible
					stdin = fdopen(stdin_fd, (char const *)"r");
				}
			}
		}
	}

	/**
	The below code stores the handle for the existing stdout to be used later and the existing stdout is closed.
	stdout is then initialised to the new File pointer where the operations are done onto that.
	Later (look below for the code), the saved stdout is restored back.
	**/
	if (pclp->m_redirOutName)
	{
		if ((stdout_fd = fileno(stdout)) != -1)		// Handle of the existing stdout.
		{
			stdout_fd_dup = dup(stdout_fd);
			if (stdout_fd_dup != -1)
			{
				// Close the existing stdout.
				fflush(stdout);		// Write any unwritten data to the file.

				// New stdout
				redirOut = fdopen (stdout_fd_dup, (char const *)"w");
				if (redirOut)
					stdout = freopen (pclp->m_redirOutName, (char const *)"w", redirOut);
				if (!stdout)
				{
					redirOut = NULL;
					// Undo the redirection.
					stdout = fdopen(stdout_fd, (char const *)"w");
				}
				setbuf(stdout, NULL);	// Unbuffered file pointer.
			}
		}
	}

	if (pclp->m_redirErrName)
	{
		if ((stderr_fd = fileno(stderr)) != -1)
		{
			stderr_fd_dup = dup(stderr_fd);
			if (stderr_fd_dup != -1)
			{
				fflush(stderr);

				redirErr = fdopen (stderr_fd_dup, (char const *)"w");
				if (redirErr)
					stderr = freopen (pclp->m_redirErrName, (char const *)"w", redirErr);
				if (!stderr)
				{
					redirErr = NULL;
					// undo the redirect, if possible
					stderr = fdopen(stderr_fd, (char const *)"w");
				}
				setbuf(stderr, NULL);	// Unbuffered file pointer.
			}
		}
	}

	if (pclp->m_redirBothName)
	{
		if ((stdout_fd = fileno(stdout)) != -1)
		{
			stdout_fd_dup = dup(stdout_fd);
			if (stdout_fd_dup != -1)
			{
				fflush(stdout);

				redirOut = fdopen (stdout_fd_dup, (char const *)"w");
				if (redirOut)
					stdout = freopen (pclp->m_redirBothName, (char const *)"w", redirOut);
				if (!stdout)
				{
					redirOut = NULL;
					// undo the redirect, if possible
					stdout = fdopen(stdout_fd, (char const *)"w");
				}
				setbuf(stdout, NULL);	// Unbuffered file pointer.
			}
		}
		if ((stderr_fd = fileno(stderr)) != -1)
		{
	        stderr_fp = stderr;
			stderr = stdout;
		}
	}

	env = NULL;
	fnSetUpEnvBlock(&env);	// Set up the ENV block

	// Run the Perl script
	exitstatus = RunPerl(pclp->m_argc, pclp->m_argv, env);

	// clean up any redirection
	//
	if (pclp->m_redirInName && redirIn)
	{
		fclose(stdin);
		stdin = fdopen(stdin_fd, (char const *)"r");		// Put back the old handle for stdin.
	}

	if (pclp->m_redirOutName && redirOut)
	{
		// Close the new stdout.
		fflush(stdout);
		fclose(stdout);

		// Put back the old handle for stdout.
		stdout = fdopen(stdout_fd, (char const *)"w");
		setbuf(stdout, NULL);	// Unbuffered file pointer.
	}

	if (pclp->m_redirErrName && redirErr)
	{
		fflush(stderr);
		fclose(stderr);

		stderr = fdopen(stderr_fd, (char const *)"w");		// Put back the old handle for stderr.
		setbuf(stderr, NULL);	// Unbuffered file pointer.
	}

	if (pclp->m_redirBothName && redirOut)
	{
		stderr = stderr_fp;

		fflush(stdout);
		fclose(stdout);

		stdout = fdopen(stdout_fd, (char const *)"w");		// Put back the old handle for stdout.
		setbuf(stdout, NULL);	// Unbuffered file pointer.
	}


	if (newscreen && newscreenhandle)
	{
		//added for --autodestroy switch
		if(!pclp->m_AutoDestroy)
		{
			if ((redirOut == NULL) && (redirIn == NULL) && (!gKillAll))
			{
				printf((char *)"\n\nPress any key to exit\n");
				getch();
			}
		}
		DestroyScreen(newscreenhandle);
	}

/**
	// Commented since a few abends were happening in fnFpSetMode
	// Set the mode for stdin and stdout
	fnFpSetMode(stdin, O_TEXT, dummy);
	fnFpSetMode(stdout, O_TEXT, dummy);
**/
	setmode(stdin, O_TEXT);
	setmode(stdout, O_TEXT);

	// Cleanup
	if(pclp->m_argv)
	{
		for(i=0; i<pclp->m_argv_len; i++)
		{
			if(pclp->m_argv[i] != NULL)
			{
				free(pclp->m_argv[i]);
				pclp->m_argv[i] = NULL;
			}
		}

		free(pclp->m_argv);
		pclp->m_argv = NULL;
	}

	if(pclp->nextarg)
	{
		free(pclp->nextarg);
		pclp->nextarg = NULL;
	}
	if(pclp->sSkippedToken != NULL)
	{
		free(pclp->sSkippedToken);
		pclp->sSkippedToken = NULL;
	}

	if(pclp->m_redirInName)
	{
		free(pclp->m_redirInName);
		pclp->m_redirInName = NULL;
	}
	if(pclp->m_redirOutName)
	{
		free(pclp->m_redirOutName);
		pclp->m_redirOutName = NULL;
	}
	if(pclp->m_redirErrName)
	{
		free(pclp->m_redirErrName);
		pclp->m_redirErrName = NULL;
	}
	if(pclp->m_redirBothName)
	{
		free(pclp->m_redirBothName);
		pclp->m_redirBothName = NULL;
	}

	// Signal a semaphore, if indicated by -{ option, to indicate that
	// the script has terminated and files are closed
	//
	if (pclp->m_qSemaphore != 0)
	{
		#ifdef MPK_ON
			kSemaphoreSignal(pclp->m_qSemaphore);
		#else
			SignalLocalSemaphore(pclp->m_qSemaphore);
		#endif	//MPK_ON
	}

	if(pclp)
	{
		free(pclp);
		pclp = NULL;
	}

	if(env)
	{
		fnDestroyEnvBlock(env);
		env = NULL;
	}

	fnUnregisterWithThreadTable();
	// Remove the thread context set during Perl_set_context
	Remove_Thread_Ctx();

	return;
}



/*============================================================================================

 Function		:	fnSetUpEnvBlock

 Description	:	Sets up the initial environment block.

 Parameters		:	penv	(IN)	-	ENV variable as char***.

 Returns		:	Nothing.

==============================================================================================*/

void fnSetUpEnvBlock(char*** penv)
{
	char** env = NULL;

	int sequence = 0;
	char var[kMaxVariableNameLen+1] = {'\0'};
	char val[kMaxValueLen+1] = {'\0'};
	char both[kMaxVariableNameLen + kMaxValueLen + 5] = {'\0'};
	size_t len  = kMaxValueLen;
	int totalcnt = 0;

	while(scanenv( &sequence, var, &len, val ))
	{
		totalcnt++;
		len  = kMaxValueLen;
	}
	// add one for null termination
	totalcnt++;

	env = (char **) malloc (totalcnt * sizeof(char *));
	if (env)
	{
		int cnt = 0;
		int i = 0;

		sequence = 0;
		len  = kMaxValueLen;

		while( (cnt < (totalcnt-1)) && scanenv( &sequence, var, &len, val ) )
		{
			val[len] = '\0';
			strcpy( both, var );
			strcat( both, (char *)"=" );
			strcat( both, val );

			env[cnt] = (char *) malloc((sizeof(both)+1) * sizeof(char));
			if (env[cnt])
			{
				strcpy(env[cnt], both);
				cnt++;
			}
			else
			{
				for(i=0; i<cnt; i++)
				{
					if(env[i])
					{
						free(env[i]);
						env[i] = NULL;
					}
				}

				free(env);
				env = NULL;

				return;
			}

			len  = kMaxValueLen;
		}

		for(i=cnt; i<=(totalcnt-1); i++)
			env[i] = NULL;
	}
	else
		return;

	*penv = env;

	return;
}



/*============================================================================================

 Function		:	fnDestroyEnvBlock

 Description	:	Frees resources used by the ENV block.

 Parameters		:	env	(IN)	-	ENV variable as char**.

 Returns		:	Nothing.

==============================================================================================*/

void fnDestroyEnvBlock(char** env)
{
	// It is assumed that this block is entered only if env is TRUE. So, the calling function
	// must check for this condition before calling fnDestroyEnvBlock.
	// If no check is made by the calling function, then the server abends.
	int k = 0;
	while (env[k] != NULL)
	{
		free(env[k]);
		env[k] = NULL;
		k++;
	}

	free(env);
	env = NULL;

	return;
}



/*============================================================================================

 Function		:	fnFpSetMode

 Description	:	Sets the mode for a file.

 Parameters		:	fp	(IN)	-	FILE pointer for the input file.
					mode	(IN)	-	Mode to be set
					e	(OUT)	-	Error.

 Returns		:	Integer which is the set value.

==============================================================================================*/

int fnFpSetMode(FILE* fp, int mode, int *err)
{
	int ret = -1;

	PFFSETMODE pf_fsetmode;

	if (mode == O_BINARY || mode == O_TEXT)
	{
		if (fp)
		{
			errno = 0;
			// the setmode call is not implemented (correctly) on NetWare,
			// but the CLIB guys were kind enough to provide another
			// call, fsetmode, which does a similar thing. It only works
			// on Moab
			pf_fsetmode = (PFFSETMODE) ImportSymbol(GetNLMHandle(), (char *)"fsetmode");
			if (pf_fsetmode)
				ret = (*pf_fsetmode) (fp, ((mode == O_BINARY) ? "b" : "t"));
			else
			{
				// we are on 4.11 instead of Moab, so we just return an error
				errno = ESERVER;
				err = &errno;
			}
			if (errno)
				err = &errno;
		}
		else
		{
			errno = EBADF;
			err = &errno;
		}
	}
	else
	{
		errno = EINVAL;
		err = &errno;
	}

	return ret;
}



/*============================================================================================

 Function		:	fnInternalPerlLaunchHandler

 Description	:	Gets called by perl to spawn a new instance of perl.

 Parameters		:	cndLine	(IN)	-	Command Line string.

 Returns		:	Nothing.

==============================================================================================*/

void fnInternalPerlLaunchHandler(char* cmdLine)
{
	int currentThreadGroup = -1;

	ScriptData* psdata=NULL;

	// Create a safe copy of the command line and pass it to the
	// new thread for parsing. The new thread will be responsible
	// to delete it when it is finished with it.
	psdata = (ScriptData *) malloc(sizeof(ScriptData));
	if (psdata)
	{
		psdata->m_commandLine = NULL;
		psdata->m_commandLine = (char *) malloc(MAX_DN_BYTES * sizeof(char));

		if(psdata->m_commandLine)
		{
			strcpy(psdata->m_commandLine, cmdLine);
			psdata->m_fromConsole = FALSE;

			#ifdef MPK_ON
				BeginThread(fnLaunchPerl, NULL, PERL_COMMAND_STACK_SIZE, (void*)psdata);
			#else
				// Start a new thread in its own thread group
				BeginThread(fnLaunchPerl, NULL, PERL_COMMAND_STACK_SIZE, (void*)psdata);
			#endif	//MPK_ON
		}
		else
		{
			free(psdata);
			psdata = NULL;
			return;
		}
	}
	else
		return;

	return;
}



/*============================================================================================

 Function		:	fnGetPerlScreenName

 Description	:	This function creates the Perl screen name.
					Gets called from main only once when the Perl NLM loads.

 Parameters		:	sPerlScreenName	(OUT)	-	Resultant Perl screen name.

 Returns		:	Nothing.

==============================================================================================*/

void fnGetPerlScreenName(char *sPerlScreenName)
{
	// HYAK:
	// The logic for using 32 in the below array sizes is like this:
	// The NetWare CLIB SDK documentation says that for base 2 conversion,
	// this number must be minimum 8. Also, in the example of the documentation,
	// 20 is used as the size and testing is done for bases from 2 upto 16.
	// So, to simply chose a number above 20 and also keeping in mind not to reserve
	// unnecessary big array sizes, I have chosen 32 !
	// Less than that may also suffice.
	char sPerlRevision[32 * sizeof(char)] = {'\0'};
	char sPerlVersion[32 * sizeof(char)] = {'\0'};
	char sPerlSubVersion[32 * sizeof(char)] = {'\0'};

	// The defines for PERL_REVISION, PERL_VERSION, PERL_SUBVERSION are available in
	// patchlevel.h  under root and gets included when  perl.h  is included.
	// The number 10 below indicates base 10.
	itoa(PERL_REVISION, sPerlRevision, 10);
	itoa(PERL_VERSION, sPerlVersion, 10);
	itoa(PERL_SUBVERSION, sPerlSubVersion, 10);

	// Concatenate substrings to get a string like Perl5.6.1 which is used as the screen name.
	sprintf(sPerlScreenName, "%s%s.%s.%s", PERL_COMMAND_NAME,
									sPerlRevision, sPerlVersion, sPerlSubVersion);

	return;
}



// Global variable to hold the environ information.
// First time it is accessed, it will be created and initialized and 
// next time onwards, the pointer will be returned.

// Improvements - Dynamically read env everytime a request comes - Is this required?
char** genviron = NULL;


/*============================================================================================

 Function		:	nw_getenviron

 Description	:	Gets the environment information.

 Parameters		:	None.

 Returns		:	Nothing.

==============================================================================================*/

char ***
nw_getenviron()
{
	if (genviron)
		return (&genviron);	// This might leak memory upto 11736 bytes on some versions of NetWare.
//		return genviron;	// Abending on some versions of NetWare.
	else
		fnSetUpEnvBlock(&genviron);

	return (&genviron);
}



/*============================================================================================

 Function		:	nw_freeenviron

 Description	:	Frees the environment information.

 Parameters		:	None.

 Returns		:	Nothing.

==============================================================================================*/

void
nw_freeenviron()
{
	if (genviron)
	{
		fnDestroyEnvBlock(genviron);
		genviron=NULL;
	}
}


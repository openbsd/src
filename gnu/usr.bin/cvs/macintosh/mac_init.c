/*
 * mac_init.c --- routines to initialize and cleanup macintosh behavior
 *
 * MDLadwig <mike@twinpeaks.prc.com> --- June 1996
 */
#include "mac_config.h"
 
#ifdef __POWERPC__
#include <MacHeadersPPC>
#else
#include <MacHeaders68K>
#endif

#include <sioux.h>
#include <GUSI.h>

extern char **Args;
extern char **EnvVars, **EnvVals;
extern int ArgC;
extern int EnvC;

extern int argc;
extern char **argv;

void
InitializeMacToolbox( void )
{
	#ifndef __POWERPC__
	SetApplLimit(GetApplLimit() - STACK_SIZE_68K);
	#endif
	
	MaxApplZone();
	MoreMasters();
}

void
MacOS_Initialize( int *argc, char ***argv )
{
	InitializeMacToolbox();
	
	GUSISetup(GUSIwithSIOUXSockets);
	GUSISetup(GUSIwithUnixSockets);

	SIOUXSettings.showstatusline = TRUE;
	SIOUXSettings.autocloseonquit = FALSE;
	SIOUXSettings.asktosaveonclose = TRUE;
	
	#ifdef AE_IO_HANDLERS
	GetUnixCommandEnvironment( "cvs" );
	*argc = ArgC;
	*argv = Args;
	#else
	*argc = ccommand(argv);
	#endif
}

void
MacOS_Cleanup ( void )
{
	RemoveConsole();		// FIXME - Ugly, but necessary until MW fixes _exit
}


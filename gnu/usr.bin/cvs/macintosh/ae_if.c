/*
 * aeLink.c
 * UNIX environment handling stuff for macos
 *
 * These routines make MacCVS able to accept a standard UNIX command line,
 * environment, output redirection, and startup directory via AppleEvents
 * and redirect all output to AppleEvents, a file, or the SIOUX tty window.
 *
 * Michael Ladwig <mike@twinpeaks.prc.com> --- April 1996
 */
#include "mac_config.h"
#ifdef AE_IO_HANDLERS
#ifdef __POWERPC__
#include <MacHeadersPPC>
#else
#include <MacHeaders68K>
#endif

#include <AppleEvents.h>
#include <AERegistry.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <sys/fcntl.h>

extern char *xmalloc (size_t bytes);

enum { outToAE, outToFile };

static int		outputMode = outToAE;
static char		tempOutputFileName[256], outputFileName[256];
static int		outputFile;
static int		noLineBuffer;

AppleEvent		gResponseEvent			=	{'null', nil};
AppleEvent		gResponseReplyEvent	=	{'null', nil};
AEAddressDesc	gResponseAddress;

static char		aeCmdIn = 0;
static char		aeLinkDone = 0;

char				**Args;
char				**EnvVars, **EnvVals;
int				ArgC	= 1;
int				EnvC	= 1;

char * CopyInfo(Handle info)
{
	if( info )
	{
		char * retarg = xmalloc(GetHandleSize(info) + 1);
	
		if (retarg)
		{
			memcpy(retarg, *info, GetHandleSize(info) );
			retarg[GetHandleSize(info)] = 0;
		}
			
		return retarg;
	}
	else
		return nil;
}

void MakeEnvironment(const AppleEvent *event)
{
	AEDesc		args;
	long			i, argCount;

	if (AEGetParamDesc(event, 'ENVT', typeAEList, &args) || AECountItems(&args, &argCount) || !argCount)
	{	
		EnvVars[EnvC] = nil;
		EnvVals[EnvC] = nil;
		return;
	}
	
	for (i = 0; i++<argCount;)
	{
		AEKeyword	key;
		AEDesc	arg;

		EnvC++;
		
		// Variable

		if (!AEGetNthDesc(&args, i, typeChar, &key, &arg))
		{
			HLock(arg.dataHandle);
			EnvVars[EnvC-1] = CopyInfo(arg.dataHandle);
			AEDisposeDesc(&arg);
		}
		
		// Value

		i++;
		if (!AEGetNthDesc(&args, i, typeChar, &key, &arg))
		{
			HLock(arg.dataHandle);
			EnvVals[EnvC-1] = CopyInfo(arg.dataHandle);
			AEDisposeDesc(&arg);
		}
	}
	AEDisposeDesc(&args);

	EnvVars[EnvC] = nil;
	EnvVals[EnvC] = nil;
}

pascal OSErr DoScript(const AppleEvent *event, AppleEvent *reply, long refCon)
{
	OSType		mode;
	DescType		typeCode;
	Size			size;
	AEDesc		args;
	long			i, argCount, addrSize;
	AEDesc		env;
	char			*argString, *anArg, startPWD[1024];
	TargetID		requestAddr;
	DescType		requestAddrType;
	Boolean		flag;

	if (AEGetParamDesc(event, '----', typeAEList, &args) || AECountItems(&args, &argCount) || !argCount) 
		return errAEEventFailed;

	// Get the address of the requesting app to we can send information back
	
	AEGetAttributePtr(event,
							keyAddressAttr,
							typeWildCard, 
							&requestAddrType,
							(Ptr) &requestAddr,
							sizeof(requestAddr),
							&addrSize);
	AECreateDesc( typeTargetID, &requestAddr, sizeof(requestAddr), &gResponseAddress );

	// Pull the command line from the event
	
	for (i = 0; i++<argCount;)
	{
		AEKeyword	key;
		AEDesc	arg;

		if (!AEGetNthDesc(&args, i, typeChar, &key, &arg))
		{
			HLock(arg.dataHandle);
			argString = CopyInfo(arg.dataHandle);
			AEDisposeDesc(&arg);
			anArg = strtok( argString, " " );
			Args[ArgC] = anArg;
			ArgC++;
			while( (anArg = strtok(NULL, " ")) != NULL )
			{
					Args[ArgC] = anArg;
					ArgC++;
			}
		}
	}
	AEDisposeDesc(&args);
	Args[ArgC] = nil;

	// Pull the environment variables from the event
	
	MakeEnvironment( event );
	
	// Figure out what mode should be used to return results
	
	if (AEGetParamPtr(event, 'MODE', typeEnumerated, &typeCode, &mode, 4, &size))
		outputMode = outToAE;
	else
	{
		switch (mode) {
			
			// Batch (return results via Apple Events)
				
			case 'TOAE':
				outputMode = outToAE;
				break;
			
			// File (return results via a file)		
			case 'FILE':
				outputMode = outToFile;
				if (AEGetParamPtr(event, 'FILE', typeChar, &typeCode, &outputFileName, sizeof(outputFileName)-1, &size))
				{
					outputMode = outToAE;
					fprintf(stderr, "MacCVS Error: No filename parameter\n" );
				}
				else
				{
					outputFileName[size] = 0;
					strcpy( tempOutputFileName, outputFileName );
					strcat( tempOutputFileName, ".TMP");
					if( (outputFile = open(tempOutputFileName, O_WRONLY | O_CREAT | O_TRUNC)) == 1 )
					{
						outputMode = outToAE;
						fprintf(stderr, "MacCVS Error: Unable to open '%s'\n", tempOutputFileName);
					}
				}
				break;
		}
	}
	
	// Check to see if there is a starting pathname for this invokation
	
	if ( ! AEGetParamPtr(event, 'SPWD', typeChar, &typeCode, &startPWD, sizeof(startPWD)-1, &size) )
	{
		startPWD[size] = 0;
		chdir(startPWD);
	}
	
	// Check to see if we should not line buffer results in AE return mode
	
	if (AEGetParamPtr(event, 'LBUF', typeBoolean, &typeCode, (Ptr) &flag, 1, &size))
		noLineBuffer = 0;
	else
		noLineBuffer = flag;

	// All Done
	
	aeLinkDone = 1;
	
	return noErr;
}

void GetUnixCommandEnvironment( char *name )
{
	long				timeOutTicks;
	EventRecord		theEvent;

#ifdef __POWERPC__
	AEInstallEventHandler( kAEMiscStandards, kAEDoScript, NewAEEventHandlerProc(DoScript), 0, false);
#else
	AEInstallEventHandler( kAEMiscStandards, kAEDoScript, DoScript, 0, false);
#endif

	// Allocate some storage for the command line and the environment
	
	Args	= (char **) xmalloc(ArgMax * sizeof(char *));
	EnvVars	= (char **) xmalloc(EnvMax * sizeof(char *));
	EnvVals	= (char **) xmalloc(EnvMax * sizeof(char *));
	
	// Initialize the first arg to the process name
	
	Args[0]	= xmalloc(strlen(name)+1);
	strcpy( Args[0], name );
	
	// Defaults
	
	ArgC = 1;
	EnvC = 0;
	outputMode = outToAE;
	
	// Wait for the command line and environment
	
	timeOutTicks = TickCount() + (60*AE_TIMEOUT_SECONDS);		// Timeout seconds set in maccvs.pch	
	while( (TickCount() < timeOutTicks) && (!aeLinkDone) )
	{		
		if (WaitNextEvent(everyEvent, &theEvent, 60, nil))
		{
			if( ! (SIOUXHandleOneEvent(&theEvent)) )
			{
				switch (theEvent.what)
				{
					case kHighLevelEvent:
						AEProcessAppleEvent(&theEvent);
						break;
				}
			}
		}
	}
}

char *
getenv( const char *var )
{
	int	i;
	
	// Look it up in the environment
	
	for( i=0; i<EnvC; i++ )
	{
		if( strcmp(EnvVars[i], var) == 0 ) return( EnvVals[i] );
	}
	
	return NULL;
}

/* Free the allocated memory */

void CleanUpArgsAndEnv( void )
{
	int	i;
	
	// Clean up the args
	
	for( i=0; i<ArgC; i++ )
		free( Args[i] );
		
	free( Args );
	
	// Clean up the environment
	
	for( i=0; i<EnvC; i++ )
		{ free( EnvVars[i] ); free( EnvVals[i] ); }
		
	free( EnvVars );
	free( EnvVals );	
}

/*
 * The following blocks of code are related to the redirection of output to
 * AppleEvents.
 */

static char		outBuf[AE_OUTBUF_SIZE];
static int		outBufLen = -1;

void InitOutBuffer( void )
{
		outBufLen = 0;
}

void SendOutBuffer( char outputDone )
{
	if( outBufLen )
	{
		AEPutParamPtr(
			&gResponseEvent,
			keyDirectObject, 
			typeChar,
			outBuf,
			outBufLen);
	}
	if( outputDone )
	{
		AEPutParamPtr(
			&gResponseEvent,
			'DONE', 
			typeChar,
			"DONE",
			4);
	}
	AESend(
		&gResponseEvent,
		&gResponseReplyEvent,
		kAEWaitReply+kAENeverInteract,
		kAENormalPriority,
		kNoTimeOut,
		nil, nil);
}

/*
 * The following three routines override the "real thing" from the CW
 * SIOUX library in order to divert output to AppleEvents.
 */

short
InstallConsole(short fd)
{
    if (outputMode == outToFile)
	return 0;

    AECreateAppleEvent ('MCVS', 'DATA',
			&gResponseAddress,
			kAutoGenerateReturnID,
			kAnyTransactionID, 
			&gResponseEvent);

    return 0;
}

long WriteCharsToConsole( char *buf, long length )
{
	char		*tCh;
	
	if( outputMode == outToFile )
	{
		write( outputFile, buf, length );
		return length;
	}
	
	if( outBufLen == -1 ) InitOutBuffer();
	
	if( (length + outBufLen) > AE_OUTBUF_SIZE )
	{
		SendOutBuffer( FALSE );
		InitOutBuffer();
	}
	
	for( tCh = buf; tCh < (char *) (buf+length); tCh++ )
	{
		if( *tCh == '\012' ) *tCh = '\015';
		
		outBuf[outBufLen] = *tCh;
		outBufLen++;
	}
	
	if( noLineBuffer && ( *(buf+length) == '\015') )
	{
		SendOutBuffer( FALSE );
		InitOutBuffer();
	}
	
	return length;
}

void RemoveConsole( void )
{
	CleanUpArgsAndEnv();
	
	if( outputMode == outToFile )
	{
		close(outputFile);
		if( rename(tempOutputFileName, outputFileName) != 0 )
			SysBeep( 100 );
		return;
	}
	
	SendOutBuffer( TRUE );

	AEDisposeDesc( &gResponseEvent );
	AEDisposeDesc( &gResponseAddress );
}
#endif // AE_IO_HANDLERS

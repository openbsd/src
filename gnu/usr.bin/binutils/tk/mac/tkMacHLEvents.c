/* 
 * tkMacHLEvents.c --
 *
 *	Implements high level event support for the Macintosh.  Currently, 
 *	the only event that really does anything is the Quit event.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacHLEvents.c 1.13 96/02/22 21:59:58
 */

#include "tcl.h"
#include "tkMacInt.h"

#include <Aliases.h>
#include <AppleEvents.h>
#include <FullPath.h>
#include <SegLoad.h>
#include <ToolUtils.h>

static pascal OSErr QuitHandler _ANSI_ARGS_((AppleEvent* event,
	AppleEvent* reply, long refcon));
static pascal OSErr OappHandler _ANSI_ARGS_((AppleEvent* event,
	AppleEvent* reply, long refcon));
static pascal OSErr OdocHandler _ANSI_ARGS_((AppleEvent* event,
	AppleEvent* reply, long refcon));
static pascal OSErr PrintHandler _ANSI_ARGS_((AppleEvent* event,
	AppleEvent* reply, long refcon));
static pascal OSErr ScriptHandler _ANSI_ARGS_((AppleEvent* event,
	AppleEvent* reply, long refcon));
static int MissedAnyParameters _ANSI_ARGS_((AppleEvent *theEvent));


/*
 *----------------------------------------------------------------------
 *
 * TkMacInitAppleEvents --
 *
 *	Initilize the Apple Events on the Macintosh.  This registers the
 *	core event handlers.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void 
TkMacInitAppleEvents(interp)
    Tcl_Interp *interp;
{
    OSErr err;
    AEEventHandlerUPP	OappHandlerUPP, OdocHandlerUPP,
	PrintHandlerUPP, QuitHandlerUPP, ScriptHandlerUPP;
	
    /* Install event handlers for the core apple events. */
    QuitHandlerUPP = NewAEEventHandlerProc(QuitHandler);
    err = AEInstallEventHandler(kCoreEventClass, kAEQuitApplication,
	    QuitHandlerUPP, (long) interp, false);

    OappHandlerUPP = NewAEEventHandlerProc(OappHandler);
    err = AEInstallEventHandler(kCoreEventClass, kAEOpenApplication,
	    OappHandlerUPP, (long) interp, false);

    OdocHandlerUPP = NewAEEventHandlerProc(OdocHandler);
    err = AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments,
	    OdocHandlerUPP, (long) interp, false);

    PrintHandlerUPP = NewAEEventHandlerProc(PrintHandler);
    err = AEInstallEventHandler(kCoreEventClass, kAEPrintDocuments,
	    PrintHandlerUPP, (long) interp, false);

    if (interp != NULL) {
	ScriptHandlerUPP = NewAEEventHandlerProc(ScriptHandler);
	err = AEInstallEventHandler('misc', 'dosc',
	    ScriptHandlerUPP, (long) interp, false);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacDoHLEvent --
 *
 *	Dispatch incomming highlevel events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the incoming event.
 *
 *----------------------------------------------------------------------
 */

void
TkMacDoHLEvent(theEvent)
    EventRecord *theEvent;
{
    AEProcessAppleEvent(theEvent);

    return;
}

/*
 *----------------------------------------------------------------------
 *
 * QuitHandler, OappHandler, etc. --
 *
 *	These are the core Apple event handlers.  Only the Quit event does
 *	anything interesting.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static pascal OSErr
QuitHandler(theAppleEvent, reply, handlerRefcon)
    AppleEvent *theAppleEvent;
    AppleEvent *reply;
    long handlerRefcon;
{
    Tcl_Interp 	*interp = (Tcl_Interp *) handlerRefcon;
    
    /*
     * Call the exit command.  However, if it doesn't quit - quit anyway.
     */
    if (interp != NULL) {
	Tcl_GlobalEval(interp, "exit");
    }
    Tcl_Exit(0);
    return noErr;
}

static pascal OSErr
OappHandler(theAppleEvent, reply, handlerRefcon)
    AppleEvent *theAppleEvent;
    AppleEvent *reply;
    long handlerRefcon;
{
    return noErr;
}

static pascal OSErr
OdocHandler(theAppleEvent, reply, handlerRefcon)
    AppleEvent *theAppleEvent;
    AppleEvent *reply;
    long handlerRefcon;
{
    Tcl_Interp 	*interp = (Tcl_Interp *) handlerRefcon;
    AEDescList fileSpecList;
    FSSpec file;
    OSErr err;
    DescType type;
    Size actual;
    long count;
    AEKeyword keyword;
    long index;
    Tcl_DString command;
    Tcl_DString pathName;
    Tcl_CmdInfo dummy;

    /*
     * Don't bother if we don't have an interp or
     * the open document procedure doesn't exist.
     */
    if ((interp == NULL) || 
    	(Tcl_GetCommandInfo(interp, "tkOpenDocument", &dummy)) == 0) {
    	return noErr;
    }
    
    /*
     * If we get any errors wil retrieving our parameters
     * we just return with no error.
     */
    err = AEGetParamDesc(theAppleEvent, keyDirectObject,
	    typeAEList, &fileSpecList);
    if (err != noErr) {
	return noErr;
    }

    err = MissedAnyParameters(theAppleEvent);
    if (err != noErr) {
	return noErr;
    }

    err = AECountItems(&fileSpecList, &count);
    if (err != noErr) {
	return noErr;
    }

    Tcl_DStringInit(&command);
    Tcl_DStringInit(&pathName);
    Tcl_DStringAppend(&command, "tkOpenDocument", -1);
    for (index = 1; index <= count; index++) {
	short length;
	Handle fullPath;
	
	Tcl_DStringSetLength(&pathName, 0);
	err = AEGetNthPtr(&fileSpecList, index, typeFSS,
		&keyword, &type, (Ptr) &file, sizeof(FSSpec), &actual);
	if ( err != noErr ) {
	    continue;
	}

	err = FSpGetFullPath(&file, &length, &fullPath);
	HLock(fullPath);
	Tcl_DStringAppend(&pathName, *fullPath, length);
	HUnlock(fullPath);
	DisposeHandle(fullPath);

	Tcl_DStringAppendElement(&command, pathName.string);
    }
    
    Tcl_GlobalEval(interp, command.string);

    Tcl_DStringFree(&command);
    Tcl_DStringFree(&pathName);
    return noErr;
}

static pascal OSErr
PrintHandler(theAppleEvent, reply, handlerRefcon)
    AppleEvent *theAppleEvent;
    AppleEvent *reply;
    long handlerRefcon;
{
    return noErr;
}

/*
 *----------------------------------------------------------------------
 *
 * DoScriptHandler --
 *
 *	This handler process the do script event.  
 *
 * Results:
 *	Scedules the given event to be processed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
 
static pascal OSErr 
ScriptHandler(theAppleEvent, reply, handlerRefcon)
    AppleEvent *theAppleEvent;
    AppleEvent *reply;
    long handlerRefcon;
{
    OSErr theErr;
    AEDescList theDesc;
    int tclErr = -1;
    Tcl_Interp *interp;
    char errString[128];

    interp = (Tcl_Interp *) handlerRefcon;

    /*
     * The do script event receives one parameter that should be data or a file.
     */
    theErr = AEGetParamDesc(theAppleEvent, keyDirectObject, typeWildCard,
	    &theDesc);
    if (theErr != noErr) {
	sprintf(errString, "AEDoScriptHandler: GetParamDesc error %d", theErr);
	theErr = AEPutParamPtr(reply, keyErrorString, typeChar, errString,
		strlen(errString));
    } else if (MissedAnyParameters(theAppleEvent)) {
	sprintf(errString, "AEDoScriptHandler: extra parameters");
	AEPutParamPtr(reply, keyErrorString, typeChar, errString,
		strlen(errString));
	theErr = -1771;
    } else {
	if (theDesc.descriptorType == (DescType)'TEXT') {
	    short length;
	    
	    length = GetHandleSize(theDesc.dataHandle);
	    SetHandleSize(theDesc.dataHandle, length + 1);
	    *(*theDesc.dataHandle + length) = '\0';

	    HLock(theDesc.dataHandle);
	    tclErr = Tcl_GlobalEval(interp, *theDesc.dataHandle);
	    HUnlock(theDesc.dataHandle);
	} else if (theDesc.descriptorType == (DescType)'alis') {
	    Boolean dummy;
	    FSSpec theFSS;
	    Handle code;
	    short length;
	    
	    theErr = ResolveAlias(NULL, (AliasHandle)theDesc.dataHandle,
		    &theFSS, &dummy);
	    if (theErr == noErr) {
		FSpGetFullPath(&theFSS, &length, &code);
		Munger(code, 0, NULL, 0, "source {", strlen("source {"));
		Munger(code, GetHandleSize(code), NULL, 0, "};", strlen("};"));
		HLock(code);
		*(*code + GetHandleSize(code) - 1) = '\0';
		tclErr = Tcl_GlobalEval(interp, *code);
		HUnlock(code);
		DisposeHandle(code);
	    } else {
		sprintf(errString, "AEDoScriptHandler: file not found");
		AEPutParamPtr(reply, keyErrorString, typeChar,
			errString, strlen(errString));
	    }
	} else {
	    sprintf(errString,
		    "AEDoScriptHandler: invalid script type '%-4.4s', must be 'alis' or 'TEXT'",
		    &theDesc.descriptorType);
	    AEPutParamPtr(reply, keyErrorString, typeChar,
		    errString, strlen(errString));
	    theErr = -1770;
	}
    }

    /*
     * If we actually go to run Tcl code - put the result in the reply.
     */
    if (tclErr >= 0) {
	if (tclErr == TCL_OK)  {
	    AEPutParamPtr(reply, keyDirectObject, typeChar,
		interp->result, strlen(interp->result));
	} else {
	    AEPutParamPtr(reply, keyErrorString, typeChar,
		interp->result, strlen(interp->result));
	    AEPutParamPtr(reply, keyErrorNumber, typeInteger,
		(Ptr) &tclErr, sizeof(int));
	}
    }
	
    AEDisposeDesc(&theDesc);

    return theErr;
}

/*
 *----------------------------------------------------------------------
 *
 * MissedAnyParameters --
 *
 *	Checks to see if parameters are still left in the event.  
 *
 * Results:
 *	True or false.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
 
static int 
MissedAnyParameters(theEvent)
    AppleEvent *theEvent;
{
   DescType returnedType;
   Size actualSize;
   OSErr err;

   err = AEGetAttributePtr(theEvent, keyMissedKeywordAttr, typeWildCard, 
   		&returnedType, NULL, 0, &actualSize);
   
   return (err != errAEDescNotFound);
}

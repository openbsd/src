/* 
 * tkUnix.c --
 *
 *	This file contains procedures that are UNIX/X-specific, and
 *	will probably have to be written differently for Windows or
 *	Macintosh platforms.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkUnix.c 1.3 96/02/15 18:55:18
 */

#include <tkInt.h>

/*
 *----------------------------------------------------------------------
 *
 * TkGetServerInfo --
 *
 *	Given a window, this procedure returns information about
 *	the window server for that window.  This procedure provides
 *	the guts of the "winfo server" command.
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
TkGetServerInfo(interp, tkwin)
    Tcl_Interp *interp;		/* The server information is returned in
				 * this interpreter's result. */
    Tk_Window tkwin;		/* Token for window;  this selects a
				 * particular display and server. */
{
    char buffer[50], buffer2[50];

    sprintf(buffer, "X%dR%d ", ProtocolVersion(Tk_Display(tkwin)),
	    ProtocolRevision(Tk_Display(tkwin)));
    sprintf(buffer2, " %d", VendorRelease(Tk_Display(tkwin)));
    Tcl_AppendResult(interp, buffer, ServerVendor(Tk_Display(tkwin)),
	    buffer2, (char *) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetDefaultScreenName --
 *
 *	Returns the name of the screen that Tk should use during
 *	initialization.
 *
 * Results:
 *	Returns the argument or a string that should not be freed by
 *	the caller.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TkGetDefaultScreenName(interp, screenName)
    Tcl_Interp *interp;		/* Interp used to find environment variables. */
    char *screenName;		/* Screen name from command line, or NULL. */
{
    if ((screenName == NULL) || (screenName[0] == '\0')) {
	screenName = Tcl_GetVar2(interp, "env", "DISPLAY", TCL_GLOBAL_ONLY);
    }
    return screenName;
}

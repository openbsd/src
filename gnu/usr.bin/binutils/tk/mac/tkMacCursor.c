/* 
 * tkMacCursor.c --
 *
 *	This file contains Macintosh specific cursor related routines.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacCursor.c 1.10 96/02/15 18:55:44
 */

#include "tkPort.h"
#include "tkInt.h"
#include "tkMacInt.h"

#include <Resources.h>
#include <ToolUtils.h>

/*
 * The following data structure contains the system specific data
 * necessary to control Windows cursors.
 */

typedef struct {
    TkCursor info;		/* Generic cursor info used by tkCursor.c */
    Handle macCursor;		/* Resource containing Macintosh cursor. */
    int type;			/* Type of Mac cursor: 
    				 * 0 = arrow, 1 = crsr, 2 = CURS */
} TkMacCursor;

/*
 * The table below is used to map from the name of a predefined cursor
 * to its resource identifier.
 */

static struct CursorName {
    char *name;
    int id;
} cursorNames[] = {
    {"ibeam",		1},
    {"text",		1},
    {"xterm",		1},
    {"cross",		2},
    {"crosshair",	2},
    {"cross-hair",	2},
    {"plus",		3},
    {"watch",		4},
    {"arrow",		5},
    {NULL,		0}
};


/*
 *----------------------------------------------------------------------
 *
 * TkGetCursorByName --
 *
 *	Retrieve a system cursor by name.  
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.  
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkGetCursorByName(interp, tkwin, string)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tk_Window tkwin;		/* Window in which cursor will be used. */
    Tk_Uid string;		/* Description of cursor.  See manual entry
				 * for details on legal syntax. */
{
    struct CursorName *namePtr;
    TkMacCursor *cursorPtr;

    /*
     * Check for the cursor in the system cursor set.
     */

    for (namePtr = cursorNames; namePtr->name != NULL; namePtr++) {
	if (strcmp(namePtr->name, string) == 0) {
	    break;
	}
    }

    cursorPtr = (TkMacCursor *) ckalloc(sizeof(TkMacCursor));
    cursorPtr->info.cursor = (Tk_Cursor) cursorPtr;
    if (namePtr->name != NULL) {
    	if (namePtr->id == 5) {
	    cursorPtr->macCursor = (Handle) -1;
	    cursorPtr->type = 0;
    	} else {
	    cursorPtr->macCursor = (Handle) GetCursor(namePtr->id);
	    cursorPtr->type = 2;
	}
    } else {
    	Handle resource;
	Str255 curName;
    	
	strcpy((char *) curName + 1, string);
	curName[0] = strlen(string);
	resource = GetNamedResource('crsr', curName);
	if (resource != NULL) {
	    short id;
	    Str255 theName;
	    ResType	theType;

	    HLock(resource);
	    GetResInfo(resource, &id, &theType, theName);
	    HUnlock(resource);
	    cursorPtr->macCursor = (Handle) GetCCursor(id);
	    cursorPtr->type = 1;
	}

	if (resource == NULL) {
	    cursorPtr->macCursor = GetNamedResource('CURS', curName);
	    cursorPtr->type = 2;
	}
    }
    if (cursorPtr->macCursor == NULL) {
	ckfree((char *)cursorPtr);
	Tcl_AppendResult(interp, "bad cursor spec \"", string, "\"",
		(char *) NULL);
	return NULL;
    } else {
	return (TkCursor *) cursorPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkCreateCursorFromData --
 *
 *	Creates a cursor from the source and mask bits.
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkCreateCursorFromData(tkwin, source, mask, width, height, xHot, yHot,
	fgColor, bgColor)
    Tk_Window tkwin;		/* Window in which cursor will be used. */
    char *source;		/* Bitmap data for cursor shape. */
    char *mask;			/* Bitmap data for cursor mask. */
    int width, height;		/* Dimensions of cursor. */
    int xHot, yHot;		/* Location of hot-spot in cursor. */
    XColor fgColor;		/* Foreground color for cursor. */
    XColor bgColor;		/* Background color for cursor. */
{
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFreeCursor --
 *
 *	This procedure is called to release a cursor allocated by
 *	TkGetCursorByName.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor data structure is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TkFreeCursor(cursorPtr)
    TkCursor *cursorPtr;
{
    TkMacCursor *macCursorPtr = (TkMacCursor *) cursorPtr;

    switch (macCursorPtr->type) {
	case 1:
	    DisposeCCursor((CCrsrHandle) macCursorPtr->macCursor);
	    break;
	case 2:
	    ReleaseResource(macCursorPtr->macCursor);
	    break;
    }
    ckfree((char *) macCursorPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TkUpdateCursor --
 *
 *	Set the windows global cursor to the cursor associated with
 *	the given Tk window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the mouse cursor.
 *
 *----------------------------------------------------------------------
 */

void
TkUpdateCursor(winPtr)
    TkWindow *winPtr;
{
    TkMacCursor *macCursor = NULL;

    /*
     * A window inherits its cursor from its parent if it doesn't
     * have one of its own.  Top level windows inherit the default
     * cursor.
     */

    while (winPtr != NULL) {
	if (winPtr->atts.cursor != None) {
	    macCursor = (TkMacCursor *) winPtr->atts.cursor;
	    break;
	} else if (winPtr->flags & TK_TOP_LEVEL) {
	    break;
	}
	winPtr = winPtr->parentPtr;
    }
    if (macCursor == NULL || macCursor->type == 0) {
	SetCursor(&qd.arrow);
    } else {
	CCrsrHandle ccursor;
	CursHandle cursor;
	
	switch (macCursor->type) {
	    case 1:
		ccursor = (CCrsrHandle) macCursor->macCursor;
		SetCCursor(ccursor);
		break;
	    case 2:
		cursor = (CursHandle) macCursor->macCursor;
		SetCursor(*cursor);
		break;
	}
    }
}

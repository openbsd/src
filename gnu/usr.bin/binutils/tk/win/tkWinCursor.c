/* 
 * tkWinCursor.c --
 *
 *	This file contains Win32 specific cursor related routines.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinCursor.c 1.6 96/02/15 18:56:06
 */

#include "tkWinInt.h"

/*
 * The following data structure contains the system specific data
 * necessary to control Windows cursors.
 */

typedef struct {
    TkCursor info;		/* Generic cursor info used by tkCursor.c */
    HCURSOR winCursor;		/* Win32 cursor handle. */
    int system;			/* 1 if cursor is a system cursor, else 0. */
} TkWinCursor;

/*
 * The table below is used to map from the name of a predefined cursor
 * to its resource identifier.
 */

static struct CursorName {
    char *name;
    LPCTSTR id;
} cursorNames[] = {
    {"starting",		IDC_APPSTARTING},
    {"arrow",			IDC_ARROW},
    {"ibeam",			IDC_IBEAM},
    {"icon",			IDC_ICON},
    {"no",			IDC_NO},
    {"size",			IDC_SIZE},
    {"size_ne_sw",		IDC_SIZENESW},
    {"size_ns",			IDC_SIZENS},
    {"size_nw_se",		IDC_SIZENWSE},
    {"size_we",			IDC_SIZEWE},
    {"uparrow",			IDC_UPARROW},
    {"wait",			IDC_WAIT},
    {"crosshair",		IDC_CROSS},
    {"fleur",			IDC_SIZE},
    {"sb_v_double_arrow",	IDC_SIZENS},
    {"sb_h_double_arrow",	IDC_SIZEWE},
    {"center_ptr",		IDC_UPARROW},
    {"watch",			IDC_WAIT},
    {"xterm",			IDC_IBEAM},
    {NULL,			0}
};

/*
 * The default cursor is used whenever no other cursor has been specified.
 */

#define TK_DEFAULT_CURSOR	IDC_ARROW


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
    TkWinCursor *cursorPtr;

    /*
     * Check for the cursor in the system cursor set.
     */

    for (namePtr = cursorNames; namePtr->name != NULL; namePtr++) {
	if (strcmp(namePtr->name, string) == 0) {
	    break;
	}
    }

    cursorPtr = (TkWinCursor *) ckalloc(sizeof(TkWinCursor));
    cursorPtr->info.cursor = (Tk_Cursor) cursorPtr;
    if (namePtr->name != NULL) {
	cursorPtr->winCursor = LoadCursor(NULL, namePtr->id);
	cursorPtr->system = 1;
    } else {
	cursorPtr->winCursor = LoadCursor(TkWinGetTkModule(), string);
	cursorPtr->system = 0;
    }
    if (cursorPtr->winCursor == NULL) {
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
    TkWinCursor *winCursorPtr = (TkWinCursor *) cursorPtr;
    ckfree((char *) winCursorPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinUpdateCursor --
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
TkWinUpdateCursor(winPtr)
    TkWindow *winPtr;
{
    HCURSOR cursor = NULL;

    /*
     * A window inherits its cursor from its parent if it doesn't
     * have one of its own.  Top level windows inherit the default
     * cursor.
     */

    while (winPtr != NULL) {
	if (winPtr->atts.cursor != None) {
	    cursor = ((TkWinCursor *) winPtr->atts.cursor)->winCursor;
	    break;
	} else if (winPtr->flags & TK_TOP_LEVEL) {
	    cursor = LoadCursor(NULL, TK_DEFAULT_CURSOR);
	    break;
	}
	winPtr = winPtr->parentPtr;
    }
    if (cursor != NULL) {
	SetCursor(cursor);
    }
}

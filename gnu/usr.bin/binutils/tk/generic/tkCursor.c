/* 
 * tkCursor.c --
 *
 *	This file maintains a database of read-only cursors for the Tk
 *	toolkit.  This allows cursors to be shared between widgets and
 *	also avoids round-trips to the X server.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkCursor.c 1.27 96/02/15 18:52:40
 */

#include "tkPort.h"
#include "tkInt.h"

/*
 * A TkCursor structure exists for each cursor that is currently
 * active.  Each structure is indexed with two hash tables defined
 * below.  One of the tables is idTable, and the other is either
 * nameTable or dataTable, also defined below.
 */

/*
 * Hash table to map from a textual description of a cursor to the
 * TkCursor record for the cursor, and key structure used in that
 * hash table:
 */

static Tcl_HashTable nameTable;
typedef struct {
    Tk_Uid name;		/* Textual name for desired cursor. */
    Display *display;		/* Display for which cursor will be used. */
} NameKey;

/*
 * Hash table to map from a collection of in-core data about a
 * cursor (bitmap contents, etc.) to a TkCursor structure:
 */

static Tcl_HashTable dataTable;
typedef struct {
    char *source;		/* Cursor bits. */
    char *mask;			/* Mask bits. */
    int width, height;		/* Dimensions of cursor (and data
				 * and mask). */
    int xHot, yHot;		/* Location of cursor hot-spot. */
    Tk_Uid fg, bg;		/* Colors for cursor. */
    Display *display;		/* Display on which cursor will be used. */
} DataKey;

/*
 * Hash table that maps from <display + cursor id> to the TkCursor structure
 * for the cursor.  This table is used by Tk_FreeCursor.
 */

static Tcl_HashTable idTable;
typedef struct {
    Display *display;		/* Display for which cursor was allocated. */
    Tk_Cursor cursor;		/* Cursor identifier. */
} IdKey;

static int initialized = 0;	/* 0 means static structures haven't been
				 * initialized yet. */

/*
 * Forward declarations for procedures defined in this file:
 */

static void		CursorInit _ANSI_ARGS_((void));

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetCursor --
 *
 *	Given a string describing a cursor, locate (or create if necessary)
 *	a cursor that fits the description.
 *
 * Results:
 *	The return value is the X identifer for the desired cursor,
 *	unless string couldn't be parsed correctly.  In this case,
 *	None is returned and an error message is left in interp->result.
 *	The caller should never modify the cursor that is returned, and
 *	should eventually call Tk_FreeCursor when the cursor is no longer
 *	needed.
 *
 * Side effects:
 *	The cursor is added to an internal database with a reference count.
 *	For each call to this procedure, there should eventually be a call
 *	to Tk_FreeCursor, so that the database can be cleaned up when cursors
 *	aren't needed anymore.
 *
 *----------------------------------------------------------------------
 */

Tk_Cursor
Tk_GetCursor(interp, tkwin, string)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tk_Window tkwin;		/* Window in which cursor will be used. */
    Tk_Uid string;		/* Description of cursor.  See manual entry
				 * for details on legal syntax. */
{
    NameKey nameKey;
    IdKey idKey;
    Tcl_HashEntry *nameHashPtr, *idHashPtr;
    register TkCursor *cursorPtr;
    int new;

    if (!initialized) {
	CursorInit();
    }

    nameKey.name = string;
    nameKey.display = Tk_Display(tkwin);
    nameHashPtr = Tcl_CreateHashEntry(&nameTable, (char *) &nameKey, &new);
    if (!new) {
	cursorPtr = (TkCursor *) Tcl_GetHashValue(nameHashPtr);
	cursorPtr->refCount++;
	return cursorPtr->cursor;
    }

    cursorPtr = TkGetCursorByName(interp, tkwin, string);

    if (cursorPtr == NULL) {
	Tcl_DeleteHashEntry(nameHashPtr);
	return None;
    }

    /*
     * Add information about this cursor to our database.
     */

    cursorPtr->refCount = 1;
    cursorPtr->otherTable = &nameTable;
    cursorPtr->hashPtr = nameHashPtr;
    idKey.display = nameKey.display;
    idKey.cursor = cursorPtr->cursor;
    idHashPtr = Tcl_CreateHashEntry(&idTable, (char *) &idKey, &new);
    if (!new) {
	panic("cursor already registered in Tk_GetCursor");
    }
    Tcl_SetHashValue(nameHashPtr, cursorPtr);
    Tcl_SetHashValue(idHashPtr, cursorPtr);

    return cursorPtr->cursor;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetCursorFromData --
 *
 *	Given a description of the bits and colors for a cursor,
 *	make a cursor that has the given properties.
 *
 * Results:
 *	The return value is the X identifer for the desired cursor,
 *	unless it couldn't be created properly.  In this case, None is
 *	returned and an error message is left in interp->result.  The
 *	caller should never modify the cursor that is returned, and
 *	should eventually call Tk_FreeCursor when the cursor is no
 *	longer needed.
 *
 * Side effects:
 *	The cursor is added to an internal database with a reference count.
 *	For each call to this procedure, there should eventually be a call
 *	to Tk_FreeCursor, so that the database can be cleaned up when cursors
 *	aren't needed anymore.
 *
 *----------------------------------------------------------------------
 */

Tk_Cursor
Tk_GetCursorFromData(interp, tkwin, source, mask, width, height,
	xHot, yHot, fg, bg)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tk_Window tkwin;		/* Window in which cursor will be used. */
    char *source;		/* Bitmap data for cursor shape. */
    char *mask;			/* Bitmap data for cursor mask. */
    int width, height;		/* Dimensions of cursor. */
    int xHot, yHot;		/* Location of hot-spot in cursor. */
    Tk_Uid fg;			/* Foreground color for cursor. */
    Tk_Uid bg;			/* Background color for cursor. */
{
    DataKey dataKey;
    IdKey idKey;
    Tcl_HashEntry *dataHashPtr, *idHashPtr;
    register TkCursor *cursorPtr;
    int new;
    XColor fgColor, bgColor;

    if (!initialized) {
	CursorInit();
    }

    dataKey.source = source;
    dataKey.mask = mask;
    dataKey.width = width;
    dataKey.height = height;
    dataKey.xHot = xHot;
    dataKey.yHot = yHot;
    dataKey.fg = fg;
    dataKey.bg = bg;
    dataKey.display = Tk_Display(tkwin);
    dataHashPtr = Tcl_CreateHashEntry(&dataTable, (char *) &dataKey, &new);
    if (!new) {
	cursorPtr = (TkCursor *) Tcl_GetHashValue(dataHashPtr);
	cursorPtr->refCount++;
	return cursorPtr->cursor;
    }

    /*
     * No suitable cursor exists yet.  Make one using the data
     * available and add it to the database.
     */

    if (XParseColor(dataKey.display, Tk_Colormap(tkwin), fg, &fgColor) == 0) {
	Tcl_AppendResult(interp, "invalid color name \"", fg, "\"",
		(char *) NULL);
	goto error;
    }
    if (XParseColor(dataKey.display, Tk_Colormap(tkwin), bg, &bgColor) == 0) {
	Tcl_AppendResult(interp, "invalid color name \"", bg, "\"",
		(char *) NULL);
	goto error;
    }

    cursorPtr = TkCreateCursorFromData(tkwin, source, mask, width, height,
	    xHot, yHot, fgColor, bgColor);

    if (cursorPtr == NULL) {
	goto error;
    }

    cursorPtr->refCount = 1;
    cursorPtr->otherTable = &dataTable;
    cursorPtr->hashPtr = dataHashPtr;
    idKey.display = dataKey.display;
    idKey.cursor = cursorPtr->cursor;
    idHashPtr = Tcl_CreateHashEntry(&idTable, (char *) &idKey, &new);
    if (!new) {
	panic("cursor already registered in Tk_GetCursorFromData");
    }
    Tcl_SetHashValue(dataHashPtr, cursorPtr);
    Tcl_SetHashValue(idHashPtr, cursorPtr);
    return cursorPtr->cursor;

    error:
    Tcl_DeleteHashEntry(dataHashPtr);
    return None;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_NameOfCursor --
 *
 *	Given a cursor, return a textual string identifying it.
 *
 * Results:
 *	If cursor was created by Tk_GetCursor, then the return
 *	value is the "string" that was used to create it.
 *	Otherwise the return value is a string giving the X
 *	identifier for the cursor.  The storage for the returned
 *	string is only guaranteed to persist up until the next
 *	call to this procedure.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Tk_NameOfCursor(display, cursor)
    Display *display;		/* Display for which cursor was allocated. */
    Tk_Cursor cursor;		/* Identifier for cursor whose name is
				 * wanted. */
{
    IdKey idKey;
    Tcl_HashEntry *idHashPtr;
    TkCursor *cursorPtr;
    static char string[20];

    if (!initialized) {
	printid:
	sprintf(string, "cursor id 0x%x", (unsigned int) cursor);
	return string;
    }
    idKey.display = display;
    idKey.cursor = cursor;
    idHashPtr = Tcl_FindHashEntry(&idTable, (char *) &idKey);
    if (idHashPtr == NULL) {
	goto printid;
    }
    cursorPtr = (TkCursor *) Tcl_GetHashValue(idHashPtr);
    if (cursorPtr->otherTable != &nameTable) {
	goto printid;
    }
    return ((NameKey *) cursorPtr->hashPtr->key.words)->name;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_FreeCursor --
 *
 *	This procedure is called to release a cursor allocated by
 *	Tk_GetCursor or TkGetCursorFromData.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The reference count associated with cursor is decremented, and
 *	it is officially deallocated if no-one is using it anymore.
 *
 *----------------------------------------------------------------------
 */

void
Tk_FreeCursor(display, cursor)
    Display *display;		/* Display for which cursor was allocated. */
    Tk_Cursor cursor;		/* Identifier for cursor to be released. */
{
    IdKey idKey;
    Tcl_HashEntry *idHashPtr;
    register TkCursor *cursorPtr;

    if (!initialized) {
	panic("Tk_FreeCursor called before Tk_GetCursor");
    }

    idKey.display = display;
    idKey.cursor = cursor;
    idHashPtr = Tcl_FindHashEntry(&idTable, (char *) &idKey);
    if (idHashPtr == NULL) {
	panic("Tk_FreeCursor received unknown cursor argument");
    }
    cursorPtr = (TkCursor *) Tcl_GetHashValue(idHashPtr);
    cursorPtr->refCount--;
    if (cursorPtr->refCount == 0) {
	Tcl_DeleteHashEntry(cursorPtr->hashPtr);
	Tcl_DeleteHashEntry(idHashPtr);
	TkFreeCursor(cursorPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CursorInit --
 *
 *	Initialize the structures used for cursor management.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Read the code.
 *
 *----------------------------------------------------------------------
 */

static void
CursorInit()
{
    initialized = 1;
    Tcl_InitHashTable(&nameTable, sizeof(NameKey)/sizeof(int));
    Tcl_InitHashTable(&dataTable, sizeof(DataKey)/sizeof(int));

    /*
     * The call below is tricky:  can't use sizeof(IdKey) because it
     * gets padded with extra unpredictable bytes on some 64-bit
     * machines.
     */

    Tcl_InitHashTable(&idTable, (sizeof(Display *) + sizeof(Tk_Cursor))
	    /sizeof(int));
}

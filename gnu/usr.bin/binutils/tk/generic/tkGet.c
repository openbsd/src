/* 
 * tkGet.c --
 *
 *	This file contains a number of "Tk_GetXXX" procedures, which
 *	parse text strings into useful forms for Tk.  This file has
 *	the simpler procedures, like Tk_GetDirection and Tk_GetUid.
 *	The more complex procedures like Tk_GetColor are in separate
 *	files.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkGet.c 1.12 96/02/15 18:53:33
 */

#include "tkInt.h"
#include "tkPort.h"

/*
 * The hash table below is used to keep track of all the Tk_Uids created
 * so far.
 */

static Tcl_HashTable uidTable;
static int initialized = 0;

/*
 *--------------------------------------------------------------
 *
 * Tk_GetAnchor --
 *
 *	Given a string, return the corresponding Tk_Anchor.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	position is stored at *anchorPtr;  otherwise TCL_ERROR
 *	is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tk_GetAnchor(interp, string, anchorPtr)
    Tcl_Interp *interp;		/* Use this for error reporting. */
    char *string;		/* String describing a direction. */
    Tk_Anchor *anchorPtr;	/* Where to store Tk_Anchor corresponding
				 * to string. */
{
    switch (string[0]) {
	case 'n':
	    if (string[1] == 0) {
		*anchorPtr = TK_ANCHOR_N;
		return TCL_OK;
	    } else if ((string[1] == 'e') && (string[2] == 0)) {
		*anchorPtr = TK_ANCHOR_NE;
		return TCL_OK;
	    } else if ((string[1] == 'w') && (string[2] == 0)) {
		*anchorPtr = TK_ANCHOR_NW;
		return TCL_OK;
	    }
	    goto error;
	case 's':
	    if (string[1] == 0) {
		*anchorPtr = TK_ANCHOR_S;
		return TCL_OK;
	    } else if ((string[1] == 'e') && (string[2] == 0)) {
		*anchorPtr = TK_ANCHOR_SE;
		return TCL_OK;
	    } else if ((string[1] == 'w') && (string[2] == 0)) {
		*anchorPtr = TK_ANCHOR_SW;
		return TCL_OK;
	    } else {
		goto error;
	    }
	case 'e':
	    if (string[1] == 0) {
		*anchorPtr = TK_ANCHOR_E;
		return TCL_OK;
	    }
	    goto error;
	case 'w':
	    if (string[1] == 0) {
		*anchorPtr = TK_ANCHOR_W;
		return TCL_OK;
	    }
	    goto error;
	case 'c':
	    if (strncmp(string, "center", strlen(string)) == 0) {
		*anchorPtr = TK_ANCHOR_CENTER;
		return TCL_OK;
	    }
	    goto error;
    }

    error:
    Tcl_AppendResult(interp, "bad anchor position \"", string,
	    "\": must be n, ne, e, se, s, sw, w, nw, or center",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_NameOfAnchor --
 *
 *	Given a Tk_Anchor, return the string that corresponds
 *	to it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Tk_NameOfAnchor(anchor)
    Tk_Anchor anchor;		/* Anchor for which identifying string
				 * is desired. */
{
    switch (anchor) {
	case TK_ANCHOR_N: return "n";
	case TK_ANCHOR_NE: return "ne";
	case TK_ANCHOR_E: return "e";
	case TK_ANCHOR_SE: return "se";
	case TK_ANCHOR_S: return "s";
	case TK_ANCHOR_SW: return "sw";
	case TK_ANCHOR_W: return "w";
	case TK_ANCHOR_NW: return "nw";
	case TK_ANCHOR_CENTER: return "center";
    }
    return "unknown anchor position";
}

/*
 *--------------------------------------------------------------
 *
 * Tk_GetJoinStyle --
 *
 *	Given a string, return the corresponding Tk_JoinStyle.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	justification is stored at *joinPtr;  otherwise
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tk_GetJoinStyle(interp, string, joinPtr)
    Tcl_Interp *interp;		/* Use this for error reporting. */
    char *string;		/* String describing a justification style. */
    int *joinPtr;		/* Where to store join style corresponding
				 * to string. */
{
    int c;
    size_t length;

    c = string[0];
    length = strlen(string);

    if ((c == 'b') && (strncmp(string, "bevel", length) == 0)) {
	*joinPtr = JoinBevel;
	return TCL_OK;
    }
    if ((c == 'm') && (strncmp(string, "miter", length) == 0)) {
	*joinPtr = JoinMiter;
	return TCL_OK;
    }
    if ((c == 'r') && (strncmp(string, "round", length) == 0)) {
	*joinPtr = JoinRound;
	return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad join style \"", string,
	    "\": must be bevel, miter, or round",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_NameOfJoinStyle --
 *
 *	Given a Tk_JoinStyle, return the string that corresponds
 *	to it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Tk_NameOfJoinStyle(join)
    int join;			/* Join style for which identifying string
				 * is desired. */
{
    switch (join) {
	case JoinBevel: return "bevel";
	case JoinMiter: return "miter";
	case JoinRound: return "round";
    }
    return "unknown join style";
}

/*
 *--------------------------------------------------------------
 *
 * Tk_GetCapStyle --
 *
 *	Given a string, return the corresponding Tk_CapStyle.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	justification is stored at *capPtr;  otherwise
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tk_GetCapStyle(interp, string, capPtr)
    Tcl_Interp *interp;		/* Use this for error reporting. */
    char *string;		/* String describing a justification style. */
    int *capPtr;		/* Where to store cap style corresponding
				 * to string. */
{
    int c;
    size_t length;

    c = string[0];
    length = strlen(string);

    if ((c == 'b') && (strncmp(string, "butt", length) == 0)) {
	*capPtr = CapButt;
	return TCL_OK;
    }
    if ((c == 'p') && (strncmp(string, "projecting", length) == 0)) {
	*capPtr = CapProjecting;
	return TCL_OK;
    }
    if ((c == 'r') && (strncmp(string, "round", length) == 0)) {
	*capPtr = CapRound;
	return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad cap style \"", string,
	    "\": must be butt, projecting, or round",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_NameOfCapStyle --
 *
 *	Given a Tk_CapStyle, return the string that corresponds
 *	to it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Tk_NameOfCapStyle(cap)
    int cap;			/* Cap style for which identifying string
				 * is desired. */
{
    switch (cap) {
	case CapButt: return "butt";
	case CapProjecting: return "projecting";
	case CapRound: return "round";
    }
    return "unknown cap style";
}

/*
 *--------------------------------------------------------------
 *
 * Tk_GetJustify --
 *
 *	Given a string, return the corresponding Tk_Justify.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	justification is stored at *justifyPtr;  otherwise
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tk_GetJustify(interp, string, justifyPtr)
    Tcl_Interp *interp;		/* Use this for error reporting. */
    char *string;		/* String describing a justification style. */
    Tk_Justify *justifyPtr;	/* Where to store Tk_Justify corresponding
				 * to string. */
{
    int c;
    size_t length;

    c = string[0];
    length = strlen(string);

    if ((c == 'l') && (strncmp(string, "left", length) == 0)) {
	*justifyPtr = TK_JUSTIFY_LEFT;
	return TCL_OK;
    }
    if ((c == 'r') && (strncmp(string, "right", length) == 0)) {
	*justifyPtr = TK_JUSTIFY_RIGHT;
	return TCL_OK;
    }
    if ((c == 'c') && (strncmp(string, "center", length) == 0)) {
	*justifyPtr = TK_JUSTIFY_CENTER;
	return TCL_OK;
    }

    Tcl_AppendResult(interp, "bad justification \"", string,
	    "\": must be left, right, or center",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_NameOfJustify --
 *
 *	Given a Tk_Justify, return the string that corresponds
 *	to it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Tk_NameOfJustify(justify)
    Tk_Justify justify;		/* Justification style for which
				 * identifying string is desired. */
{
    switch (justify) {
	case TK_JUSTIFY_LEFT: return "left";
	case TK_JUSTIFY_RIGHT: return "right";
	case TK_JUSTIFY_CENTER: return "center";
    }
    return "unknown justification style";
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetUid --
 *
 *	Given a string, this procedure returns a unique identifier
 *	for the string.
 *
 * Results:
 *	This procedure returns a Tk_Uid corresponding to the "string"
 *	argument.  The Tk_Uid has a string value identical to string
 *	(strcmp will return 0), but it's guaranteed that any other
 *	calls to this procedure with a string equal to "string" will
 *	return exactly the same result (i.e. can compare Tk_Uid
 *	*values* directly, without having to call strcmp on what they
 *	point to).
 *
 * Side effects:
 *	New information may be entered into the identifier table.
 *
 *----------------------------------------------------------------------
 */

Tk_Uid
Tk_GetUid(string)
    char *string;		/* String to convert. */
{
    int dummy;

    if (!initialized) {
	Tcl_InitHashTable(&uidTable, TCL_STRING_KEYS);
	initialized = 1;
    }
    return (Tk_Uid) Tcl_GetHashKey(&uidTable,
	    Tcl_CreateHashEntry(&uidTable, string, &dummy));
}

/*
 *--------------------------------------------------------------
 *
 * Tk_GetScreenMM --
 *
 *	Given a string, returns the number of screen millimeters
 *	corresponding to that string.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	screen distance is stored at *doublePtr;  otherwise
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tk_GetScreenMM(interp, tkwin, string, doublePtr)
    Tcl_Interp *interp;		/* Use this for error reporting. */
    Tk_Window tkwin;		/* Window whose screen determines conversion
				 * from centimeters and other absolute
				 * units. */
    char *string;		/* String describing a screen distance. */
    double *doublePtr;		/* Place to store converted result. */
{
    char *end;
    double d;

    d = strtod(string, &end);
    if (end == string) {
	error:
	Tcl_AppendResult(interp, "bad screen distance \"", string,
		"\"", (char *) NULL);
	return TCL_ERROR;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    switch (*end) {
	case 0:
	    d /= WidthOfScreen(Tk_Screen(tkwin));
	    d *= WidthMMOfScreen(Tk_Screen(tkwin));
	    break;
	case 'c':
	    d *= 10;
	    end++;
	    break;
	case 'i':
	    d *= 25.4;
	    end++;
	    break;
	case 'm':
	    end++;
	    break;
	case 'p':
	    d *= 25.4/72.0;
	    end++;
	    break;
	default:
	    goto error;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    if (*end != 0) {
	goto error;
    }
    *doublePtr = d;
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_GetPixels --
 *
 *	Given a string, returns the number of pixels corresponding
 *	to that string.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	rounded pixel distance is stored at *intPtr;  otherwise
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tk_GetPixels(interp, tkwin, string, intPtr)
    Tcl_Interp *interp;		/* Use this for error reporting. */
    Tk_Window tkwin;		/* Window whose screen determines conversion
				 * from centimeters and other absolute
				 * units. */
    char *string;		/* String describing a justification style. */
    int *intPtr;		/* Place to store converted result. */
{
    char *end;
    double d;

    d = strtod(string, &end);
    if (end == string) {
	error:
	Tcl_AppendResult(interp, "bad screen distance \"", string,
		"\"", (char *) NULL);
	return TCL_ERROR;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    switch (*end) {
	case 0:
	    break;
	case 'c':
	    d *= 10*WidthOfScreen(Tk_Screen(tkwin));
	    d /= WidthMMOfScreen(Tk_Screen(tkwin));
	    end++;
	    break;
	case 'i':
	    d *= 25.4*WidthOfScreen(Tk_Screen(tkwin));
	    d /= WidthMMOfScreen(Tk_Screen(tkwin));
	    end++;
	    break;
	case 'm':
	    d *= WidthOfScreen(Tk_Screen(tkwin));
	    d /= WidthMMOfScreen(Tk_Screen(tkwin));
	    end++;
	    break;
	case 'p':
	    d *= (25.4/72.0)*WidthOfScreen(Tk_Screen(tkwin));
	    d /= WidthMMOfScreen(Tk_Screen(tkwin));
	    end++;
	    break;
	default:
	    goto error;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) {
	end++;
    }
    if (*end != 0) {
	goto error;
    }
    if (d < 0) {
	*intPtr = (int) (d - 0.5);
    } else {
	*intPtr = (int) (d + 0.5);
    }
    return TCL_OK;
}

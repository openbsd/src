/* 
 * tkMacFont.c --
 *
 *	This file maintains a database of looked-up fonts for the Tk
 *	toolkit, in order to avoid round-trips to the server to map
 *	font names to XFontStructs.  It also provides several utility
 *	procedures for measuring and displaying text.  Many of these
 *	routines use Mac specific calls.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacFont.c 1.35 96/04/09 19:53:15
 */

#include "tkPort.h"
#include "tkInt.h"
#include "tkMacInt.h"

#include <Windows.h>
#include <Strings.h>
#include <Fonts.h>
#include <Resources.h>

/*
 * This module caches extra information about fonts in addition to
 * what X already provides.  The extra information is used by the
 * TkMeasureChars procedure, and consists of two parts:  a type and
 * a width.  The type is one of the following:
 *
 * NORMAL:		Standard character.
 * TAB:			Tab character:  output enough space to
 *			get to next tab stop.
 * NEWLINE:		Newline character:  don't output anything more
 *			on this line (character has infinite width).
 * REPLACE:		This character doesn't print:  instead of
 *			displaying character, display a replacement
 *			sequence of the form "\xdd" where dd is the
 *			hex equivalent of the character.
 * SKIP:		Don't display anything for this character.  This
 *			is only used where the font doesn't contain
 *			all the characters needed to generate
 *			replacement sequences.
 * The width gives the total width of the displayed character or
 * sequence:  for replacement sequences, it gives the width of the
 * sequence.
 */

#define NORMAL		1
#define TAB		2
#define NEWLINE		3
#define REPLACE		4
#define SKIP		5

/*
 * Size of buffer used to measure a series of chars.
 */
#define BUF_SZ 40

/*
 * One of the following data structures exists for each font that is
 * currently active.  The structure is indexed with two hash tables,
 * one based on font name and one based on XFontStruct address.
 */

typedef struct {
    XFontStruct *fontStructPtr;	/* X information about font. */
    Display *display;		/* Display to which font belongs. */
    int refCount;		/* Number of active uses of this font. */
    char *types;		/* Malloc'ed array giving types of all
				 * chars in the font (may be NULL). */
    unsigned char *widths;	/* Malloc'ed array giving widths of all
				 * chars in the font (may be NULL). */
    int tabWidth;		/* Width of tabs in this font. */
    Tcl_HashEntry *nameHashPtr;	/* Entry in name-based hash table (needed
				 * when deleting this structure). */
} TkFont;

/*
 * Hash table for name -> TkFont mapping, and key structure used to
 * index into that table:
 */

static Tcl_HashTable nameTable;
typedef struct {
    Tk_Uid name;		/* Name of font. */
    Display *display;		/* Display for which font is valid. */
} NameKey;

/*
 * Hash table for font struct -> TkFont mapping. This table is
 * indexed by the XFontStruct address.
 */

static Tcl_HashTable fontTable;

static int initialized = 0;	/* 0 means static structures haven't been
				 * initialized yet. */

/*
 * To speed up TkMeasureChars, the variables below keep the last
 * mapping from (XFontStruct *) to (TkFont *).
 */

static TkFont *lastFontPtr = NULL;
static XFontStruct *lastFontStructPtr = NULL;
static GWorldPtr gWorld = NULL;

/*
 * Characters used when displaying control sequences as their
 * hex equivalents.
 */

static char hexChars[] = "0123456789abcdefx\\";

/*
 * The following table maps some control characters to sequences
 * like '\n' rather than '\x10'.  A zero entry in the table means
 * no such mapping exists, and the table only maps characters
 * less than 0x10.
 */

static char mapChars[] = {
    0, 0, 0, 0, 0, 0, 0,
    'a', 'b', 't', 'n', 'v', 'f', 'r',
    0
};

/*
 * The Macintosh doesn't have have a notion of Font objects like Unix
 * and Windows does.  These datastructures are used to help create such
 * a notion.
 */
 
static Tcl_HashTable macFontTable;
static int macFontInit = 0;	/* 0 means static structures haven't been
				 * initialized yet. */
typedef struct {
    int txFont;		/* Name of font family. */
    int txFace;		/* The style of font. */
    int txSize;		/* Point Size. */
} FontKey;

/*
 * Forward declarations for procedures defined in this file:
 */

static void		FontInit _ANSI_ARGS_((void));
static void		SetFontMetrics _ANSI_ARGS_((TkFont *fontPtr));
static void		MacFontInit _ANSI_ARGS_((void));
static int		XNameToFont _ANSI_ARGS_((_Xconst char *name,
			    short *family, short *style, short *size));

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetFontStruct --
 *
 *	Given a string name for a font, map the name to an XFontStruct
 *	describing the font.
 *
 * Results:
 *	The return value is normally a pointer to the font description
 *	for the desired font.  If an error occurs in mapping the string
 *	to a font, then an error message will be left in interp->result
 *	and NULL will be returned.
 *
 * Side effects:
 *	The font is added to an internal database with a reference count.
 *	For each call to this procedure, there should eventually be a call
 *	to Tk_FreeFontStruct, so that the database is cleaned up when fonts
 *	aren't in use anymore.
 *
 *----------------------------------------------------------------------
 */

XFontStruct *
Tk_GetFontStruct(interp, tkwin, name)
    Tcl_Interp *interp;		/* Place to leave error message if
				 * font can't be found. */
    Tk_Window tkwin;		/* Window in which font will be used. */
    Tk_Uid name;		/* Name of font (in form suitable for
				 * passing to XLoadQueryFont). */
{
    NameKey nameKey;
    Tcl_HashEntry *nameHashPtr, *fontHashPtr;
    int new;
    register TkFont *fontPtr;
    XFontStruct *fontStructPtr;

    if (!initialized) {
	FontInit();
    }

    /*
     * First, check to see if there's already a mapping for this font
     * name.
     */

    nameKey.name = name;
    nameKey.display = Tk_Display(tkwin);
    nameHashPtr = Tcl_CreateHashEntry(&nameTable, (char *) &nameKey, &new);
    if (!new) {
	fontPtr = (TkFont *) Tcl_GetHashValue(nameHashPtr);
	fontPtr->refCount++;
	return fontPtr->fontStructPtr;
    }

    /*
     * The name isn't currently known.  Map from the name to a font, and
     * add a new structure to the database.
     */

    fontStructPtr = XLoadQueryFont(nameKey.display, name);
    if (fontStructPtr == NULL) {
	Tcl_DeleteHashEntry(nameHashPtr);
	Tcl_AppendResult(interp, "font \"", name, "\" doesn't exist",
		(char *) NULL);
	return NULL;
    }
    fontPtr = (TkFont *) ckalloc(sizeof(TkFont));
    fontPtr->display = nameKey.display;
    fontPtr->fontStructPtr = fontStructPtr;
    fontPtr->refCount = 1;
    fontPtr->types = NULL;
    fontPtr->widths = NULL;
    fontPtr->nameHashPtr = nameHashPtr;
    fontHashPtr = Tcl_CreateHashEntry(&fontTable, (char *) fontStructPtr, &new);
    if (!new) {
	panic("XFontStruct already registered in Tk_GetFontStruct");
    }
    Tcl_SetHashValue(nameHashPtr, fontPtr);
    Tcl_SetHashValue(fontHashPtr, fontPtr);
    return fontPtr->fontStructPtr;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_NameOfFontStruct --
 *
 *	Given a font, return a textual string identifying it.
 *
 * Results:
 *	If font was created by Tk_GetFontStruct, then the return
 *	value is the "string" that was used to create it.
 *	Otherwise the return value is a string giving the X
 *	identifier for the font.  The storage for the returned
 *	string is only guaranteed to persist up until the next
 *	call to this procedure.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Tk_NameOfFontStruct(fontStructPtr)
    XFontStruct *fontStructPtr;		/* Font whose name is desired. */
{
    Tcl_HashEntry *fontHashPtr;
    TkFont *fontPtr;
    static char string[20];

    if (!initialized) {
	printid:
	sprintf(string, "font id 0x%x", (unsigned int) fontStructPtr->fid);
	return string;
    }
    fontHashPtr = Tcl_FindHashEntry(&fontTable, (char *) fontStructPtr);
    if (fontHashPtr == NULL) {
	goto printid;
    }
    fontPtr = (TkFont *) Tcl_GetHashValue(fontHashPtr);
    return ((NameKey *) fontPtr->nameHashPtr->key.words)->name;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_FreeFontStruct --
 *
 *	This procedure is called to release a font allocated by
 *	Tk_GetFontStruct.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The reference count associated with font is decremented, and
 *	the font is officially deallocated if no-one is using it
 *	anymore.
 *
 *----------------------------------------------------------------------
 */

void
Tk_FreeFontStruct(fontStructPtr)
    XFontStruct *fontStructPtr;	/* Font to be released. */
{
    Tcl_HashEntry *fontHashPtr;
    register TkFont *fontPtr;

    if (!initialized) {
	panic("Tk_FreeFontStruct called before Tk_GetFontStruct");
    }

    fontHashPtr = Tcl_FindHashEntry(&fontTable, (char *) fontStructPtr);
    if (fontHashPtr == NULL) {
	panic("Tk_FreeFontStruct received unknown font argument");
    }
    fontPtr = (TkFont *) Tcl_GetHashValue(fontHashPtr);
    fontPtr->refCount--;
    if (fontPtr->refCount == 0) {
	/*
	 * We really should call Tk_FreeXId below to release the font's
	 * resource identifier, but this seems to cause problems on
	 * many X servers (as of 5/1/94) where the font resource isn't
	 * really released, which can cause the wrong font to be used
	 * later on.  So, don't release the resource id after all, even
	 * though this results in an id leak.
	 *
	 * Tk_FreeXId(fontPtr->display, (XID) fontPtr->fontStructPtr->fid);
	 */

	XFreeFont(fontPtr->display, fontPtr->fontStructPtr);
	Tcl_DeleteHashEntry(fontPtr->nameHashPtr);
	Tcl_DeleteHashEntry(fontHashPtr);
	if (fontPtr->types != NULL) {
	    ckfree(fontPtr->types);
	}
	if (fontPtr->widths != NULL) {
	    ckfree((char *) fontPtr->widths);
	}
	ckfree((char *) fontPtr);
	lastFontStructPtr = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FontInit --
 *
 *	Initialize the structure used for font management.
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
FontInit()
{
    initialized = 1;
    Tcl_InitHashTable(&nameTable, sizeof(NameKey)/sizeof(int));
    Tcl_InitHashTable(&fontTable, TCL_ONE_WORD_KEYS);
}

/*
 *--------------------------------------------------------------
 *
 * SetFontMetrics --
 *
 *	This procedure is called to fill in the "widths" and "types"
 *	arrays for a font.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	FontPtr gets modified to hold font metric information.
 *
 *--------------------------------------------------------------
 */

static void
SetFontMetrics(fontPtr)
    register TkFont *fontPtr;		/* Font structure in which to
					 * set metrics. */
{
    int i, replaceOK, baseWidth, len;
    register XFontStruct *fontStructPtr = fontPtr->fontStructPtr;
    short widths[10];    
    char replace[10];    
    char *p, c;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    Rect rect;
    QDErr err;

    if (gWorld == NULL) {
	err = NewGWorld(&gWorld, 0, &rect, NULL, NULL, 0);
	if (err != noErr) {
	    panic("NewGWorld failed in SetFontMetrics");
	}
    }
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(gWorld, NULL);

    TextFont((short) fontStructPtr->min_byte1);
    TextSize((short) fontStructPtr->max_byte1);
    TextFace((short) fontStructPtr->min_char_or_byte2);

    /*
     * Pass 1: initialize the arrays.
     */

    fontPtr->types = (char *) ckalloc(256);
    fontPtr->widths = (unsigned char *) ckalloc(256);
    for (i = 0; i < 256; i++) {
	fontPtr->types[i] = REPLACE;
    }

    /*
     * Pass 2:  for all characters that exist in the font and are
     * not control characters, fill in the type and width
     * information.
     */

    for (i = ' '; i < 256;  i++) {
	if ((i == 0177) || (i > fontStructPtr->max_char_or_byte2)) {
	    continue;
	}
	fontPtr->types[i] = NORMAL;
	c = i & 0xff;
	fontPtr->widths[i] = (unsigned char) CharWidth(c);
    }

    /*
     * Pass 3: fill in information for characters that have to
     * be replaced with  "\xhh" strings.  If the font doesn't
     * have the characters needed for this, then just use the
     * font's default character.
     */

    replaceOK = 1;
    baseWidth = fontPtr->widths['\\'] + fontPtr->widths['x'];
    replace[0] = '\\';
    for (p = hexChars; *p != 0; p++) {
	if (fontPtr->types[*p] != NORMAL) {
	    replaceOK = 0;
	    break;
	}
    }
    for (i = 0; i < 256; i++) {
	if (fontPtr->types[i] != REPLACE) {
	    continue;
	}
	if (replaceOK) {
	    if ((i < sizeof(mapChars)) && (mapChars[i] != 0)) {
		replace[1] = mapChars[i];
		len = 2;
	    } else {
		replace[1] = 'x';
		replace[2] = hexChars[(i >> 4) & 0xf];
		replace[3] = hexChars[i & 0xf];
		len = 4;
	    }
	    TkGetCharPositions(fontPtr->fontStructPtr, replace, len, widths);
	    fontPtr->widths[i] = widths[len];
	} else {
	    fontPtr->types[i] = SKIP;
	    fontPtr->widths[i] = 0;
	}
    }

    /*
     * Lastly, fill in special information for newline and tab.
     */

    fontPtr->types['\n'] = NEWLINE;
    fontPtr->widths['\n'] = 0;
    fontPtr->types['\r'] = NEWLINE;
    fontPtr->widths['\r'] = 0;
    fontPtr->types['\t'] = TAB;
    fontPtr->widths['\t'] = 0;
    if (fontPtr->types['0'] == NORMAL) {
	fontPtr->tabWidth = 8*fontPtr->widths['0'];
    } else {
	fontPtr->tabWidth = 8*fontStructPtr->max_bounds.width;
    }

    /*
     * Make sure the tab width isn't zero (some fonts may not have enough
     * information to set a reasonable tab width).
     */

    if (fontPtr->tabWidth == 0) {
	fontPtr->tabWidth = 1;
    }

    SetGWorld(saveWorld, saveDevice);
}

/*
 *--------------------------------------------------------------
 *
 * TkMeasureChars --
 *
 *	Measure the number of characters from a string that
 *	will fit in a given horizontal span.  The measurement
 *	is done under the assumption that TkDisplayChars will
 *	be used to actually display the characters.
 *
 *	Note: this is one of the slowest funcrtions on the Mac
 *	version of Tk.  We should probably try to optimize for
 *	certain cases - like fixed sized fonts.
 *
 * Results:
 *	The return value is the number of characters from source
 *	that fit in the span given by startX and maxX.  *nextXPtr
 *	is filled in with the x-coordinate at which the first
 *	character that didn't fit would be drawn, if it were to
 *	be drawn.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
TkMeasureChars(fontStructPtr, source, maxChars, startX, maxX,
	tabOrigin, flags, nextXPtr)
    XFontStruct *fontStructPtr;	/* Font in which to draw characters. */
    char *source;		/* Characters to be displayed.  Need not
				 * be NULL-terminated. */
    int maxChars;		/* Maximum # of characters to consider from
				 * source. */
    int startX;			/* X-position at which first character will
				 * be drawn. */
    int maxX;			/* Don't consider any character that would
				 * cross this x-position. */
    int tabOrigin;		/* X-location that serves as "origin" for
				 * tab stops. */
    int flags;			/* Various flag bits OR-ed together.
				 * TK_WHOLE_WORDS means stop on a word boundary
				 * (just before a space character) if
				 * possible.  TK_AT_LEAST_ONE means always
				 * return a value of at least one, even
				 * if the character doesn't fit. 
				 * TK_PARTIAL_OK means it's OK to display only
				 * a part of the last character in the line.
				 * TK_NEWLINES_NOT_SPECIAL means that newlines
				 * are treated just like other control chars:
				 * they don't terminate the line,*/
    int *nextXPtr;		/* Return x-position of terminating
				 * character here. */
{
    register TkFont *fontPtr;
    register char *p;		/* Current character. */
    register int c;
    char *term;			/* Pointer to most recent character that
				 * may legally be a terminating character. */
    int termX;			/* X-position just after term. */
    int curX;			/* X-position corresponding to p. */
    int newX;			/* X-position corresponding to p+1. */
    int type;
    short widths[BUF_SZ];	/* Array of normal characters */
    int norm; 			/* Index into widths array */
    int rem;

    /*
     * Find the TkFont structure for this font, and make sure its
     * font metrics exist.
     */

    if (lastFontStructPtr == fontStructPtr) {
	fontPtr = lastFontPtr;
    } else {
	Tcl_HashEntry *fontHashPtr;

	if (!initialized) {
	    badArg:
	    panic("TkMeasureChars received unknown font argument");
	}
    
	fontHashPtr = Tcl_FindHashEntry(&fontTable, (char *) fontStructPtr);
	if (fontHashPtr == NULL) {
	    goto badArg;
	}
	fontPtr = (TkFont *) Tcl_GetHashValue(fontHashPtr);
	lastFontStructPtr = fontPtr->fontStructPtr;
	lastFontPtr = fontPtr;
    }
    if (fontPtr->types == NULL) {
	SetFontMetrics(fontPtr);
    }

    /*
     * Scan the input string one character at a time, until a character
     * is found that crosses maxX.
     */

    norm = -1;
    newX = curX = startX;
    termX = 0;		/* Not needed, but eliminates compiler warning. */
    term = source;
    for (p = source, c = *p & 0xff; maxChars > 0; p++, maxChars--) {
	type = fontPtr->types[c];
	if (type == NORMAL) {
	    if (norm == -1) {
	    	TkGetCharPositions(fontPtr->fontStructPtr, p, 
		    (BUF_SZ < maxChars) ? BUF_SZ : maxChars, widths);
	    	norm = 0;
	    } else if (norm >= BUF_SZ - 2) {
	    	TkGetCharPositions(fontPtr->fontStructPtr, p-1, 
		    (BUF_SZ < (maxChars+1)) ? BUF_SZ : (maxChars+1), widths);
	    	norm = 1;
	    } else {
		norm++;
	    }
	} else {
	    norm = -1;
	}
	if (type == NORMAL) {
            newX += widths[norm+1] - widths[norm];
	} else if (type == REPLACE) {
	    newX += fontPtr->widths[c];
	} else if (type == TAB) {
	    if (!(flags & TK_IGNORE_TABS)) {
		newX += fontPtr->tabWidth;
		rem = (newX - tabOrigin) % fontPtr->tabWidth;
		if (rem < 0) {
		    rem += fontPtr->tabWidth;
		}
		newX -= rem;
	    }
	} else if (type == NEWLINE) {
	    if (flags & TK_NEWLINES_NOT_SPECIAL) {
		newX += fontPtr->widths[c];
	    } else {
		break;
	    }
	} else if (type != SKIP) {
	    panic("Unknown type %d in TkMeasureChars", type);
	}
	if (newX > maxX) {
	    break;
	}
	c = p[1] & 0xff;
	if (isspace(UCHAR(c)) || (c == 0)) {
	    term = p+1;
	    termX = newX;
	}
	curX = newX;
    }

    /*
     * P points to the first character that doesn't fit in the desired
     * span.  Use the flags to figure out what to return.
     */

    if ((flags & TK_PARTIAL_OK) && (curX < maxX)) {
	curX = newX;
	p++;
    }
    if ((flags & TK_AT_LEAST_ONE) && (term == source) && (maxChars > 0)
	    && !isspace(UCHAR(*term))) {
	term = p;
	termX = curX;
	if (term == source) {
	    term++;
	    termX = newX;
	}
    } else if ((maxChars == 0) || !(flags & TK_WHOLE_WORDS)) {
	term = p;
	termX = curX;
    }
    *nextXPtr = termX;
    return term-source;
}

/*
 *--------------------------------------------------------------
 *
 * TkDisplayChars --
 *
 *	Draw a string of characters on the screen, converting
 *	tabs to the right number of spaces and control characters
 *	to sequences of the form "\xhh" where hh are two hex
 *	digits.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets drawn on the screen.
 *
 *--------------------------------------------------------------
 */

void
TkDisplayChars(display, drawable, gc, fontStructPtr, string, numChars,
	x, y, tabOrigin, flags)
    Display *display;		/* Display on which to draw. */
    Drawable drawable;		/* Window or pixmap in which to draw. */
    GC gc;			/* Graphics context for actually drawing
				 * characters. */
    XFontStruct *fontStructPtr;	/* Font used in GC;  must have been allocated
				 * by Tk_GetFontStruct.  Used to compute sizes
				 * of tabs, etc. */
    char *string;		/* Characters to be displayed. */
    int numChars;		/* Number of characters to display from
				 * string. */
    int x, y;			/* Coordinates at which to draw string. */
    int tabOrigin;		/* X-location that serves as "origin" for
				 * tab stops. */
    int flags;			/* Flags to control display.  Only
				 * TK_NEWLINES_NOT_SPECIAL is supported right
				 * now.  See TkMeasureChars for information
				 * about it. */
{
    register TkFont *fontPtr;
    register char *p;		/* Current character being scanned. */
    register int c;
    int type;
    char *start;		/* First character waiting to be displayed. */
    int startX;			/* X-coordinate corresponding to start. */
    int curX;			/* X-coordinate corresponding to p. */
    char replace[10];
    int rem;

    /*
     * Find the TkFont structure for this font, and make sure its
     * font metrics exist.
     */

    if (lastFontStructPtr == fontStructPtr) {
	fontPtr = lastFontPtr;
    } else {
	Tcl_HashEntry *fontHashPtr;

	if (!initialized) {
	    badArg:
	    panic("TkDisplayChars received unknown font argument");
	}
    
	fontHashPtr = Tcl_FindHashEntry(&fontTable, (char *) fontStructPtr);
	if (fontHashPtr == NULL) {
	    goto badArg;
	}
	fontPtr = (TkFont *) Tcl_GetHashValue(fontHashPtr);
	lastFontStructPtr = fontPtr->fontStructPtr;
	lastFontPtr = fontPtr;
    }
    if (fontPtr->types == NULL) {
	SetFontMetrics(fontPtr);
    }

    /*
     * Scan the string one character at a time.  Display control
     * characters immediately, but delay displaying normal characters
     * in order to pass many characters to the server all together.
     */

    startX = curX = x;
    start = string;
    for (p = string; numChars > 0; numChars--, p++) {
	c = *p & 0xff;
	type = fontPtr->types[c];
	if (type == NORMAL) {
	    continue;
	}
	if (p != start) {
	    curX += XTextWidth(fontPtr->fontStructPtr, start, p - start);
	    XDrawString(display, drawable, gc, startX, y, start, p - start); 
	    startX = curX;
	}
	if (type == TAB) {
	    curX += fontPtr->tabWidth;
	    rem = (curX - tabOrigin) % fontPtr->tabWidth;
	    if (rem < 0) {
		rem += fontPtr->tabWidth;
	    }
	    curX -= rem;
	} else if (type == REPLACE || 
		(type == NEWLINE && flags & TK_NEWLINES_NOT_SPECIAL)) {
	    if ((c < sizeof(mapChars)) && (mapChars[c] != 0)) {
		replace[0] = '\\';
	        replace[1] = mapChars[c];
		XDrawString(display, drawable, gc, startX, y, replace, 2);
		curX += fontPtr->widths[c];
	    } else {
		replace[0] = '\\';
	        replace[1] = 'x';
	        replace[2] = hexChars[(c >> 4) & 0xf];
	        replace[3] = hexChars[c & 0xf];
		XDrawString(display, drawable, gc, startX, y, replace, 4);
		curX += fontPtr->widths[c];
  	    }
	} else if (type == NEWLINE) {
	    y += fontStructPtr->ascent + fontStructPtr->descent;
	    curX = x;
	} else if (type != SKIP) {
	    panic("Unknown type %d in TkDisplayChars", type);
	}
	startX = curX;
	start = p+1;
    }

    /*
     * At the very end, there may be one last batch of normal characters
     * to display.
     */

    if (p != start) {
	XDrawString(display, drawable, gc, startX, y, start, p - start); 
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkUnderlineChars --
 *
 *	This procedure draws an underline for a given range of characters
 *	in a given string, using appropriate information for the string's
 *	font.  It doesn't draw the characters (which are assumed to have
 *	been displayed previously);  it just draws the underline.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets displayed in "drawable".
 *
 *----------------------------------------------------------------------
 */

void
TkUnderlineChars(display, drawable, gc, fontStructPtr, string, x, y,
	tabOrigin, flags, firstChar, lastChar)
    Display *display;		/* Display on which to draw. */
    Drawable drawable;		/* Window or pixmap in which to draw. */
    GC gc;			/* Graphics context for actually drawing
				 * underline. */
    XFontStruct *fontStructPtr;	/* Font used in GC;  must have been allocated
				 * by Tk_GetFontStruct.  Used to character
				 * dimensions, etc. */
    char *string;		/* String containing characters to be
				 * underlined. */
    int x, y;			/* Coordinates at which first character of
				 * string is drawn. */
    int tabOrigin;		/* X-location that serves as "origin" for
				 * tab stops. */
    int flags;			/* Flags that were passed to TkDisplayChars. */
    int firstChar;		/* Index of first character to underline. */
    int lastChar;		/* Index of last character to underline. */
{
    int xUnder, yUnder, width, height;
    unsigned long value;

    /*
     * First compute the vertical span of the underline, using font
     * properties if they exist.
     */

    if (XGetFontProperty(fontStructPtr, XA_UNDERLINE_POSITION, &value)) {
	yUnder = y + value;
    } else {
	yUnder = y + fontStructPtr->max_bounds.descent/2;
    }
    if (XGetFontProperty(fontStructPtr, XA_UNDERLINE_THICKNESS, &value)) {
	height = value;
    } else {
	height = 2;
    }

    /*
     * Now compute the horizontal span of the underline.
     */

    TkMeasureChars(fontStructPtr, string, firstChar, x, (int) 1000000,
	    tabOrigin, flags, &xUnder);
    TkMeasureChars(fontStructPtr, string+firstChar, lastChar+1-firstChar,
	    xUnder, (int) 1000000, tabOrigin, flags, &width);
    width -= xUnder;

    XFillRectangle(display, drawable, gc, xUnder, yUnder,
	    (unsigned int) width, (unsigned int) height);
}

/*
 *----------------------------------------------------------------------
 *
 * TkComputeTextGeometry --
 *
 *	This procedure computes the amount of screen space needed to
 *	display a multi-line string of text.
 *
 * Results:
 *	There is no return value.  The dimensions of the screen area
 *	needed to display the text are returned in *widthPtr, and *heightPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkComputeTextGeometry(fontStructPtr, string, numChars, wrapLength,
	widthPtr, heightPtr)
    XFontStruct *fontStructPtr;	/* Font that will be used to display text. */
    char *string;		/* String whose dimensions are to be
				 * computed. */
    int numChars;		/* Number of characters to consider from
				 * string. */
    int wrapLength;		/* Longest permissible line length, in
				 * pixels.  <= 0 means no automatic wrapping:
				 * just let lines get as long as needed. */
    int *widthPtr;		/* Store width of string here. */
    int *heightPtr;		/* Store height of string here. */
{
    int thisWidth, maxWidth, numLines;
    char *p;

    if (wrapLength <= 0) {
	wrapLength = INT_MAX;
    }
    maxWidth = 0;
    for (numLines = 1, p = string; (p - string) < numChars; numLines++) {
	p += TkMeasureChars(fontStructPtr, p, numChars - (p - string), 0,
		wrapLength, 0, TK_WHOLE_WORDS|TK_AT_LEAST_ONE, &thisWidth);
	if (thisWidth > maxWidth) {
	    maxWidth = thisWidth;
	}
	if (*p == 0) {
	    break;
	}

	/*
	 * If the character that didn't fit in this line was a white
	 * space character then skip it.
	 */

	if (isspace(UCHAR(*p))) {
	    p++;
	}
    }
    *widthPtr = maxWidth;
    *heightPtr = numLines * (fontStructPtr->ascent + fontStructPtr->descent);
}

/*
 *----------------------------------------------------------------------
 *
 * TkDisplayText --
 *
 *	description.
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
TkDisplayText(display, drawable, fontStructPtr, string, numChars, x, y,
	length, justify, underline, gc)
    Display *display;		/* X display to use for drawing text. */
    Drawable drawable;		/* Window or pixmapin which to draw the
				 * text. */
    XFontStruct *fontStructPtr;	/* Font that determines geometry of text
				 * (should be same as font in gc). */
    char *string;		/* String to display;  may contain embedded
				 * newlines. */
    int numChars;		/* Number of characters to use from string. */
    int x, y;			/* Pixel coordinates within drawable of
				 * upper left corner of display area. */
    int length;			/* Line length in pixels;  used to compute
				 * word wrap points and also for
				 * justification.   Must be > 0. */
    Tk_Justify justify;		/* How to justify lines. */
    int underline;		/* Index of character to underline, or < 0
				 * for no underlining. */
    GC gc;			/* Graphics context to use for drawing text. */
{
    char *p;
    int charsThisLine, lengthThisLine, xThisLine;

    /*
     * Work through the string one line at a time.  Display each line
     * in four steps:
     *     1. Compute the line's length.
     *     2. Figure out where to display the line for justification.
     *     3. Display the line.
     *     4. Underline one character if needed.
     */

    y += fontStructPtr->ascent;
    for (p = string; numChars > 0; ) {
	charsThisLine = TkMeasureChars(fontStructPtr, p, numChars, 0, length,
		0, TK_WHOLE_WORDS|TK_AT_LEAST_ONE, &lengthThisLine);
	if (justify == TK_JUSTIFY_LEFT) {
	    xThisLine = x;
	} else if (justify == TK_JUSTIFY_CENTER) {
	    xThisLine = x + (length - lengthThisLine)/2;
	} else {
	    xThisLine = x + (length - lengthThisLine);
	}
	TkDisplayChars(display, drawable, gc, fontStructPtr, p, charsThisLine,
		xThisLine, y, xThisLine, 0);
	if ((underline >= 0) && (underline < charsThisLine)) {
	    TkUnderlineChars(display, drawable, gc, fontStructPtr, p,
		    xThisLine, y, xThisLine, 0, underline, underline);
	}
	p += charsThisLine;
	numChars -= charsThisLine;
	underline -= charsThisLine;
	y += fontStructPtr->ascent + fontStructPtr->descent;

	/*
	 * If the character that didn't fit was a space character, skip it.
	 */

	if (isspace(UCHAR(*p))) {
	    p++;
	    numChars--;
	    underline--;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MacFontInit --
 *
 *	Initialize the structure used for font management.
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
MacFontInit()
{
    macFontInit = 1;
    Tcl_InitHashTable(&macFontTable, sizeof(FontKey)/sizeof(int));
}

/*
 *----------------------------------------------------------------------
 *
 * ParseFontName --
 *
 *	Parse given font name into family, style & size.
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
ParseFontName(name, family, style, size)
    char *name;
    short *family;
    short *style;
    short *size;
{
    int argc, argc2, i, pointSize;
    char **argv, **argv2;
    Tcl_Interp *dummy;

    /*
     * We define are fant name to be a list of the following form:
     * {family size {style list}}
     */

    *family = 0;
    *style = 0;
    *size = 0;

    dummy = Tcl_CreateInterp();
    if (Tcl_SplitList(dummy, (char *) name, &argc, &argv) != TCL_OK) {
	Tcl_DeleteInterp(dummy);
    	return;
    }
    
    c2pstr(argv[0]);
    GetFNum((StringPtr) argv[0], family);
    
    if (Tcl_GetInt(dummy, argv[1], &pointSize) != TCL_OK) {
	ckfree((char*) argv);
	Tcl_DeleteInterp(dummy);
    	return;
    }
    *size = (short) pointSize;

    if (Tcl_SplitList(dummy, (char *) argv[2], &argc2, &argv2) != TCL_OK) {
	ckfree((char*) argv);
	Tcl_DeleteInterp(dummy);
    	return;
    }
    
    for (i = 0; i < argc2; i++) {
    	if (!strcmp(argv2[i], "normal")) {
    	    *style = normal;
    	} else if (!strcmp(argv2[i], "bold")) {
    	    *style |= bold;
    	} else if (!strcmp(argv2[i], "italic")) {
    	    *style |= italic;
    	} else if (!strcmp(argv2[i], "underline")) {
    	    *style |= underline;
    	} else if (!strcmp(argv2[i], "outline")) {
    	    *style |= outline;
    	} else if (!strcmp(argv2[i], "shadow")) {
    	    *style |= shadow;
    	} else if (!strcmp(argv2[i], "condense")) {
    	    *style |= condense;
    	} else if (!strcmp(argv2[i], "extend")) {
    	    *style |= extend;
    	} else {
    	    /* ignore for now */
    	}
    }
    ckfree((char*) argv);
    ckfree((char*) argv2);
    Tcl_DeleteInterp(dummy);
}

/*
 *----------------------------------------------------------------------
 *
 * XLoadFont --
 *
 *	Get the font handle for the specified font.
 *
 * Results:
 *	Returns the font handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Font
XLoadFont(display, name)
    Display* display;
    _Xconst char* name;
{
    FontKey fontKey, *fontPtr;
    Tcl_HashEntry *fontHashPtr;
    int new, result;
    short family;
    short style;
    short size;
    
    if (!macFontInit) {
	MacFontInit();
    }
    
    result = XNameToFont(name, &family, &style, &size);
    if (result == false) {
	ParseFontName(name, &family, &style, &size);
    }
    
    fontKey.txFont = family;
    fontKey.txFace = style;
    fontKey.txSize = size;
    fontHashPtr = Tcl_CreateHashEntry(&macFontTable, (char *) &fontKey, &new);
    if (!new) {
	return (Font) Tcl_GetHashValue(fontHashPtr);
    }

    fontPtr = (FontKey *) ckalloc(sizeof(FontKey));
    fontPtr->txFont = family;
    fontPtr->txFace = style;
    fontPtr->txSize = size;
    Tcl_SetHashValue(fontHashPtr, fontPtr);
    return (Font) fontPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * XQueryFont --
 *
 *	Retrieve information about the specified font.
 *
 * Results:
 *	Returns a newly allocated XFontStruct.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

XFontStruct *
XQueryFont(display, font_ID)
    Display* display;
    XID font_ID;
{
    XFontStruct *fontPtr;
    XCharStruct bounds;
    FontInfo fi;
    int i;
    FontKey *macFontPtr;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    Rect rect;
    QDErr err;

    rect.top = rect.left = 0;
    rect.bottom = rect.right = 1;
    if (gWorld == NULL) {
	err = NewGWorld(&gWorld, 0, &rect, NULL, NULL, 0);
	if (err != noErr) {
	    panic("NewGWorld failed in XQueryFont");
	}
    }
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(gWorld, NULL);

    fontPtr = (XFontStruct *) ckalloc(sizeof(XFontStruct));

    fontPtr->fid = font_ID;
    macFontPtr = (FontKey *) font_ID;

    TextFont((short) macFontPtr->txFont);
    TextSize((short) macFontPtr->txSize);
    TextFace((short) macFontPtr->txFace);
    GetFontInfo(&fi);

    fontPtr->direction = FontLeftToRight;
    fontPtr->min_byte1 = 0;
    fontPtr->max_byte1 = 0;
    fontPtr->min_char_or_byte2 = 0;	/* TODO */
    fontPtr->max_char_or_byte2 = 256;	/* TODO */
    fontPtr->all_chars_exist = true;
    fontPtr->default_char = 0;	/* TODO */
    fontPtr->n_properties = 0;
    fontPtr->properties = NULL;

    bounds.lbearing = 0;
    bounds.rbearing = fi.widMax;
    bounds.width = fi.widMax;
    bounds.ascent = fi.ascent;
    bounds.descent = fi.descent;
    bounds.attributes = 0;
    fontPtr->min_bounds = bounds;
    fontPtr->max_bounds = bounds;
	
    fontPtr->per_char = (XCharStruct *) ckalloc(sizeof(XCharStruct) * 1000);
    for (i = 0; i < 1000; i++) {
	fontPtr->per_char[i].lbearing = 0;
	fontPtr->per_char[i].rbearing = fi.widMax;
	fontPtr->per_char[i].width = fi.widMax;
	fontPtr->per_char[i].ascent = fi.ascent;
	fontPtr->per_char[i].descent = fi.descent;
    }

    fontPtr->ascent = fi.ascent;
    fontPtr->descent = fi.descent;

    /*
     * Override these fields to hide Mac info
     */
    fontPtr->min_byte1 = macFontPtr->txFont;	/* txFont */
    fontPtr->max_byte1 = macFontPtr->txSize;	/* txSize */
    fontPtr->min_char_or_byte2 = macFontPtr->txFace;	/* txStyle */

    SetGWorld(saveWorld, saveDevice);
    return fontPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * XLoadQueryFont --
 *
 *	Macintosh implementation of the X function XLoadQueryFont.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

XFontStruct * 
XLoadQueryFont(display, name)
    Display* display;
    _Xconst char* name;
{
    Font font;
    font = XLoadFont(display, name);
    return XQueryFont(display, font);
}

/*
 *----------------------------------------------------------------------
 *
 * XTextExtents --
 *
 *	Macintosh implementation of the X function XTextExtents.
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
XTextExtents(font_struct, string, nchars, direction_return,
	font_ascent_return, font_descent_return, overall_return)
    XFontStruct*	font_struct;
    const char*		string;
    int				nchars;
    int*			direction_return;
    int*			font_ascent_return;
    int*			font_descent_return;
    XCharStruct*	overall_return;
{
    short txFont, txFace, txSize;
    short int width;
	
    txFont = qd.thePort->txFont;
    txFace = qd.thePort->txFace;
    txSize = qd.thePort->txSize;
    TextFont((short) font_struct->min_byte1);
    TextSize((short) font_struct->max_byte1);
    TextFace((short) font_struct->min_char_or_byte2);

    *direction_return = font_struct->direction;
    *font_ascent_return = font_struct->ascent;
    *font_descent_return = font_struct->descent;
	
    width = TextWidth(string, (short) 0, (short) strlen(string));
    overall_return->ascent = font_struct->ascent;
    overall_return->descent = font_struct->descent;
    overall_return->width = width;
    overall_return->lbearing = 0;
    overall_return->rbearing = width;

    TextFont(txFont);
    TextSize(txSize);
    TextFace(txFace);
}

int 
XTextWidth(font_struct, string, count)
    XFontStruct* font_struct;
    char* string;
    int	count;
{
    short txFont, txFace, txSize;
    int width;

    txFont = qd.thePort->txFont;
    txFace = qd.thePort->txFace;
    txSize = qd.thePort->txSize;
    TextFont((short) font_struct->min_byte1);
    TextSize((short) font_struct->max_byte1);
    TextFace((short) font_struct->min_char_or_byte2);

    width = TextWidth(string, (short) 0, (short) count);
    TextFont(txFont);
    TextSize(txSize);
    TextFace(txFace);

    return width;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeFont --
 *
 *	Releases resources associated with the specified font.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the memory referenced by font_struct.
 *
 *----------------------------------------------------------------------
 */

void
XFreeFont(display, font_struct)
    Display* display;
    XFontStruct* font_struct;
{
    if (font_struct != NULL) {
	if (font_struct->per_char != NULL) {
	    ckfree((char *) font_struct->per_char);
	}
	ckfree((char *) font_struct);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XGetFontProperty --
 *
 *	Called to get font properties.  Since font properties are not
 *	supported under Windows, this function is a no-op.
 *
 * Results:
 *	Always returns false
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
XGetFontProperty(font_struct, atom, value_return)
    XFontStruct* font_struct;
    Atom atom;
    unsigned long* value_return;
{
    return False;
}

/*
 *--------------------------------------------------------------
 *
 * TkGetCharPositions --
 *
 *	Given a font and a string to print this function will
 *	return an array of shorts that point to the starting
 *	X position of each character in the string.  The array
 *	will always start with zero.
 *
 * Results:
 *	The buffer array will be filled with X indexes.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
TkGetCharPositions(font_struct, string, count, buffer)
    XFontStruct *font_struct;	/* X information about font. */
    char * string;
    int count;
    short *buffer;
{
    short txFont, txFace, txSize;

    txFont = qd.thePort->txFont;
    txFace = qd.thePort->txFace;
    txSize = qd.thePort->txSize;
    TextFont((short) font_struct->min_byte1);
    TextSize((short) font_struct->max_byte1);
    TextFace((short) font_struct->min_char_or_byte2);

    MeasureText((short) count, string, buffer);

    TextFont(txFont);
    TextSize(txSize);
    TextFace(txFace);

    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFontList --
 *
 *	Creates a list of all the available fonts for this machine..
 *
 * Results:
 *	The interp will have the result set to a Tcl list containing
 *	all the available fonts for this machine.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkFontList(interp, display)
    Tcl_Interp *interp;		/* Current interpreter. */
    Display *display;		/* X display to use for drawing text. */
{
    int count, i;
    short id;
    Str255 theName;
    ResType theType;
    Handle handle;
			
    Tcl_ResetResult(interp);
    count = CountResources('FOND');
    for (i = 1; i <= count; i++) {
	handle = GetIndResource('FOND', i);
	if (handle != NULL) {
	    HLock(handle);
	    GetResInfo(handle, &id, &theType, theName);
	    HUnlock(handle);
	    if (theName[0] == 0) {
		continue;
	    }
	    theName[theName[0]+1] = '\0';
	    Tcl_AppendElement(interp, (char *) theName + 1);
	}
    }

    count = CountResources('FONT');
    for (i = 1; i <= count; i++) {
	handle = GetIndResource('FONT', i);
	if (handle != NULL) {
	    HLock(handle);
	    GetResInfo(handle, &id, &theType, theName);
	    HUnlock(handle);
	    GetResInfo(handle, &id, &theType, theName);
	    if (theName[0] == 0) {
		continue;
	    }
	    theName[theName[0]+1] = '\0';
	    Tcl_AppendElement(interp, (char *) theName + 1);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacFontInfo --
 *
 *	Given a font id, return the family, style & size of the font.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Read the code.
 *
 *----------------------------------------------------------------------
 */

void
TkMacFontInfo(font_ID, family, style, size)
    Font font_ID;
    short *family;
    short *style;
    short *size;
{
    FontKey *macFontPtr;

    macFontPtr = (FontKey *) font_ID;
    *family = macFontPtr->txFont;
    *style = macFontPtr->txFace;
    *size = macFontPtr->txSize;
}

/*
 *----------------------------------------------------------------------
 *
 * XNameToFont --
 *
 *	This function constructs a logical font description from an
 *	X font name.  This code only handles font names with all 13
 *	parts, although a part can be '*'.
 *
 * Results:
 *	Returns false if the font name was syntactically invalid,
 *	else true.  Sets the fields of the passed in LOGFONT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
XNameToFont(name, family, style, size)
    _Xconst char *name;
    short *family;
    short *style;
    short *size;
{
    const char *head, *tail;
    const char *field[13];
    int flen[13];
    int i, len;
    char fontName[256];

    /*
     * Valid font name patterns must have a leading '-' or '*'.
     */

    head = tail = name;
    if (*tail == '-') {
	head++; tail++;
    } else if (*tail != '*') {
	return False;
    }

    /*
     * Identify field boundaries.  Stores a pointer to the beginning
     * of each field in field[i], and the length of the field in flen[i].
     * Fields are separated by dashes.  Each '*' becomes a field by itself.
     */

    i = 0;
    while (*tail != '\0' && i < 12) {
	if (*tail == '-') {
	    flen[i] = tail - head;
	    field[i] = head;
	    tail++;
	    head = tail;
	    i++;
	} else if (*tail == '*') {
	    len = tail - head;
	    if (len > 0) {
		flen[i] = tail - head;
		field[i] = head;
	    } else {
		flen[i] = 1;
		field[i] = head;
		tail++;
		if (*tail == '-') {
		    tail++;
		}
	    }
	    head = tail;
	    i++;
	} else {
	    tail++;
	}
    }

    /*
     * We handle the last field as a special case, since it may contain
     * an emedded hyphen.
     */

    flen[i] = strlen(head);
    field[i] = head;

    /*
     * Bail if we don't have all of the fields.
     */

    if (i != 12) {
	return false;
    } 

    /*
     * Now fill in the logical font description from the fields we have
     * identified.
     */

    *family = 0;
    *size = 0;
    *style = 0;
    
    /*
     * Field 1: Foundry.  Skip.
     */

    /*
     * Field 2: Font Family.
     */

    i = 1;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?')))) {
	len = (flen[i] < 256) ? flen[i] : 256 - 1;
	strncpy(fontName, field[i], len);
	fontName[len] = '\0';
	c2pstr(fontName);
	GetFNum((StringPtr) fontName, family);
    }

    /*
     * Field 3: Weight.  Default is medium.
     */

    i = 2;
    if ((flen[i] > 0) && (strncasecmp((char *) field[i], "bold", flen[i]) ==
	    0)) {
    	*style |= bold;
    }
	    
    /*
     * Field 4: Slant.  Default is Roman.
     */
    
    i = 3;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?')))) {
	if (strncasecmp((char *) field[i], "r", flen[i]) == 0) {
	    /* Roman.  Don't do anything */
	} else if (strncasecmp((char *) field[i], "i", flen[i]) == 0) {
	    /* Italic */
	    *style |= italic;
	} else if (strncasecmp((char *) field[i], "o", flen[i]) == 0) {
	    /* Oblique.  Don't do anything */
	} else if (strncasecmp((char *) field[i], "ri", flen[i]) == 0) {
	    /* Reverse Italic.  Don't do anything */
	} else if (strncasecmp((char *) field[i], "ro", flen[i]) == 0) {
	    /* Reverse Oblique.  Don't do anything */
	} else if (strncasecmp((char *) field[i], "ot", flen[i]) == 0) {
	    /* Other */
	} else {
	    return false;
	}
    }

    /*
     * Field 5 & 6: Set Width & Blank.  Skip.
     */

    /*
     * Field 7: Pixels.  Use this as the points if no points set.
     */

    i = 6;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?')))) {
	*size = atoi(field[i]);
    }

    /*
     * Field 8: Points in tenths of a point.
     */

    i = 7;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?')))) {
	*size = atoi(field[i]) / 10;
    }

    /*
     * Field 9: Horizontal Resolution in DPI.  Skip.
     * Field 10: Vertical Resolution in DPI.  Skip.
     */

    /*
     * Field 11: Spacing.
     */

    i = 10;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?'))))
    {
	if (flen[i] != 1) {
	    return false;
	}
	if (field[i][0] == 'p' || field[i][0] == 'P') {
	} else if (field[i][0] == 'm' || field[i][0] == 'm' ||
		   field[i][0] == 'c' || field[i][0] == 'c') {
	} else {
	    return false;
	}
    }

    /*
     * Field 12: Average Width.
     */

    i = 11;
    if (!(flen[i] == 0 ||
	  (flen[i] == 1 && (field[i][0] == '*' || field[i][0] == '?')))) {
    }

    /*
     * Field 13: Character Set.  Skip.
     */

    return true;
}


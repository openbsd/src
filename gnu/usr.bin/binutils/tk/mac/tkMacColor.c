/* 
 * tkMacColor.c --
 *
 *	This file maintains a database of color values for the Tk
 *	toolkit, in order to avoid round-trips to the server to
 *	map color names to pixel values.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacColor.c 1.22 96/02/15 18:55:41
 */

#include "tkPort.h"
#include "tk.h"
#include "tkInt.h"
#include "tkMacInt.h"
#include "xcolors.h"

#include <Quickdraw.h>

/*
 * A two-level data structure is used to manage the color database.
 * The top level consists of one entry for each color name that is
 * currently active, and the bottom level contains one entry for each
 * pixel value that is still in use.  The distinction between
 * levels is necessary because the same pixel may have several
 * different names.  There are two hash tables, one used to index into
 * each of the data structures.  The name hash table is used when
 * allocating colors, and the pixel hash table is used when freeing
 * colors.
 */

/*
 * One of the following data structures is used to keep track of
 * each color that this module has allocated from the X display
 * server.  These entries are indexed by two hash tables defined
 * below:  nameTable and valueTable.
 */

#define COLOR_MAGIC ((unsigned int) 0x46140277)
#define PIXEL_MAGIC ((unsigned char) 0x69)

typedef struct TkColor {
    XColor color;		/* Information about this color. */
    unsigned int magic;		/* Used for quick integrity check on this
				 * structure.   Must always have the
				 * value COLOR_MAGIC. */
    GC gc;			/* Simple gc with this color as foreground
				 * color and all other fields defaulted.
				 * May be None. */
    Screen *screen;		/* Screen where this color is valid.  Used
				 * to delete it. */
    Colormap colormap;		/* Colormap from which this entry was
				 * allocated. */
    Visual *visual;             /* Visual associated with colormap. */
    int refCount;		/* Number of uses of this structure. */
    Tcl_HashTable *tablePtr;	/* Hash table that indexes this structure
				 * (needed when deleting structure). */
    Tcl_HashEntry *hashPtr;	/* Pointer to hash table entry for this
				 * structure. (for use in deleting entry). */
} TkColor;

/*
 * Hash table for name -> TkColor mapping, and key structure used to
 * index into that table:
 */

static Tcl_HashTable nameTable;
typedef struct {
    Tk_Uid name;		/* Name of desired color. */
    Colormap colormap;		/* Colormap from which color will be
				 * allocated. */
    Display *display;		/* Display for colormap. */
} NameKey;

/*
 * Hash table for value -> TkColor mapping, and key structure used to
 * index into that table:
 */

static Tcl_HashTable valueTable;
typedef struct {
    int red, green, blue;	/* Values for desired color. */
    Colormap colormap;		/* Colormap from which color will be
				 * allocated. */
    Display *display;		/* Display for colormap. */
} ValueKey;

static int initialized = 0;	/* 0 means static structures haven't been
				 * initialized yet. */

/*
 * Structures of the following type are used to report "out of color map
 * space" errors.
 */

typedef struct ReportInfo {
    Tcl_Interp *interp;		/* Interpreter in which to report error. */
    Tcl_DString errorInfo;	/* Additional information for errorInfo. */
} ReportInfo;

/*
 * Forward declarations for procedures defined in this file:
 */

static void		ColorInit _ANSI_ARGS_((void));
static int		TkParseColor _ANSI_ARGS_((const char* spec, XColor *color));

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetColor --
 *
 *	Given a string name for a color, map the name to a corresponding
 *	XColor structure.
 *
 * Results:
 *	The return value is a pointer to an XColor structure that
 *	indicates the red, blue, and green intensities for the color
 *	given by "name", and also specifies a pixel value to use to
 *	draw in that color in window "tkwin".  If an error occurs,
 *	then NULL is returned and an error message will be left in
 *	interp->result.
 *
 * Side effects:
 *	The color is added to an internal database with a reference count.
 *	For each call to this procedure, there should eventually be a call
 *	to Tk_FreeColor, so that the database is cleaned up when colors
 *	aren't in use anymore.
 *
 *----------------------------------------------------------------------
 */

XColor *
Tk_GetColor(interp, tkwin, name)
    Tcl_Interp *interp;		/* Place to leave error message if
				 * color can't be found. */
    Tk_Window tkwin;		/* Window in which color will be used. */
    Tk_Uid name;		/* Name of color to allocated (in form
				 * suitable for passing to XParseColor). */
{
    NameKey nameKey;
    Tcl_HashEntry *nameHashPtr;
    int new;
    TkColor *tkColPtr;
    XColor color;
    Display *display = Tk_Display(tkwin);
    long pixel;

    if (!initialized) {
	ColorInit();
    }

    /*
     * First, check to see if there's already a mapping for this color
     * name.
     */

    nameKey.name = name;
    nameKey.colormap = Tk_Colormap(tkwin);
    nameKey.display = display;
    nameHashPtr = Tcl_CreateHashEntry(&nameTable, (char *) &nameKey, &new);
    if (!new) {
	tkColPtr = (TkColor *) Tcl_GetHashValue(nameHashPtr);
	tkColPtr->refCount++;
	return &tkColPtr->color;
    }

    /*
     * The name isn't currently known.  Map from the name to a pixel
     * value.
     */

    if (TkParseColor(name, &color) == 0) {
	if (*name == '#') {
	    Tcl_AppendResult(interp, "invalid color name \"", name,
		    "\"", (char *) NULL);
	} else {
	    Tcl_AppendResult(interp, "unknown color name \"", name,
		    "\"", (char *) NULL);
	}
	Tcl_DeleteHashEntry(nameHashPtr);
	return (XColor *) NULL;
    }

    /*
     * Now create a new TkColor structure and add it to nameTable.
     */

    tkColPtr = (TkColor *) ckalloc(sizeof(TkColor));
    tkColPtr->color = color;
    pixel = PIXEL_MAGIC;
    pixel = (pixel << 8) | (tkColPtr->color.red >> 8);
    pixel = (pixel << 8) | (tkColPtr->color.green >> 8);
    pixel = (pixel << 8) | (tkColPtr->color.blue >> 8);
    tkColPtr->color.pixel = pixel;
    tkColPtr->magic = COLOR_MAGIC;
    tkColPtr->gc = None;
    tkColPtr->screen = Tk_Screen(tkwin);
    tkColPtr->colormap = nameKey.colormap;
    tkColPtr->visual  = Tk_Visual(tkwin);
    tkColPtr->refCount = 1;
    tkColPtr->tablePtr = &nameTable;
    tkColPtr->hashPtr = nameHashPtr;
    Tcl_SetHashValue(nameHashPtr, tkColPtr);

    return &tkColPtr->color;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetColorByValue --
 *
 *	Given a desired set of red-green-blue intensities for a color,
 *	locate a pixel value to use to draw that color in a given
 *	window.
 *
 * Results:
 *	The return value is a pointer to an XColor structure that
 *	indicates the closest red, blue, and green intensities available
 *	to those specified in colorPtr, and also specifies a pixel
 *	value to use to draw in that color in window "tkwin".
 *
 * Side effects:
 *	The color is added to an internal database with a reference count.
 *	For each call to this procedure, there should eventually be a call
 *	to Tk_FreeColor, so that the database is cleaned up when colors
 *	aren't in use anymore.
 *
 *----------------------------------------------------------------------
 */

XColor *
Tk_GetColorByValue(tkwin, colorPtr)
    Tk_Window tkwin;		/* Window in which color will be used. */
    XColor *colorPtr;		/* Red, green, and blue fields indicate
				 * desired color. */
{
    ValueKey valueKey;
    Tcl_HashEntry *valueHashPtr;
    int new;
    TkColor *tkColPtr;
    Display *display = Tk_Display(tkwin);
    long pixel;

    if (!initialized) {
	ColorInit();
    }

    /*
     * First, check to see if there's already a mapping for this color
     * name.
     */

    valueKey.red = colorPtr->red;
    valueKey.green = colorPtr->green;
    valueKey.blue = colorPtr->blue;
    valueKey.colormap = Tk_Colormap(tkwin);
    valueKey.display = display;
    valueHashPtr = Tcl_CreateHashEntry(&valueTable, (char *) &valueKey, &new);
    if (!new) {
	tkColPtr = (TkColor *) Tcl_GetHashValue(valueHashPtr);
	tkColPtr->refCount++;
	return &tkColPtr->color;
    }

    /*
     * The name isn't currently known.  Find a pixel value for this
     * color and add a new structure to valueTable.
     */

    tkColPtr = (TkColor *) ckalloc(sizeof(TkColor));
    tkColPtr->color.red = valueKey.red;
    tkColPtr->color.green = valueKey.green;
    tkColPtr->color.blue = valueKey.blue;
    pixel = PIXEL_MAGIC;
    pixel = (pixel << 8) | (valueKey.red >> 8);
    pixel = (pixel << 8) | (valueKey.green >> 8);
    pixel = (pixel << 8) | (valueKey.blue >> 8);
    tkColPtr->color.pixel = pixel;
    
    tkColPtr->magic = COLOR_MAGIC;
    tkColPtr->gc = None;
    tkColPtr->screen = Tk_Screen(tkwin);
    tkColPtr->colormap = valueKey.colormap;
    tkColPtr->visual  = Tk_Visual(tkwin);
    tkColPtr->refCount = 1;
    tkColPtr->tablePtr = &valueTable;
    tkColPtr->hashPtr = valueHashPtr;
    Tcl_SetHashValue(valueHashPtr, tkColPtr);
    return &tkColPtr->color;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_NameOfColor --
 *
 *	Given a color, return a textual string identifying
 *	the color.
 *
 * Results:
 *	If colorPtr was created by Tk_GetColor, then the return
 *	value is the "string" that was used to create it.
 *	Otherwise the return value is a string that could have
 *	been passed to Tk_GetColor to allocate that color.  The
 *	storage for the returned string is only guaranteed to
 *	persist up until the next call to this procedure.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Tk_NameOfColor(colorPtr)
    XColor *colorPtr;		/* Color whose name is desired. */
{
    register TkColor *tkColPtr = (TkColor *) colorPtr;
    static char string[20];

    if ((tkColPtr->magic == COLOR_MAGIC)
	    && (tkColPtr->tablePtr == &nameTable)) {
	return ((NameKey *) tkColPtr->hashPtr->key.words)->name;
    }
    sprintf(string, "#%4x%4x%4x", colorPtr->red, colorPtr->green,
	    colorPtr->blue);
    return string;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GCForColor --
 *
 *	Given a color allocated from this module, this procedure
 *	returns a GC that can be used for simple drawing with that
 *	color.
 *
 * Results:
 *	The return value is a GC with color set as its foreground
 *	color and all other fields defaulted.  This GC is only valid
 *	as long as the color exists;  it is freed automatically when
 *	the last reference to the color is freed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

GC
Tk_GCForColor(colorPtr, drawable)
    XColor *colorPtr;		/* Color for which a GC is desired. Must
				 * have been allocated by Tk_GetColor or
				 * Tk_GetColorByName. */
    Drawable drawable;		/* Drawable in which the color will be
				 * used (must have same screen and depth
				 * as the one for which the color was
				 * allocated). */
{
    TkColor *tkColPtr = (TkColor *) colorPtr;
    XGCValues gcValues;

    /*
     * Do a quick sanity check to make sure this color was really
     * allocated by Tk_GetColor.
     */

    if (tkColPtr->magic != COLOR_MAGIC) {
	panic("Tk_GCForColor called with bogus color");
    }

    if (tkColPtr->gc == None) {
	gcValues.foreground = tkColPtr->color.pixel;
	tkColPtr->gc = XCreateGC(DisplayOfScreen(tkColPtr->screen),
		drawable, GCForeground, &gcValues);
    }
    return tkColPtr->gc;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_FreeColor --
 *
 *	This procedure is called to release a color allocated by
 *	Tk_GetColor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The reference count associated with colorPtr is deleted, and
 *	the color is released to X if there are no remaining uses
 *	for it.
 *
 *----------------------------------------------------------------------
 */

void
Tk_FreeColor(colorPtr)
    XColor *colorPtr;		/* Color to be released.  Must have been
				 * allocated by Tk_GetColor or
				 * Tk_GetColorByValue. */
{
    register TkColor *tkColPtr = (TkColor *) colorPtr;
    Screen *screen = tkColPtr->screen;

    /*
     * Do a quick sanity check to make sure this color was really
     * allocated by Tk_GetColor.
     */

    if (tkColPtr->magic != COLOR_MAGIC) {
	panic("Tk_FreeColor called with bogus color");
    }

    tkColPtr->refCount--;
    if (tkColPtr->refCount == 0) {
	Tcl_DeleteHashEntry(tkColPtr->hashPtr);
	tkColPtr->magic = 0;
	ckfree((char *) tkColPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ColorInit --
 *
 *	Initialize the structure used for color management.
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
ColorInit()
{
    initialized = 1;
    Tcl_InitHashTable(&nameTable, sizeof(NameKey)/sizeof(int));
    Tcl_InitHashTable(&valueTable, sizeof(ValueKey)/sizeof(int));
}

/*
 *----------------------------------------------------------------------
 *
 * TkCmapStressed --
 *
 *	Check to see whether a given colormap is known to be out
 *	of entries.  The Mac is never stressed - it will dither
 *  colors that do not actually exist.
 *
 * Results:
 *	1 is returned if "colormap" is stressed (i.e. it has run out
 *	of entries recently), 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkCmapStressed(tkwin, colormap)
    Tk_Window tkwin;		/* Window that identifies the display
				 * containing the colormap. */
    Colormap colormap;		/* Colormap to check for stress. */
{
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkParseColor --
 *
 *	Tk implementation of XParseColor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkParseColor(spec, color)
	const char *spec;
	XColor *color;
{
    int	index, size, size2;
    long pixel;
	
    if (spec[0] == '#') {
	char fmt[16];
	int i, red, green, blue;

	if ((i = strlen(spec+1))%3) {
	    return 0;
	}
	i /= 3;

	sprintf(fmt, "%%%dx%%%dx%%%dx", i, i, i);
	sscanf(spec+1, fmt, &red, &green, &blue);
	color->red = ((unsigned short) red) << (4 * (4 - i));
	color->green = ((unsigned short) green) << (4 * (4 - i));
	color->blue = ((unsigned short) blue) << (4 * (4 - i));
	pixel = PIXEL_MAGIC;
	pixel = (pixel << 8) | (color->red >> 8);
	pixel = (pixel << 8) | (color->green >> 8);
	pixel = (pixel << 8) | (color->blue >> 8);
	color->pixel = pixel;
    } else {
	index = 0;
	size = strlen(spec) + 1;
	while (xColors[index].name != NULL) {
	    size2 = strlen(xColors[index].name);
	    if (!strncasecmp(xColors[index].name, (char *) spec, size)) {
		break;
	    }
	    index++;
	}

	if (xColors[index].name == NULL) {
	    return 0;
	}

	color->red = xColors[index].red << 8;
	color->green = xColors[index].green << 8;
	color->blue = xColors[index].blue << 8;
	pixel = PIXEL_MAGIC;
	pixel = (pixel << 8) | (color->red >> 8);
	pixel = (pixel << 8) | (color->green >> 8);
	pixel = (pixel << 8) | (color->blue >> 8);
	color->pixel = pixel;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * XParseColor --
 *
 *	Decodes an X color specification.
 *
 * Results:
 *	Sets exact_def_return to the parsed color.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Status
XParseColor(display, map, spec, color)
    Display *display;
    Colormap map;
    const char* spec;
    XColor *color;
{
    display->request++;
    return TkParseColor(spec, color);
}

/*
 *----------------------------------------------------------------------
 *
 * TkSetMacColor --
 *
 *	Populates a Macintosh RGBColor structure from a X style
 *	pixel value.
 *
 * Results:
 *	Returns false if not a real pixel, true otherwise.
 *
 * Side effects:
 *	The variable macColor is updated to the pixels value.
 *
 *----------------------------------------------------------------------
 */

int
TkSetMacColor(pixel, macColor)
    unsigned long pixel;
    RGBColor *macColor;
{
    if ((pixel >> 24) != PIXEL_MAGIC) {
    	return false;
    }
    
    macColor->blue = (unsigned short) ((pixel & 0xFF) << 8);
    macColor->green = (unsigned short) (((pixel >> 8) & 0xFF) << 8);
    macColor->red = (unsigned short) (((pixel >> 16) & 0xFF) << 8);
    return true;
}

/*
 *----------------------------------------------------------------------
 *
 * Stub functions --
 *
 *	These functions are just stubs for functions that either
 *	don't make sense on the Mac or have yet to be implemented.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	These calls do nothing - which may not be expected.
 *
 *----------------------------------------------------------------------
 */

Status
XAllocColor(display, map, in_out)
    Display *display;
    Colormap map;
    XColor *in_out;
{
    unsigned long pixel;
    
    display->request++;
    pixel = PIXEL_MAGIC;
    pixel = (pixel << 8) | (in_out->red >> 8);
    pixel = (pixel << 8) | (in_out->green >> 8);
    pixel = (pixel << 8) | (in_out->blue >> 8);
    in_out->pixel = pixel;
    return 1;
}

Colormap
XCreateColormap(display, window, visual, alloc)
    Display *display;
    Window window;
    Visual *visual;
    int alloc;
{
    return 0;
}

void
XFreeColormap(display, colormap)
    Display* display;
    Colormap colormap;
{
}

void
XFreeColors(display, colormap, pixels, npixels, planes)
    Display* display;
    Colormap colormap;
    unsigned long* pixels;
    int npixels;
    unsigned long planes;
{
    /*
     * The Macintosh version of Tk uses TrueColor.  Nothing
     * needs to be done to release colors as there really is
     * no colormap in the Tk sense.
     */
}

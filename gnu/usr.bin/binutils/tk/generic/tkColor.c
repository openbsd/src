/* 
 * tkColor.c --
 *
 *	This file maintains a database of color values for the Tk
 *	toolkit, in order to avoid round-trips to the server to
 *	map color names to pixel values.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkColor.c 1.40 96/03/28 09:12:20
 */

#include "tkPort.h"
#include "tk.h"
#include "tkInt.h"

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

typedef struct TkColor {
    XColor color;		/* Information about this color. */
    unsigned int magic;		/* Used for quick integrity check on this
				 * structure.   Must always have the
				 * value COLOR_MAGIC. */
    GC gc;			/* Simple gc with this color as foreground
				 * color and all other fields defaulted.
				 * May be None. */
    Screen *screen;		/* Screen where this color is valid.  Used
				 * to delete it, and to find its display. */
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
 * If a colormap fills up, attempts to allocate new colors from that
 * colormap will fail.  When that happens, we'll just choose the
 * closest color from those that are available in the colormap.
 * One of the following structures will be created for each "stressed"
 * colormap to keep track of the colors that are available in the
 * colormap (otherwise we would have to re-query from the server on
 * each allocation, which would be very slow).  These entries are
 * flushed after a few seconds, since other clients may release or
 * reallocate colors over time.
 */

struct TkStressedCmap {
    Colormap colormap;			/* X's token for the colormap. */
    int numColors;			/* Number of entries currently active
					 * at *colorPtr. */
    XColor *colorPtr;			/* Pointer to malloc'ed array of all
					 * colors that seem to be available in
					 * the colormap.  Some may not actually
					 * be available, e.g. because they are
					 * read-write for another client;  when
					 * we find this out, we remove them
					 * from the array. */
    struct TkStressedCmap *nextPtr;	/* Next in list of all stressed
					 * colormaps for the display. */
};

/*
 * Forward declarations for procedures defined in this file:
 */

static void		ColorInit _ANSI_ARGS_((void));
static void		DeleteStressedCmap _ANSI_ARGS_((Display *display,
			    Colormap colormap));
static void		FindClosestColor _ANSI_ARGS_((Tk_Window tkwin,
			    XColor *desiredColorPtr, XColor *actualColorPtr));

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
 *	draw in that color.  If an error occurs, NULL is returned and
 *	an error message will be left in interp->result.
 *
 * Side effects:
 *	The color is added to an internal database with a reference count.
 *	For each call to this procedure, there should eventually be a call
 *	to Tk_FreeColor so that the database is cleaned up when colors
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
     * value.  Call XAllocNamedColor rather than XParseColor for non-# names:
     * this saves a server round-trip for those names.
     */

    if (*name != '#') {
	XColor screen;

	if (XAllocNamedColor(display, nameKey.colormap, name, &screen,
		&color) != 0) {
	    DeleteStressedCmap(display, nameKey.colormap);
	} else {
	    /*
	     * Couldn't allocate the color.  Try translating the name to
	     * a color value, to see whether the problem is a bad color
	     * name or a full colormap.  If the colormap is full, then
	     * pick an approximation to the desired color.
	     */

	    if (XLookupColor(display, nameKey.colormap, name, &color,
		    &screen) == 0) {
		Tcl_AppendResult(interp, "unknown color name \"",
			name, "\"", (char *) NULL);
		Tcl_DeleteHashEntry(nameHashPtr);
		return (XColor *) NULL;
	    }
	    FindClosestColor(tkwin, &screen, &color);
	}
    } else {
	if (XParseColor(display, nameKey.colormap, name, &color) == 0) {
	    Tcl_AppendResult(interp, "invalid color name \"", name,
		    "\"", (char *) NULL);
	    Tcl_DeleteHashEntry(nameHashPtr);
	    return (XColor *) NULL;
	}
	if (XAllocColor(display, nameKey.colormap, &color) != 0) {
	    DeleteStressedCmap(display, nameKey.colormap);
	} else {
	    FindClosestColor(tkwin, &color, &color);
	}
    }

    /*
     * Now create a new TkColor structure and add it to nameTable.
     */

    tkColPtr = (TkColor *) ckalloc(sizeof(TkColor));
    tkColPtr->color = color;
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
 *	value to use to draw in that color.
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
    Tk_Window tkwin;		/* Window where color will be used. */
    XColor *colorPtr;		/* Red, green, and blue fields indicate
				 * desired color. */
{
    ValueKey valueKey;
    Tcl_HashEntry *valueHashPtr;
    int new;
    TkColor *tkColPtr;
    Display *display = Tk_Display(tkwin);

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
    if (XAllocColor(display, valueKey.colormap, &tkColPtr->color) != 0) {
	DeleteStressedCmap(display, valueKey.colormap);
    } else {
	FindClosestColor(tkwin, &tkColPtr->color, &tkColPtr->color);
    }
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
    sprintf(string, "#%04x%04x%04x", colorPtr->red, colorPtr->green,
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
    Visual *visual;
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

	/*
	 * Careful!  Don't free black or white, since this will
	 * make some servers very unhappy.  Also, there is a bug in
	 * some servers (such Sun's X11/NeWS server) where reference
	 * counting is performed incorrectly, so that if a color is
	 * allocated twice in different places and then freed twice,
	 * the second free generates an error (this bug existed as of
	 * 10/1/92).  To get around this problem, ignore errors that
	 * occur during the free operation.
	 */

	visual = tkColPtr->visual;
	if ((visual->class != StaticGray) && (visual->class != StaticColor)
		&& (tkColPtr->color.pixel != BlackPixelOfScreen(screen))
		&& (tkColPtr->color.pixel != WhitePixelOfScreen(screen))) {
	    Tk_ErrorHandler handler;

	    handler = Tk_CreateErrorHandler(DisplayOfScreen(screen),
		    -1, -1, -1, (Tk_ErrorProc *) NULL, (ClientData) NULL);
	    XFreeColors(DisplayOfScreen(screen), tkColPtr->colormap,
		    &tkColPtr->color.pixel, 1, 0L);
	    Tk_DeleteErrorHandler(handler);
	}
	if (tkColPtr->gc != None) {
	    XFreeGC(DisplayOfScreen(screen), tkColPtr->gc);
	}
	DeleteStressedCmap(DisplayOfScreen(screen), tkColPtr->colormap);
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
 * FindClosestColor --
 *
 *	When Tk can't allocate a color because a colormap has filled
 *	up, this procedure is called to find and allocate the closest
 *	available color in the colormap.
 *
 * Results:
 *	There is no return value, but *actualColorPtr is filled in
 *	with information about the closest available color in tkwin's
 *	colormap.  This color has been allocated via X, so it must
 *	be released by the caller when the caller is done with it.
 *
 * Side effects:
 *	A color is allocated.
 *
 *----------------------------------------------------------------------
 */

static void
FindClosestColor(tkwin, desiredColorPtr, actualColorPtr)
    Tk_Window tkwin;			/* Window where color will be used. */
    XColor *desiredColorPtr;		/* RGB values of color that was
					 * wanted (but unavailable). */
    XColor *actualColorPtr;		/* Structure to fill in with RGB and
					 * pixel for closest available
					 * color. */
{
    TkStressedCmap *stressPtr;
    float tmp, distance, closestDistance;
    int i, closest, numFound;
    XColor *colorPtr;
    TkDisplay *dispPtr = ((TkWindow *) tkwin)->dispPtr;
    Colormap colormap = Tk_Colormap(tkwin);
    XVisualInfo template, *visInfoPtr;

    /*
     * Find the TkStressedCmap structure for this colormap, or create
     * a new one if needed.
     */

    for (stressPtr = dispPtr->stressPtr; ; stressPtr = stressPtr->nextPtr) {
	if (stressPtr == NULL) {
	    stressPtr = (TkStressedCmap *) ckalloc(sizeof(TkStressedCmap));
	    stressPtr->colormap = colormap;
	    template.visualid = XVisualIDFromVisual(Tk_Visual(tkwin));
	    visInfoPtr = XGetVisualInfo(Tk_Display(tkwin),
		    VisualIDMask, &template, &numFound);
	    if (numFound < 1) {
		panic("FindClosestColor couldn't lookup visual");
	    }
	    stressPtr->numColors = visInfoPtr->colormap_size;
	    XFree((char *) visInfoPtr);
	    stressPtr->colorPtr = (XColor *) ckalloc((unsigned)
		    (stressPtr->numColors * sizeof(XColor)));
	    for (i = 0; i  < stressPtr->numColors; i++) {
		stressPtr->colorPtr[i].pixel = (unsigned long) i;
	    }
	    XQueryColors(dispPtr->display, colormap, stressPtr->colorPtr,
		    stressPtr->numColors);
	    stressPtr->nextPtr = dispPtr->stressPtr;
	    dispPtr->stressPtr = stressPtr;
	    break;
	}
	if (stressPtr->colormap == colormap) {
	    break;
	}
    }

    /*
     * Find the color that best approximates the desired one, then
     * try to allocate that color.  If that fails, it must mean that
     * the color was read-write (so we can't use it, since it's owner
     * might change it) or else it was already freed.  Try again,
     * over and over again, until something succeeds.
     */

    while (1)  {
	if (stressPtr->numColors == 0) {
	    panic("FindClosestColor ran out of colors");
	}
	closestDistance = 1e30;
	closest = 0;
	for (colorPtr = stressPtr->colorPtr, i = 0; i < stressPtr->numColors;
		colorPtr++, i++) {
	    /*
	     * Use Euclidean distance in RGB space, weighted by Y (of YIQ)
	     * as the objective function;  this accounts for differences
	     * in the color sensitivity of the eye.
	     */
    
	    tmp = .30*(((int) desiredColorPtr->red) - (int) colorPtr->red);
	    distance = tmp*tmp;
	    tmp = .61*(((int) desiredColorPtr->green) - (int) colorPtr->green);
	    distance += tmp*tmp;
	    tmp = .11*(((int) desiredColorPtr->blue) - (int) colorPtr->blue);
	    distance += tmp*tmp;
	    if (distance < closestDistance) {
		closest = i;
		closestDistance = distance;
	    }
	}
	if (XAllocColor(dispPtr->display, colormap,
		&stressPtr->colorPtr[closest]) != 0) {
	    *actualColorPtr = stressPtr->colorPtr[closest];
	    return;
	}

	/*
	 * Couldn't allocate the color.  Remove it from the table and
	 * go back to look for the next best color.
	 */

	stressPtr->colorPtr[closest] =
		stressPtr->colorPtr[stressPtr->numColors-1];
	stressPtr->numColors -= 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkCmapStressed --
 *
 *	Check to see whether a given colormap is known to be out
 *	of entries.
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
    TkStressedCmap *stressPtr;

    for (stressPtr = ((TkWindow *) tkwin)->dispPtr->stressPtr;
	    stressPtr != NULL; stressPtr = stressPtr->nextPtr) {
	if (stressPtr->colormap == colormap) {
	    return 1;
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteStressedCmap --
 *
 *	This procedure releases the information cached for "colormap"
 *	so that it will be refetched from the X server the next time
 *	it is needed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The TkStressedCmap structure for colormap is deleted;  the
 *	colormap is no longer considered to be "stressed".
 *
 * Note:
 *	This procedure is invoked whenever a color in a colormap is
 *	freed, and whenever a color allocation in a colormap succeeds.
 *	This guarantees that TkStressedCmap structures are always
 *	deleted before the corresponding Colormap is freed.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteStressedCmap(display, colormap)
    Display *display;		/* Xlib's handle for the display
				 * containing the colormap. */
    Colormap colormap;		/* Colormap to flush. */
{
    TkStressedCmap *prevPtr, *stressPtr;
    TkDisplay *dispPtr = TkGetDisplay(display);

    for (prevPtr = NULL, stressPtr = dispPtr->stressPtr; stressPtr != NULL;
	    prevPtr = stressPtr, stressPtr = stressPtr->nextPtr) {
	if (stressPtr->colormap == colormap) {
	    if (prevPtr == NULL) {
		dispPtr->stressPtr = stressPtr->nextPtr;
	    } else {
		prevPtr->nextPtr = stressPtr->nextPtr;
	    }
	    ckfree((char *) stressPtr->colorPtr);
	    ckfree((char *) stressPtr);
	    return;
	}
    }
}

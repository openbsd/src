/* 
 * tkWinColor.c --
 *
 *	Functions to map color names to system color values.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 1994 Software Research Associates, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinColor.c 1.10 96/03/01 18:08:42
 */

#include "tkWinInt.h"
#include "xcolors.h"

/*
 * This variable indicates whether the color table has been initialized.
 */

static int initialized = 0;

/*
 * colorTable is a hash table used to look up X colors by name.
 */

static Tcl_HashTable colorTable;

/*
 * The SystemColorEntries array contains the names and index values for the
 * Windows indirect system color names.
 */

typedef struct {
    char *name;
    int index;
} SystemColorEntry;

static SystemColorEntry sysColorEntries[] = {
    "SystemActiveBorder",		COLOR_ACTIVEBORDER,
    "SystemActiveCaption",		COLOR_ACTIVECAPTION,
    "SystemAppWorkspace",		COLOR_APPWORKSPACE,
    "SystemBackground",			COLOR_BACKGROUND,
    "SystemButtonFace",			COLOR_BTNFACE,
    "SystemButtonHighlight",		COLOR_BTNHIGHLIGHT,
    "SystemButtonShadow",		COLOR_BTNSHADOW,
    "SystemButtonText",			COLOR_BTNTEXT,
    "SystemCaptionText",		COLOR_CAPTIONTEXT,
    "SystemDisabledText",		COLOR_GRAYTEXT,
    "SystemHighlight",			COLOR_HIGHLIGHT,
    "SystemHighlightText",		COLOR_HIGHLIGHTTEXT,
    "SystemInactiveBorder",		COLOR_INACTIVEBORDER,
    "SystemInactiveCaption",		COLOR_INACTIVECAPTION,
    "SystemInactiveCaptionText",	COLOR_INACTIVECAPTIONTEXT,
    "SystemMenu",			COLOR_MENU,
    "SystemMenuText",			COLOR_MENUTEXT,
    "SystemScrollbar",			COLOR_SCROLLBAR,
    "SystemWindow",			COLOR_WINDOW,
    "SystemWindowFrame",		COLOR_WINDOWFRAME,
    "SystemWindowText",			COLOR_WINDOWTEXT,
    NULL,				0
};

/*
 * The sysColors array is initialized by SetSystemColors().
 */

static XColorEntry sysColors[] = {
    0, 0, 0, "SystemActiveBorder",
    0, 0, 0, "SystemActiveCaption",
    0, 0, 0, "SystemAppWorkspace",
    0, 0, 0, "SystemBackground",
    0, 0, 0, "SystemButtonFace",
    0, 0, 0, "SystemButtonHighlight",
    0, 0, 0, "SystemButtonShadow",
    0, 0, 0, "SystemButtonText",
    0, 0, 0, "SystemCaptionText",
    0, 0, 0, "SystemDisabledText",
    0, 0, 0, "SystemHighlight",
    0, 0, 0, "SystemHighlightText",
    0, 0, 0, "SystemInactiveBorder",
    0, 0, 0, "SystemInactiveCaption",
    0, 0, 0, "SystemInactiveCaptionText",
    0, 0, 0, "SystemMenu",
    0, 0, 0, "SystemMenuText",
    0, 0, 0, "SystemScrollbar",
    0, 0, 0, "SystemWindow",
    0, 0, 0, "SystemWindowFrame",
    0, 0, 0, "SystemWindowText",
    0, 0, 0, NULL
};

/*
 * Forward declarations for functions defined later in this file.
 */
static int GetColorByName _ANSI_ARGS_((char *name, XColor *color));
static int GetColorByValue _ANSI_ARGS_((char *value, XColor *color));
static void InitColorTable _ANSI_ARGS_((void));
static void SetSystemColors _ANSI_ARGS_((void));



/*
 *----------------------------------------------------------------------
 *
 * SetSystemColors --
 *
 *	Initializes the sysColors array with the current values for
 *	the system colors.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the RGB values stored in the sysColors array.
 *
 *----------------------------------------------------------------------
 */

static void
SetSystemColors()
{
    SystemColorEntry *sPtr;
    XColorEntry *ePtr;
    COLORREF color;

    for (ePtr = sysColors, sPtr = sysColorEntries;
	 sPtr->name != NULL; ePtr++, sPtr++)
    {
	color = GetSysColor(sPtr->index);
	ePtr->red = GetRValue(color);
	ePtr->green = GetGValue(color);
	ePtr->blue = GetBValue(color);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InitColorTable --
 *
 *	Initialize color name database.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Builds a hash table of color names and RGB values.
 *
 *----------------------------------------------------------------------
 */

static void
InitColorTable()
{
    XColorEntry *colorPtr;
    Tcl_HashEntry *hPtr;
    int dummy;

    Tcl_InitHashTable(&colorTable, TCL_STRING_KEYS);

    /*
     * Add X colors to table.
     */

    for (colorPtr = xColors; colorPtr->name != NULL; colorPtr++) {
        hPtr = Tcl_CreateHashEntry(&colorTable, strlwr(colorPtr->name),
		&dummy);
        Tcl_SetHashValue(hPtr, colorPtr);
    }
    
    /*
     * Add Windows indirect system colors to table.
     */

    SetSystemColors();
    for (colorPtr = sysColors; colorPtr->name != NULL; colorPtr++) {
        hPtr = Tcl_CreateHashEntry(&colorTable, strlwr(colorPtr->name),
		&dummy);
        Tcl_SetHashValue(hPtr, colorPtr);
    }

    initialized = 1;
}

/*
 *----------------------------------------------------------------------
 *
 * GetColorByName --
 *
 *	Looks for a color in the color table by name, then finds the
 *	closest available color in the palette and converts it to an
 *	XColor structure.
 *
 * Results:
 *	If it finds a match, the color is returned in the color
 *	parameter and the return value is 1.  Otherwise the return
 *	value is 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetColorByName(name, color)
    char *name;			/* An X color name, e.g. "red" */
    XColor *color;		/* The closest available color. */
{
    Tcl_HashEntry *hPtr;
    XColorEntry *colorPtr;

    if (!initialized) {
	InitColorTable();
    }

    hPtr = Tcl_FindHashEntry(&colorTable, (char *) strlwr(name));

    if (hPtr == NULL) {
	return 0;
    }

    colorPtr = (XColorEntry *) Tcl_GetHashValue(hPtr);
    color->pixel = PALETTERGB(colorPtr->red, colorPtr->green, colorPtr->blue); 
    color->red = colorPtr->red << 8;
    color->green = colorPtr->green << 8;
    color->blue = colorPtr->blue << 8;
    color->pad = 0;

    return 1;
}      

/*
 *----------------------------------------------------------------------
 *
 * GetColorByValue --
 *
 *	Parses an X RGB color string and finds the closest available
 *	color in the palette and converts it to an XColor structure.
 *	The returned color will have RGB values in the range 0 to 255.
 *
 * Results:
 *	If it finds a match, the color is returned in the color
 *	parameter and the return value is 1.  Otherwise the return
 *	value is 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetColorByValue(value, color)
    char *value;		/* a string of the form "#RGB", "#RRGGBB", */
				/* "#RRRGGGBBB", or "#RRRRGGGGBBBB" */
    XColor *color;		/* The closest available color. */
{
    char fmt[16];
    int i;

    i = strlen(value+1);
    if (i % 3) {
	return 0;
    }
    i /= 3;
    if (i == 0) {
	return 0;
    }
    sprintf(fmt, "%%%dx%%%dx%%%dx", i, i, i);
    sscanf(value+1, fmt, &color->red, &color->green, &color->blue);
    /*
     * Scale the parse values into 8 bits.
     */
    if (i == 1) {
	color->red <<= 4;
	color->green <<= 4;
	color->blue <<= 4;
    } else if (i != 2) {
	color->red >>= (4*(i-2));
	color->green >>= (4*(i-2));
	color->blue >>= (4*(i-2));
    }	
    color->pad = 0;
    color->pixel = PALETTERGB(color->red, color->green, color->blue); 
    color->red = GetRValue(color->pixel) << 8;
    color->green = GetGValue(color->pixel) << 8;
    color->blue = GetBValue(color->pixel) << 8;

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

int
XParseColor(display, colormap, spec, exact_def_return)
    Display* display;
    Colormap colormap;
    _Xconst char* spec;
    XColor* exact_def_return;
{
    /*
     * Note that we are violating the const-ness of spec.  This is
     * probably OK in most cases.  But this is a bug in general.
     */

    if (spec[0] == '#') {
	return GetColorByValue((char *) spec, exact_def_return);
    } else {
	return GetColorByName((char *) spec, exact_def_return);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XAllocColor --
 *
 *	Find the closest available color to the specified XColor.
 *
 * Results:
 *	Updates the color argument and returns 1 on success.  Otherwise
 *	returns 0.
 *
 * Side effects:
 *	Allocates a new color in the palette.
 *
 *----------------------------------------------------------------------
 */

int
XAllocColor(display, colormap, color)
    Display* display;
    Colormap colormap;
    XColor* color;
{
    TkWinColormap *cmap = (TkWinColormap *) colormap;
    PALETTEENTRY entry, closeEntry;
    HDC dc = GetDC(NULL);
    
    entry.peRed = (color->red) >> 8;
    entry.peGreen = (color->green) >> 8;
    entry.peBlue = (color->blue) >> 8;
    entry.peFlags = 0;

    if (GetDeviceCaps(dc, RASTERCAPS) & RC_PALETTE) {
	unsigned long sizePalette = GetDeviceCaps(dc, SIZEPALETTE);
	UINT newPixel, closePixel;
	int new, refCount;
	Tcl_HashEntry *entryPtr;

	/*
	 * Find the nearest existing palette entry.
	 */
	
	newPixel = RGB(entry.peRed, entry.peGreen, entry.peBlue);
	closePixel = GetNearestPaletteIndex(cmap->palette, newPixel);
	GetPaletteEntries(cmap->palette, closePixel, 1, &closeEntry);
	closePixel = RGB(closeEntry.peRed, closeEntry.peGreen,
		closeEntry.peBlue);

	/*
	 * If this is not a duplicate, allocate a new entry.
	 */
	
	if (newPixel != closePixel) {
	    /*
	     * Fails if the palette is full.
	     */

	    if (cmap->size == sizePalette) {
		return 0;
	    }
	
	    cmap->size++;
	    ResizePalette(cmap->palette, cmap->size);
	    SetPaletteEntries(cmap->palette, cmap->size - 1, 1, &entry);
	}
	color->pixel = PALETTERGB(entry.peRed, entry.peGreen, entry.peBlue);
	entryPtr = Tcl_CreateHashEntry(&cmap->refCounts,
		(char *) color->pixel, &new);
	if (new) {
	    refCount = 1;
	} else {
	    refCount = ((int) Tcl_GetHashValue(entryPtr)) + 1;
	}
	Tcl_SetHashValue(entryPtr, (ClientData)refCount);

    } else {
	
	/*
	 * Determine what color will actually be used on non-colormap systems.
	 */
	
	color->pixel = GetNearestColor(dc,
		RGB(entry.peRed, entry.peGreen, entry.peBlue));
	color->red = (GetRValue(color->pixel) << 8);
	color->green = (GetGValue(color->pixel) << 8);
	color->blue = (GetBValue(color->pixel) << 8);
    }

    ReleaseDC(NULL, dc);
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * XAllocNamedColor --
 *
 *	Find the closest color of the given name.
 *
 * Results:
 *	Returns 1 on success with the resulting color in
 *	exact_def_return.  Returns 0 on failure.
 *
 * Side effects:
 *	Allocates a new color in the palette.
 *
 *----------------------------------------------------------------------
 */

int
XAllocNamedColor(display, colormap, color_name, screen_def_return,
	exact_def_return)
    Display* display;
    Colormap colormap;
    _Xconst char* color_name;
    XColor* screen_def_return;
    XColor* exact_def_return;
{
    int rval = GetColorByName((char *) color_name, exact_def_return);
    if (rval) {
	*screen_def_return = *exact_def_return;
	return XAllocColor(display, colormap, exact_def_return);
    } 
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeColors --
 *
 *	Deallocate a block of colors.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes entries for the current palette and compacts the
 *	remaining set.
 *
 *----------------------------------------------------------------------
 */

void
XFreeColors(display, colormap, pixels, npixels, planes)
    Display* display;
    Colormap colormap;
    unsigned long* pixels;
    int npixels;
    unsigned long planes;
{
    TkWinColormap *cmap = (TkWinColormap *) colormap;
    COLORREF cref;
    UINT count, index, refCount;
    int i;
    PALETTEENTRY entry, *entries;
    Tcl_HashEntry *entryPtr;
    HDC dc = GetDC(NULL);

    /*
     * We don't have to do anything for non-palette devices.
     */
    
    if (GetDeviceCaps(dc, RASTERCAPS) & RC_PALETTE) {

	/*
	 * This is really slow for large values of npixels.
	 */

	for (i = 0; i < npixels; i++) {
	    entryPtr = Tcl_FindHashEntry(&cmap->refCounts,
		    (char *) pixels[i]);
	    if (!entryPtr) {
		panic("Tried to free a color that isn't allocated.");
	    }
	    refCount = (int) Tcl_GetHashValue(entryPtr) - 1;
	    if (refCount == 0) {
		cref = pixels[i] & 0x00ffffff;
		index = GetNearestPaletteIndex(cmap->palette, cref);
		GetPaletteEntries(cmap->palette, index, 1, &entry);
		if (cref == RGB(entry.peRed, entry.peGreen, entry.peBlue)) {
		    count = cmap->size - index;
		    entries = (PALETTEENTRY *) ckalloc(sizeof(PALETTEENTRY)
			    * count);
		    GetPaletteEntries(cmap->palette, index+1, count, entries);
		    SetPaletteEntries(cmap->palette, index, count, entries);
		    ckfree((char *) entries);
		    cmap->size--;
		} else {
		    panic("Tried to free a color that isn't allocated.");
		}
		Tcl_DeleteHashEntry(entryPtr);
	    }
	}
    }
    ReleaseDC(NULL, dc);
}

/*
 *----------------------------------------------------------------------
 *
 * XCreateColormap --
 *
 *	Allocate a new colormap.
 *
 * Results:
 *	Returns a newly allocated colormap.
 *
 * Side effects:
 *	Allocates an empty palette and color list.
 *
 *----------------------------------------------------------------------
 */

Colormap
XCreateColormap(display, w, visual, alloc)
    Display* display;
    Window w;
    Visual* visual;
    int alloc;
{
    LOGPALETTE logPalette;
    TkWinColormap *cmap = (TkWinColormap *) ckalloc(sizeof(TkWinColormap));

    logPalette.palVersion = 0x300;
    logPalette.palNumEntries = 1;

    cmap->palette = CreatePalette(&logPalette);
    cmap->size = 0;
    cmap->stale = 0;
    Tcl_InitHashTable(&cmap->refCounts, TCL_ONE_WORD_KEYS);
    return (Colormap)cmap;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeColormap --
 *
 *	Frees the resources associated with the given colormap.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes the palette associated with the colormap.  Note that
 *	the palette must not be selected into a device context when
 *	this occurs.
 *
 *----------------------------------------------------------------------
 */

void
XFreeColormap(display, colormap)
    Display* display;
    Colormap colormap;
{
    TkWinColormap *cmap = (TkWinColormap *) colormap;
    if (!DeleteObject(cmap->palette)) {
	panic("Unable to free colormap, palette is still selected.");
    }
    Tcl_DeleteHashTable(&cmap->refCounts);
    ckfree((char *) cmap);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinSelectPalette --
 *
 *	This function sets up the specified device context with a
 *	given palette.  If the palette is stale, it realizes it in
 *	the background unless the palette is the current global
 *	palette.
 *
 * Results:
 *	Returns the previous palette selected into the device context.
 *
 * Side effects:
 *	May change the system palette.
 *
 *----------------------------------------------------------------------
 */

HPALETTE
TkWinSelectPalette(dc, colormap)
    HDC dc;
    Colormap colormap;
{
    TkWinColormap *cmap = (TkWinColormap *) colormap;
    HPALETTE oldPalette;

    oldPalette = SelectPalette(dc, cmap->palette,
	    (cmap->palette == TkWinGetSystemPalette()) ? FALSE : TRUE);
    RealizePalette(dc);
    return oldPalette;
}

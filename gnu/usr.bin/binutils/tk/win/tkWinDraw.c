/* 
 * tkWinDraw.c --
 *
 *	This file contains the Xlib emulation functions pertaining to
 *	actually drawing objects on a window.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 * Copyright (c) 1994 Software Research Associates, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinDraw.c 1.18 96/03/01 17:43:41
 */

#include "tkWinInt.h"

/*
 * These macros convert between X's bizarre angle units to radians.
 */

#define PI 3.14159265358979
#define XAngleToRadians(a) ((double)(a) / 64 * PI / 180);

/*
 * Translation table between X gc functions and Win32 raster op modes.
 */

static int ropModes[] = {
    R2_BLACK,			/* GXclear */
    R2_MASKPEN,			/* GXand */
    R2_MASKPENNOT,		/* GXandReverse */
    R2_COPYPEN,			/* GXcopy */
    R2_MASKNOTPEN,		/* GXandInverted */
    R2_NOT,			/* GXnoop */
    R2_XORPEN,			/* GXxor */
    R2_MERGEPEN,		/* GXor */
    R2_NOTMERGEPEN,		/* GXnor */
    R2_NOTXORPEN,		/* GXequiv */
    R2_NOT,			/* GXinvert */
    R2_MERGEPENNOT,		/* GXorReverse */
    R2_NOTCOPYPEN,		/* GXcopyInverted */
    R2_MERGENOTPEN,		/* GXorInverted */
    R2_NOTMASKPEN,		/* GXnand */
    R2_WHITE			/* GXset */
};

/*
 * Translation table between X gc functions and Win32 BitBlt op modes.  Some
 * of the operations defined in X don't have names, so we have to construct
 * new opcodes for those functions.  This is arcane and probably not all that
 * useful, but at least it's accurate.
 */

#define NOTSRCAND	(DWORD)0x00220326 /* dest = (NOT source) AND dest */
#define NOTSRCINVERT	(DWORD)0x00990066 /* dest = (NOT source) XOR dest */
#define SRCORREVERSE	(DWORD)0x00DD0228 /* dest = source OR (NOT dest) */
#define SRCNAND		(DWORD)0x007700E6 /* dest = NOT (source AND dest) */

static int bltModes[] = {
    BLACKNESS,			/* GXclear */
    SRCAND,			/* GXand */
    SRCERASE,			/* GXandReverse */
    SRCCOPY,			/* GXcopy */
    NOTSRCAND,			/* GXandInverted */
    PATCOPY,			/* GXnoop */
    SRCINVERT,			/* GXxor */
    SRCPAINT,			/* GXor */
    NOTSRCERASE,		/* GXnor */
    NOTSRCINVERT,		/* GXequiv */
    DSTINVERT,			/* GXinvert */
    SRCORREVERSE,		/* GXorReverse */
    NOTSRCCOPY,			/* GXcopyInverted */
    MERGEPAINT,			/* GXorInverted */
    SRCNAND,			/* GXnand */
    WHITENESS			/* GXset */
};

/*
 * The following raster op uses the source bitmap as a mask for the
 * pattern.  This is used to draw in a foreground color but leave the
 * background color transparent.
 */

#define MASKPAT		0x00E20746 /* dest = (src & pat) | (!src & dst) */

/*
 * The following two raster ops are used to copy the foreground and background
 * bits of a source pattern as defined by a stipple used as the pattern.
 */

#define COPYFG		0x00CA0749 /* dest = (pat & src) | (!pat & dst) */
#define COPYBG		0x00AC0744 /* dest = (!pat & src) | (pat & dst) */

/*
 * Macros used later in the file.
 */

#define MIN(a,b)	((a>b) ? b : a)
#define MAX(a,b)	((a<b) ? b : a)

/*
 * The followng typedef is used to pass Windows GDI drawing functions.
 */

typedef WINGDIAPI BOOL (WINAPI *WinDrawFunc) _ANSI_ARGS_((HDC dc,
			    CONST POINT* points, int npoints));

/*
 * Forward declarations for procedures defined in this file:
 */

static POINT *		ConvertPoints _ANSI_ARGS_((XPoint *points, int npoints,
			    int mode, RECT *bbox));
static void		DrawOrFillArc _ANSI_ARGS_((Display *display,
			    Drawable d, GC gc, int x, int y,
			    unsigned int width, unsigned int height,
			    int angle1, int angle2, int fill));
static void		RenderObject _ANSI_ARGS_((HDC dc, GC gc,
			    XPoint* points, int npoints, int mode, HPEN pen,
			    WinDrawFunc func));

/*
 *----------------------------------------------------------------------
 *
 * TkWinGetDrawableDC --
 *
 *	Retrieve the DC from a drawable.
 *
 * Results:
 *	Returns the window DC for windows.  Returns a new memory DC
 *	for pixmaps.
 *
 * Side effects:
 *	Sets up the palette for the device context, and saves the old
 *	device context state in the passed in TkWinDCState structure.
 *
 *----------------------------------------------------------------------
 */

HDC
TkWinGetDrawableDC(display, d, state)
    Display *display;
    Drawable d;
    TkWinDCState* state;
{
    HDC dc;
    TkWinDrawable *twdPtr = (TkWinDrawable *)d;
    Colormap cmap;

    if (twdPtr->type != TWD_BITMAP) {
	TkWindow *winPtr = twdPtr->window.winPtr;
    
 	dc = GetDC(twdPtr->window.handle);
	if (winPtr == NULL) {
	    cmap = DefaultColormap(display, DefaultScreen(display));
	} else {
	    cmap = winPtr->atts.colormap;
	}
    } else {
	HDC dcMem;
	dc = GetDC(NULL);
	dcMem = CreateCompatibleDC(dc);
	ReleaseDC(NULL, dc);
	SelectObject(dcMem, twdPtr->bitmap.handle);
	dc = dcMem;
	cmap = twdPtr->bitmap.colormap;
    }
    state->palette = TkWinSelectPalette(dc, cmap);
    return dc;
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinReleaseDrawableDC --
 *
 *	Frees the resources associated with a drawable's DC.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Restores the old bitmap handle to the memory DC for pixmaps.
 *
 *----------------------------------------------------------------------
 */

void
TkWinReleaseDrawableDC(d, dc, state)
    Drawable d;
    HDC dc;
    TkWinDCState *state;
{
    TkWinDrawable *twdPtr = (TkWinDrawable *)d;
    SelectPalette(dc, state->palette, TRUE);
    RealizePalette(dc);
    if (twdPtr->type != TWD_BITMAP) {
	ReleaseDC(TkWinGetHWND(d), dc);
    } else {
	DeleteDC(dc);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertPoints --
 *
 *	Convert an array of X points to an array of Win32 points.
 *
 * Results:
 *	Returns the converted array of POINTs.
 *
 * Side effects:
 *	Allocates a block of memory that should not be freed.
 *
 *----------------------------------------------------------------------
 */

static POINT *
ConvertPoints(points, npoints, mode, bbox)
    XPoint *points;
    int npoints;
    int mode;			/* CoordModeOrigin or CoordModePrevious. */
    RECT *bbox;			/* Bounding box of points. */
{
    static POINT *winPoints = NULL; /* Array of points that is reused. */
    static int nWinPoints = -1;	    /* Current size of point array. */
    int i;

    /*
     * To avoid paying the cost of a malloc on every drawing routine,
     * we reuse the last array if it is large enough.
     */

    if (npoints > nWinPoints) {
	if (winPoints != NULL) {
	    ckfree((char *) winPoints);
	}
	winPoints = (POINT *) ckalloc(sizeof(POINT) * npoints);
	if (winPoints == NULL) {
	    nWinPoints = -1;
	    return NULL;
	}
	nWinPoints = npoints;
    }

    bbox->left = bbox->right = points[0].x;
    bbox->top = bbox->bottom = points[0].y;
    
    if (mode == CoordModeOrigin) {
	for (i = 0; i < npoints; i++) {
	    winPoints[i].x = points[i].x;
	    winPoints[i].y = points[i].y;
	    bbox->left = MIN(bbox->left, winPoints[i].x);
	    bbox->right = MAX(bbox->right, winPoints[i].x);
	    bbox->top = MIN(bbox->top, winPoints[i].y);
	    bbox->bottom = MAX(bbox->bottom, winPoints[i].y);
	}
    } else {
	winPoints[0].x = points[0].x;
	winPoints[0].y = points[0].y;
	for (i = 1; i < npoints; i++) {
	    winPoints[i].x = winPoints[i-1].x + points[i].x;
	    winPoints[i].y = winPoints[i-1].y + points[i].y;
	    bbox->left = MIN(bbox->left, winPoints[i].x);
	    bbox->right = MAX(bbox->right, winPoints[i].x);
	    bbox->top = MIN(bbox->top, winPoints[i].y);
	    bbox->bottom = MAX(bbox->bottom, winPoints[i].y);
	}
    }
    return winPoints;
}

/*
 *----------------------------------------------------------------------
 *
 * XCopyArea --
 *
 *	Copies data from one drawable to another using block transfer
 *	routines.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Data is moved from a window or bitmap to a second window or
 *	bitmap.
 *
 *----------------------------------------------------------------------
 */

void
XCopyArea(display, src, dest, gc, src_x, src_y, width, height, dest_x, dest_y)
    Display* display;
    Drawable src;
    Drawable dest;
    GC gc;
    int src_x, src_y;
    unsigned int width, height;
    int dest_x, dest_y;
{
    HDC srcDC, destDC;
    TkWinDCState srcState, destState;

    srcDC = TkWinGetDrawableDC(display, src, &srcState);

    if (src != dest) {
	destDC = TkWinGetDrawableDC(display, dest, &destState);
    } else {
	destDC = srcDC;
    }

    BitBlt(destDC, dest_x, dest_y, width, height, srcDC, src_x, src_y,
	    bltModes[gc->function]);

    if (src != dest) {
	TkWinReleaseDrawableDC(dest, destDC, &destState);
    }
    TkWinReleaseDrawableDC(src, srcDC, &srcState);
}

/*
 *----------------------------------------------------------------------
 *
 * XCopyPlane --
 *
 *	Copies a bitmap from a source drawable to a destination
 *	drawable.  The plane argument specifies which bit plane of
 *	the source contains the bitmap.  Note that this implementation
 *	ignores the gc->function.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the destination drawable.
 *
 *----------------------------------------------------------------------
 */

void
XCopyPlane(display, src, dest, gc, src_x, src_y, width, height, dest_x,
	dest_y, plane)
    Display* display;
    Drawable src;
    Drawable dest;
    GC gc;
    int src_x, src_y;
    unsigned int width, height;
    int dest_x, dest_y;
    unsigned long plane;
{
    HDC srcDC, destDC;
    TkWinDCState srcState, destState;
    HBRUSH bgBrush, fgBrush, oldBrush;

    display->request++;

    if (plane != 1) {
	panic("Unexpected plane specified for XCopyPlane");
    }

    srcDC = TkWinGetDrawableDC(display, src, &srcState);

    if (src != dest) {
	destDC = TkWinGetDrawableDC(display, dest, &destState);
    } else {
	destDC = srcDC;
    }

    if (gc->clip_mask == src) {

	/*
	 * Case 1: transparent bitmaps are handled by setting the
	 * destination to the foreground color whenever the source
	 * pixel is set.
	 */

	fgBrush = CreateSolidBrush(gc->foreground);
	oldBrush = SelectObject(destDC, fgBrush);
	BitBlt(destDC, dest_x, dest_y, width, height, srcDC, src_x, src_y,
		MASKPAT);
	SelectObject(destDC, oldBrush);
	DeleteObject(fgBrush);
    } else if (gc->clip_mask == None) {

	/*
	 * Case 2: opaque bitmaps.  Windows handles the conversion
	 * from one bit to multiple bits by setting 0 to the
	 * foreground color, and 1 to the background color (seems
	 * backwards, but there you are).
	 */

	SetBkMode(destDC, OPAQUE);
	SetBkColor(destDC, gc->foreground);
	SetTextColor(destDC, gc->background);
	BitBlt(destDC, dest_x, dest_y, width, height, srcDC, src_x, src_y,
		SRCCOPY);
    } else {

	/*
	 * Case 3: two arbitrary bitmaps.  Copy the source rectangle
	 * into a color pixmap.  Use the result as a brush when
	 * copying the clip mask into the destination.	 
	 */

	HDC memDC, maskDC;
	HBITMAP bitmap;
	TkWinDCState maskState;

	fgBrush = CreateSolidBrush(gc->foreground);
	bgBrush = CreateSolidBrush(gc->background);
	maskDC = TkWinGetDrawableDC(display, gc->clip_mask, &maskState);
	memDC = CreateCompatibleDC(destDC);
	bitmap = CreateBitmap(width, height, 1, 1, NULL);
	SelectObject(memDC, bitmap);

	/*
	 * Set foreground bits.  We create a new bitmap containing
	 * (source AND mask), then use it to set the foreground color
	 * into the destination.
	 */

	BitBlt(memDC, 0, 0, width, height, srcDC, src_x, src_y, SRCCOPY);
	BitBlt(memDC, 0, 0, width, height, maskDC, dest_x - gc->clip_x_origin,
		dest_y - gc->clip_y_origin, SRCAND);
	oldBrush = SelectObject(destDC, fgBrush);
	BitBlt(destDC, dest_x, dest_y, width, height, memDC, 0, 0, MASKPAT);

	/*
	 * Set background bits.  Same as foreground, except we use
	 * ((NOT source) AND mask) and the background brush.
	 */

	BitBlt(memDC, 0, 0, width, height, srcDC, src_x, src_y, NOTSRCCOPY);
	BitBlt(memDC, 0, 0, width, height, maskDC, dest_x - gc->clip_x_origin,
		dest_y - gc->clip_y_origin, SRCAND);
	SelectObject(destDC, bgBrush);
	BitBlt(destDC, dest_x, dest_y, width, height, memDC, 0, 0, MASKPAT);
	

	TkWinReleaseDrawableDC(gc->clip_mask, maskDC, &maskState);
	SelectObject(destDC, oldBrush);
	DeleteDC(memDC);
	DeleteObject(bitmap);
	DeleteObject(fgBrush);
	DeleteObject(bgBrush);
    }
    if (src != dest) {
	TkWinReleaseDrawableDC(dest, destDC, &destState);
    }
    TkWinReleaseDrawableDC(src, srcDC, &srcState);
}

/*
 *----------------------------------------------------------------------
 *
 * TkPutImage --
 *
 *	Copies a subimage from an in-memory image to a rectangle of
 *	of the specified drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws the image on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
TkPutImage(colors, ncolors, display, d, gc, image, src_x, src_y, dest_x,
	dest_y, width, height)
    unsigned long *colors;		/* Array of pixel values used by this
					 * image.  May be NULL. */
    int ncolors;			/* Number of colors used, or 0. */
    Display* display;
    Drawable d;				/* Destination drawable. */
    GC gc;
    XImage* image;			/* Source image. */
    int src_x, src_y;			/* Offset of subimage. */      
    int dest_x, dest_y;			/* Position of subimage origin in
					 * drawable.  */
    unsigned int width, height;		/* Dimensions of subimage. */
{
    HDC dc, dcMem;
    TkWinDCState state;
    BITMAPINFO *infoPtr;
    HBITMAP bitmap;
    char *data;

    display->request++;

    dc = TkWinGetDrawableDC(display, d, &state);
    SetROP2(dc, ropModes[gc->function]);
    dcMem = CreateCompatibleDC(dc);

    if (image->bits_per_pixel == 1) {
	data = TkAlignImageData(image, sizeof(WORD), MSBFirst);
	bitmap = CreateBitmap(width, height, 1, 1, data);
	SetTextColor(dc, gc->foreground);
	SetBkColor(dc, gc->background);
	ckfree(data);
    } else {    
	int i, usePalette;

	/*
	 * Do not use a palette for TrueColor images.
	 */
	
	usePalette = (image->bits_per_pixel < 24);
	
	if (usePalette) {
	    infoPtr = (BITMAPINFO*) ckalloc(sizeof(BITMAPINFOHEADER)
		    + sizeof(RGBQUAD)*ncolors);
	} else {
	    infoPtr = (BITMAPINFO*) ckalloc(sizeof(BITMAPINFOHEADER));
	}
	
	infoPtr->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	infoPtr->bmiHeader.biWidth = image->width;

	/*
	 * The following code works around a bug in Win32s.  CreateDIBitmap
	 * fails under Win32s for top-down images.  So we have to reverse the
	 * order of the scanlines.  If we are not running under Win32s, we can
	 * just declare the image to be top-down.
	 */

	if ((GetVersion() & 0x80000000)) {
	    int y;
	    char *srcPtr, *dstPtr, *temp;

	    temp = ckalloc((unsigned) image->bytes_per_line);
	    srcPtr = image->data;
	    dstPtr = image->data+(image->bytes_per_line * (image->height - 1));
	    for (y = 0; y < (image->height/2); y++) {
		memcpy(temp, srcPtr, image->bytes_per_line);
		memcpy(srcPtr, dstPtr, image->bytes_per_line);
		memcpy(dstPtr, temp, image->bytes_per_line);
		srcPtr += image->bytes_per_line;
		dstPtr -= image->bytes_per_line;
	    }
	    ckfree(temp);
	    infoPtr->bmiHeader.biHeight = image->height; /* Bottom-up order */
	} else {
	    infoPtr->bmiHeader.biHeight = -image->height; /* Top-down order */
	}
	infoPtr->bmiHeader.biPlanes = 1;
	infoPtr->bmiHeader.biBitCount = image->bits_per_pixel;
	infoPtr->bmiHeader.biCompression = BI_RGB;
	infoPtr->bmiHeader.biSizeImage = 0;
	infoPtr->bmiHeader.biXPelsPerMeter = 0;
	infoPtr->bmiHeader.biYPelsPerMeter = 0;
	infoPtr->bmiHeader.biClrImportant = 0;

	if (usePalette) {
	    infoPtr->bmiHeader.biClrUsed = ncolors;
	    for (i = 0; i < ncolors; i++) {
		infoPtr->bmiColors[i].rgbBlue = GetBValue(colors[i]);
		infoPtr->bmiColors[i].rgbGreen = GetGValue(colors[i]);
		infoPtr->bmiColors[i].rgbRed = GetRValue(colors[i]);
		infoPtr->bmiColors[i].rgbReserved = 0;
	    }
	} else {
	    infoPtr->bmiHeader.biClrUsed = 0;
	}
	bitmap = CreateDIBitmap(dc, &infoPtr->bmiHeader, CBM_INIT,
		image->data, infoPtr, DIB_RGB_COLORS);
	ckfree(infoPtr);
    }
    bitmap = SelectObject(dcMem, bitmap);
    BitBlt(dc, dest_x, dest_y, width, height, dcMem, src_x, src_y, SRCCOPY);
    DeleteObject(SelectObject(dcMem, bitmap));
    DeleteObject(dcMem);
    TkWinReleaseDrawableDC(d, dc, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawString --
 *
 *	Draw a single string in the current font.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Renders the specified string in the drawable.
 *
 *----------------------------------------------------------------------
 */

void
XDrawString(display, d, gc, x, y, string, length)
    Display* display;
    Drawable d;
    GC gc;
    int x;
    int y;
    _Xconst char* string;
    int length;
{
    HDC dc;
    HFONT oldFont;
    TkWinDCState state;

    display->request++;

    if (d == None) {
	return;
    }

    dc = TkWinGetDrawableDC(display, d, &state);
    SetROP2(dc, ropModes[gc->function]);

    if ((gc->fill_style == FillStippled
	    || gc->fill_style == FillOpaqueStippled)
	    && gc->stipple != None) {
	TkWinDrawable *twdPtr = (TkWinDrawable *)gc->stipple;
	HBRUSH oldBrush, stipple;
	HBITMAP oldBitmap, bitmap;
	HDC dcMem;
	TEXTMETRIC tm;
	SIZE size;

	if (twdPtr->type != TWD_BITMAP) {
	    panic("unexpected drawable type in stipple");
	}

	/*
	 * Select stipple pattern into destination dc.
	 */
	
	dcMem = CreateCompatibleDC(dc);

	stipple = CreatePatternBrush(twdPtr->bitmap.handle);
	SetBrushOrgEx(dc, gc->ts_x_origin, gc->ts_y_origin, NULL);
	oldBrush = SelectObject(dc, stipple);

	SetTextAlign(dcMem, TA_LEFT | TA_TOP);
	SetTextColor(dcMem, gc->foreground);
	SetBkMode(dcMem, TRANSPARENT);
	SetBkColor(dcMem, RGB(0, 0, 0));

	if (gc->font != None) {
	    oldFont = SelectObject(dcMem, (HFONT)gc->font);
	}

	/*
	 * Compute the bounding box and create a compatible bitmap.
	 */

	GetTextExtentPoint(dcMem, string, length, &size);
	GetTextMetrics(dcMem, &tm);
	size.cx -= tm.tmOverhang;
	bitmap = CreateCompatibleBitmap(dc, size.cx, size.cy);
	oldBitmap = SelectObject(dcMem, bitmap);

	/*
	 * The following code is tricky because fonts are rendered in multiple
	 * colors.  First we draw onto a black background and copy the white
	 * bits.  Then we draw onto a white background and copy the black bits.
	 * Both the foreground and background bits of the font are ANDed with
	 * the stipple pattern as they are copied.
	 */

	PatBlt(dcMem, 0, 0, size.cx, size.cy, BLACKNESS);
	TextOut(dcMem, 0, 0, string, length);
	BitBlt(dc, x, y - tm.tmAscent, size.cx, size.cy, dcMem,
		0, 0, 0xEA02E9);
	PatBlt(dcMem, 0, 0, size.cx, size.cy, WHITENESS);
	TextOut(dcMem, 0, 0, string, length);
	BitBlt(dc, x, y - tm.tmAscent, size.cx, size.cy, dcMem,
		0, 0, 0x8A0E06);

	/*
	 * Destroy the temporary bitmap and restore the device context.
	 */

	if (gc->font != None) {
	    SelectObject(dcMem, oldFont);
	}
	SelectObject(dcMem, oldBitmap);
	DeleteObject(bitmap);
	DeleteDC(dcMem);
	SelectObject(dc, oldBrush);
	DeleteObject(stipple);
    } else {
	SetTextAlign(dc, TA_LEFT | TA_BASELINE);
	SetTextColor(dc, gc->foreground);
	SetBkMode(dc, TRANSPARENT);
	if (gc->font != None) {
	    oldFont = SelectObject(dc, (HFONT)gc->font);
	}
	TextOut(dc, x, y, string, length);
	if (gc->font != None) {
	    SelectObject(dc, oldFont);
	}
    }
    TkWinReleaseDrawableDC(d, dc, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * XFillRectangles --
 *
 *	Fill multiple rectangular areas in the given drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws onto the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XFillRectangles(display, d, gc, rectangles, nrectangles)
    Display* display;
    Drawable d;
    GC gc;
    XRectangle* rectangles;
    int nrectangles;
{
    HDC dc;
    HBRUSH brush;
    int i;
    RECT rect;
    TkWinDCState state;

    if (d == None) {
	return;
    }

    dc = TkWinGetDrawableDC(display, d, &state);
    SetROP2(dc, ropModes[gc->function]);
    brush = CreateSolidBrush(gc->foreground);

    if ((gc->fill_style == FillStippled
	    || gc->fill_style == FillOpaqueStippled)
	    && gc->stipple != None) {
	TkWinDrawable *twdPtr = (TkWinDrawable *)gc->stipple;
	HBRUSH oldBrush, stipple;
	HBITMAP oldBitmap, bitmap;
	HDC dcMem;
	HBRUSH bgBrush = CreateSolidBrush(gc->background);

	if (twdPtr->type != TWD_BITMAP) {
	    panic("unexpected drawable type in stipple");
	}

	/*
	 * Select stipple pattern into destination dc.
	 */
	
	stipple = CreatePatternBrush(twdPtr->bitmap.handle);
	SetBrushOrgEx(dc, gc->ts_x_origin, gc->ts_y_origin, NULL);
	oldBrush = SelectObject(dc, stipple);
	dcMem = CreateCompatibleDC(dc);

	/*
	 * For each rectangle, create a drawing surface which is the size of
	 * the rectangle and fill it with the background color.  Then merge the
	 * result with the stipple pattern.
	 */

	for (i = 0; i < nrectangles; i++) {
	    bitmap = CreateCompatibleBitmap(dc, rectangles[i].width,
		    rectangles[i].height);
	    oldBitmap = SelectObject(dcMem, bitmap);
	    rect.left = 0;
	    rect.top = 0;
	    rect.right = rectangles[i].width;
	    rect.bottom = rectangles[i].height;
	    FillRect(dcMem, &rect, brush);
	    BitBlt(dc, rectangles[i].x, rectangles[i].y, rectangles[i].width,
		    rectangles[i].height, dcMem, 0, 0, COPYFG);
	    if (gc->fill_style == FillOpaqueStippled) {
		FillRect(dcMem, &rect, bgBrush);
		BitBlt(dc, rectangles[i].x, rectangles[i].y,
			rectangles[i].width, rectangles[i].height, dcMem,
			0, 0, COPYBG);
	    }
	    SelectObject(dcMem, oldBitmap);
	    DeleteObject(bitmap);
	}
	
	DeleteDC(dcMem);
	SelectObject(dc, oldBrush);
	DeleteObject(stipple);
    } else {
	for (i = 0; i < nrectangles; i++) {
	    rect.left = rectangles[i].x;
	    rect.top = rectangles[i].y;
	    rect.right = rect.left + rectangles[i].width;
	    rect.bottom = rect.top + rectangles[i].height;
	    FillRect(dc, &rect, brush);
	}
    }
    DeleteObject(brush);
    TkWinReleaseDrawableDC(d, dc, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * RenderObject --
 *
 *	This function draws a shape using a list of points, a
 *	stipple pattern, and the specified drawing function.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
RenderObject(dc, gc, points, npoints, mode, pen, func)
    HDC dc;
    GC gc;
    XPoint* points;
    int npoints;
    int mode;
    HPEN pen;
    WinDrawFunc func;
{
    RECT rect;
    HPEN oldPen;
    HBRUSH oldBrush;
    POINT *winPoints = ConvertPoints(points, npoints, mode, &rect);
    
    if ((gc->fill_style == FillStippled
	    || gc->fill_style == FillOpaqueStippled)
	    && gc->stipple != None) {

	TkWinDrawable *twdPtr = (TkWinDrawable *)gc->stipple;
	HDC dcMem;
	LONG width, height;
	HBITMAP oldBitmap;
	int i;
	HBRUSH oldMemBrush;
	
	if (twdPtr->type != TWD_BITMAP) {
	    panic("unexpected drawable type in stipple");
	}

    
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

	/*
	 * Select stipple pattern into destination dc.
	 */
	
	oldBrush = SelectObject(dc, CreatePatternBrush(twdPtr->bitmap.handle));
	SetBrushOrgEx(dc, gc->ts_x_origin, gc->ts_y_origin, NULL);

	/*
	 * Create temporary drawing surface containing a copy of the
	 * destination equal in size to the bounding box of the object.
	 */
	
	dcMem = CreateCompatibleDC(dc);
	oldBitmap = SelectObject(dcMem, CreateCompatibleBitmap(dc, width,
		height));
	oldPen = SelectObject(dcMem, pen);
	BitBlt(dcMem, 0, 0, width, height, dc, rect.left, rect.top, SRCCOPY);

	/*
	 * Translate the object to 0,0 for rendering in the temporary drawing
	 * surface. 
	 */

	for (i = 0; i < npoints; i++) {
	    winPoints[i].x -= rect.left;
	    winPoints[i].y -= rect.top;
	}

	/*
	 * Draw the object in the foreground color and copy it to the
	 * destination wherever the pattern is set.
	 */

	SetPolyFillMode(dcMem, (gc->fill_rule == EvenOddRule) ? ALTERNATE
		: WINDING);
	oldMemBrush = SelectObject(dcMem, CreateSolidBrush(gc->foreground));
	(*func)(dcMem, winPoints, npoints);
	BitBlt(dc, rect.left, rect.top, width, height, dcMem, 0, 0, COPYFG);

	/*
	 * If we are rendering an opaque stipple, then draw the polygon in the
	 * background color and copy it to the destination wherever the pattern
	 * is clear.
	 */

	if (gc->fill_style == FillOpaqueStippled) {
	    DeleteObject(SelectObject(dcMem,
		    CreateSolidBrush(gc->background)));
	    (*func)(dcMem, winPoints, npoints);
	    BitBlt(dc, rect.left, rect.top, width, height, dcMem, 0, 0,
		    COPYBG);
	}

	SelectObject(dcMem, oldPen);
	DeleteObject(SelectObject(dcMem, oldMemBrush));
	DeleteObject(SelectObject(dcMem, oldBitmap));
	DeleteDC(dcMem);
    } else {
	oldPen = SelectObject(dc, pen);
	oldBrush = SelectObject(dc, CreateSolidBrush(gc->foreground));
	SetROP2(dc, ropModes[gc->function]);

	SetPolyFillMode(dc, (gc->fill_rule == EvenOddRule) ? ALTERNATE
		: WINDING);

	(*func)(dc, winPoints, npoints);

	SelectObject(dc, oldPen);
    }
    DeleteObject(SelectObject(dc, oldBrush));
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawLines --
 *
 *	Draw connected lines.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Renders a series of connected lines.
 *
 *----------------------------------------------------------------------
 */

void
XDrawLines(display, d, gc, points, npoints, mode)
    Display* display;
    Drawable d;
    GC gc;
    XPoint* points;
    int npoints;
    int mode;
{
    HPEN pen;
    TkWinDCState state;
    HDC dc;
    
    if (d == None) {
	return;
    }

    dc = TkWinGetDrawableDC(display, d, &state);

    pen = CreatePen(PS_SOLID, gc->line_width, gc->foreground);
    RenderObject(dc, gc, points, npoints, mode, pen, Polyline);
    DeleteObject(pen);
    
    TkWinReleaseDrawableDC(d, dc, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * XFillPolygon --
 *
 *	Draws a filled polygon.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws a filled polygon on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XFillPolygon(display, d, gc, points, npoints, shape, mode)
    Display* display;
    Drawable d;
    GC gc;
    XPoint* points;
    int npoints;
    int shape;
    int mode;
{
    HPEN pen;
    TkWinDCState state;
    HDC dc;

    if (d == None) {
	return;
    }

    dc = TkWinGetDrawableDC(display, d, &state);

    pen = GetStockObject(NULL_PEN);
    RenderObject(dc, gc, points, npoints, mode, pen, Polygon);

    TkWinReleaseDrawableDC(d, dc, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawRectangle --
 *
 *	Draws a rectangle.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws a rectangle on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XDrawRectangle(display, d, gc, x, y, width, height)
    Display* display;
    Drawable d;
    GC gc;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
{
    HPEN pen, oldPen;
    TkWinDCState state;
    HBRUSH oldBrush;
    HDC dc;

    if (d == None) {
	return;
    }

    dc = TkWinGetDrawableDC(display, d, &state);

    pen = CreatePen(PS_SOLID, gc->line_width, gc->foreground);
    oldPen = SelectObject(dc, pen);
    oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    SetROP2(dc, ropModes[gc->function]);

    Rectangle(dc, x, y, x+width+1, y+height+1);

    DeleteObject(SelectObject(dc, oldPen));
    SelectObject(dc, oldBrush);
    TkWinReleaseDrawableDC(d, dc, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * XDrawArc --
 *
 *	Draw an arc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws an arc on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XDrawArc(display, d, gc, x, y, width, height, angle1, angle2)
    Display* display;
    Drawable d;
    GC gc;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    int angle1;
    int angle2;
{
    display->request++;

    DrawOrFillArc(display, d, gc, x, y, width, height, angle1, angle2, 0);
}

/*
 *----------------------------------------------------------------------
 *
 * XFillArc --
 *
 *	Draw a filled arc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Draws a filled arc on the specified drawable.
 *
 *----------------------------------------------------------------------
 */

void
XFillArc(display, d, gc, x, y, width, height, angle1, angle2)
    Display* display;
    Drawable d;
    GC gc;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    int angle1;
    int angle2;
{
    display->request++;

    DrawOrFillArc(display, d, gc, x, y, width, height, angle1, angle2, 1);
}

/*
 *----------------------------------------------------------------------
 *
 * DrawOrFillArc --
 *
 *	This procedure handles the rendering of drawn or filled
 *	arcs and chords.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Renders the requested arc.
 *
 *----------------------------------------------------------------------
 */

static void
DrawOrFillArc(display, d, gc, x, y, width, height, angle1, angle2, fill)
    Display *display;
    Drawable d;
    GC gc;
    int x, y;			/* left top */
    unsigned int width, height;
    int angle1;			/* angle1: three-o'clock (deg*64) */
    int angle2;			/* angle2: relative (deg*64) */
    int fill;			/* ==0 draw, !=0 fill */
{
    HDC dc;
    HBRUSH brush, oldBrush;
    HPEN pen, oldPen;
    TkWinDCState state;
    int xr, yr, xstart, ystart, xend, yend;
    double radian_start, radian_end, radian_tmp;

    if (d == None) {
	return;
    }

    dc = TkWinGetDrawableDC(display, d, &state);

    SetROP2(dc, ropModes[gc->function]);

    /*
     * Convert the X arc description to a Win32 arc description.
     */

    xr = (width % 2) ? (width / 2) : ((width - 1) / 2);
    yr = (height % 2) ? (height / 2) : ((height - 1) / 2);

    radian_start = XAngleToRadians(angle1);
    radian_end = XAngleToRadians(angle1+angle2);
    if( angle2 < 0 ) {
	radian_tmp = radian_start;
	radian_start = radian_end;
	radian_end = radian_tmp;
    }

    xstart = x + (int) ((double)xr * (1+cos(radian_start)));
    ystart = y + (int) ((double)yr * (1-sin(radian_start)));
    xend = x + (int) ((double)xr * (1+cos(radian_end)));
    yend = y + (int) ((double)yr * (1-sin(radian_end)));

    /*
     * Now draw a filled or open figure.
     */

    if (!fill) {
	pen = CreatePen(PS_SOLID, gc->line_width, gc->foreground);
	oldPen = SelectObject(dc, pen);
	oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
	Arc(dc, x, y, x + width, y + height, xstart, ystart, xend, yend);
	DeleteObject(SelectObject(dc, oldPen));
	SelectObject(dc, oldBrush);
    } else {
	brush = CreateSolidBrush(gc->foreground);
	oldBrush = SelectObject(dc, brush);
	oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
	if (gc->arc_mode == ArcChord) {
	    Chord(dc, x, y, x + width, y + height, xstart, ystart, xend, yend);
	} else if ( gc->arc_mode == ArcPieSlice ) {
	    Pie(dc, x, y, x + width, y + height, xstart, ystart, xend, yend);
	}
	DeleteObject(SelectObject(dc, oldBrush));
	SelectObject(dc, oldPen);
    }
    TkWinReleaseDrawableDC(d, dc, &state);
}

/*
 *----------------------------------------------------------------------
 *
 * TkScrollWindow --
 *
 *	Scroll a rectangle of the specified window and accumulate
 *	a damage region.
 *
 * Results:
 *	Returns 0 if the scroll genereated no additional damage.
 *	Otherwise, sets the region that needs to be repainted after
 *	scrolling and returns 1.
 *
 * Side effects:
 *	Scrolls the bits in the window.
 *
 *----------------------------------------------------------------------
 */

int
TkScrollWindow(tkwin, gc, x, y, width, height, dx, dy, damageRgn)
    Tk_Window tkwin;		/* The window to be scrolled. */
    GC gc;			/* GC for window to be scrolled. */
    int x, y, width, height;	/* Position rectangle to be scrolled. */
    int dx, dy;			/* Distance rectangle should be moved. */
    TkRegion damageRgn;		/* Region to accumulate damage in. */
{
    HWND hwnd = TkWinGetHWND(Tk_WindowId(tkwin));
    RECT scrollRect;

    scrollRect.left = x;
    scrollRect.top = y;
    scrollRect.right = x + width;
    scrollRect.bottom = y + height;
    return (ScrollWindowEx(hwnd, dx, dy, &scrollRect, NULL, (HRGN) damageRgn,
	    NULL, 0) == NULLREGION) ? 0 : 1;
}

/* 
 * tkMacDraw.c --
 *
 *	This file contains functions that preform drawing to
 *	Xlib windows.  Most of the functions simple emulate
 *	Xlib functions.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacDraw.c 1.34 96/02/15 18:56:05
 */

#include "tkInt.h"
#include "X.h"
#include "Xlib.h"
#include "xbytes.h"
#include <stdio.h>
#include <tcl.h>

#include <Windows.h>
#include <Fonts.h>
#include <QDOffscreen.h>
#include "tkMacInt.h"

#ifndef PI
#    define PI 3.14159265358979323846
#endif

static PixPatHandle gPenPat = NULL;

static void SetUpGraphicsPort _ANSI_ARGS_((GC gc));
static BitMapPtr MakeStippleMap _ANSI_ARGS_((Drawable drawable, Drawable stipple));
/* TODO: the following function may be used outside this file. */
void SetUpClippingRgn _ANSI_ARGS_((Drawable drawable));

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
    int src_x;
    int src_y;
    unsigned int width;
    unsigned int height;
    int dest_x;
    int dest_y;
{
    Rect srcRect, destRect;
    BitMapPtr srcBit, destBit;
    MacDrawable *srcDraw = (MacDrawable *) src;
    MacDrawable *destDraw = (MacDrawable *) dest;
    GWorldPtr srcPort, destPort;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    short tmode;

    destPort = TkMacGetDrawablePort(dest);
    srcPort = TkMacGetDrawablePort(src);

    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);

    SetUpClippingRgn(dest);

    srcBit = &((GrafPtr) srcPort)->portBits;
    destBit = &((GrafPtr) destPort)->portBits;
    SetRect(&srcRect, (short) (srcDraw->xOff + src_x),
	    (short) (srcDraw->yOff + src_y),
	    (short) (srcDraw->xOff + src_x + width),
	    (short) (srcDraw->yOff + src_y + height));	
    SetRect(&destRect, (short) (destDraw->xOff + dest_x),
	    (short) (destDraw->yOff + dest_y), 
	    (short) (destDraw->xOff + dest_x + width),
	    (short) (destDraw->yOff + dest_y + height));	
    tmode = srcCopy;

    CopyBits(srcBit, destBit, &srcRect, &destRect, tmode, NULL);
    SetGWorld(saveWorld, saveDevice);
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
XCopyPlane(display, src, dest, gc, src_x, src_y, 
	width, height, dest_x, dest_y, plane)
    Display* display;
    Drawable src;
    Drawable dest;
    GC gc;
    int src_x;
    int src_y;
    unsigned int width;
    unsigned int height;
    int dest_x;
    int dest_y;
    unsigned long plane;
{
    Rect srcRect, destRect;
    BitMapPtr srcBit, destBit;
    MacDrawable *srcDraw = (MacDrawable *) src;
    MacDrawable *destDraw = (MacDrawable *) dest;
    GWorldPtr srcPort, destPort;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    RGBColor macColor; 
    short tmode;

    destPort = TkMacGetDrawablePort(dest);
    srcPort = TkMacGetDrawablePort(src);
    
    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);

    SetUpClippingRgn(dest);

    srcBit = &((GrafPtr) srcPort)->portBits;
    destBit = &((GrafPtr) destPort)->portBits;
    SetRect(&srcRect, (short) (srcDraw->xOff + src_x),
	    (short) (srcDraw->yOff + src_y),
	    (short) (srcDraw->xOff + src_x + width),
	    (short) (srcDraw->yOff + src_y + height));
    SetRect(&destRect, (short) (destDraw->xOff + dest_x),
	    (short) (destDraw->yOff + dest_y), 
	    (short) (destDraw->xOff + dest_x + width),
	    (short) (destDraw->yOff + dest_y + height));
    tmode = srcOr;

    if (TkSetMacColor(gc->foreground, &macColor) == true) {
	RGBForeColor(&macColor);
    }

    if (TkSetMacColor(gc->background, &macColor) == true) {
	RGBBackColor(&macColor);
	tmode = srcCopy;
    }

    CopyBits(srcBit, destBit, &srcRect, &destRect, tmode, NULL);
    SetGWorld(saveWorld, saveDevice);
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
TkPutImage(colors, ncolors, display, d, gc, image, src_x, src_y, 
		dest_x, dest_y, width, height)
    unsigned long *colors;
    int ncolors;
    Display* display;
    Drawable d;
    GC gc;
    XImage* image;
    int src_x;
    int src_y;
    int dest_x;
    int dest_y;
    unsigned int width;
    unsigned int height;	  
{
    MacDrawable *destDraw = (MacDrawable *) d;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    int i, j;
    BitMap bitmap;
    char *newData = NULL;

    destPort = TkMacGetDrawablePort(d);

    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);

    SetUpClippingRgn(d);

    if (image->depth == 1) {

	/* 
	 * This code assumes a pixel depth of 1 
	 */

	bitmap.bounds.top = bitmap.bounds.left = 0;
	bitmap.bounds.right = (short) width;
	bitmap.bounds.bottom = (short) height;
	if ((image->bytes_per_line % 2) == 1) {
	    char *newPtr, *oldPtr;
	    newData = (char *) ckalloc(height * (image->bytes_per_line + 1));
	    newPtr = newData;
	    oldPtr = image->data;
	    for (i = 0; i < height; i++) {
		for (j = 0; j < image->bytes_per_line; j++) {
		    *newPtr = xBitReverseTable[(unsigned char) *oldPtr];
		    newPtr++, oldPtr++;
		}
	    *newPtr = 0;
	    newPtr++;
	    }
	    bitmap.baseAddr = newData;
	    bitmap.rowBytes = image->bytes_per_line + 1;
	} else {
	    newData = (char *) ckalloc(height * image->bytes_per_line);
	    for (i = 0; i < height * image->bytes_per_line; i++) {
		newData[i] = xBitReverseTable[(unsigned char) image->data[i]];
	    }		
	    bitmap.baseAddr = newData;
	    bitmap.rowBytes = image->bytes_per_line;
	}

	CopyBits(&bitmap, &((GrafPtr) destPort)->portBits, 
		&bitmap.bounds, &bitmap.bounds, srcCopy, NULL);

    } else {
    	/* Color image */
    	PixMap pixmap;
    	
	pixmap.bounds.left = 0;
	pixmap.bounds.top = 0;
	pixmap.bounds.right = (short) width;
	pixmap.bounds.bottom = (short) height;
	pixmap.pixelType = RGBDirect;
	pixmap.pmVersion = 4;	/* 32bit clean */
	pixmap.packType = 0;
	pixmap.packSize = 0;
	pixmap.hRes = 0x00480000;
	pixmap.vRes = 0x00480000;
	pixmap.pixelSize = 32;
	pixmap.cmpCount = 3;
	pixmap.cmpSize = 8;
	pixmap.planeBytes = 0;
	pixmap.pmTable = NULL;
	pixmap.pmReserved = 0;
	pixmap.baseAddr = image->data;
	pixmap.rowBytes = image->bytes_per_line | 0x8000;
	
	CopyBits((BitMap *) &pixmap, &((GrafPtr) destPort)->portBits, 
	    &pixmap.bounds, &pixmap.bounds, srcCopy, NULL);
    }
    
    if (newData != NULL) {
	ckfree(newData);
    }
    SetGWorld(saveWorld, saveDevice);
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
    const char* string;
    int length;
{ 
    MacDrawable *macWin = (MacDrawable *) d;
    CGrafPtr saveWorld;
    GWorldPtr destPort;
    BitMapPtr stippleMap;
    GDHandle saveDevice;
    RGBColor macColor;
    short txFont, txFace, txSize;
    short family, style, size;


    destPort = TkMacGetDrawablePort(d);

    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);

    SetUpClippingRgn(d);

    SetUpGraphicsPort(gc);

    TkMacFontInfo(gc->font, &family, &style, &size);
    txFont = qd.thePort->txFont;
    txFace = qd.thePort->txFace;
    txSize = qd.thePort->txSize;

    if ((gc->fill_style == FillStippled
	    || gc->fill_style == FillOpaqueStippled)
	    && gc->stipple != None) {
	Pixmap pixmap;
	GWorldPtr bufferPort;
	
	stippleMap = MakeStippleMap(d, gc->stipple);

	pixmap = XCreatePixmap(display, d, 	
		stippleMap->bounds.right, stippleMap->bounds.bottom, 0);
		
	bufferPort = TkMacGetDrawablePort(pixmap);
	SetGWorld(bufferPort, NULL);
	
	TextFont(family);
	TextSize(size);
	TextFace(style);
	
	if (TkSetMacColor(gc->foreground, &macColor) == true) {
	    RGBForeColor(&macColor);
	}

	ShowPen();
	MoveTo((short) 0, (short) 0);
	FillRect(&stippleMap->bounds, &qd.white);
	MoveTo((short) x, (short) y);
	DrawText(string, 0, (short) length);

	destPort = TkMacGetDrawablePort(d);
	SetGWorld(destPort, NULL);
	CopyDeepMask(&((GrafPtr) bufferPort)->portBits, stippleMap, 
		&((GrafPtr) destPort)->portBits, &stippleMap->bounds, &stippleMap->bounds,
		&((GrafPtr) destPort)->portRect /* Is this right? */, srcOr, NULL);
	/* TODO: this doesn't work quite right - it does a blend.   you can't draw white
	   text when you have a stipple.
	 */
		
	XFreePixmap(display, pixmap);
	ckfree(stippleMap->baseAddr);
	ckfree((char *)stippleMap);
    } else {
	TextFont(family);
	TextSize(size);
	TextFace(style);
	
	if (TkSetMacColor(gc->foreground, &macColor) == true) {
	    RGBForeColor(&macColor);
	}

	ShowPen();
	MoveTo((short) (macWin->xOff + x), (short) (macWin->yOff + y));
	DrawText(string, 0, (short) length);
    }
    
    TextFont(txFont);
    TextSize(txSize);
    TextFace(txFace);
    SetGWorld(saveWorld, saveDevice);
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
XFillRectangles(display, d, gc, rectangles, n_rectangels)
    Display* display;
    Drawable d;
    GC gc;
    XRectangle *rectangles;
    int n_rectangels;
{
    MacDrawable *macWin = (MacDrawable *) d;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    Rect theRect;
    int i;

    destPort = TkMacGetDrawablePort(d);

    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);

    SetUpClippingRgn(d);

    SetUpGraphicsPort(gc);

    for (i=0; i<n_rectangels; i++) {
	theRect.left = (short) (macWin->xOff + rectangles[i].x);
	theRect.top = (short) (macWin->yOff + rectangles[i].y);
	theRect.right = (short) (theRect.left + rectangles[i].width);
	theRect.bottom = (short) (theRect.top + rectangles[i].height);
	FillCRect(&theRect, gPenPat);
    }

    SetGWorld(saveWorld, saveDevice);
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
    MacDrawable *macWin = (MacDrawable *) d;
    CGrafPtr saveWorld;
    GWorldPtr destPort;
    GDHandle saveDevice;
    int i;

    destPort = TkMacGetDrawablePort(d);

    display->request++;
    if (npoints < 2) {
    	return;  /* TODO: generate BadValue error. */
    }
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    
    SetUpClippingRgn(d);

    SetUpGraphicsPort(gc);

    ShowPen();

    PenPixPat(gPenPat);
    MoveTo((short) (macWin->xOff + points[0].x),
	    (short) (macWin->yOff + points[0].y));
    for (i = 1; i < npoints; i++) {
	if (mode == CoordModeOrigin) {
	    LineTo((short) (macWin->xOff + points[i].x),
		    (short) (macWin->yOff + points[i].y));
	} else {
	    Line((short) (macWin->xOff + points[i].x),
		    (short) (macWin->yOff + points[i].y));
	}
    }

    SetGWorld(saveWorld, saveDevice);
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
    MacDrawable *macWin = (MacDrawable *) d;
    PolyHandle polygon;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    int i;

    destPort = TkMacGetDrawablePort(d);

    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);

    SetUpClippingRgn(d);
    
    SetUpGraphicsPort(gc);

    PenNormal();
    polygon = OpenPoly();

    MoveTo((short) (macWin->xOff + points[0].x),
	    (short) (macWin->yOff + points[0].y));
    for (i = 1; i < npoints; i++) {
	if (mode == CoordModePrevious) {
	    Line((short) (macWin->xOff + points[i].x),
		    (short) (macWin->yOff + points[i].y));
	} else {
	    LineTo((short) (macWin->xOff + points[i].x),
		    (short) (macWin->yOff + points[i].y));
	}
    }

    ClosePoly();

    FillCPoly(polygon, gPenPat);

    KillPoly(polygon);
    SetGWorld(saveWorld, saveDevice);
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
    MacDrawable *macWin = (MacDrawable *) d;
    Rect theRect;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;

    destPort = TkMacGetDrawablePort(d);

    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);

    SetUpClippingRgn(d);

    SetUpGraphicsPort(gc);

    theRect.left = (short) (macWin->xOff + x);
    theRect.top = (short) (macWin->yOff + y);
    theRect.right = (short) (theRect.left + width);
    theRect.bottom = (short) (theRect.top + height);
	
    ShowPen();
    PenPixPat(gPenPat);
    FrameRect(&theRect);

    SetGWorld(saveWorld, saveDevice);
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
    MacDrawable *macWin = (MacDrawable *) d;
    Rect theRect;
    short start, extent;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;

    destPort = TkMacGetDrawablePort(d);

    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);

    SetUpClippingRgn(d);

    SetUpGraphicsPort(gc);

    theRect.left = (short) (macWin->xOff + x);
    theRect.top = (short) (macWin->yOff + y);
    theRect.right = (short) (theRect.left + width);
    theRect.bottom = (short) (theRect.top + height);
    start = (short) (90 - (angle1 / 64));
    extent = (short) (-(angle2 / 64));

    ShowPen();
    PenPixPat(gPenPat);
    FrameArc(&theRect, start, extent);

    SetGWorld(saveWorld, saveDevice);
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
    MacDrawable *macWin = (MacDrawable *) d;
    Rect theRect;
    short start, extent;
    PolyHandle polygon;
    double sin1, cos1, sin2, cos2, angle;
    double boxWidth, boxHeight;
    double vertex[2], center1[2], center2[2];
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;

    destPort = TkMacGetDrawablePort(d);

    display->request++;
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);

    SetUpClippingRgn(d);

    SetUpGraphicsPort(gc);

    theRect.left = (short) (macWin->xOff + x);
    theRect.top = (short) (macWin->yOff + y);
    theRect.right = (short) (theRect.left + width);
    theRect.bottom = (short) (theRect.top + height);
    start = (short) (90 - (angle1 / 64));
    extent = (short) (- (angle2 / 64));

    if (gc->arc_mode == ArcChord) {
    	boxWidth = theRect.right - theRect.left;
    	boxHeight = theRect.bottom - theRect.top;
    	angle = -(angle1/64.0)*PI/180.0;
    	sin1 = sin(angle);
    	cos1 = cos(angle);
    	angle -= (angle2/64.0)*PI/180.0;
    	sin2 = sin(angle);
    	cos2 = cos(angle);
    	vertex[0] = (theRect.left + theRect.right)/2.0;
    	vertex[1] = (theRect.top + theRect.bottom)/2.0;
    	center1[0] = vertex[0] + cos1*boxWidth/2.0;
    	center1[1] = vertex[1] + sin1*boxHeight/2.0;
    	center2[0] = vertex[0] + cos2*boxWidth/2.0;
    	center2[1] = vertex[1] + sin2*boxHeight/2.0;

	polygon = OpenPoly();
	MoveTo((short) ((theRect.left + theRect.right)/2),
		(short) ((theRect.top + theRect.bottom)/2));
	
	LineTo((short) (center1[0] + 0.5), (short) (center1[1] + 0.5));
	LineTo((short) (center2[0] + 0.5), (short) (center2[1] + 0.5));
	ClosePoly();

	ShowPen();
	FillCArc(&theRect, start, extent, gPenPat);
	FillCPoly(polygon, gPenPat);

	KillPoly(polygon);
    } else {
	ShowPen();
	FillCArc(&theRect, start, extent, gPenPat);
    }

    SetGWorld(saveWorld, saveDevice);
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
    MacDrawable *destDraw = (MacDrawable *) Tk_WindowId(tkwin);
    RgnHandle rgn = (RgnHandle) damageRgn;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    RgnHandle rgn2;
    GWorldPtr destPort;

    destPort = TkMacGetDrawablePort(Tk_WindowId(tkwin));

    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);

    SetUpClippingRgn(Tk_WindowId(tkwin));

    /*
     * Old version
     
     SetRect(&srcRect, destDraw->xOff + x, destDraw->yOff + y,
         destDraw->xOff + x + width, destDraw->yOff + y + height);
     SetEmptyRgn(rgn);
     ScrollRect(&srcRect, dx, dy, rgn);
    */

    /*
     * TODO:
     * This method also doesn't work in some cases -
     * However, it does do better than the old version.
     */
       
    XCopyArea(Tk_Display(tkwin), Tk_WindowId(tkwin), Tk_WindowId(tkwin), gc,
	    x, y, width, height, x + dx, y + dy);
    SetRectRgn(rgn, (short) (destDraw->xOff + x),
	    (short) (destDraw->yOff + y),
	    (short) (destDraw->xOff + x + width),
	    (short) (destDraw->yOff + y + height));
    SectRgn(rgn, destPort->visRgn, rgn);
    rgn2 = NewRgn();
    CopyRgn(rgn, rgn2);
    OffsetRgn(rgn2, dx, dy);
    DiffRgn(rgn, rgn2, rgn);
    DisposeRgn(rgn2);
    
    InvalRgn(rgn); 
    SetEmptyRgn(rgn);
    return 0;
    
    SetGWorld(saveWorld, saveDevice);

    if (EmptyRgn(rgn)) {
	return 0;
    } else {
	return 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SetUpGraphicsPort --
 *
 *	Set up the graphics port from the given GC.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The current port is adjusted.
 *
 *----------------------------------------------------------------------
 */

static void
SetUpGraphicsPort(gc)
    GC gc;
{
    RGBColor macColor;

    if (gPenPat == NULL) {
	gPenPat = NewPixPat();
    }
    
    if (TkSetMacColor(gc->foreground, &macColor) == true) {
        /* TODO: cache RGBPats for preformace - measure gains...  */
	MakeRGBPat(gPenPat, &macColor);
    }
    
    PenNormal();
    if (gc->line_width > 1) {
	PenSize(gc->line_width, gc->line_width);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SetUpClippingRgn --
 *
 *	Set up the clipping region so that drawing only occurs on the
 *	specified X subwindow.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The clipping region in the current port is changed.
 *
 *----------------------------------------------------------------------
 */

void
SetUpClippingRgn(drawable)
    Drawable drawable;
{
    MacDrawable *macDraw = (MacDrawable *) drawable;

    if (macDraw->winPtr != NULL) {
	if (macDraw->flags & TK_CLIP_INVALID) {
	    TkMacUpdateClipRgn(macDraw->winPtr);
	}

	if (macDraw->clipRgn != NULL) {
	    SetClip(macDraw->clipRgn);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MakeStippleMap --
 *
 *	Given a drawable and a stipple pattern this function draws the
 *	pattern repeatedly over the drawable.  The drawable can then
 *	be used as a mask for bit-bliting a stipple pattern over an
 *	object.
 *
 * Results:
 *	A BitMap data structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static BitMapPtr
MakeStippleMap(drawable, stipple)
    Drawable drawable;
    Drawable stipple;
{
    MacDrawable *destDraw = (MacDrawable *) drawable;
    GWorldPtr destPort;
    BitMapPtr bitmapPtr;
    int width, height, stippleHeight, stippleWidth;
    int i, j;
    char * data;
    Rect bounds;

    destPort = TkMacGetDrawablePort(drawable);
    width = destPort->portRect.right - destPort->portRect.left;
    height = destPort->portRect.bottom - destPort->portRect.top;
    
    bitmapPtr = (BitMap *) ckalloc(sizeof(BitMap));
    data = (char *) ckalloc(height * ((width / 8) + 1));
    bitmapPtr->bounds.top = bitmapPtr->bounds.left = 0;
    bitmapPtr->bounds.right = (short) width;
    bitmapPtr->bounds.bottom = (short) height;
    bitmapPtr->baseAddr = data;
    bitmapPtr->rowBytes = (width / 8) + 1;

    destPort = TkMacGetDrawablePort(stipple);
    stippleWidth = destPort->portRect.right - destPort->portRect.left;
    stippleHeight = destPort->portRect.bottom - destPort->portRect.top;

    for (i = 0; i < height; i += stippleHeight) {
	for (j = 0; j < width; j += stippleWidth) {
	    bounds.left = j;
	    bounds.top = i;
	    bounds.right = j + stippleWidth;
	    bounds.bottom = i + stippleHeight;
	    
	    CopyBits(&((GrafPtr) destPort)->portBits, bitmapPtr, 
		&((GrafPtr) destPort)->portRect, &bounds, srcCopy, NULL);
	}
    }
    return bitmapPtr;
}

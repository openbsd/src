/* 
 * tkWinRegion.c --
 *
 *	Tk Region emulation code.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinRegion.c 1.6 96/02/15 18:56:06
 */

#include "tkWinInt.h"


/*
 *----------------------------------------------------------------------
 *
 * TkCreateRegion --
 *
 *	Construct an empty region.
 *
 * Results:
 *	Returns a new region handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkRegion
TkCreateRegion()
{
    RECT rect;
    memset(&rect, 0, sizeof(RECT));
    return (TkRegion) CreateRectRgnIndirect(&rect);
}

/*
 *----------------------------------------------------------------------
 *
 * TkDestroyRegion --
 *
 *	Destroy the specified region.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the storage associated with the specified region.
 *
 *----------------------------------------------------------------------
 */

void
TkDestroyRegion(r)
    TkRegion r;
{
    DeleteObject((HRGN) r);
}

/*
 *----------------------------------------------------------------------
 *
 * TkClipBox --
 *
 *	Computes the bounding box of a region.
 *
 * Results:
 *	Sets rect_return to the bounding box of the region.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkClipBox(r, rect_return)
    TkRegion r;
    XRectangle* rect_return;
{
    RECT rect;
    GetRgnBox((HRGN)r, &rect);
    rect_return->x = rect.left;
    rect_return->y = rect.top;
    rect_return->width = rect.right - rect.left;
    rect_return->height = rect.bottom - rect.top;
}

/*
 *----------------------------------------------------------------------
 *
 * TkIntersectRegion --
 *
 *	Compute the intersection of two regions.
 *
 * Results:
 *	Returns the result in the dr_return region.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkIntersectRegion(sra, srb, dr_return)
    TkRegion sra;
    TkRegion srb;
    TkRegion dr_return;
{
    CombineRgn((HRGN) dr_return, (HRGN) sra, (HRGN) srb, RGN_AND);
}

/*
 *----------------------------------------------------------------------
 *
 * TkUnionRectWithRegion --
 *
 *	Create the union of a source region and a rectangle.
 *
 * Results:
 *	Returns the result in the dr_return region.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkUnionRectWithRegion(rectangle, src_region, dest_region_return)
    XRectangle* rectangle;
    TkRegion src_region;
    TkRegion dest_region_return;
{
    HRGN rectRgn = CreateRectRgn(rectangle->x, rectangle->y,
	    rectangle->x + rectangle->width, rectangle->y + rectangle->height);
    CombineRgn((HRGN) dest_region_return, (HRGN) src_region,
	    (HRGN) rectRgn, RGN_OR);
    DeleteObject(rectRgn);
}

/*
 *----------------------------------------------------------------------
 *
 * TkRectInRegion --
 *
 *	Test whether a given rectangle overlaps with a region.
 *
 * Results:
 *	Returns RectanglePart or RectangleOut.  Note that this is
 *	not a complete implementation since it doesn't test for
 *	RectangleIn.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkRectInRegion(r, x, y, width, height)
    TkRegion r;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
{
    RECT rect;
    rect.top = y;
    rect.left = x;
    rect.bottom = y+height;
    rect.right = x+width;
    return RectInRegion((HRGN)r, &rect) ? RectanglePart : RectangleOut;
}

/* 
 * ximage.c --
 *
 *	X bitmap and image routines.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) ximage.c 1.5 96/02/15 18:55:44
 */

#include "tkInt.h"


/*
 *----------------------------------------------------------------------
 *
 * XCreateBitmapFromData --
 *
 *	Construct a single plane pixmap from bitmap data.
 *
 * Results:
 *	Returns a new Pixmap.
 *
 * Side effects:
 *	Allocates a new bitmap and drawable.
 *
 *----------------------------------------------------------------------
 */

Pixmap
XCreateBitmapFromData(display, d, data, width, height)
    Display* display;
    Drawable d;
    _Xconst char* data;
    unsigned int width;
    unsigned int height;
{
    XImage ximage;
    GC gc;
    Pixmap pix;

    pix = XCreatePixmap(display, d, width, height, 1);
    gc = XCreateGC(display, pix, 0, NULL);
    if (gc == NULL) {
	return None;
    }
    ximage.height = height;
    ximage.width = width;
    ximage.depth = 1;
    ximage.bits_per_pixel = 1;
    ximage.xoffset = 0;
    ximage.format = XYBitmap;
    ximage.data = (char *)data;
    ximage.byte_order = LSBFirst;
    ximage.bitmap_unit = 8;
    ximage.bitmap_bit_order = LSBFirst;
    ximage.bitmap_pad = 8;
    ximage.bytes_per_line = (width+7)/8;

    TkPutImage(NULL, 0, display, pix, gc, &ximage, 0, 0, 0, 0, width, height);
    XFreeGC(display, gc);
    return pix;
}

/*
 *----------------------------------------------------------------------
 *
 * XReadBitmapFile --
 *
 *	Loads a bitmap image in X bitmap format into the specified
 *	drawable.
 *
 * Results:
 *	Sets the size, hotspot, and bitmap on success.
 *
 * Side effects:
 *	Creates a new bitmap from the file data.
 *
 *----------------------------------------------------------------------
 */

int
XReadBitmapFile(display, d, filename, width_return, height_return,
	bitmap_return, x_hot_return, y_hot_return) 
    Display* display;
    Drawable d;
    _Xconst char* filename;
    unsigned int* width_return;
    unsigned int* height_return;
    Pixmap* bitmap_return;
    int* x_hot_return;
    int* y_hot_return;
{
    Tcl_Interp *dummy;
    char *data;

    dummy = Tcl_CreateInterp();

    data = TkGetBitmapData(dummy, NULL, (char *) filename,
	    (int *) width_return, (int *) height_return, x_hot_return,
	    y_hot_return);
    if (data == NULL) {
	return BitmapFileInvalid;
    }

    *bitmap_return = XCreateBitmapFromData(display, d, data, *width_return,
	    *height_return);

    Tcl_DeleteInterp(dummy);
    ckfree(data);
    return BitmapSuccess;
}

/* 
 * tkWinImage.c --
 *
 *	This file contains routines for manipulation full-color images.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinImage.c 1.4 96/02/15 18:56:16
 */

#include "tkWinInt.h"

static int		PutPixel _ANSI_ARGS_((XImage *image, int x, int y,
			    unsigned long pixel));

/*
 *----------------------------------------------------------------------
 *
 * PutPixel --
 *
 *	Set a single pixel in an image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
PutPixel(image, x, y, pixel)
    XImage *image;
    int x, y;
    unsigned long pixel;
{
    char *destPtr = &(image->data[(y * image->bytes_per_line)
    	+ (x * (image->bits_per_pixel >> 3))]);
    destPtr[0] = GetBValue(pixel);
    destPtr[1] = GetGValue(pixel);
    destPtr[2] = GetRValue(pixel);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * XCreateImage --
 *
 *	Allocates storage for a new XImage.
 *
 * Results:
 *	Returns a newly allocated XImage.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

XImage *
XCreateImage(display, visual, depth, format, offset, data, width, height,
	bitmap_pad, bytes_per_line)
    Display* display;
    Visual* visual;
    unsigned int depth;
    int format;
    int offset;
    char* data;
    unsigned int width;
    unsigned int height;
    int bitmap_pad;
    int bytes_per_line;
{
    XImage* imagePtr = (XImage *) ckalloc(sizeof(XImage));
    imagePtr->width = width;
    imagePtr->height = height;
    imagePtr->xoffset = offset;
    imagePtr->format = format;
    imagePtr->data = data;
    imagePtr->byte_order = LSBFirst;
    imagePtr->bitmap_unit = 32;
    imagePtr->bitmap_bit_order = LSBFirst;
    imagePtr->bitmap_pad = bitmap_pad;
    imagePtr->depth = depth;

    /*
     * Round to the nearest word boundary.
     */
    
    imagePtr->bytes_per_line = bytes_per_line ? bytes_per_line
 	: ((depth * width + 31) >> 3) & ~3;

    /*
     * If the screen supports TrueColor, then we use 3 bytes per pixel, and
     * we have to install our own pixel routine.
     */
    
    if (visual->class == TrueColor) {
	imagePtr->bits_per_pixel = 24;
	imagePtr->f.put_pixel = PutPixel;
    } else {
	imagePtr->bits_per_pixel = 8;
	imagePtr->f.put_pixel = NULL;
    }
    imagePtr->red_mask = visual->red_mask;
    imagePtr->green_mask = visual->green_mask;
    imagePtr->blue_mask = visual->blue_mask;
    imagePtr->f.create_image = NULL;
    imagePtr->f.destroy_image = NULL;
    imagePtr->f.get_pixel = NULL;
    imagePtr->f.sub_image = NULL;
    imagePtr->f.add_pixel = NULL;
    
    return imagePtr;
}

/* 
 * xgc.c --
 *
 *	This file contains generic routines for manipulating X graphics
 *	contexts. 
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) xgc.c 1.5 96/03/08 11:47:03
 */

#include <stdlib.h>
#include <tk.h>

#ifdef MAC_TCL
#	include <Xlib.h>
#else
#	include <X11/Xlib.h>
#endif


/*
 *----------------------------------------------------------------------
 *
 * XCreateGC --
 *
 *	Allocate a new GC, and initialize the specified fields.
 *
 * Results:
 *	Returns a newly allocated GC. 
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

GC
XCreateGC(display, d, mask, values)
    Display* display;
    Drawable d;
    unsigned long mask;
    XGCValues* values;
{
    GC gp;

    gp = (XGCValues *)ckalloc(sizeof(XGCValues));
    if (!gp) {
	return None;
    }

    gp->function = 	(mask & GCFunction) 	?values->function	:GXcopy;
    gp->plane_mask = 	(mask & GCPlaneMask) 	?values->plane_mask 	:~0;
    gp->foreground = 	(mask & GCForeground) 	?values->foreground 	:0;
    gp->background = 	(mask & GCBackground) 	?values->background 	:0xffffff;
    gp->line_width = 	(mask & GCLineWidth)	?values->line_width	:0;	
    gp->line_style = 	(mask & GCLineStyle)	?values->line_style	:LineSolid;
    gp->cap_style =  	(mask & GCCapStyle)	?values->cap_style	:0;
    gp->join_style = 	(mask & GCJoinStyle)	?values->join_style	:0;
    gp->fill_style =  	(mask & GCFillStyle)	?values->fill_style	:FillSolid;
    gp->fill_rule =  	(mask & GCFillRule)	?values->fill_rule	:WindingRule;
    gp->arc_mode = 	(mask & GCArcMode)	?values->arc_mode	:ArcPieSlice;
    gp->tile = 		(mask & GCTile)		?values->tile		:None;
    gp->stipple = 	(mask & GCStipple)	?values->stipple	:None;
    gp->ts_x_origin = 	(mask & GCTileStipXOrigin)	?values->ts_x_origin:0;
    gp->ts_y_origin = 	(mask & GCTileStipYOrigin)	?values->ts_y_origin:0;
    gp->font = 		(mask & GCFont)		?values->font		:None;
    gp->subwindow_mode = (mask & GCSubwindowMode)?values->subwindow_mode:ClipByChildren;
    gp->graphics_exposures = (mask & GCGraphicsExposures)?values->graphics_exposures:True;
    gp->clip_x_origin = (mask & GCClipXOrigin)	?values->clip_x_origin	:0;
    gp->clip_y_origin = (mask & GCClipYOrigin)	?values->clip_y_origin	:0;
    gp->clip_mask = 	(mask & GCClipMask)	?values->clip_mask	:None;
    gp->dash_offset = 	(mask & GCDashOffset)	?values->dash_offset	:0;
    gp->dashes = 	(mask & GCDashList)	?values->dashes		:4;

    return gp;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreeGC --
 *
 *	Deallocates the specified graphics context.
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
XFreeGC(d, gc)
    Display * d;
    GC gc;
{
    if (gc != None) {
	ckfree((char *) gc);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XSetForeground, etc. --
 *
 *	The following functions are simply accessor functions for
 *	the GC slots.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Each function sets some slot in the GC.
 *
 *----------------------------------------------------------------------
 */

void 
XSetForeground(display, gc, foreground)
    Display *display;
    GC gc;
    unsigned long foreground;
{
    gc->foreground = foreground;
}

void 
XSetBackground(display, gc, background)
    Display *display;
    GC gc;
    unsigned long background;
{
    gc->background = background;
}

void
XSetFunction(display, gc, function)
    Display *display;
    GC gc;
    int function;
{
    gc->function = function;
}

void
XSetFillRule(display, gc, fill_rule)
    Display *display;
    GC gc;
    int fill_rule;
{
    gc->fill_rule = fill_rule;
}

void
XSetFillStyle(display, gc, fill_style)
    Display *display;
    GC gc;
    int fill_style;
{
    gc->fill_style = fill_style;
}

void
XSetTSOrigin(display, gc, x, y)
    Display *display;
    GC gc;
    int x, y;
{
    gc->ts_x_origin = x;
    gc->ts_y_origin = y;
}

void
XSetArcMode(display, gc, arc_mode)
    Display *display;
    GC gc;
    int arc_mode;
{
    gc->arc_mode = arc_mode;
}

void
XSetStipple(display, gc, stipple)
    Display *display;
    GC gc;
    Pixmap stipple;
{
    gc->stipple = stipple;
}

void
XSetLineAttributes(display, gc, line_width, line_style, cap_style,
	join_style)
    Display *display;
    GC gc;
    unsigned int line_width;
    int line_style;
    int cap_style;
    int join_style;
{
    gc->line_style = line_style;
    gc->cap_style = cap_style;
    gc->join_style = join_style;
}

void
XSetClipMask(display, gc, pixmap)
    Display* display;
    GC gc;
    Pixmap pixmap;
{
    gc->clip_mask = pixmap;
}

void
XSetClipOrigin(display, gc, clip_x_origin, clip_y_origin)
    Display* display;
    GC gc;
    int clip_x_origin;
    int clip_y_origin;
{
    gc->clip_x_origin = clip_x_origin;
    gc->clip_y_origin = clip_y_origin;
}

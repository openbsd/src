/* 
 * tkMacXStubs.c --
 *
 *	This file contains most of the X calls called by Tk.  Many of
 * these calls are just stubs and either don't make sense on the
 * Macintosh or thier implamentation just doesn't do anything.  Other
 * calls will eventually be moved into other files.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacXStubs.c 1.68 96/02/15 18:56:03
 */

#include "tkInt.h"
#include <X.h>
#include <Xlib.h>
#include <stdio.h>
#include <tcl.h>

#include "xcolors.h"
#include <Xatom.h>

#include <Windows.h>
#include <Fonts.h>
#include <QDOffscreen.h>
#include "tkMacInt.h"

/*
 * Because this file is still under major development Debugger statements are
 * used through out this file.  The define TCL_DEBUG will decide whether
 * the debugger statements actually call the debugger or not.
 */

#ifndef TCL_DEBUG
#   define Debugger()
#endif
 
#define ROOT_ID 10

/*
 * Declarations of static variables used in this file.
 */

static Display *gMacDisplay = NULL; /* Macintosh display. */
static char *macScreenName = "Macintosh:0";
				/* Default name of macintosh display. */

/*
 * Forward declarations of procedures used in this file.
 */

static XID MacXIdAlloc _ANSI_ARGS_((Display *display));
static int DefaultErrorHandler _ANSI_ARGS_((Display* display,
	XErrorEvent* err_evt));

/*
 * Other declrations
 */

int TkMacXDestroyImage _ANSI_ARGS_((XImage *image));
unsigned long TkMacXGetPixel _ANSI_ARGS_((XImage *image, int x, int y));
int TkMacXPutPixel _ANSI_ARGS_((XImage *image, int x, int y,
	unsigned long pixel));
XImage *TkMacXSubImage _ANSI_ARGS_((XImage *image, int x, int y, 
	unsigned int width, unsigned int height));
int TkMacXAddPixel _ANSI_ARGS_((XImage *image, long value));
int _XInitImageFuncPtrs _ANSI_ARGS_((XImage *image));
void Tk_GetToplevelCoords _ANSI_ARGS_((Tk_Window tkwin, int *xPtr, int *yPtr));


/*
 *----------------------------------------------------------------------
 *
 * XOpenDisplay --
 *
 *	Create the Display structure and fill it with device
 *	specific information.
 *
 * Results:
 *	Returns a Display structure on success or NULL on failure.
 *
 * Side effects:
 *	Allocates a new Display structure.
 *
 *----------------------------------------------------------------------
 */

Display *
XOpenDisplay(display_name)
    _Xconst char *display_name;
{
    Display *display;
    Screen *screen;
    GDHandle graphicsDevice;

    if (gMacDisplay != NULL) {
	if (strcmp(gMacDisplay->display_name, display_name) == 0) {
	    return gMacDisplay;
	} else {
	    return NULL;
	}
    }

    graphicsDevice = GetMainDevice();
    display = (Display *) ckalloc(sizeof(Display));
    display->resource_alloc = MacXIdAlloc;
    screen = (Screen *) ckalloc(sizeof(Screen) * 2);
    display->default_screen = 0;
    display->request = 0;
    display->nscreens = 1;
    display->screens = screen;
    display->display_name = macScreenName;
    display->qlen = 0;
    
    screen->root = ROOT_ID;
    screen->display = display;
    screen->root_depth = 64;	/* Todo: get max depth */
    screen->height = (*graphicsDevice)->gdRect.bottom -
	(*graphicsDevice)->gdRect.top;
    screen->width = (*graphicsDevice)->gdRect.right -
	(*graphicsDevice)->gdRect.left;
    screen->mwidth = screen->width / 4;  /* TODO: determine resolution */
    screen->mheight = screen->height / 4;
    screen->black_pixel = 0x00000000;
    screen->white_pixel = 0x00FFFFFF;
    screen->root_visual = (Visual *) ckalloc(sizeof(Visual));
    screen->root_visual->visualid = 0;
    screen->root_visual->class = TrueColor;
    screen->root_visual->red_mask = 0x00FF0000;
    screen->root_visual->green_mask = 0x0000FF00;
    screen->root_visual->blue_mask = 0x000000FF;
    screen->root_visual->bits_per_rgb = 8;
    screen->root_visual->map_entries = 2 ^ 8;

    gMacDisplay = display;
    return display;
}

/*
 *----------------------------------------------------------------------
 *
 * MacXIdAlloc --
 *
 *	This procedure is invoked by Xlib as the resource allocator
 *	for a display.
 *
 * Results:
 *	The return value is an X resource identifier that isn't currently
 *	in use.
 *
 * Side effects:
 *	The identifier is removed from the stack of free identifiers,
 *	if it was previously on the stack.
 *
 *----------------------------------------------------------------------
 */

static XID
MacXIdAlloc(display)
    Display *display;			/* Display for which to allocate. */
{
	static long int cur_id = 100;
	/*
	 * Some special XIds are reserved
	 *   - this is why we start at 100
	 */

	return ++cur_id;
}

/*
 *----------------------------------------------------------------------
 *
 * DefaultErrorHandler --
 *
 *	This procedure is the default X error handler.  Tk uses it's
 *	own error handler so this call should never be called.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	This function will call panic and exit.
 *
 *----------------------------------------------------------------------
 */

static int
DefaultErrorHandler(display, err_evt)
    Display* display;
    XErrorEvent* err_evt;
{
    /*
     * This call should never be called.  Tk replaces
     * it with its own error handler.
     */
    panic("Warning hit bogus error handler!");
    return 0;
}


char *
XGetAtomName(display, atom)
    Display * display;
    Atom atom;
{
    display->request++;
    return NULL;
}

int
_XInitImageFuncPtrs(XImage *image)
{
    return 0;
}

XErrorHandler
XSetErrorHandler(handler)
    XErrorHandler handler;
{
    return DefaultErrorHandler;
}

Window
XRootWindow(Display *display, int screen_number)
{
    display->request++;
    return ROOT_ID;
}

XImage *
XGetImage(display, d, x, y, width, height, plane_mask, format)
    Display *display;
    Drawable d;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
    unsigned long plane_mask;
    int format;
{
    Debugger();
    return NULL;
}

void
XGetInputFocus(display, focus_return, revert_to_return)
    Display* display;
    Window* focus_return;
    int* revert_to_return;
{
    WindowPtr frontWin;
    Window window;
    Tk_Window tkwin;

    display->request++;
    *focus_return = None;
    *revert_to_return = RevertToNone;
    
    frontWin = FrontWindow();
    if (frontWin != NULL) {
	window = TkMacGetXWindow(frontWin);
	tkwin = Tk_IdToWindow(display, window);
	if (tkwin != NULL) {
	    *focus_return = window;
	}
    }
}

int
XWithdrawWindow(display, window, screen)
    Display *display;
    Window window;
    int screen;
{
    display->request++;
    XUnmapWindow(display, window);
    return 1;
}

int
XIconifyWindow(display, window, screen)
    Display *display;
    Window window;
    int screen;
{
    display->request++;
    XUnmapWindow(display, window);
    return 1;
}

Bool
XTranslateCoordinates(display, src_w, dest_w, src_x, src_y, dest_x_return,
	dest_y_return, child_return)
    Display* display;
    Window src_w;
    Window dest_w;
    int src_x;
    int src_y;
    int* dest_x_return;
    int* dest_y_return;
    Window* child_return;
{
    /* Used in the wm code */
    Debugger();
    return 0;
}

int
XGetGeometry(display, d, root_return, x_return, y_return, width_return,
	height_return, border_width_return, depth_return)
    Display* display;
    Drawable d;
    Window* root_return;
    int* x_return;
    int* y_return;
    unsigned int* width_return;
    unsigned int* height_return;
    unsigned int* border_width_return;
    unsigned int* depth_return;
{
    /* Used in tkCanvPs.c & wm code */
    Debugger();
    return 0;
}

void
XDeleteProperty(display, w, property)
    Display* display;
    Window w;
    Atom property;
{
    Debugger();
}

void
XSetWMClientMachine(disp, w, prop)
    Display *disp;
    Window w;
    XTextProperty *prop;
{
    Debugger();
}

void
XSetCommand(display, w, argv, argc)
    Display* display;
    Window w;
    char** argv;
    int argc;
{
    Debugger();
}

void
XChangeProperty(display, w, property, type, format, mode, data, nelements)
    Display* display;
    Window w;
    Atom property;
    Atom type;
    int format;
    int mode;
    _Xconst unsigned char* data;
    int nelements;
{
    Debugger();
}

void
XSetTransientForHint(display, w, prop_window)
    Display* display;
    Window w;
    Window prop_window;
{
    /*
     * This is ignored for now.  Eventually this should be displayed
     * as a modal dialog window type.  However, we should *really* redefine
     * toplevel to take window types instead of doing all this weird
     * stuff.
     */
}

Status
XStringListToTextProperty(list, count, text_prop_return)
    char** list;
    int count;
    XTextProperty* text_prop_return;
{
    /* Used in wm code */
    Debugger();
    return 0;
}

void
XSelectInput(display, w, event_mask)
    Display* display;
    Window w;
    long event_mask;
{
    Debugger();
}

int
XQueryTree(display, w, root_return, parent_return, children_return,
	nchildren_return)
    Display* display;
    Window w;
    Window* root_return;
    Window* parent_return;
    Window** children_return;
    unsigned int* nchildren_return;
{
    /* Used in wm code */
    Debugger();
    return 0;
}

void
XBell(display, percent)
    Display* display;
    int percent;
{
    SysBeep(percent);
}

void
XSetWMNormalHints(display, w, hints)
    Display* display;
    Window w;
    XSizeHints* hints;
{
    /*
     * Do nothing.  Shouldn't even be called.
     */
}

XSizeHints *
XAllocSizeHints()
{
    /*
     * Always return NULL.  Tk code checks to see if NULL
     * is returned & does nothing if it is.
     */
    
    return NULL;
}

void
XSetInputFocus(display, focus, revert_to, time)
    Display* display;
    Window focus;
    int revert_to;
    Time time;
{
    /*
     * Currently a no-op
     * TODO: implement focus!
     */
    display->request++;
}

int
XSetWMColormapWindows(display, w, colormap_windows, count)
    Display* display;
    Window w;
    Window* colormap_windows;
    int count;
{
    /* used in wm code */
    return 0;
}

Status
XGetWMColormapWindows(display, w, windows_return, count_return)
    Display* display;
    Window w;
    Window** windows_return;
    int* count_return;
{
    /* used in wm code */
    return 0;
}

void
TkSetRegion(display, gc, r)
    Display* display;
    GC gc;
    TkRegion r;
{
    /* Called by photo widget */
}

XImage * 
XCreateImage(display, visual, depth, format, offset, data,
			width, height, bitmap_pad, bytes_per_line)
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
    XImage *ximage;

    display->request++;
    ximage = (XImage *) ckalloc(sizeof(XImage));

    ximage->height = height;
    ximage->width = width;
    ximage->depth = depth;
    ximage->xoffset = offset;
    ximage->format = format;
    ximage->data = data;
    ximage->bitmap_pad = bitmap_pad;
    if (bytes_per_line == 0) {
	ximage->bytes_per_line = width * 4;  /* assuming 32 bits per pixel */
    } else {
	ximage->bytes_per_line = bytes_per_line;
    }

    if (format == ZPixmap) {
	ximage->bits_per_pixel = 32;
	ximage->bitmap_unit = 32;
    } else {
	ximage->bits_per_pixel = 1;
	ximage->bitmap_unit = 8;
    }
    ximage->byte_order = LSBFirst;
    ximage->bitmap_bit_order = LSBFirst;
    ximage->red_mask = 0x00FF0000;
    ximage->green_mask = 0x0000FF00;
    ximage->blue_mask = 0x000000FF;

    ximage->f.destroy_image = TkMacXDestroyImage;
    ximage->f.get_pixel = TkMacXGetPixel;
    ximage->f.put_pixel = TkMacXPutPixel;
    ximage->f.sub_image = TkMacXSubImage;
    ximage->f.add_pixel = TkMacXAddPixel;

    return ximage;
}

GContext
XGContextFromGC(gc)
    GC gc;
{
    /* TODO - currently a no-op */
    return 0;
}

Status
XSendEvent(display, w, propagate, event_mask, event_send)
    Display* display;
    Window w;
    Bool propagate;
    long event_mask;
    XEvent* event_send;
{
    Debugger();
    return 0;
}

int
XGetWindowProperty(display, w, property, long_offset, long_length, delete,
	req_type, actual_type_return, actual_format_return, nitems_return,
	bytes_after_return, prop_return)
    Display *display;
    Window w;
    Atom property;
    long long_offset;
    long long_length;
    Bool delete;
    Atom req_type;
    Atom *actual_type_return;
    int *actual_format_return;
    unsigned long *nitems_return;
    unsigned long *bytes_after_return;
    unsigned char ** prop_return;
{
    display->request++;
    *actual_type_return = None;
    *actual_format_return = *bytes_after_return = 0;
    *nitems_return = 0;
    return 0;
}

void
XRefreshKeyboardMapping()
{
    /* used by tkXEvent.c */
    Debugger();
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetRootCoords --
 *
 *	Given a token for a window, this procedure traces through the
 *	window's lineage to find the (virtual) root-window coordinates
 *	corresponding to point (0,0) in the window.
 *
 * Results:
 *	The locations pointed to by xPtr and yPtr are filled in with
 *	the root coordinates of the (0,0) point in tkwin.  If a virtual
 *	root window is in effect for the window, then the coordinates
 *	in the virtual root are returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tk_GetToplevelCoords(tkwin, xPtr, yPtr)
    Tk_Window tkwin;		/* Token for window. */
    int *xPtr;			/* Where to store x-displacement of (0,0). */
    int *yPtr;			/* Where to store y-displacement of (0,0). */
{
    int x, y;
    register TkWindow *winPtr = (TkWindow *) tkwin;

    /*
     * Search back through this window's parents all the way to a
     * top-level window, combining the offsets of each window within
     * its parent.
     */

    x = y = 0;
    while (1) {
	if (winPtr->flags & TK_TOP_LEVEL) {
	    break;
	}
	x += winPtr->changes.x + winPtr->changes.border_width;
	y += winPtr->changes.y + winPtr->changes.border_width;
	winPtr = winPtr->parentPtr;
    }
    *xPtr = x;
    *yPtr = y;
}

void 
XSetIconName(display, w, icon_name)
    Display* display;
    Window w;
    const char icon_name;
{
    /*
     * This is a no-op, no icon name for Macs.
     */
    display->request++;
}

void 
XDefineCursor(display, w, cursor)
    Display* display;
    Window w;
    Cursor cursor;
{
    /* 
     * This function is just a no-op.  The cursor for a
     * a given window is retrieved from the atts field
     * of the windows winPtr data structure.
     */
    display->request++;
}

void 
XForceScreenSaver(display, mode)
    Display* display;
    int mode;
{
    /* 
     * This function is just a no-op.  It is defined to 
     * reset the screen saver.  However, there is no real
     * way to do this on a Mac.  Let me know if there is!
     */
    display->request++;
}

/*
 *----------------------------------------------------------------------
 *
 * Selection --
 *
 *	Tk uses the selection mechanism for selections and the
 *	clipboard.  The Macintosh will probably need to use a
 *	mechanism.  This issue is being put off until a later
 *	date.
 *
 *----------------------------------------------------------------------
 */

void
XConvertSelection(display, selection, target, property, requestor, time)
    Display* display;
    Atom selection;
    Atom target;
    Atom property;
    Window requestor;
    Time time;
{
    XEvent event;

    event.xselection.type = SelectionNotify;
    event.xselection.serial = display->request;
    event.xselection.send_event = false;
    event.xselection.display = display;
    event.xselection.requestor = requestor;
    event.xselection.selection = selection;
    event.xselection.target = target;
    event.xselection.property = None;	/* No conversion is ever done. */
    event.xselection.time = GenerateTime();

    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
}

void
XSetSelectionOwner(display, selection, owner, time)
    Display* display;
    Atom selection;
    Window owner;
    Time time;
{
    /*
     * Do nothing.
     */
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetServerInfo --
 *
 *	Given a window, this procedure returns information about
 *	the window server for that window.  This procedure provides
 *	the guts of the "winfo server" command.
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
TkGetServerInfo(interp, tkwin)
    Tcl_Interp *interp;		/* The server information is returned in
				 * this interpreter's result. */
    Tk_Window tkwin;		/* Token for window;  this selects a
				 * particular display and server. */
{
    char buffer[50], buffer2[50];

    sprintf(buffer, "X%dR%d ", ProtocolVersion(Tk_Display(tkwin)),
	    ProtocolRevision(Tk_Display(tkwin)));
    sprintf(buffer2, " %d", VendorRelease(Tk_Display(tkwin)));
    Tcl_AppendResult(interp, buffer, ServerVendor(Tk_Display(tkwin)),
	    buffer2, (char *) NULL);
}
/*
 * Image stuff 
 */

int 
TkMacXDestroyImage(image)
    XImage *image;
{
    Debugger();
    return 0;
}

unsigned long 
TkMacXGetPixel(image, x, y)
    XImage *image;
    int x; 
    int y;
{
    Debugger();
    return 0;
}

int 
TkMacXPutPixel(image, x, y, pixel)
    XImage *image;
    int x;
    int y;
    unsigned long pixel;
{
    /* Debugger(); */
    return 0;
}

XImage *
TkMacXSubImage(image, x, y, width, height)
    XImage *image;
    int x;
    int y;
    unsigned int width;
    unsigned int height;
{
    Debugger();
    return NULL;
}

int 
TkMacXAddPixel(image, value)
    XImage *image;
    long value;
{
    Debugger();
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * XChangeWindowAttributes, XSetWindowBackground,
 * XSetWindowBackgroundPixmap, XSetWindowBorder, XSetWindowBorderPixmap,
 * XSetWindowBorderWidth, XSetWindowColormap
 *
 *	These functions are all no-ops.  They all have equivilent
 *	Tk calls that should always be used instead.
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
XChangeWindowAttributes(display, w, value_mask, attributes)
    Display* display;
    Window w;
    unsigned long value_mask;
    XSetWindowAttributes* attributes;
{
}

void 
XSetWindowBackground(display, window, value)
	Display *display;
	Window window;
	unsigned long value;
{
}

void
XSetWindowBackgroundPixmap(display, w, background_pixmap)
    Display* display;
    Window w;
    Pixmap background_pixmap;
{
}

void
XSetWindowBorder(display, w, border_pixel)
    Display* display;
    Window w;
    unsigned long border_pixel;
{
}

void
XSetWindowBorderPixmap(display, w, border_pixmap)
    Display* display;
    Window w;
    Pixmap border_pixmap;
{
}

void
XSetWindowBorderWidth(display, w, width)
    Display* display;
    Window w;
    unsigned int width;
{
}

void
XSetWindowColormap(display, w, colormap)
    Display* display;
    Window w;
    Colormap colormap;
{
    Debugger();
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetDefaultScreenName --
 *
 *	Returns the name of the screen that Tk should use during
 *	initialization.
 *
 * Results:
 *	Returns a statically allocated string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TkGetDefaultScreenName(interp, screenName)
    Tcl_Interp *interp;		/* Not used. */
    char *screenName;		/* If NULL, use default string. */
{
    if ((screenName == NULL) || (screenName[0] == '\0')) {
	screenName = macScreenName;
    }
    return screenName;
}

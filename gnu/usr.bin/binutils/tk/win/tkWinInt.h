/*
 * tkWinInt.h --
 *
 *	This file contains declarations that are shared among the
 *	Windows-specific parts of Tk, but aren't used by the rest of
 *	Tk.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinInt.h 1.17 96/04/11 17:51:55
 */

#ifndef _TKWININT
#define _TKWININT

#ifndef _TKINT
#include "tkInt.h"
#endif

/*
 * Note that the include of windows.h must not be done in tkWinPort.h,
 * because some of the windows declarations conflict with internal static
 * functions in the generic code (e.g. CreateBitmap).
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

/*
 * The TkWinDCState is used to save the state of a device context
 * so that it can be restored later.
 */

typedef struct TkWinDCState {
    HPALETTE palette;
} TkWinDCState;


/*
 * The TkWinDrawable is the internal implementation of an X Drawable (either
 * a Window or a Pixmap).  The following constants define the valid Drawable
 * types.
 */

#define TWD_BITMAP	1
#define TWD_WINDOW	2
#define TWD_WM_WINDOW	3

typedef struct {
    int type;
    HWND handle;
    TkWindow *winPtr;
} TkWinWindow;

typedef struct {
    int type;
    HBITMAP handle;
    Colormap colormap;
    int depth;
} TkWinBitmap;
    
typedef union {
    int type;
    TkWinWindow window;
    TkWinBitmap bitmap;
} TkWinDrawable;

/*
 * The following macros are used to retrieve internal values from a Drawable.
 */

#define TkWinGetHWND(w) (((TkWinDrawable *) w)->window.handle)
#define TkWinGetWinPtr(w) (((TkWinDrawable*)w)->window.winPtr)
#define TkWinGetHBITMAP(w) (((TkWinDrawable*)w)->bitmap.handle)
#define TkWinGetColormap(w) (((TkWinDrawable*)w)->bitmap.colormap)

/*
 * The following structure is used to encapsulate palette information.
 */

typedef struct {
    HPALETTE palette;		/* Palette handle used when drawing. */
    UINT size;			/* Number of entries in the palette. */
    int stale;			/* 1 if palette needs to be realized,
				 * otherwise 0.  If the palette is stale,
				 * then an idle handler is scheduled to
				 * realize the palette. */
    Tcl_HashTable refCounts;	/* Hash table of palette entry reference counts
				 * indexed by pixel value. */
} TkWinColormap;

/*
 * The following macro retrieves the Win32 palette from a colormap.
 */

#define TkWinGetPalette(colormap) (((TkWinColormap *) colormap)->palette)

/*
 * The following macros define the class names for Tk Window types.
 */

#define TK_WIN_TOPLEVEL_CLASS_NAME "TkTopLevel"
#define TK_WIN_CHILD_CLASS_NAME "TkChild"

/*
 * Internal procedures used by more than one source file.
 */

extern LRESULT CALLBACK	TkWinChildProc _ANSI_ARGS_((HWND hwnd, UINT message,
			    WPARAM wParam, LPARAM lParam));
extern void		TkWinClipboardRender _ANSI_ARGS_((TkWindow *winPtr,
			    UINT format));
extern HINSTANCE 	TkWinGetAppInstance _ANSI_ARGS_((void));
extern HDC		TkWinGetDrawableDC _ANSI_ARGS_((Display *display,
			    Drawable d, TkWinDCState* state));
extern TkWinDrawable *	TkWinGetDrawableFromHandle _ANSI_ARGS_((HWND hwnd));
extern unsigned int	TkWinGetModifierState _ANSI_ARGS_((UINT message,
			    WPARAM wParam, LPARAM lParam));
extern HPALETTE		TkWinGetSystemPalette _ANSI_ARGS_((void));
extern HMODULE		TkWinGetTkModule _ANSI_ARGS_((void));
extern void		TkWinPointerDeadWindow _ANSI_ARGS_((TkWindow *winPtr));
extern void		TkWinPointerEvent _ANSI_ARGS_((XEvent *event,
			    TkWindow *winPtr));
extern void		TkWinPointerInit _ANSI_ARGS_((void));
extern void		TkWinReleaseDrawableDC _ANSI_ARGS_((Drawable d,
			    HDC hdc, TkWinDCState* state));
extern HPALETTE		TkWinSelectPalette _ANSI_ARGS_((HDC dc,
			    Colormap colormap));
extern LRESULT CALLBACK	TkWinTopLevelProc _ANSI_ARGS_((HWND hwnd, UINT message,
			    WPARAM wParam, LPARAM lParam));
extern void		TkWinUpdateCursor _ANSI_ARGS_((TkWindow *winPtr));
extern void		TkWinWmConfigure _ANSI_ARGS_((TkWindow *winPtr,
			    WINDOWPOS *pos));
extern int		TkWinWmInstallColormaps _ANSI_ARGS_((HWND hwnd,
			    int message, int isForemost));
extern void		TkWinWmSetLimits _ANSI_ARGS_((HWND hwnd,
			    MINMAXINFO *info));
extern void 		TkWinXInit _ANSI_ARGS_((HINSTANCE hInstance));

#endif /* _TKWININT */


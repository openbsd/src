/*
 * tkMacInt.h --
 *
 *	Declarations of Macintosh specific shared variables and procedures.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacInt.h 1.27 96/03/26 17:37:39
 */

#ifndef _TKMACINT
#define _TKMACINT

#include "tkInt.h"
#include "tkPort.h"

#include <AppleEvents.h>
#include <Windows.h>
#include <QDOffscreen.h>

struct TkWindowPrivate {
    TkWindow *winPtr;     	/* Ptr to tk window or NULL if Pixmap */
    GWorldPtr portPtr;     	/* Either WindowRef or off screen world */
    int xOff;	       		/* X offset from toplevel window */
    int yOff;		       	/* Y offset from toplevel window */
    RgnHandle clipRgn;		/* Visable region of window */
    RgnHandle aboveClipRgn;	/* Visable region of window & it's children */
    int magic;			/* Magic number use to ident Tk toplevels */
    int referenceCount;		/* Don't delete toplevel until children are
				 * gone. */
    struct TkWindowPrivate *toplevel;	/* Pointer to the toplevel
					 * datastruct. */
    int flags;			/* Various state see defines below. */
    TkWindow *scrollWinPtr;	/* Ptr to scrollbar handling grow widget. */
};
typedef struct TkWindowPrivate MacDrawable;

/*
 * Defines use for the flags field of the MacDrawable data structure.
 */
#define TK_SCROLLBAR_GROW	1
#define TK_CLIP_INVALID		2
#define TK_NEVER_MAPPED		4

/*
 * Internal procedures shared among Macintosh Tk modules but not exported
 * to the outside world:
 */

extern Time		GenerateTime _ANSI_ARGS_(());
extern int		HandleWMEvent _ANSI_ARGS_((EventRecord *theEvent));
extern void		InvalClipRgns _ANSI_ARGS_((TkWindow *winPtr));
extern void 		TkAboutDlg _ANSI_ARGS_((void));
extern void		TkCreateMacEventSource _ANSI_ARGS_((void));
extern void 		TkFontList _ANSI_ARGS_((Tcl_Interp *interp,
			    Display *display));
extern Window		TkGetTransientMaster _ANSI_ARGS_((TkWindow *winPtr));
extern int		TkGenerateButtonEvent _ANSI_ARGS_((int x, int y,
			    unsigned int state));
extern int 		TkGetCharPositions _ANSI_ARGS_((
			    XFontStruct *font_struct, char *string,
			    int count, short *buffer));
extern void		TkGenWMDestroyEvent _ANSI_ARGS_((Tk_Window tkwin));
extern void		TkGenWMMoveRequestEvent _ANSI_ARGS_((Tk_Window tkwin,
			    int x, int y));
extern void		TkGenWMResizeRequestEvent _ANSI_ARGS_((Tk_Window tkwin,
			    int width, int height));
extern int		TkMacConvertEvent _ANSI_ARGS_((EventRecord *eventPtr));
extern void		tkMacInstallMWConsole _ANSI_ARGS_((
			    Tcl_Interp *interp));
extern void		TkMacDoHLEvent _ANSI_ARGS_((EventRecord *theEvent));
extern void 		TkMacFontInfo _ANSI_ARGS_((Font fontId, short *family,
			    short *style, short *size));
extern GWorldPtr 	TkMacGetDrawablePort _ANSI_ARGS_((Drawable drawable));
extern TkWindow * 	TkMacGetScrollbarGrowWindow _ANSI_ARGS_((
			    TkWindow *winPtr));
extern Window 		TkMacGetXWindow _ANSI_ARGS_((WindowRef macWinPtr));
extern int		TkMacGrowToplevel _ANSI_ARGS_((WindowRef whichWindow,
			    Point start));
extern void 		TkMacHandleMenuSelect _ANSI_ARGS_((long mResult,
			    int optionKeyPressed));
extern void		TkMacInitAppleEvents _ANSI_ARGS_((Tcl_Interp *interp));
extern void 		TkMacInitMenus _ANSI_ARGS_((Tcl_Interp 	*interp));
extern void		TkMacPointerDeadWindow _ANSI_ARGS_((TkWindow *winPtr));
extern int		TkMacResizable _ANSI_ARGS_((TkWindow *winPtr));
extern void		TkMacSetScrollbarGrow _ANSI_ARGS_((TkWindow *winPtr,
			    int flag));
extern void 		TkMacUpdateClipRgn _ANSI_ARGS_((TkWindow *winPtr));
extern RgnHandle 	TkMacVisableClipRgn _ANSI_ARGS_((TkWindow *winPtr));
extern void		TkMacWinBounds _ANSI_ARGS_((TkWindow *winPtr,
			    Rect *bounds));
extern void		TkResumeClipboard _ANSI_ARGS_((void));
extern int 		TkSetMacColor _ANSI_ARGS_((unsigned long pixel,
			    RGBColor *macColor));
extern void		TkSuspendClipboard _ANSI_ARGS_((void));
extern void		TkUpdateCursor _ANSI_ARGS_((TkWindow *winPtr));
extern int		TkWMGrowToplevel _ANSI_ARGS_((WindowRef whichWindow,
			    Point start));
extern void		TkMacWMInitBounds _ANSI_ARGS_((TkWindow *winPtr,
			    Rect *geometry));
extern Tk_Window	Tk_TopCoordsToWindow _ANSI_ARGS_((Tk_Window tkwin,
			    int rootX, int rootY, int *newX, int *newY));

#endif /* _TKMACINT */

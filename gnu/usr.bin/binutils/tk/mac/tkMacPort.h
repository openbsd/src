/*
 * tkMacPort.h --
 *
 *	This file is included by all of the Tk C files.  It contains
 *	information that may be configuration-dependent, such as
 *	#includes for system include files and a few other things.
 *
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacPort.h 1.32 96/04/12 13:14:26
 */

#ifndef _TKMACPORT
#define _TKMACPORT

/*
 * Macro to use instead of "void" for arguments that must have
 * type "void *" in ANSI C;  maps them to type "char *" in
 * non-ANSI systems.  This macro may be used in some of the include
 * files below, which is why it is defined here.
 */

#ifndef VOID
#   ifdef __STDC__
#       define VOID void
#   else
#       define VOID char
#   endif
#endif

#ifndef _TCL
#   include <tcl.h>
#endif

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if (defined(THINK_C) || defined(__MWERKS__))
double hypot(double x, double y);
#endif
#include <ctype.h>
#include <limits.h>

#include <Xlib.h>
#include <cursorfont.h>
#include <keysym.h>
#include <Xatom.h>
#include <Xfuncproto.h>
#include <Xutil.h>

/*
 * Not all systems declare the errno variable in errno.h. so this
 * file does it explicitly.
 */

extern int errno;

/*
 * Define "NBBY" (number of bits per byte) if it's not already defined.
 */

#ifndef NBBY
#   define NBBY 8
#endif

/*
 * Widget commands for the Native Macintosh look & feel.
 */
EXTERN int	Tk_MacScrollbarCmd(ClientData clientData, Tcl_Interp *interp,
			int argc, char **argv);

/*
 * Hack to correct bugs in Macintosh version of fopen.  This
 * is needed in the parts of Tk that have not yet been converted
 * to use channels.  It works around various problems with fopen
 * such as not understanding file paths which include aliases.
 */
EXTERN FILE * TclMacFOpenHack _ANSI_ARGS_((const char *path,
	const char *mode));
#define fopen(path, mode) TclMacFOpenHack(path, mode)

/*
 * Declarations for various library procedures that may not be declared
 * in any other header file.
 */

extern void panic();
extern int strncasecmp _ANSI_ARGS_((const char *s1, const char *s2,
	size_t length));

/*
 * Defines for X functions that are used by Tk but are treated as
 * no-op functions on the Macintosh.
 */
#define XFlush(display)
#define XFree(data) {if ((data) != NULL) ckfree((char *) (data));}
#define XGrabServer(display)
#define XNoOp(display) {display->request++;}
#define XUngrabServer(display)
#define XSynchronize(display, bool) {display->request++;}
#define XSync(display, bool) {display->request++;}
#define XVisualIDFromVisual(visual) (visual->visualid)

/*
 * The following function is not used on the Mac, so we stub it out.
 */
#define TkSetPixmapColormap(p,c) {}

#endif /* _TKMACPORT */

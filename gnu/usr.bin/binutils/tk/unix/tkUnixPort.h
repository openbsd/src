/*
 * tkUnixPort.h --
 *
 *	This file is included by all of the Tk C files.  It contains
 *	information that may be configuration-dependent, such as
 *	#includes for system include files and a few other things.
 *
 * Copyright (c) 1991-1993 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkUnixPort.h 1.18 96/03/14 10:22:47
 */

#ifndef _UNIXPORT
#define _UNIXPORT

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

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#ifdef HAVE_LIMITS_H
#   include <limits.h>
#else
#   include "../compat/limits.h"
#endif
#include <math.h>
#include <pwd.h>
#ifdef NO_STDLIB_H
#   include "../compat/stdlib.h"
#else
#   include <stdlib.h>
#endif
#include <string.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#ifndef _TCL
#   include <tcl.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#else
#   include "../compat/unistd.h"
#endif
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

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
 * These macros are just wrappers for the equivalent X Region calls.
 */

#define TkClipBox(rgn, rect) XClipBox((Region) rgn, rect)
#define TkCreateRegion() (TkRegion) XCreateRegion()
#define TkDestroyRegion(rgn) XDestroyRegion((Region) rgn)
#define TkIntersectRegion(a, b, r) XIntersectRegion((Region) a, \
	(Region) b, (Region) r)
#define TkRectInRegion(r, x, y, w, h) XRectInRegion((Region) r, x, y, w, h)
#define TkSetRegion(d, gc, rgn) XSetRegion(d, gc, (Region) rgn)
#define TkUnionRectWithRegion(rect, src, ret) XUnionRectWithRegion(rect, \
	(Region) src, (Region) ret)

/*
 * The TkPutImage macro strips off the color table information, which isn't
 * needed for X.
 */

#define TkPutImage(colors, ncolors, display, pixels, gc, image, destx, desty, srcx, srcy, width, height) \
	XPutImage(display, pixels, gc, image, destx, desty, srcx, \
	srcy, width, height);

/*
 * Supply macros for seek offsets, if they're not already provided by
 * an include file.
 */

#ifndef SEEK_SET
#   define SEEK_SET 0
#endif

#ifndef SEEK_CUR
#   define SEEK_CUR 1
#endif

#ifndef SEEK_END
#   define SEEK_END 2
#endif

/*
 * Declarations for various library procedures that may not be declared
 * in any other header file.
 */

extern void		panic();

/*
 * These functions do nothing under Unix, so we just eliminate calls them.
 */

#define TkSelUpdateClipboard(a,b) {}
#define TkSetPixmapColormap(p,c) {}

#endif /* _UNIXPORT */

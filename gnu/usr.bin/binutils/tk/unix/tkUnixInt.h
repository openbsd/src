/*
 * tkUnixInt.h --
 *
 *	This file contains declarations that are shared among the
 *	UNIX-specific parts of Tk but aren't used by the rest of
 *	Tk.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkUnixInt.h 1.2 95/11/08 10:18:13
 */

#ifndef _TKUNIXINT
#define _TKUNIXINT

/*
 * Prototypes for procedures that are referenced in files other
 * than the ones they're defined in.
 */

extern void		TkCreateXEventSource _ANSI_ARGS_((void));

#endif /* _TKUNIXINT */

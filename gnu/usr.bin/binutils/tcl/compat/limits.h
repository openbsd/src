/*
 * limits.h --
 *
 *	This is a dummy header file to #include in Tcl when there
 *	is no limits.h in /usr/include.  There are only a few
 *	definitions here;  also see tclPort.h, which already
 *	#defines some of the things here if they're not arleady
 *	defined.
 *
 * Copyright (c) 1991 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) limits.h 1.7 96/02/15 14:43:55
 */

#define LONG_MIN		0x80000000
#define LONG_MAX		0x7fffffff
#define INT_MIN			0x80000000
#define INT_MAX			0x7fffffff

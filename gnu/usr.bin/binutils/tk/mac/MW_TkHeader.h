/*
 * MW_TkHeader.h --
 *
 *	This file is a global header file for the MetroWerks CodeWarrior
 *  environment.  It essentially acts as a place to set compiler
 *  flags.  See MetroWerks documention for more details.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) MW_TkHeader.h 1.10 96/03/04 11:37:47
 */

#define MAC_TCL
#define TK_LIBRARY ":library"

#define NO_GETTOD 1
#define NO_UNION_WAIT 1
#define HAVE_UNISTD_H 1
#define HAS_STDARG 1
#define HAVE_LIMITS_H
#define REDO_KEYSYM_LOOKUP

/*
 * The following two defines are used to prepare for the coming
 * of Copland.
 */

#define STRICT_CONTROLS 0
#define STRICT_WINDOWS  0

/*
 * Define the following symbol if you want
 * comprehensive debugging turned on.
 */

/* #define TCL_DEBUG */

#ifdef TCL_DEBUG
#   define TCL_MEM_DEBUG
#   define TK_TEST
#endif


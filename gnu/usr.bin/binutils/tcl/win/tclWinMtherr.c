/* 
 * tclWinMtherr.c --
 *
 *	This function provides a default implementation of the
 *	_matherr function for Borland C++.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinMtherr.c 1.2 96/02/15 11:54:05
 */

#include "tclInt.h"
#include "tclPort.h"
#include <math.h>

/*
 * The following variable is secretly shared with Tcl so we can
 * tell if expression evaluation is in progress.  If not, matherr
 * just emulates the default behavior, which includes printing
 * a message.
 */

extern int tcl_MathInProgress;

/*
 *----------------------------------------------------------------------
 *
 * _matherr --
 *
 *	This procedure is invoked by Borland C++ when certain
 *	errors occur in mathematical functions.  This procedure
 *	replaces the default implementation which generates pop-up
 *	warnings.
 *
 * Results:
 *	Returns 1 to indicate that we've handled the error
 *	locally.
 *
 * Side effects:
 *	Sets errno based on what's in xPtr.
 *
 *----------------------------------------------------------------------
 */

int
_matherr(xPtr)
    struct exception *xPtr;	/* Describes error that occurred. */
{
    if (!tcl_MathInProgress) {
	return 0;
    }
    if ((xPtr->type == DOMAIN) || (xPtr->type == SING)) {
	errno = EDOM;
    } else {
	errno = ERANGE;
    }
    return 1;
}

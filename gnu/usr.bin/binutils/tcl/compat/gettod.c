/* 
 * gettod.c --
 *
 *	This file provides the gettimeofday function on systems
 *	that only have the System V ftime function.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) gettod.c 1.2 96/02/15 12:08:26
 */

#include "tcl.h"
#include "tclPort.h"
#include <sys/timeb.h>

#undef timezone

int
gettimeofday(tp, tz)
struct timeval *tp;
struct timezone *tz;
{
    struct timeb t;
    ftime(&t);
    tp->tv_sec = t.time;
    tp->tv_usec = t. millitm * 1000;
    return 0;
}


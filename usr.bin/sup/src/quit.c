/*	$OpenBSD: quit.c,v 1.3 1997/04/01 07:35:15 todd Exp $	*/

/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator   or   Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the rights
 * to redistribute these changes.
 */
/*
 *  quit  --  print message and exit
 *
 *  Usage:  quit (status,format [,arg]...);
 *	int status;
 *	(... format and arg[s] make up a printf-arglist)
 *
 *  Quit is a way to easily print an arbitrary message and exit.
 *  It is most useful for error exits from a program:
 *	if (open (...) < 0) then quit (1,"Can't open...",file);
 *
 **********************************************************************
 * HISTORY
 * 
 **********************************************************************
 */

#include <stdio.h>
#include "supcdefs.h"
#include "supextern.h"

void
#ifdef __STDC__
quit (int status, char * fmt, ...)
#else
quit (va_alist)
va_dcl
#endif
{
	va_list args;
#ifdef __STDC__
	va_start(args, fmt);
#else
	int status;
	char *fmt;

	va_start(args);
	status = va_arg(args, int);
	fmt = va_arg(args, char *);
#endif

	fflush(stdout);
	(void) vfprintf(stderr, fmt, args);
	va_end(args);
	exit(status);
}

/*	$OpenBSD: ex_tcl.c,v 1.7 2014/11/12 04:28:41 bentley Exp $	*/

/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>

#include "../common/common.h"

/* 
 * ex_tcl -- :[line [,line]] tcl [command]
 *	Run a command through the tcl interpreter.
 *
 * PUBLIC: int ex_tcl(SCR*, EXCMD *);
 */
int 
ex_tcl(SCR *sp, EXCMD *cmdp)
{
	msgq(sp, M_ERR, "302|Vi was not loaded with a Tcl interpreter");
	return (1);
}

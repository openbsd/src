/*	$OpenBSD: rf_strutils.c,v 1.1 1999/01/11 14:29:51 niklas Exp $	*/
/*	$NetBSD: rf_strutils.c,v 1.1 1998/11/13 04:20:35 oster Exp $	*/
/*
 * rf_strutils.c
 *
 * String-parsing funcs
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * rf_strutils.c -- some simple utilities for munging on strings.
 * I put them in a file by themselves because they're needed in
 * setconfig, in the user-level driver, and in the kernel.
 *
 * :  
 * Log: rf_strutils.c,v 
 * Revision 1.2  1996/06/02 17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 */

#include "rf_utils.h"

/* finds a non-white character in the line */
char *rf_find_non_white(char *p)
{
  for (; *p != '\0' && (*p == ' ' || *p == '\t'); p++);
  return(p);
}

/* finds a white character in the line */
char *rf_find_white(char *p)
{
  for (; *p != '\0' && (*p != ' ' && *p != '\t'); p++);
  return(p);
}

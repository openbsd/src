/*	$OpenBSD: rf_utility.c,v 1.1 1999/01/11 14:49:45 niklas Exp $	*/

/*
 * rf_utility.c
 */
/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
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
 * define symbols for raidframe utils which share .c files with
 * raidframe proper
 */

#include "rf_utility.h"

#ifdef RF_DBG_OPTION
#undef RF_DBG_OPTION
#endif /* RF_DBG_OPTION */

#ifdef __STDC__
#define RF_DBG_OPTION(_option_,_defval_) long rf_##_option_ = _defval_;
#else /* __STDC__ */
#define RF_DBG_OPTION(_option_,_defval_) long rf_/**/_option_ = _defval_;
#endif /* __STDC__ */
#include "rf_optnames.h"

int rf_mutex_init(m)
  int  *m;
{
	*m = 0;
	return(0);
}

int rf_mutex_destroy(m)
  int  *m;
{
	*m = 0;
	return(0);
}

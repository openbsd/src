/*	$OpenBSD: clockvar.h,v 1.5 2008/04/07 22:36:24 miod Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * Adopted for r4400: Per Fogelstrom
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
 * Definitions for "cpu-independent" clock handling.
 */

/*
 * tod_time structure:
 *
 * structure passed to TOD clocks when setting them.  broken out this
 * way, so that the time_t -> field conversion can be shared.
 */
struct tod_time {
	int	year;			/* year 1900.... */
	int	mon;			/* month (1 - 12) */
	int	day;			/* day (1 - 31) */
	int	hour;			/* hour (0 - 23) */
	int	min;			/* minute (0 - 59) */
	int	sec;			/* second (0 - 59) */
	int	dow;			/* day of week (0 - 6; 0 = Sunday) */
};

/*
 * tod_desc structure:
 *
 * provides tod-specific functions to do necessary operations.
 */

struct tod_desc {
	void	*tod_cookie;
	void	(*tod_get)(void *, time_t, struct tod_time *);
	void	(*tod_set)(void *, struct tod_time *);
	int	tod_valid;
};

extern struct tod_desc sys_tod;

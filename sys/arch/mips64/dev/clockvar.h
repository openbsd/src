/*	$OpenBSD: clockvar.h,v 1.2 2004/08/10 20:28:13 deraadt Exp $	*/

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
 * Definitions for "cpu-independent" clock handling for the mips arc arch.
 */

/*
 * clocktime structure:
 *
 * structure passed to TOY clocks when setting them.  broken out this
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
 * clockdesc structure:
 *
 * provides clock-specific functions to do necessary operations.
 */
struct clock_softc;

struct clock_desc {
	void	(*clk_attach)(struct device *, struct device *, void *);
	void	(*clk_init)(struct clock_softc *);
	void	(*clk_get)(struct clock_softc *, time_t, struct tod_time *);
	void	(*clk_set)(struct clock_softc *, struct tod_time *);
	int	clk_hz;
	int	clk_stathz;
	int	clk_profhz;
};

struct clock_softc {
	struct	device sc_dev;
	struct	clock_desc sc_clock;
	int	sc_initted;
	bus_space_tag_t sc_clk_t;
	bus_space_handle_t sc_clk_h;
	void	*ih;
};

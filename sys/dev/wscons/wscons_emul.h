/*	$NetBSD: wscons_emul.h,v 1.2 1996/04/12 06:10:32 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
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

#define	ANSICONS_NARGS		4

struct wscons_emul_data {
	int	ac_state;			/* current state; see below */

	const struct wscons_emulfuncs *ac_ef;	/* emul. callback functions */
	void	*ac_efa;			/* arg. for callbacks */

	int	ac_nrow, ac_ncol;		/* number of rows/columns */
	int	ac_crow, ac_ccol;		/* current row/column */

	u_int	ac_args[ANSICONS_NARGS];	/* emulation args */
};

#define	ANSICONS_STATE_NORMAL	0		/* normal processing */
#define	ANSICONS_STATE_HAVEESC	1		/* seen start of ctl seq */
#define	ANSICONS_STATE_CONTROL	2		/* processing ctl seq */

void	wscons_emul_attach __P((struct wscons_emul_data *,
	    const struct wscons_odev_spec *));
void	wscons_emul_input __P((struct wscons_emul_data *, char *, int));

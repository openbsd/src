/*	$OpenBSD: signal.h,v 1.7 2006/01/08 14:20:17 millert Exp $	*/

/* 
 * Copyright (c) 1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: signal.h 1.3 94/12/16$
 */

#ifndef	_HPPA_SIGNAL_H_
#define	_HPPA_SIGNAL_H_

#include <sys/cdefs.h>

/*
 * Machine-dependent signal definitions
 */

typedef int sig_atomic_t;

#if __BSD_VISIBLE
#include <machine/trap.h>
#endif

#if __BSD_VISIBLE || __XPG_VISIBLE >= 420
/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 */
struct	sigcontext {
	unsigned	sc_onstack;	/* sigstack state to restore */
	unsigned	sc_mask;	/* signal mask to restore */
	unsigned	sc_ps;		/* psl to restore */
	unsigned	sc_fp;		/* fp to restore */
	unsigned	sc_pcoqh;	/* pc offset queue (head) to restore */
	unsigned	sc_pcoqt;	/* pc offset queue (tail) to restore */
	unsigned	sc_resv[2];
	unsigned	sc_regs[32];
	unsigned	sc_fpregs[64];
};
#endif /* __BSD_VISIBLE || __XPG_VISIBLE >= 420 */
#endif  /* !_HPPA_SIGNAL_H_ */

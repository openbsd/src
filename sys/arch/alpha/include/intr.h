/*	$OpenBSD: intr.h,v 1.3 1996/10/30 22:39:09 niklas Exp $	*/
/*	$NetBSD: intr.h,v 1.2 1996/07/09 00:33:25 cgd Exp $	*/

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
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

#ifndef _ALPHA_INTR_H_
#define _ALPHA_INTR_H_

#define	IPL_NONE	0	/* disable only this interrupt */
#define	IPL_BIO		1	/* disable block I/O interrupts */
#define	IPL_NET		2	/* disable network interrupts */
#define	IPL_TTY		3	/* disable terminal interrupts */
#define	IPL_CLOCK	4	/* disable clock interrupts */
#define	IPL_HIGH	5	/* disable all interrupts */

#define	IST_NONE	0	/* none (dummy) */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#define splx(s)								\
	    (s == ALPHA_PSL_IPL_0 ? spl0() : alpha_pal_swpipl(s))
#define splsoft()               alpha_pal_swpipl(ALPHA_PSL_IPL_SOFT)
#define splsoftclock()          splsoft()
#define splsoftnet()            splsoft()
#define splnet()                alpha_pal_swpipl(ALPHA_PSL_IPL_IO)
#define splbio()                alpha_pal_swpipl(ALPHA_PSL_IPL_IO)
#define splimp()                alpha_pal_swpipl(ALPHA_PSL_IPL_IO)
#define spltty()                alpha_pal_swpipl(ALPHA_PSL_IPL_IO)
#define splclock()              alpha_pal_swpipl(ALPHA_PSL_IPL_CLOCK)
#define splstatclock()          alpha_pal_swpipl(ALPHA_PSL_IPL_CLOCK)
#define splhigh()               alpha_pal_swpipl(ALPHA_PSL_IPL_HIGH)

/*
 * simulated software interrupt register
 */
extern u_int64_t ssir;

#define	SIR_NET		0x1
#define	SIR_CLOCK	0x2

#define	siroff(x)	ssir &= ~(x)
#define	setsoftnet()	ssir |= SIR_NET
#define	setsoftclock()	ssir |= SIR_CLOCK

#endif

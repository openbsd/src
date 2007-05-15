/*	$OpenBSD: intr.h,v 1.7 2007/05/15 05:26:45 miod Exp $	*/
/*	$NetBSD: intr.h,v 1.12 2003/06/16 20:00:59 thorpej Exp $	*/

/*
 * Copyright (c) 2001, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_EVBARM_INTR_H_
#define	_EVBARM_INTR_H_

#ifdef _KERNEL


/* Interrupt priority "levels". */
#define	IPL_NONE	0	/* nothing */
#define	IPL_SOFT	1	/* generic software interrupts */
#define	IPL_SOFTCLOCK	2	/* software clock interrupt */
#define	IPL_SOFTNET	3	/* software network interrupt */
#define	IPL_BIO		4	/* block I/O */
#define	IPL_NET		5	/* network */
#define	IPL_SOFTSERIAL	6	/* software serial interrupt */
#define	IPL_TTY		7	/* terminals */
#define	IPL_VM		8	/* memory allocation */
#define	IPL_AUDIO	9	/* audio device */
#define	IPL_CLOCK	10	/* clock interrupt */
#define	IPL_STATCLOCK	11	/* statistics clock interrupt */
#define	IPL_HIGH	12	/* everything */
#define	IPL_SERIAL	13	/* serial device */

#define	NIPL		14

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#define IST_LEVEL_LOW	 IST_LEVEL
#define IST_LEVEL_HIGH   4
#define IST_EDGE_FALLING IST_EDGE
#define IST_EDGE_RISING  5
#define IST_EDGE_BOTH    6

#ifndef _LOCORE

#include <sys/device.h>
#include <sys/queue.h>

#define	splhigh()	_splraise(IPL_HIGH)
#define	splsoft()	_splraise(IPL_SOFT)
#define	splsoftclock()	_splraise(IPL_SOFTCLOCK)
#define	splsoftnet()	_splraise(IPL_SOFTNET)
#define	splbio()	_splraise(IPL_BIO)
#define	splnet()	_splraise(IPL_NET)
#define	spltty()	_splraise(IPL_TTY)
#define	splvm()		_splraise(IPL_VM)
#define	splaudio()	_splraise(IPL_AUDIO)
#define	splclock()	_splraise(IPL_CLOCK)
#define	splstatclock()	_splraise(IPL_STATCLOCK)
#define	splserial()	_splraise(IPL_SERIAL)

#define	spl0()		_spllower(IPL_NONE)

#define	splsched()	splhigh()
#define	spllock()	splhigh()

#endif /* ! _LOCORE */

#include <arm/xscale/pxa2x0_intr.h>

#endif /* _KERNEL */

#endif	/* _EVBARM_INTR_H_ */


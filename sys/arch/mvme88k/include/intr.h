/*	$OpenBSD: intr.h,v 1.23 2013/05/17 22:51:59 miod Exp $	*/
/*
 * Copyright (C) 2000 Steve Murphree, Jr.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1996 Nivas Madhur
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MACHINE_INTR_H_
#define _MACHINE_INTR_H_

/*
 * IPL levels.
 * We use 6 as IPL_HIGH so that abort can be programmed at 7 so that
 * it is always possible to break into the system unless interrupts
 * are disabled.
 */

#define IPL_NONE	0
#define IPL_SOFTINT	1
#define IPL_BIO		2
#define IPL_NET		3
#define IPL_TTY		5
#define IPL_VM		5
#define IPL_CLOCK	5
#define IPL_STATCLOCK	5
#define	IPL_SCHED	5
#define IPL_HIGH	6
#define IPL_NMI		7
#define IPL_ABORT	7

#define	NIPLS		8

#if defined(_KERNEL) && !defined(_LOCORE)

#include <sys/evcount.h>

struct intrhand {
	SLIST_ENTRY(intrhand) ih_link;
	int	(*ih_fn)(void *);
	void	*ih_arg;
	int	ih_ipl;
	int	ih_wantframe;
	struct evcount ih_count;
};

int	intr_establish(int, struct intrhand *, const char *);
int	intr_findvec(int, int, int);

/*
 * There are 256 possible VME interrupt vectors on a mvme88k platform
 * (although some systems use VME vectors for onboard devices).
 * Use either vmeintr_establish() or intr_establish() to register a handler
 * for the given vector. Vector number is used to index into the
 * intr_handlers[] table.
 */
#define	NVMEINTR	256
typedef SLIST_HEAD(, intrhand) intrhand_t;
extern intrhand_t intr_handlers[NVMEINTR];

/*
 * Currently registered VME interrupt vectors for a given IPL, if they
 * are unique. Used to help the MVME181 and MVME188 interrupt handlers when
 * they fail to complete the VME interrupt acknowledge cycle to get the
 * interrupt vector number.
 */
extern u_int vmevec_hints[NIPLS];

/* Interrupt masks (MVME181, MVME188) */
extern u_int32_t int_mask_val[NIPLS];

/*
 * Logical values for non-VME interrupt sources on boards with dedicated
 * interrupt controllers (MVME181, MVME188). Not all sources may be available
 * on a given board.
 */

#define	INTSRC_ABORT	1	/* abort button */
#define	INTSRC_ACFAIL	2	/* AC failure */
#define	INTSRC_SYSFAIL	3	/* system failure */
#define	INTSRC_PARERR	4	/* memory parity error */
#define	INTSRC_CIO	5	/* Z8536 */
#define	INTSRC_DTIMER	6	/* MC68692 timer interrupt */
#define	INTSRC_DUART	7	/* MC68692 serial interrupt */
#define	INTSRC_VME	8	/* up to seven VME interrupt levels */

#endif /* _KERNEL && !_LOCORE */

#include <m88k/intr.h>

#endif /* _MACHINE_INTR_H_ */

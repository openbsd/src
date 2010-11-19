/*	$OpenBSD: altq.h,v 1.7 2010/11/19 22:36:11 mikeb Exp $	*/
/*	$KAME: altq.h,v 1.6 2000/12/14 08:12:45 thorpej Exp $	*/

/*
 * Copyright (C) 1998-2000
 *	Sony Computer Science Laboratories Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ALTQ_ALTQ_H_
#define	_ALTQ_ALTQ_H_

/* altq discipline type */
#define	ALTQT_NONE		 0	/* reserved */
#define	ALTQT_CBQ		 1	/* cbq */
#define	ALTQT_WFQ		 2	/* wfq */
#define	ALTQT_AFMAP		 3	/* afmap */
#define	ALTQT_FIFOQ		 4	/* fifoq */
#define	ALTQT_RED		 5	/* red */
#define	ALTQT_RIO		 6	/* rio */
#define	ALTQT_LOCALQ		 7	/* local use */
#define	ALTQT_HFSC		 8	/* hfsc */
#define	ALTQT_CDNR		 9	/* traffic conditioner */
#define	ALTQT_BLUE		10	/* blue */
#define	ALTQT_PRIQ		11	/* priority queue */
#define	ALTQT_MAX		12	/* should be max discipline type + 1 */

/* simple token bucket meter profile */
struct tb_profile {
	u_int	rate;	/* rate in bit-per-sec */
	u_int	depth;	/* depth in bytes */
};

/*
 * generic packet counter
 */
struct pktcntr {
	u_int64_t	packets;
	u_int64_t	bytes;
};

#define	PKTCNTR_ADD(cntr, len)	\
	do { (cntr)->packets++; (cntr)->bytes += len; } while (0)

#ifdef _KERNEL
#include <altq/altq_var.h>
#endif

#endif /* _ALTQ_ALTQ_H_ */

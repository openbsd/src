/*	$OpenBSD: sbusreg.h,v 1.6 2010/07/10 19:32:24 miod Exp $	*/
/*	$NetBSD: sbusreg.h,v 1.3 1997/09/14 19:17:25 pk Exp $ */

/*
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
 *
 *	@(#)sbusreg.h	8.1 (Berkeley) 6/11/93
 */

/*
 * Sun-4c S-bus definitions.  (Should be made generic!)
 *
 * SBus slot 0 is not a separate slot; it talks to the onboard I/O devices.
 * It is, however, addressed just like any `real' SBus.
 *
 * SBus device addresses are obtained from the FORTH PROMs.  They come
 * in `absolute' and `relative' address flavors, so we have to handle both.
 * Relative addresses do *not* include the slot number.
 */
#define	SBUS_PAGE_SHIFT		(13 + PAGE_SHIFT)
#define	SBUS_BASE		(0 - (4 << SBUS_PAGE_SHIFT))
#define	SBUS_PAGE_MASK		((1 << SBUS_PAGE_SHIFT) - 1)
#define	SBUS_ADDR(slot, off)	(SBUS_BASE + ((slot) << SBUS_PAGE_SHIFT) + (off))
#define	SBUS_ABS(a)		((unsigned)(a) >= SBUS_BASE)
#define	SBUS_ABS_TO_SLOT(a)	(((a) - SBUS_BASE) >> SBUS_PAGE_SHIFT)
#define	SBUS_ABS_TO_OFFSET(a)	(((a) - SBUS_BASE) & SBUS_PAGE_MASK)

struct sbusreg {
	u_int32_t	sbus_afsr;	/* M-to-S Asynchronous Fault Status */
	u_int32_t	sbus_afar;	/* M-to-S Asynchronous Fault Address */
	u_int32_t	sbus_arbiter;	/* Arbiter Enable  */
	u_int32_t	sbus_reserved1;

#define NSBUSCFG	20
	/* Actual number dependent on machine model */
	u_int32_t	sbus_sbuscfg[NSBUSCFG];	/* SBus configuration control */
};

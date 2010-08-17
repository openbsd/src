/*	$OpenBSD: fdvar.h,v 1.9 2010/08/17 20:05:06 miod Exp $	*/
/*
 *	$NetBSD: fdvar.h,v 1.5 1996/12/08 23:40:34 pk Exp $
 *
 * Copyright (c) 1995 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */

#define	FD_BSIZE(fd)	(128 << fd->sc_type->secsize)
#define	FDC_MAXIOSIZE	NBPG	/* XXX should be MAXBSIZE */

#define FDC_NSTATUS	10

#ifndef _LOCORE
struct fdcio {
	/*
	 * 82072 (sun4c) and 82077 (sun4m) controllers have different
	 * register layout; so we cache some here.
	 */
	volatile u_int8_t	*fdcio_reg_msr;
	volatile u_int8_t	*fdcio_reg_fifo;
	volatile u_int8_t	*fdcio_reg_dor;	/* 82077 only */

	/*
	 * Interrupt state.
	 */
	int	fdcio_itask;
	int	fdcio_istatus;

	/*
	 * IO state.
	 */
	char	*fdcio_data;		/* pseudo-dma data */
	int	fdcio_tc;		/* pseudo-dma Terminal Count */
	u_char	fdcio_status[FDC_NSTATUS];	/* copy of registers */
	int	fdcio_nstat;		/* # of valid status bytes */

	struct	intrhand fdcio_ih;
	void		*fdcio_sih;	/* softintr cookie */
};
#endif /* _LOCORE */

/* itask values */
#define FDC_ITASK_NONE		0	/* No HW interrupt expected */
#define FDC_ITASK_SENSEI	1	/* Do SENSEI on next HW interrupt */
#define FDC_ITASK_DMA		2	/* Do Pseudo-DMA */
#define FDC_ITASK_RESULT	3	/* Pick up command results */

/* istatus values */
#define FDC_ISTATUS_NONE	0	/* No status available */
#define FDC_ISTATUS_SPURIOUS	1	/* Spurious HW interrupt detected */
#define FDC_ISTATUS_ERROR	2	/* Operation completed abnormally */
#define FDC_ISTATUS_DONE	3	/* Operation completed normally */

#define SUNOS_FDIOCEJECT	_IO('f', 24)

/* $OpenBSD: sccvar.h,v 1.7 2010/06/06 11:26:17 miod Exp $ */
/* $NetBSD: sccvar.h,v 1.7 2001/08/26 16:39:56 simonb Exp $ */

/* 
 * Copyright (c) 1991,1990,1989,1994,1995 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	from: @(#)sccreg.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Definitions for Intel 82530 serial communications chip.  Each chip is a
 * dual uart with the A channels used for the keyboard and mouse with the B
 * channel(s) for comm ports with modem control. Since some registers are
 * used for the other channel, the following macros are used to access the
 * register ports.
 */

#if 1
#define alpha_sparse
#endif

#define	DENSE
#ifdef	alpha_sparse
#undef	DENSE
#define	SPARSE
#endif

typedef struct {
	struct {
		volatile u_int	scc_command;	/* Register select. */
#ifdef SPARSE
		u_int		scc_pad0;
#endif
		volatile u_int	scc_data;	/* Rx/Tx buffer */
#ifdef SPARSE
		u_int		scc_pad1;
#endif
	} scc_channel[2];
} scc_regmap_t;

#define	scc_get_datum(d, v) \
	do { (v) = ((d) >> 8) & 0xff; alpha_mb(); DELAY(5); } while (0)
#define	scc_set_datum(d, v) \
	do { (d) = (volatile unsigned int)(v) << 8; alpha_mb(); DELAY(5); } while (0)

/* From <pmax/dev/pdma.h>. */
struct pdma {
	void	*p_addr;
	char	*p_mem;
	char	*p_end;
	int	p_arg;
	void	(*p_fcn)(struct tty *tp);
};

/*
 * Minor device numbers for scc.  Weird because B channel comes first and
 * the A channels are wired for keyboard/mouse and the B channels for the
 * comm port(s).
 */
#define	SCCCOMM2_PORT	0x0
#define	SCCMOUSE_PORT	0x1
#define	SCCCOMM3_PORT	0x2
#define	SCCKBD_PORT	0x3

extern int alpha_donot_kludge_scc;

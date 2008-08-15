/*	$OpenBSD: dzvar.h,v 1.7 2008/08/15 22:50:25 miod Exp $	*/
/*	$NetBSD: dzvar.h,v 1.8 2000/06/04 02:14:12 matt Exp $	*/
/*
 * Copyright (c) 1996  Ken C. Wellsch.  All rights reserved.
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
 */

/* A DZ-11 has 8 ports while a DZV/DZQ-11 has only 4. We use 8 by default */

#define	NDZLINE 	8

#define	DZ_DZ		8
#define	DZ_DZV		4
#define	DZ_DC		4

#define DZ_C2I(c)	((c)<<3)	/* convert controller # to index */
#define DZ_I2C(c)	((c)>>3)	/* convert minor to controller # */
#define DZ_PORT(u)	((u)&07)	/* extract the port # */

struct	dz_softc {
	struct	device	sc_dev;		/* Autoconf blaha */
	struct	evcount	sc_rintrcnt;	/* receive interrupt counts */
	struct	evcount	sc_tintrcnt;	/* transmit interrupt counts */
	int	sc_rcvec, sc_tcvec;	/* XXX used by attachment glue only */
	struct	dz_regs	sc_dr;		/* reg pointers */
	bus_space_tag_t	sc_iot;
	bus_space_handle_t sc_ioh;
	int		sc_type;	/* DZ11 or DZV11? */
	int		sc_rxint;	/* Receive interrupt count XXX */
	int		sc_openings;	/* # of times we've been opened */
	u_char		sc_brk;		/* Break asserted on some lines */
	u_char		sc_dsr;		/* DSR set bits if no mdm ctrl */
	struct dz_linestate {
		struct dz_softc	*dz_sc;		/* backpointer to softc */
		int		dz_line;	/* sub-driver unit number */
		void		*dz_private;	/* sub-driver data pointer */
		int		(*dz_catch)(void *, int); /* Fast catch recv */
		struct	tty *	dz_tty;		/* what we work on */
#ifdef notyet
		caddr_t		dz_mem;		/* pointers to clist output */
		caddr_t		dz_end;		/*   allowing pdma action */
#endif
	} sc_dz[NDZLINE];
};

void   dzattach(struct dz_softc *);
void   dzrint(void *);
void   dzxint(void *);
void   dzreset(struct device *);

#define	DZ_READ_BYTE(sc, adr) \
	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (sc)->sc_dr.adr)
#define	DZ_READ_WORD(sc, adr) \
	bus_space_read_2((sc)->sc_iot, (sc)->sc_ioh, (sc)->sc_dr.adr)
#define	DZ_WRITE_BYTE(sc, adr, val) \
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (sc)->sc_dr.adr, val)
#define	DZ_WRITE_WORD(sc, adr, val) \
	bus_space_write_2((sc)->sc_iot, (sc)->sc_ioh, (sc)->sc_dr.adr, val)

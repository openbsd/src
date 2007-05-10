/*	$OpenBSD: bivar.h,v 1.7 2007/05/10 17:59:26 deraadt Exp $ */
/*	$NetBSD: bivar.h,v 1.8 2000/07/26 12:41:40 ragge Exp $ */
/*
 * Copyright (c) 1996, 1999 Ludd, University of Lule}, Sweden.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
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
 */



/*
 * per-BI-adapter state.
 */
struct bi_softc {
	struct device sc_dev;
	bus_space_tag_t sc_iot;		/* Space tag for the BI bus */
	bus_dma_tag_t sc_dmat;
	bus_addr_t sc_addr;		/* Address base address for this bus */
	int sc_busnr;			/* (Physical) number of this bus */
	int sc_lastiv;			/* last available interrupt vector */
	int sc_intcpu;
};

/*
 * Struct used for autoconfiguration; attaching of BI nodes.
 */
struct bi_attach_args {
	bus_space_tag_t ba_iot;
	bus_space_handle_t ba_ioh;	/* Base address for this node */
	bus_dma_tag_t ba_dmat;
	int ba_busnr;
	int ba_nodenr;
	int ba_intcpu;	/* Mask of which cpus to interrupt */
	int ba_ivec;	/* Interrupt vector to use */
	void *ba_icookie;
};

/*
 * BI node list.
 */
struct bi_list {
	u_short bl_nr;		/* Unit ID# */
	u_short bl_havedriver;	/* Have device driver (informal) */
	char *bl_name;		/* DEC name */
};

/* bl_havedriver field meaning */
#define	DT_UNSUPP	0	/* pseudo define */
#define	DT_HAVDRV	1	/* device have driver */
#define	DT_ADAPT	2	/* is an adapter */
#define	DT_QUIET	4	/* don't complain when not conf'ed */
#define	DT_VEC		8	/* uses a interrupt vector */

/* Prototype */
void bi_attach (struct bi_softc *);
void bi_intr_establish (void *, int, void (*)(void *), void *);

/*	$OpenBSD: isa_machdep.h,v 1.4 2005/12/13 00:18:19 jsg Exp $	*/
/*	$NetBSD: isa_machdep.h,v 1.2 2003/05/09 23:51:28 fvdl Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)isa.h	5.7 (Berkeley) 5/9/91
 */

/*
 * Various pieces of the i386 port want to include this file without
 * or in spite of using isavar.h, and should be fixed.
 */

#ifndef _I386_ISA_MACHDEP_H_			/* XXX */
#define _I386_ISA_MACHDEP_H_			/* XXX */

#include <machine/bus.h>
#include <dev/isa/isadmavar.h>

/*
 * XXX THIS FILE IS A MESS.  copyright: berkeley's probably.
 * contents from isavar.h and isareg.h, mostly the latter.
 * perhaps charles's?
 *
 * copyright from berkeley's isa.h which is now dev/isa/isareg.h.
 */

/*
 * Types provided to machine-independent ISA code.
 */

typedef void *isa_chipset_tag_t;

struct device;			/* XXX */
struct isabus_attach_args;	/* XXX */

/*
 * Functions provided to machine-independent ISA code.
 */
void	isa_attach_hook(struct device *, struct device *,
	    struct isabus_attach_args *);
int	isa_intr_alloc(isa_chipset_tag_t, int, int, int *);
void	*isa_intr_establish(isa_chipset_tag_t ic, int irq, int type,
	    int level, int (*ih_fun)(void *), void *ih_arg, char *);
void	isa_intr_disestablish(isa_chipset_tag_t ic, void *handler);
int	isa_mem_alloc(bus_space_tag_t, bus_size_t, bus_size_t,
	    bus_addr_t, int, bus_addr_t *, bus_space_handle_t *);
void	isa_mem_free(bus_space_tag_t, bus_space_handle_t, bus_size_t);

int isa_intr_check(isa_chipset_tag_t, int, int);

/*
 * for ACPI code
 */

void	isa_reinit_irq(void);

/*
 * ALL OF THE FOLLOWING ARE MACHINE-DEPENDENT, AND SHOULD NOT BE USED
 * BY PORTABLE CODE.
 */

extern struct x86_bus_dma_tag isa_bus_dma_tag;

/*
 * Cookie used by ISA dma.  A pointer to one of these it stashed in
 * the DMA map.
 */
struct x86_isa_dma_cookie {
	int	id_flags;		/* flags; see below */

	void	*id_origbuf;		/* pointer to orig buffer if
					   bouncing */
	bus_size_t id_origbuflen;	/* ...and size */

	void	*id_bouncebuf;		/* pointer to the bounce buffer */
	bus_size_t id_bouncebuflen;	/* ...and size */
	int	id_nbouncesegs;		/* number of valid bounce segs */
	bus_dma_segment_t id_bouncesegs[0]; /* array of bounce buffer
					       physical memory segments */
};

/* id_flags */
#define	ID_MIGHT_NEED_BOUNCE	0x01	/* map could need bounce buffers */
#define	ID_HAS_BOUNCE		0x02	/* map currently has bounce buffers */
#define	ID_IS_BOUNCING		0x04	/* map is bouncing current xfer */



/*
 * XXX Various seemingly PC-specific constants, some of which may be
 * unnecessary anyway.
 */

/*
 * RAM Physical Address Space (ignoring the above mentioned "hole")
 */
#define	RAM_BEGIN	0x0000000	/* Start of RAM Memory */
#define	RAM_END		0x1000000	/* End of RAM Memory */
#define	RAM_SIZE	(RAM_END - RAM_BEGIN)

/*
 * Oddball Physical Memory Addresses
 */
#define	COMPAQ_RAMRELOC	0x80c00000	/* Compaq RAM relocation/diag */
#define	COMPAQ_RAMSETUP	0x80c00002	/* Compaq RAM setup */
#define	WEITEK_FPU	0xC0000000	/* WTL 2167 */
#define	CYRIX_EMC	0xC0000000	/* Cyrix EMC */

/*
 * stuff that used to be in pccons.c
 */
#define	MONO_BASE	0x3B4
#define	MONO_BUF	0xB0000
#define	CGA_BASE	0x3D4
#define	CGA_BUF		0xB8000

/*
 * Variables and macros to deal with the ISA I/O hole.
 * XXX These should be converted to machine- and bus-mapping-independent
 * function definitions, invoked through the softc.
 */

extern u_long atdevbase;           /* kernel virtual address of "hole" */

/*
 * Given a kernel virtual address for some location
 * in the "hole" I/O space, return a physical address.
 */
#define ISA_PHYSADDR(v) ((void *) ((u_long)(v) - atdevbase + IOM_BEGIN))

/*
 * Given a physical address in the "hole",
 * return a kernel virtual address.
 */
#define ISA_HOLE_VADDR(p)  ((void *) ((u_long)(p) - IOM_BEGIN + atdevbase))


/*
 * Miscellanous functions.
 */
void sysbeep(int, int);		/* beep with the system speaker */

#endif /* _I386_ISA_MACHDEP_H_ XXX */

/*	$OpenBSD: isavar.h,v 1.22 1997/10/13 14:46:45 pefo Exp $	*/
/*	$NetBSD: isavar.h,v 1.24 1996/10/21 22:41:11 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Chris G. Demetriou
 * Copyright (c) 1992 Berkeley Software Design, Inc.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 * 4. The name of Berkeley Software Design must not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI Id: isavar.h,v 1.5 1992/12/01 18:06:00 karels Exp 
 */

#ifndef _DEV_ISA_ISAVAR_H_
#define	_DEV_ISA_ISAVAR_H_

/*
 * Definitions for ISA autoconfiguration.
 */

#include <sys/queue.h>
#include <machine/bus.h>

/* 
 * Structures and definitions needed by the machine-dependent header.
 */
struct isabus_attach_args;

#if (alpha + amiga + i386 + arc + wgrisc + power4e != 1)
ERROR: COMPILING FOR UNSUPPORTED MACHINE, OR MORE THAN ONE.
#endif
#if alpha
#include <alpha/isa/isa_machdep.h>
#endif
#if amiga
#include <amiga/isa/isa_machdep.h>
#endif
#if i386
#include <i386/isa/isa_machdep.h>
#endif
#if arc
#include <arc/isa/isa_machdep.h>
#endif
#if wgrisc
#include <wgrisc/isa/isa_machdep.h>
#endif
#if power4e
#include <powerpc/isa/isa_machdep.h>
#endif

/*
 * ISA bus attach arguments
 */
struct isabus_attach_args {
	char	*iba_busname;		/* XXX should be common */
	bus_space_tag_t iba_iot;	/* isa i/o space tag */
	bus_space_tag_t iba_memt;	/* isa mem space tag */
	isa_chipset_tag_t iba_ic;
};

/*
 * ISA driver attach arguments
 */
struct isa_attach_args {
	bus_space_tag_t ia_iot;		/* isa i/o space tag */
	bus_space_tag_t ia_memt;	/* isa mem space tag */

	isa_chipset_tag_t ia_ic;

	int	ia_iobase;		/* base i/o address */
	int	ia_iosize;		/* span of ports used */
	int	ia_irq;			/* interrupt request */
	int	ia_drq;			/* DMA request */
	int	ia_maddr;		/* physical i/o mem addr */
	u_int	ia_msize;		/* size of i/o memory */
	void	*ia_aux;		/* driver specific */

	bus_space_handle_t ia_delaybah;	/* i/o handle for `delay port' */

	/* XXX need fixes, some are duplicated */
	/* begin isapnp section */
	int id;				/* logical device ID */
	int comp_id;			/* compatible device ID */
	int csn;			/* card selection number */
	int ldn;			/* logical device number */
	struct {
		int num;
		int type;
	} irq[2];
	int drq[2];
	int port[8];
	struct {
		int base;
		int control;
		int range;
	} mem[4];
	/* end isapnp stuff */
};

#define	IOBASEUNK	-1		/* i/o address is unknown */
#define	IRQUNK		-1		/* interrupt request line is unknown */
#define	DRQUNK		-1		/* DMA request line is unknown */
#define	MADDRUNK	-1		/* shared memory address is unknown */

/*
 * Per-device ISA variables
 */
struct isadev {
	struct  device *id_dev;		/* back pointer to generic */
	TAILQ_ENTRY(isadev)
		id_bchain;		/* bus chain */
};

/*
 * ISA master bus
 */
struct isa_softc {
	struct	device sc_dev;		/* base device */
	TAILQ_HEAD(, isadev)
		sc_subdevs;		/* list of all children */

	bus_space_tag_t sc_iot;		/* isa io space tag */
	bus_space_tag_t sc_memt;	/* isa mem space tag */

	isa_chipset_tag_t sc_ic;

	/*
	 * This i/o handle is used to map port 0x84, which is
	 * read to provide a 1.25us delay.  This access handle
	 * is mapped in isaattach(), and exported to drivers
	 * via isa_attach_args.
	 */
	bus_space_handle_t   sc_delaybah;

	/*
	 * This points to the isapnp_softc structure that holds
	 * information of PnP devices on the ISA bus.
	 */
	void *pnpsc;
};

#define		cf_iobase		cf_loc[0]
#define		cf_iosize		cf_loc[1]
#define		cf_maddr		cf_loc[2]
#define		cf_msize		cf_loc[3]
#define		cf_irq			cf_loc[4]
#define		cf_drq			cf_loc[5]
#define		cf_pnpid		cf_loc[6]

/*
 * ISA interrupt handler manipulation.
 * 
 * To establish an ISA interrupt handler, a driver calls isa_intr_establish()
 * with the interrupt number, type, level, function, and function argument of
 * the interrupt it wants to handle.  Isa_intr_establish() returns an opaque
 * handle to an event descriptor if it succeeds, and invokes panic() if it
 * fails.  (XXX It should return NULL, then drivers should handle that, but
 * what should they do?)  Interrupt handlers should return 0 for "interrupt
 * not for me", 1  for "I took care of it", or -1 for "I guess it was mine,
 * but I wasn't expecting it."
 *
 * To remove an interrupt handler, the driver calls isa_intr_disestablish() 
 * with the handle returned by isa_intr_establish() for that handler.
 */

/* ISA interrupt sharing types */
char	*isa_intr_typename __P((int type));

void	isascan __P((struct device *parent, void *match));
int	isaprint __P((void *, const char *));

#ifdef NEWCONFIG
/*
 * Establish a device as being on the ISA bus (XXX NOT IMPLEMENTED).
 */
void isa_establish __P((struct isadev *, struct device *));
#endif

#endif /* _DEV_ISA_ISAVAR_H_ */

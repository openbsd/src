/*	$OpenBSD: pcivar.h,v 1.14 1999/02/01 16:35:49 pefo Exp $	*/
/*	$NetBSD: pcivar.h,v 1.23 1997/06/06 23:48:05 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
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

#ifndef _DEV_PCI_PCIVAR_H_
#define	_DEV_PCI_PCIVAR_H_

/*
 * Definitions for PCI autoconfiguration.
 *
 * This file describes types and functions which are used for PCI
 * configuration.  Some of this information is machine-specific, and is
 * provided by pci_machdep.h.
 */

#include <machine/bus.h>
#include <dev/pci/pcireg.h>

/*
 * Structures and definitions needed by the machine-dependent header.
 */
typedef u_int32_t pcireg_t;		/* configuration space register XXX */
struct pcibus_attach_args;

/*
 * Machine-dependent definitions.
 */
#if (alpha + atari + i386 + arc + powerpc + galileo != 1)
ERROR: COMPILING FOR UNSUPPORTED MACHINE, OR MORE THAN ONE.
#endif
#if alpha
#include <alpha/pci/pci_machdep.h>
#endif
#if atari
#include <atari/pci/pci_machdep.h>
#endif
#if i386
#include <i386/pci/pci_machdep.h>
#endif
#if arc
#include <arc/pci/pci_machdep.h>
#endif
#if powerpc
#include <powerpc/pci/pci_machdep.h>
#endif
#if galileo
#include <galileo/pci/pci_machdep.h>
#endif

/*
 * PCI bus attach arguments.
 */
struct pcibus_attach_args {
	char	*pba_busname;		/* XXX should be common */
	bus_space_tag_t pba_iot;	/* pci i/o space tag */
	bus_space_tag_t pba_memt;	/* pci mem space tag */
	bus_dma_tag_t pba_dmat;		/* DMA tag */
	pci_chipset_tag_t pba_pc;

	int		pba_bus;	/* PCI bus number */

	/*
	 * Interrupt swizzling information.  These fields
	 * are only used by secondary busses.
	 */
	u_int		pba_intrswiz;	/* how to swizzle pins */
	pcitag_t	pba_intrtag;	/* intr. appears to come from here */
};

/*
 * PCI device attach arguments.
 */
struct pci_attach_args {
	bus_space_tag_t pa_iot;		/* pci i/o space tag */
	bus_space_tag_t pa_memt;	/* pci mem space tag */
	bus_dma_tag_t pa_dmat;		/* DMA tag */
	pci_chipset_tag_t pa_pc;

	u_int		pa_device;
	u_int		pa_function;
	pcitag_t	pa_tag;
	pcireg_t	pa_id, pa_class;

	/*
	 * Interrupt information.
	 *
	 * "Intrline" is used on systems whose firmware puts
	 * the right routing data into the line register in
	 * configuration space.  The rest are used on systems
	 * that do not.
	 */
	u_int		pa_intrswiz;	/* how to swizzle pins if ppb */
	pcitag_t	pa_intrtag;	/* intr. appears to come from here */
	pci_intr_pin_t	pa_intrpin;	/* intr. appears on this pin */
	pci_intr_line_t	pa_intrline;	/* intr. routing information */
};

/*
 * Locators devices that attach to 'pcibus', as specified to config.
 */
#define	pcibuscf_bus		cf_loc[0]
#define	PCIBUS_UNK_BUS		-1		/* wildcarded 'bus' */

/*
 * Locators for PCI devices, as specified to config.
 */
#define	pcicf_dev		cf_loc[0]
#define	PCI_UNK_DEV		-1		/* wildcarded 'dev' */

#define	pcicf_function		cf_loc[1]
#define	PCI_UNK_FUNCTION	-1		/* wildcarded 'function' */

/*
 * Configuration space access and utility functions.  (Note that most,
 * e.g. make_tag, conf_read, conf_write are declared by pci_machdep.h.)
 */
int	pci_io_find __P((pci_chipset_tag_t, pcitag_t, int, bus_addr_t *,
	    bus_size_t *));
int	pci_mem_find __P((pci_chipset_tag_t, pcitag_t, int, bus_addr_t *,
	    bus_size_t *, int *));

/*
 * Helper functions for autoconfiguration.
 */
void	pci_devinfo __P((pcireg_t, pcireg_t, int, char *));
void	set_pci_isa_bridge_callback __P((void (*)(void *), void *));

#endif /* _DEV_PCI_PCIVAR_H_ */

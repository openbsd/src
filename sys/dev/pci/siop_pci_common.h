/*	$OpenBSD: siop_pci_common.h,v 1.3 2001/04/15 06:01:31 krw Exp $ */
/*	$NetBSD: siop_pci_common.h,v 1.2 2000/10/23 14:57:23 bouyer Exp $	*/

/*
 * Copyright (c) 2000 Manuel Bouyer.
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
 *	This product includes software developed by Manuel Bouyer
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

/* common functions for the siop and esiop pci front ends */

/* structure describing each chip */
struct siop_product_desc {
	u_int32_t product;
	int	revision;
	int	features; /* features are defined in siopvar.h */
	u_int8_t maxburst;
	u_int8_t maxoff;  /* maximum supported offset */
	u_int8_t clock_div; /* clock divider to use for async. logic */
	u_int8_t scf_index; /* Index into a period_factor_to_scf.scf */
#define     SF_CLOCK_2500	0
#define	    SF_CLOCK_1250	1
#define	    SF_CLOCK_625	2
	int 	ram_size; /* size of RAM, if appropriate */
};

const struct siop_product_desc * siop_lookup_product __P((u_int32_t, int));

/* Driver internal state */
struct siop_pci_softc {
	struct siop_softc siop;
	pci_chipset_tag_t	sc_pc;	/* PCI registers info */
	pcitag_t		sc_tag;
	void			*sc_ih;	/* PCI interrupt handle */
	const struct siop_product_desc *sc_pp; /* Adapter description */
};

int siop_pci_attach_common __P((struct siop_pci_softc *,
	struct pci_attach_args *));
void siop_pci_reset __P((struct siop_softc *));

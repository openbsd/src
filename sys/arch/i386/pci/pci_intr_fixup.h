/*	$OpenBSD: pci_intr_fixup.h,v 1.3 2000/03/28 03:37:59 mickey Exp $	 */
/*	$NetBSD: pci_intr_fixup.h,v 1.1 1999/11/17 01:20:38 thorpej Exp $  */

/*
 * Copyright (c) 1999, by UCHIYAMA Yasushi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

typedef void *pciintr_icu_handle_t;

struct pciintr_icu {
	int	(*pi_getclink) __P((pciintr_icu_handle_t, int, int *));
	int	(*pi_get_intr) __P((pciintr_icu_handle_t, int, int *));
	int	(*pi_set_intr) __P((pciintr_icu_handle_t, int, int));
	int	(*pi_get_trigger) __P((pciintr_icu_handle_t, int, int *));
	int	(*pi_set_trigger) __P((pciintr_icu_handle_t, int, int));
};

typedef const struct pciintr_icu *pciintr_icu_tag_t;

#define	pciintr_icu_getclink(t, h, link, pirqp)				\
	(*(t)->pi_getclink)((h), (link), (pirqp))
#define	pciintr_icu_get_intr(t, h, pirq, irqp)				\
	(*(t)->pi_get_intr)((h), (pirq), (irqp))
#define	pciintr_icu_set_intr(t, h, pirq, irq)				\
	(*(t)->pi_set_intr)((h), (pirq), (irq))
#define	pciintr_icu_get_trigger(t, h, irq, triggerp)			\
	(*(t)->pi_get_trigger)((h), (irq), (triggerp))
#define	pciintr_icu_set_trigger(t, h, irq, trigger)			\
	(*(t)->pi_set_trigger)((h), (irq), (trigger))

int pci_intr_fixup __P((pci_chipset_tag_t, bus_space_tag_t, u_int16_t *));

/*
 * Init functions for our known PCI ICUs.
 */
int	piix_init __P((pci_chipset_tag_t, bus_space_tag_t, pcitag_t,
	    pciintr_icu_tag_t *, pciintr_icu_handle_t *));
int	opti82c558_init __P((pci_chipset_tag_t, bus_space_tag_t, pcitag_t,
	    pciintr_icu_tag_t *, pciintr_icu_handle_t *));
int	opti82c700_init __P((pci_chipset_tag_t, bus_space_tag_t, pcitag_t,
	    pciintr_icu_tag_t *, pciintr_icu_handle_t *));
int	via82c586_init __P((pci_chipset_tag_t, bus_space_tag_t, pcitag_t,
	    pciintr_icu_tag_t *, pciintr_icu_handle_t *));
int	sis85c503_init __P((pci_chipset_tag_t, bus_space_tag_t, pcitag_t,
	    pciintr_icu_tag_t *, pciintr_icu_handle_t *));

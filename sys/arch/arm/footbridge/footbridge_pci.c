/*	$OpenBSD: footbridge_pci.c,v 1.4 2004/08/17 19:40:45 drahn Exp $	*/
/*	$NetBSD: footbridge_pci.c,v 1.4 2001/09/05 16:17:35 matt Exp $	*/

/*
 * Copyright (c) 1997,1998 Mark Brinicombe.
 * Copyright (c) 1997,1998 Causality Limited
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#define _ARM32_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <arm/footbridge/dc21285reg.h>
#include <arm/footbridge/dc21285mem.h>

#include "isa.h"
#if NISA > 0
#include <dev/isa/isavar.h>
#endif

void		footbridge_pci_attach_hook (struct device *,
		    struct device *, struct pcibus_attach_args *);
int		footbridge_pci_bus_maxdevs (void *, int);
pcitag_t	footbridge_pci_make_tag (void *, int, int, int);
void		footbridge_pci_decompose_tag (void *, pcitag_t, int *,
		    int *, int *);
pcireg_t	footbridge_pci_conf_read (void *, pcitag_t, int);
void		footbridge_pci_conf_write (void *, pcitag_t, int,
		    pcireg_t);
int		footbridge_pci_intr_map (struct pci_attach_args *,
		    pci_intr_handle_t *);
const char	*footbridge_pci_intr_string (void *, pci_intr_handle_t);
void		*footbridge_pci_intr_establish (void *, pci_intr_handle_t,
		    int, int (*)(void *), void *, char *);
void		footbridge_pci_intr_disestablish (void *, void *);


struct arm32_pci_chipset footbridge_pci_chipset = {
	NULL,	/* conf_v */
#ifdef netwinder
	netwinder_pci_attach_hook,
#else
	footbridge_pci_attach_hook,
#endif
	footbridge_pci_bus_maxdevs,
	footbridge_pci_make_tag,
	footbridge_pci_decompose_tag,
	footbridge_pci_conf_read,
	footbridge_pci_conf_write,
	NULL,	/* intr_v */
	footbridge_pci_intr_map,
	footbridge_pci_intr_string,
	footbridge_pci_intr_establish,
	footbridge_pci_intr_disestablish
};

/*
 * PCI doesn't have any special needs; just use the generic versions
 * of these functions.
 */
struct arm32_bus_dma_tag footbridge_pci_bus_dma_tag = {
	0,
	0,
	NULL,
	_bus_dmamap_create, 
	_bus_dmamap_destroy,
	_bus_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	_bus_dmamap_unload,
	_bus_dmamap_sync,
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

/*
 * Currently we only support 12 devices as we select directly in the
 * type 0 config cycle
 * (See conf_{read,write} for more detail
 */
#define MAX_PCI_DEVICES	21

/*static int
pci_intr(void *arg)
{
	printf("pci int %x\n", (int)arg);
	return(0);
}*/


void
footbridge_pci_attach_hook(parent, self, pba)
	struct device *parent, *self;
	struct pcibus_attach_args *pba;
{
#ifdef PCI_DEBUG
	printf("footbridge_pci_attach_hook()\n");
#endif

/*	intr_claim(18, IPL_NONE, "pci int 0", pci_intr, (void *)0x10000);
	intr_claim(8, IPL_NONE, "pci int 1", pci_intr, (void *)0x10001);
	intr_claim(9, IPL_NONE, "pci int 2", pci_intr, (void *)0x10002);
	intr_claim(11, IPL_NONE, "pci int 3", pci_intr, (void *)0x10003);*/
}

int
footbridge_pci_bus_maxdevs(pcv, busno)
	void *pcv;
	int busno;
{
#ifdef PCI_DEBUG
	printf("footbridge_pci_bus_maxdevs(pcv=%p, busno=%d)\n", pcv, busno);
#endif
	return(MAX_PCI_DEVICES);
}

pcitag_t
footbridge_pci_make_tag(pcv, bus, device, function)
	void *pcv;
	int bus, device, function;
{
#ifdef PCI_DEBUG
	printf("footbridge_pci_make_tag(pcv=%p, bus=%d, device=%d, function=%d)\n",
	    pcv, bus, device, function);
#endif
	return ((bus << 16) | (device << 11) | (function << 8));
}

void
footbridge_pci_decompose_tag(pcv, tag, busp, devicep, functionp)
	void *pcv;
	pcitag_t tag;
	int *busp, *devicep, *functionp;
{
#ifdef PCI_DEBUG
	printf("footbridge_pci_decompose_tag(pcv=%p, tag=0x%08x, bp=%x, dp=%x, fp=%x)\n",
	    pcv, tag, busp, devicep, functionp);
#endif

	if (busp != NULL)
		*busp = (tag >> 16) & 0xff;
	if (devicep != NULL)
		*devicep = (tag >> 11) & 0x1f;
	if (functionp != NULL)
		*functionp = (tag >> 8) & 0x7;
}

pcireg_t
footbridge_pci_conf_read(pcv, tag, reg)
	void *pcv;
	pcitag_t tag;
	int reg;
{
	int bus, device, function;
	u_int address;
	pcireg_t data;

	footbridge_pci_decompose_tag(pcv, tag, &bus, &device, &function);
	if (bus == 0)
		/* Limited to 12 devices or we exceed type 0 config space */
		address = DC21285_PCI_TYPE_0_CONFIG_VBASE | (3 << 22) | (device << 11);
	else
		address = DC21285_PCI_TYPE_1_CONFIG_VBASE | (device << 11) |
		    (bus << 16);

	address |= (function << 8) | reg;

	data = *((unsigned int *)address);
#ifdef PCI_DEBUG
	printf("footbridge_pci_conf_read(pcv=%p tag=0x%08x reg=0x%02x)=0x%08x\n",
	    pcv, tag, reg, data);
#endif
	return(data);
}

void
footbridge_pci_conf_write(pcv, tag, reg, data)
	void *pcv;
	pcitag_t tag;
	int reg;
	pcireg_t data;
{
	int bus, device, function;
	u_int address;

	footbridge_pci_decompose_tag(pcv, tag, &bus, &device, &function);
	if (bus == 0)
		address = DC21285_PCI_TYPE_0_CONFIG_VBASE | (3 << 22) | (device << 11);
	else
		address = DC21285_PCI_TYPE_1_CONFIG_VBASE | (device << 11) |
		    (bus << 16);

	address |= (function << 8) | reg;

#ifdef PCI_DEBUG
	printf("footbridge_pci_conf_write(pcv=%p tag=0x%08x reg=0x%02x, 0x%08x)\n",
	    pcv, tag, reg, data);
#endif

	*((unsigned int *)address) = data;
}

int
footbridge_pci_intr_map(pa, ihp)
	struct pci_attach_args *pa;
	pci_intr_handle_t *ihp;
{
	int pin = pa->pa_intrpin, line = pa->pa_intrline;
	int intr = -1;

#ifdef PCI_DEBUG
	void *pcv = pa->pa_pc;
	pcitag_t intrtag = pa->pa_intrtag;
	int bus, device, function;

	footbridge_pci_decompose_tag(pcv, intrtag, &bus, &device, &function);
	printf("footbride_pci_intr_map: pcv=%p, tag=%08lx pin=%d line=%d dev=%d\n",
	    pcv, intrtag, pin, line, device);
#endif

	/*
	 * Only the line is used to map the interrupt.
	 * The firmware is expected to setup up the interrupt
	 * line as seen from the CPU
	 * This means the firmware deals with the interrupt rotation
	 * between slots etc.
	 *
	 * Perhaps the firmware should also to the final mapping
	 * to a 21285 interrupt bit so the code below would be
	 * completely MI.
	 */

	switch (line) {
	case PCI_INTERRUPT_PIN_NONE:
	case 0xff:
		/* No IRQ */
		printf("pci_intr_map: no mapping for pin %c\n", '@' + pin);
		*ihp = -1;
		return(1);
		break;
#ifdef __cats__
	/* This is machine dependant and needs to be moved */
	case PCI_INTERRUPT_PIN_A:
		intr = IRQ_PCI;
		break;
	case PCI_INTERRUPT_PIN_B:
		intr = IRQ_IN_L0;
		break;
	case PCI_INTERRUPT_PIN_C:
		intr = IRQ_IN_L1;
		break;
	case PCI_INTERRUPT_PIN_D:
		intr = IRQ_IN_L3;
		break;
#endif
	default:
		/*
		 * Experimental firmware feature ...
		 *
		 * If the interrupt line is in the range 0x80 to 0x8F
		 * then the lower 4 bits indicate the ISA interrupt
		 * bit that should be used.
		 * If the interrupt line is in the range 0x40 to 0x5F
		 * then the lower 5 bits indicate the actual DC21285
		 * interrupt bit that should be used.
		 */

		if (line >= 0x40 && line <= 0x5f)
			intr = line & 0x1f;
		else if (line >= 0x80 && line <= 0x8f)
			intr = line;
		else {
	                printf("footbridge_pci_intr_map: out of range interrupt"
			       "pin %d line %d (%#x)\n", pin, line, line);
			*ihp = -1;
			return(1);
		}
		break;
	}

#ifdef PCI_DEBUG
	printf("pin %d, line %d mapped to int %d\n", pin, line, intr);
#endif

	*ihp = intr;
	return(0);
}


const char *
footbridge_pci_intr_string(pcv, ih)
	void *pcv;
	pci_intr_handle_t ih;
{
	static char irqstr[32];

#ifdef PCI_DEBUG
	printf("footbridge_pci_intr_string(pcv=0x%p, ih=0x%lx)\n", pcv, ih);
#endif
	if (ih == 0)
		panic("footbridge_pci_intr_string: bogus handle 0x%lx", ih);

#if NISA > 0
	if (ih >= 0x80 && ih <= 0x8f) {
		snprintf(irqstr, sizeof(irqstr), "isairq %ld", (ih & 0x0f));
		return(irqstr);
	}
#endif
	snprintf(irqstr, sizeof(irqstr), "irq %ld", ih);
	return(irqstr);	
}


void *
footbridge_pci_intr_establish(pcv, ih, level, func, arg, name)
	void *pcv;
	pci_intr_handle_t ih;
	int level, (*func) (void *);
	void *arg;
	char *name;
{
	void *intr;

#ifdef PCI_DEBUG
	printf("footbridge_pci_intr_establish(pcv=%p, ih=0x%lx, level=%d, func=%p, arg=%p)\n",
	    pcv, ih, level, func, arg);
#endif

#if NISA > 0
	/*
	 * XXX the IDE driver will attach the interrupts in compat mode and
	 * thus we need to fail this here.
	 * This assumes that the interrupts are 14 and 15 which they are for
	 * IDE compat mode.
	 * Really the firmware should make this clear in the interrupt reg.
	 */
	if (ih >= 0x80 && ih <= 0x8d) {
		intr = isa_intr_establish(NULL, (ih & 0x0f), IST_EDGE,
		    level, func, arg, name);
	} else
#endif
	intr = footbridge_intr_claim(ih, level, name, func, arg);

	return(intr);
}

void
footbridge_pci_intr_disestablish(pcv, cookie)
	void *pcv;
	void *cookie;
{
#ifdef PCI_DEBUG
	printf("footbridge_pci_intr_disestablish(pcv=%p, cookie=0x%x)\n",
	    pcv, cookie);
#endif

	footbridge_intr_disestablish(cookie);
}

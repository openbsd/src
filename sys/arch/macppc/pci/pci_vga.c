/*	$OpenBSD: pci_vga.c,v 1.1 2001/09/01 15:55:17 drahn Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

struct pcivga_softc {
	struct device      sc_dev;
	void 		  *sc_ih;
	pci_chipset_tag_t  sc_pc;
	bus_space_tag_t    sc_memt;
	bus_space_handle_t sc_iomem;
	bus_space_handle_t sc_vmem;
};

int pcivga_probe __P((struct device *, void *, void *));
void pcivga_attach __P((struct device *, struct device *, void *));

struct cfattach pcivga_ca = {
	sizeof(struct pcivga_softc), pcivga_probe, pcivga_attach
};

struct cfdriver pcivga_cd = {
	NULL, "pcivga", DV_DULL
};

/*
 * PCI probe
 */
int
pcivga_probe(parent, cf, aux)
	struct device *parent;
	void *cf; 
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if(PCI_CLASS(pa->pa_class) != PCI_CLASS_DISPLAY ||
	   PCI_SUBCLASS(pa->pa_class) != PCI_SUBCLASS_DISPLAY_VGA) {
		return(0);
	}
	return(1);
}


void
pcivga_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct pcivga_softc *sc = (void *)self;
	bus_size_t msize;
	bus_addr_t maddr;
	int cacheable;
	int class;

	sc->sc_memt = pa->pa_memt;
	sc->sc_pc = pa->pa_pc;

	printf(": Generic VGA controller\n");

	/* Map Control register aperture (0x10) */
	if(pci_mem_find(pa->pa_pc, pa->pa_tag, 0x10,
		  &maddr, &msize, &cacheable) != 0) {
		printf("%s: can't find PCI card memory", self->dv_xname);
		return;
	}
	if(bus_space_map(sc->sc_memt, maddr, msize, 0, &sc->sc_iomem) != 0) {
		printf("%s: couldn't map ioreg region\n", self->dv_xname);
		return;
	}

#if 0
	/* Map Video memory aperture (0x14) */
	if(pci_mem_find(pa->pa_pc, pa->pa_tag, 0x14,
		  &maddr, &msize, &cacheable) != 0) {
		printf("%s: can't find PCI card memory", self->dv_xname);
		return;
	}
	if(bus_space_map(sc->sc_memt, maddr, msize, 0, &sc->sc_vmem) != 0) {
		printf("%s: couldn't map video region\n", self->dv_xname);
		return;
	}
#endif

printf("IO=%x\n", sc->sc_iomem);
class = pci_conf_read (pa->pa_pc, pa->pa_tag, 0x40);
pci_conf_write(pa->pa_pc, pa->pa_tag, 0x40, class & ~0x40000100);
class = pci_conf_read (pa->pa_pc, pa->pa_tag, 0x40);
printf("Opt=%x\n", class);
class = pci_conf_read (pa->pa_pc, pa->pa_tag, 0x4);
printf("Devctl=%x\n", class);
class = pci_conf_read (pa->pa_pc, pa->pa_tag, 0x30);
printf("Rombase=%x\n", class);

mdbpanic();

}

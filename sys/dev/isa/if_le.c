/*	$NetBSD: if_le.c,v 1.37 1995/11/25 01:24:00 cgd Exp $	*/

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/pio.h>

#include "isa.h"
#include "pci.h"

#if NISA > 0
#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <i386/isa/isa_machdep.h>
#endif

#if NPCI > 0
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#endif

#include <dev/isa/if_levar.h>
#include <dev/ic/am7990reg.h>
#define LE_NEED_BUF_CONTIG
#include <dev/ic/am7990var.h>

char *card_type[] = {"unknown", "BICC Isolan", "NE2100", "DEPCA", "PCnet-ISA", "PCnet-PCI"};

#define	LE_SOFTC(unit)	lecd.cd_devs[unit]
#define	LE_DELAY(x)	delay(x)

int leprobe __P((struct device *, void *, void *));
int depca_probe __P((struct le_softc *, struct isa_attach_args *));
int ne2100_probe __P((struct le_softc *, struct isa_attach_args *));
int bicc_probe __P((struct le_softc *, struct isa_attach_args *));
int lance_probe __P((struct le_softc *));
void leattach __P((struct device *, struct device *, void *));
int leintr __P((void *));
int leintredge __P((void *));
void leshutdown __P((void *));

struct cfdriver lecd = {
	NULL, "le", leprobe, leattach, DV_IFNET, sizeof(struct le_softc)
};

integrate void
lewrcsr(sc, port, val)
	struct le_softc *sc;
	u_int16_t port, val;
{

	outw(sc->sc_rap, port);
	outw(sc->sc_rdp, val);
}

integrate u_int16_t
lerdcsr(sc, port)
	struct le_softc *sc;
	u_int16_t port;
{
	u_int16_t val;

	outw(sc->sc_rap, port);
	val = inw(sc->sc_rdp);
	return (val);
}

int
leprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct le_softc *sc = match;
	extern struct cfdriver isacd, pcicd;

#if NISA > 0
	if (parent->dv_cfdata->cf_driver == &isacd) {
		struct isa_attach_args *ia = aux;

		if (bicc_probe(sc, ia))
			return (1);
		if (ne2100_probe(sc, ia))
			return (1);
		if (depca_probe(sc, ia))
			return (1);
	}
#endif

#if NPCI > 0
	if (parent->dv_cfdata->cf_driver == &pcicd) {
		struct pci_attach_args *pa = aux;

		if (pa->pa_id == 0x20001022)
			return (1);
	}
#endif

	return (0);
}

#if NISA > 0
int
depca_probe(sc, ia)
	struct le_softc *sc;
	struct isa_attach_args *ia;
{
	int iobase = ia->ia_iobase, port;
	u_long sum, rom_sum;
	u_char x;
	int i;

	sc->sc_rap = iobase + DEPCA_RAP;
	sc->sc_rdp = iobase + DEPCA_RDP;
	sc->sc_card = DEPCA;

	if (lance_probe(sc) == 0)
		return 0;

	outb(iobase + DEPCA_CSR, DEPCA_CSR_DUM);

	/*
	 * Extract the physical MAC address from the ROM.
	 *
	 * The address PROM is 32 bytes wide, and we access it through
	 * a single I/O port.  On each read, it rotates to the next
	 * position.  We find the ethernet address by looking for a
	 * particular sequence of bytes (0xff, 0x00, 0x55, 0xaa, 0xff,
	 * 0x00, 0x55, 0xaa), and then reading the next 8 bytes (the
	 * ethernet address and a checksum).
	 *
	 * It appears that the PROM can be at one of two locations, so
	 * we just try both.
	 */
	port = iobase + DEPCA_ADP;
	for (i = 0; i < 32; i++)
		if (inb(port) == 0xff && inb(port) == 0x00 &&
		    inb(port) == 0x55 && inb(port) == 0xaa &&
		    inb(port) == 0xff && inb(port) == 0x00 &&
		    inb(port) == 0x55 && inb(port) == 0xaa)
			goto found;
	port = iobase + DEPCA_ADP + 1;
	for (i = 0; i < 32; i++)
		if (inb(port) == 0xff && inb(port) == 0x00 &&
		    inb(port) == 0x55 && inb(port) == 0xaa &&
		    inb(port) == 0xff && inb(port) == 0x00 &&
		    inb(port) == 0x55 && inb(port) == 0xaa)
			goto found;
	printf("%s: address not found\n", sc->sc_dev.dv_xname);
	return 0;

found:
	for (i = 0; i < sizeof(sc->sc_arpcom.ac_enaddr); i++)
		sc->sc_arpcom.ac_enaddr[i] = inb(port);

#if 0
	sum =
	    (sc->sc_arpcom.ac_enaddr[0] <<  2) +
	    (sc->sc_arpcom.ac_enaddr[1] << 10) +
	    (sc->sc_arpcom.ac_enaddr[2] <<  1) +
	    (sc->sc_arpcom.ac_enaddr[3] <<  9) +
	    (sc->sc_arpcom.ac_enaddr[4] <<  0) +
	    (sc->sc_arpcom.ac_enaddr[5] <<  8);
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);

	rom_sum = inb(port);
	rom_sum |= inb(port) << 8;

	if (sum != rom_sum) {
		printf("%s: checksum mismatch; calculated %04x != read %04x",
		    sc->sc_dev.dv_xname, sum, rom_sum);
		return 0;
	}
#endif

	outb(iobase + DEPCA_CSR, DEPCA_CSR_NORMAL);

	ia->ia_iosize = 16;
	ia->ia_drq = DRQUNK;
	return 1;
}

int
ne2100_probe(sc, ia)
	struct le_softc *sc;
	struct isa_attach_args *ia;
{
	int iobase = ia->ia_iobase;
	int i;

	sc->sc_rap = iobase + NE2100_RAP;
	sc->sc_rdp = iobase + NE2100_RDP;
	sc->sc_card = NE2100;

	if (lance_probe(sc) == 0)
		return 0;

	/*
	 * Extract the physical MAC address from the ROM.
	 */
	for (i = 0; i < sizeof(sc->sc_arpcom.ac_enaddr); i++)
		sc->sc_arpcom.ac_enaddr[i] = inb(iobase + i);

	ia->ia_iosize = 24;
	return 1;
}

int
bicc_probe(sc, ia)
	struct le_softc *sc;
	struct isa_attach_args *ia;
{
	int iobase = ia->ia_iobase;
	int i;

	sc->sc_rap = iobase + BICC_RAP;
	sc->sc_rdp = iobase + BICC_RDP;
	sc->sc_card = BICC;

	if (lance_probe(sc) == 0)
		return 0;

	/*
	 * Extract the physical MAC address from the ROM.
	 */
	for (i = 0; i < sizeof(sc->sc_arpcom.ac_enaddr); i++)
		sc->sc_arpcom.ac_enaddr[i] = inb(iobase + i * 2);

	ia->ia_iosize = 16;
	return 1;
}

/*
 * Determine which chip is present on the card.
 */
int
lance_probe(sc)
	struct le_softc *sc;
{

	/* Stop the LANCE chip and put it in a known state. */
	lewrcsr(sc, LE_CSR0, LE_C0_STOP);
	LE_DELAY(100);

	if (lerdcsr(sc, LE_CSR0) != LE_C0_STOP)
		return 0;

	lewrcsr(sc, LE_CSR3, sc->sc_conf3);
	return 1;
}
#endif

void
leattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct le_softc *sc = (void *)self;
	extern struct cfdriver isacd, pcicd;

#if NPCI > 0
	if (parent->dv_cfdata->cf_driver == &pcicd) {
		struct pci_attach_args *pa = aux;
		int iobase;

		if (pa->pa_id == 0x20001022) {
			int i;

			if (pci_map_io(pa->pa_tag, 0x10, &iobase))
				return;

			sc->sc_rap = iobase + NE2100_RAP;
			sc->sc_rdp = iobase + NE2100_RDP;
			sc->sc_card = PCnet_PCI;

			/*
			 * Extract the physical MAC address from the ROM.
			 */
			for (i = 0; i < sizeof(sc->sc_arpcom.ac_enaddr); i++)
				sc->sc_arpcom.ac_enaddr[i] = inb(iobase + i);
		}
	}
#endif

#if NISA > 0
	if (sc->sc_card == DEPCA) {
		struct isa_attach_args *ia = aux;
		u_char *mem, val;
		int i;

		mem = sc->sc_mem = ISA_HOLE_VADDR(ia->ia_maddr);

		val = 0xff;
		for (;;) {
			for (i = 0; i < ia->ia_msize; i++)
				mem[i] = val;
			for (i = 0; i < ia->ia_msize; i++)
				if (mem[i] != val) {
					printf("%s: failed to clear memory\n",
					    sc->sc_dev.dv_xname);
					return;
				}
			if (val == 0x00)
				break;
			val -= 0x55;
		}

		sc->sc_conf3 = LE_C3_ACON;
		sc->sc_addr = 0;
		sc->sc_memsize = ia->ia_msize;
	} else
#endif
	{
		sc->sc_mem = malloc(16384, M_DEVBUF, M_NOWAIT);
		if (sc->sc_mem == 0) {
			printf("%s: couldn't allocate memory for card\n",
			    sc->sc_dev.dv_xname);
			return;
		}

		sc->sc_conf3 = 0;
		sc->sc_addr = kvtop(sc->sc_mem);
		sc->sc_memsize = 16384;
	}

	sc->sc_copytodesc = copytobuf_contig;
	sc->sc_copyfromdesc = copyfrombuf_contig;
	sc->sc_copytobuf = copytobuf_contig;
	sc->sc_copyfrombuf = copyfrombuf_contig;
	sc->sc_zerobuf = zerobuf_contig;

	sc->sc_arpcom.ac_if.if_name = lecd.cd_name;
	leconfig(sc);

	printf("%s: type %s\n", sc->sc_dev.dv_xname, card_type[sc->sc_card]);

#if NISA > 0
	if (parent->dv_cfdata->cf_driver == &isacd) {
		struct isa_attach_args *ia = aux;

		if (ia->ia_drq != DRQUNK)
			isa_dmacascade(ia->ia_drq);

		sc->sc_ih = isa_intr_establish(ia->ia_irq, ISA_IST_EDGE,
		    ISA_IPL_NET, leintredge, sc);
	}
#endif

#if NPCI > 0
	if (parent->dv_cfdata->cf_driver == &pcicd) {
		struct pci_attach_args *pa = aux;

		pci_conf_write(pa->pa_tag, PCI_COMMAND_STATUS_REG,
		    pci_conf_read(pa->pa_tag, PCI_COMMAND_STATUS_REG) |
		    PCI_COMMAND_MASTER_ENABLE);

		sc->sc_ih = pci_map_int(pa->pa_tag, PCI_IPL_NET, leintr, sc);
	}
#endif

	sc->sc_sh = shutdownhook_establish(leshutdown, sc);
}

void
leshutdown(arg)
	void *arg;
{
	struct le_softc *sc = arg;

	lestop(sc);
}

#if NISA > 0
/*
 * Controller interrupt.
 */
leintredge(arg)
	void *arg;
{

	if (leintr(arg) == 0)
		return (0);
	for (;;)
		if (leintr(arg) == 0)
			return (1);
}
#endif

#include <dev/ic/am7990.c>

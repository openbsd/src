/*    $OpenBSD: if_sn_obio.c,v 1.24 2007/09/10 20:29:46 miod Exp $    */
/*    $NetBSD: if_sn_obio.c,v 1.9 1997/04/22 20:56:15 scottr Exp $    */

/*
 * Copyright (C) 1997 Allen Briggs
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
 *      This product includes software developed by Allen Briggs
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/viareg.h>

#include <mac68k/dev/obiovar.h>
#include <mac68k/dev/if_snreg.h>
#include <mac68k/dev/if_snvar.h>

#define SONIC_REG_BASE	0x50F0A000
#define SONIC_PROM_BASE	0x50F08000

static int	sn_obio_match(struct device *, void *, void *);
static void	sn_obio_attach(struct device *, struct device *, void *);
static int	sn_obio_getaddr(struct sn_softc *, u_int8_t *);
static int	sn_obio_getaddr_kludge(struct sn_softc *, u_int8_t *);

struct cfattach sn_obio_ca = {
	sizeof(struct sn_softc), sn_obio_match, sn_obio_attach
};

static int
sn_obio_match(struct device *parent, void *cf, void *aux)
{
	struct obio_attach_args *oa = (struct obio_attach_args *)aux;
	bus_space_handle_t bsh;
	int found = 0;

	if (!mac68k_machine.sonic)
		return 0;

	if (bus_space_map(oa->oa_tag,
	    SONIC_REG_BASE, SN_REGSIZE, 0, &bsh))
		return 0;

	if (mac68k_bus_space_probe(oa->oa_tag, bsh, 0, 4))
		found = 1;

	bus_space_unmap(oa->oa_tag, bsh, SN_REGSIZE);

	return found;
}

/*
 * Install interface into kernel networking data structures
 */
static void
sn_obio_attach(struct device *parent, struct device *self, void *aux)
{
	struct obio_attach_args *oa = (struct obio_attach_args *)aux;
	struct sn_softc	*sc = (void *)self;
	u_int8_t myaddr[ETHER_ADDR_LEN];
	int i;

	sc->snr_dcr = DCR_WAIT0 | DCR_DMABLOCK | DCR_RFT16 | DCR_TFT16;
	sc->snr_dcr2 = 0;

	sc->slotno = 9;

	switch (current_mac_model->machineid) {
	case MACH_MACC610:
	case MACH_MACC650:
	case MACH_MACQ610:
	case MACH_MACQ650:
	case MACH_MACQ700:
	case MACH_MACQ800:
	case MACH_MACQ900:
	case MACH_MACQ950:
		sc->snr_dcr |= DCR_EXBUS;
		sc->bitmode = 1;
		break;

	case MACH_MACLC575:
	case MACH_MACP580:
	case MACH_MACQ630:
		break;

	case MACH_MACPB500:
		sc->snr_dcr |= DCR_SYNC | DCR_LBR;
		sc->bitmode = 0;	/* 16 bit interface */
		break;

	default:
		printf(": unsupported machine type\n");
		return;
	}

	sc->sc_regt = oa->oa_tag;

	if (bus_space_map(sc->sc_regt,
	    SONIC_REG_BASE, SN_REGSIZE, 0, &sc->sc_regh)) {
		printf(": failed to map space for SONIC regs.\n");
		return;
	}

	/* regs are addressed as words, big-endian. */
	for (i = 0; i < SN_NREGS; i++) {
		sc->sc_reg_map[i] = (bus_size_t)((i * 4) + 2);
	}

	/*
	 * Kind of kludge this.  Comm-slot cards do not really
	 * have a visible type, as far as I can tell at this time,
	 * so assume that MacOS had it properly configured and use
	 * that configuration.
	 */
	switch (current_mac_model->machineid) {
	case MACH_MACLC575:
	case MACH_MACP580:
	case MACH_MACQ630:
		NIC_PUT(sc, SNR_CR, CR_RST);	wbflush();
		i = NIC_GET(sc, SNR_DCR);
		sc->snr_dcr |= (i & 0xfff0);
		sc->bitmode = (i & DCR_DW) ? 1 : 0;
		break;
	default:
		break;
	}

	if (sn_obio_getaddr(sc, myaddr) &&
	    sn_obio_getaddr_kludge(sc, myaddr)) { /* XXX kludge for PB */
		printf(": failed to get MAC address.\n");
		bus_space_unmap(sc->sc_regt, sc->sc_regh, SN_REGSIZE);
		return;
	}

	printf(": integrated ethernet adapter, ");

	/* snsetup returns 1 if something fails */
	if (snsetup(sc, myaddr)) {
		bus_space_unmap(sc->sc_regt, sc->sc_regh, SN_REGSIZE);
		return;
	}

	if (mac68k_machine.aux_interrupts)
		intr_establish(snintr, sc, 3, sc->sc_dev.dv_xname);
	else
		add_nubus_intr(sc->slotno, IPL_NET, snintr, sc,
		    sc->sc_dev.dv_xname);
}

static int
sn_obio_getaddr(struct sn_softc *sc, u_int8_t *lladdr)
{
	bus_space_handle_t bsh;

	if (bus_space_map(sc->sc_regt, SONIC_PROM_BASE, PAGE_SIZE, 0, &bsh)) {
		printf(": failed to map space to read SONIC address.\n%s",
		    sc->sc_dev.dv_xname);
		return (-1);
	}

	if (!mac68k_bus_space_probe(sc->sc_regt, bsh, 0, 1)) {
		bus_space_unmap(sc->sc_regt, bsh, PAGE_SIZE);
		return (-1);
	}

	sn_get_enaddr(sc->sc_regt, bsh, 0, lladdr);

	bus_space_unmap(sc->sc_regt, bsh, PAGE_SIZE);

	return (0);
}

/*
 * Assume that the SONIC was initialized in MacOS.  This should go away
 * when we can properly get the MAC address on the PBs.
 */
static int
sn_obio_getaddr_kludge(struct sn_softc *sc, u_int8_t *lladdr)
{
	int i, ors = 0;

	/* Shut down NIC */
	NIC_PUT(sc, SNR_CR, CR_RST);
	wbflush();

	NIC_PUT(sc, SNR_CEP, 15); /* For some reason, Apple fills top first. */
	wbflush();
	i = NIC_GET(sc, SNR_CAP2);
	wbflush();

	ors |= i;
	lladdr[5] = i >> 8;
	lladdr[4] = i;

	i = NIC_GET(sc, SNR_CAP1);
	wbflush();

	ors |= i;
	lladdr[3] = i >> 8;
	lladdr[2] = i;

	i = NIC_GET(sc, SNR_CAP0);
	wbflush();

	ors |= i;
	lladdr[1] = i >> 8;
	lladdr[0] = i;

	NIC_PUT(sc, SNR_CR, 0);
	wbflush();

	if (ors == 0)
		return (-1);

	return (0);
}

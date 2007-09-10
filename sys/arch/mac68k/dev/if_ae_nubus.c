/*	$OpenBSD: if_ae_nubus.c,v 1.18 2007/09/10 20:29:46 miod Exp $	*/
/*	$NetBSD: if_ae_nubus.c,v 1.17 1997/05/01 18:17:16 briggs Exp $	*/

/*
 * Copyright (C) 1997 Scott Reynolds
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
 * 3. The name of the author may not be used to endorse or promote products
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
/*
 * Some parts are derived from code adapted for MacBSD by Brad Parker
 * <brad@fcr.com>.
 *
 * Currently supports:
 *	Apple NB Ethernet Card
 *	Apple NB Ethernet Card II
 *	Interlan A310 NuBus Ethernet card
 *	Cayman Systems GatorCard
 *	Asante MacCon II/E
 *	Kinetics EtherPort SE/30
 */

#define	AE_OLD_GET_ENADDR

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <machine/bus.h>
#include <machine/viareg.h>

#include <net/if_media.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>
#include <mac68k/dev/nubus.h>
#include <mac68k/dev/if_aevar.h>
#include <mac68k/dev/if_aereg.h>

static int	ae_nubus_match(struct device *, void *, void *);
static void	ae_nubus_attach(struct device *, struct device *, void *);
static int	ae_nb_card_vendor(bus_space_tag_t, bus_space_handle_t,
		    struct nubus_attach_args *);
static int	ae_nb_get_enaddr(bus_space_tag_t, bus_space_handle_t,
		    struct nubus_attach_args *, u_int8_t *);
#ifdef DEBUG
static void	ae_nb_watchdog(struct ifnet *);
#endif

struct cfattach ae_nubus_ca = {
	sizeof(struct dp8390_softc), ae_nubus_match, ae_nubus_attach
};

static int
ae_nubus_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct nubus_attach_args *na = (struct nubus_attach_args *) aux;
	bus_space_handle_t bsh;
	int rv = 0;

	if (bus_space_map(na->na_tag, NUBUS_SLOT2PA(na->slot), NBMEMSIZE,
	    0, &bsh))
		return (0);

	if (na->category == NUBUS_CATEGORY_NETWORK &&
	    na->type == NUBUS_TYPE_ETHERNET) {
		switch (ae_nb_card_vendor(na->na_tag, bsh, na)) {
		case DP8390_VENDOR_APPLE:
		case DP8390_VENDOR_ASANTE:
		case DP8390_VENDOR_FARALLON:
		case DP8390_VENDOR_INTERLAN:
		case DP8390_VENDOR_KINETICS:
		case DP8390_VENDOR_CABLETRON:
			rv = 1;
			break;
		case DP8390_VENDOR_DAYNA:
		case DP8390_VENDOR_FOCUS:
			/* not supported yet */
			/* FALLTHROUGH */
		default:
			rv = 0;
			break;
		}
	}

	bus_space_unmap(na->na_tag, bsh, NBMEMSIZE);

	return (rv);
}

/*
 * Install interface into kernel networking data structures
 */
static void
ae_nubus_attach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;
{
	struct dp8390_softc *sc = (struct dp8390_softc *)self;
	struct nubus_attach_args *na = (struct nubus_attach_args *) aux;
#ifdef DEBUG
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
#endif
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	int i, success;
	const char *cardtype;

	bst = na->na_tag;
	if (bus_space_map(bst, NUBUS_SLOT2PA(na->slot), NBMEMSIZE,
	    0, &bsh)) {
		printf(": can't map memory space\n");
		return;
	}

	sc->sc_regt = sc->sc_buft = bst;
	sc->sc_flags = self->dv_cfdata->cf_flags;

	cardtype = nubus_get_card_name(bst, bsh, na->fmt);

	sc->is790 = 0;

	sc->mem_start = 0;
	sc->mem_size = 0;

	success = 0;

	switch (ae_nb_card_vendor(bst, bsh, na)) {
	case DP8390_VENDOR_APPLE:	/* Apple-compatible cards */
	case DP8390_VENDOR_ASANTE:
		/* Map register offsets */
		for (i = 0; i < 16; i++) /* reverse order, longword aligned */
			sc->sc_reg_map[i] = (15 - i) << 2;

		sc->dcr_reg = (ED_DCR_FT1 | ED_DCR_WTS | ED_DCR_LS);
		if (bus_space_subregion(bst, bsh,
		    AE_REG_OFFSET, AE_REG_SIZE, &sc->sc_regh)) {
			printf(": failed to map register space\n");
			break;
		}
		if ((sc->mem_size = ae_size_card_memory(bst, bsh,
		    AE_DATA_OFFSET)) == 0) {
			printf(": failed to determine size of RAM.\n");
			break;
		}
		if (bus_space_subregion(bst, bsh,
		    AE_DATA_OFFSET, sc->mem_size, &sc->sc_bufh)) {
			printf(": failed to map register space\n");
			break;
		}
#ifdef AE_OLD_GET_ENADDR
		/* Get station address from on-board ROM */
		for (i = 0; i < ETHER_ADDR_LEN; ++i)
			sc->sc_arpcom.ac_enaddr[i] =
			    bus_space_read_1(bst, bsh, (AE_ROM_OFFSET + i * 2));
#else
		if (ae_nb_get_enaddr(bst, bsh, na, sc->sc_arpcom.ac_enaddr)) {
			printf(": can't find MAC address\n");
			break;
		}
#endif

		success = 1;
		break;

	case DP8390_VENDOR_DAYNA:
		/* Map register offsets */
		for (i = 0; i < 16; i++) /* normal order, longword aligned */
			sc->sc_reg_map[i] = i << 2;

		sc->dcr_reg = (ED_DCR_FT1 | ED_DCR_WTS | ED_DCR_LS);
		if (bus_space_subregion(bst, bsh,
		    DP_REG_OFFSET, AE_REG_SIZE, &sc->sc_regh)) {
			printf(": failed to map register space\n");
			break;
		}
		sc->mem_size = 8192;
		if (bus_space_subregion(bst, bsh,
		    DP_DATA_OFFSET, sc->mem_size, &sc->sc_bufh)) {
			printf(": failed to map register space\n");
			break;
		}
#ifdef AE_OLD_GET_ENADDR
		/* Get station address from on-board ROM */
		for (i = 0; i < ETHER_ADDR_LEN; ++i)
			sc->sc_arpcom.ac_enaddr[i] =
			    bus_space_read_1(bst, bsh, (DP_ROM_OFFSET + i * 2));
#else
		if (ae_nb_get_enaddr(bst, bsh, na, sc->sc_arpcom.ac_enaddr)) {
			printf(": can't find MAC address\n");
			break;
		}
#endif

	case DP8390_VENDOR_FARALLON:
		/* Map register offsets */
		for (i = 0; i < 16; i++) /* reverse order, longword aligned */
			sc->sc_reg_map[i] = (15 - i) << 2;

		sc->dcr_reg = (ED_DCR_FT1 | ED_DCR_WTS | ED_DCR_LS);
		if (bus_space_subregion(bst, bsh,
		    AE_REG_OFFSET, AE_REG_SIZE, &sc->sc_regh)) {
			printf(": failed to map register space\n");
			break;
		}
		if ((sc->mem_size = ae_size_card_memory(bst, bsh,
		    AE_DATA_OFFSET)) == 0) {
			printf(": failed to determine size of RAM.\n");
			break;
		}
		if (bus_space_subregion(bst, bsh,
		    AE_DATA_OFFSET, sc->mem_size, &sc->sc_bufh)) {
			printf(": failed to map register space\n");
			break;
		}
#ifdef AE_OLD_GET_ENADDR
		/* Get station address from on-board ROM */
		for (i = 0; i < ETHER_ADDR_LEN; ++i)
			sc->sc_arpcom.ac_enaddr[i] =
			    bus_space_read_1(bst, bsh, (FE_ROM_OFFSET + i));
#endif

		success = 1;
		break;

	case DP8390_VENDOR_INTERLAN:
		/* Map register offsets */
		for (i = 0; i < 16; i++) /* normal order, longword aligned */
			sc->sc_reg_map[i] = i << 2;

		sc->dcr_reg = (ED_DCR_FT1 | ED_DCR_WTS | ED_DCR_LS);
		if (bus_space_subregion(bst, bsh,
		    GC_REG_OFFSET, AE_REG_SIZE, &sc->sc_regh)) {
			printf(": failed to map register space\n");
			break;
		}
		if ((sc->mem_size = ae_size_card_memory(bst, bsh,
		    GC_DATA_OFFSET)) == 0) {
			printf(": failed to determine size of RAM.\n");
			break;
		}
		if (bus_space_subregion(bst, bsh,
		    GC_DATA_OFFSET, sc->mem_size, &sc->sc_bufh)) {
			printf(": failed to map register space\n");
			break;
		}

		/* reset the NIC chip */
		bus_space_write_1(bst, bsh, GC_RESET_OFFSET, 0);

		if (ae_nb_get_enaddr(bst, bsh, na, sc->sc_arpcom.ac_enaddr)) {
			/* Fall back to snarf directly from ROM.  Ick.  */
			for (i = 0; i < ETHER_ADDR_LEN; ++i)
				sc->sc_arpcom.ac_enaddr[i] =
				    bus_space_read_1(bst, bsh,
					(GC_ROM_OFFSET + i * 4));
		}

		success = 1;
		break;

	case DP8390_VENDOR_KINETICS:
		/* Map register offsets */
		for (i = 0; i < 16; i++) /* normal order, longword aligned */
			sc->sc_reg_map[i] = i << 2;

		/* sc->use16bit = 0; */
		if (bus_space_subregion(bst, bsh,
		    KE_REG_OFFSET, AE_REG_SIZE, &sc->sc_regh)) {
			printf(": failed to map register space\n");
			break;
		}
		if ((sc->mem_size = ae_size_card_memory(bst, bsh,
		    KE_DATA_OFFSET)) == 0) {
			printf(": failed to determine size of RAM.\n");
			break;
		}
		if (bus_space_subregion(bst, bsh,
		    KE_DATA_OFFSET, sc->mem_size, &sc->sc_bufh)) {
			printf(": failed to map register space\n");
			break;
		}
		if (ae_nb_get_enaddr(bst, bsh, na, sc->sc_arpcom.ac_enaddr)) {
			printf(": can't find MAC address\n");
			break;
		}

		success = 1;
		break;
	case DP8390_VENDOR_CABLETRON:
		/* Map register offsets */
		for (i = 0; i < 16; i++)
		    sc->sc_reg_map[i] = i << 1; /* normal order, word aligned */

		sc->dcr_reg = (ED_DCR_FT1 | ED_DCR_WTS | ED_DCR_LS);
		if (bus_space_subregion(bst, bsh,
		    CT_REG_OFFSET, AE_REG_SIZE, &sc->sc_regh)) {
			printf(": failed to map register space\n");
			break;
		}
		if ((sc->mem_size = ae_size_card_memory(bst, bsh,
		    CT_DATA_OFFSET)) == 0) {
			printf(": failed to determine size of RAM.\n");
			break;
		}
		if (bus_space_subregion(bst, bsh,
		    CT_DATA_OFFSET, sc->mem_size, &sc->sc_bufh)) {
			printf(": failed to map register space\n");
			break;
		}
		if (ae_nb_get_enaddr(bst, bsh, na, sc->sc_arpcom.ac_enaddr)) {
			printf(": can't find MAC address\n");
			break;
		}
		success = 1;
	default:
		break;
	}

	if (!success) {
		bus_space_unmap(bst, bsh, NBMEMSIZE);
		return;
	}

	/*
	 * Override test_mem and write_mbuf functions; other defaults
	 * already work properly.
	 */
	sc->test_mem = ae_test_mem;
	sc->write_mbuf = ae_write_mbuf;
#ifdef DEBUG
	ifp->if_watchdog = ae_nb_watchdog;	/* Override watchdog */
#endif
	sc->sc_media_init = dp8390_media_init;

	/* Interface is always enabled. */
	sc->sc_enabled = 1;

	printf(": %s, %dKB memory", cardtype, sc->mem_size / 1024);

	if (dp8390_config(sc)) {
		bus_space_unmap(bst, bsh, NBMEMSIZE);
		return;
	}

	/* make sure interrupts are vectored to us */
	add_nubus_intr(na->slot, IPL_NET, dp8390_intr, sc, sc->sc_dev.dv_xname);
}

static int
ae_nb_card_vendor(bst, bsh, na)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	struct nubus_attach_args *na;
{
	int vendor;

	switch (na->drsw) {
	case NUBUS_DRSW_3COM:
		switch (na->drhw) {
		case NUBUS_DRHW_APPLE_SN:
		case NUBUS_DRHW_APPLE_SNT:
			vendor = DP8390_VENDOR_UNKNOWN;
			break;
		default:
			vendor = DP8390_VENDOR_APPLE;
			break;
		}
		break;
	case NUBUS_DRSW_APPLE:
		if (na->drhw == NUBUS_DRHW_ASANTE_LC) {
			vendor = DP8390_VENDOR_UNKNOWN;
			break;
		}
		/* FALLTHROUGH */
	case NUBUS_DRSW_DAYNA2:
	case NUBUS_DRSW_TECHWORKS:
	case NUBUS_DRSW_TFLLAN:
		if (na->drhw == NUBUS_DRHW_CABLETRON) {
			vendor = DP8390_VENDOR_CABLETRON;
		} else {
			vendor = DP8390_VENDOR_APPLE;
		}
		break;
	case NUBUS_DRSW_ASANTE:
		vendor = DP8390_VENDOR_ASANTE;
		break;
	case NUBUS_DRSW_FARALLON:
		vendor = DP8390_VENDOR_FARALLON;
		break;
	case NUBUS_DRSW_FOCUS:
		vendor = DP8390_VENDOR_FOCUS;
		break;
	case NUBUS_DRSW_GATOR:
		switch (na->drhw) {
		default:
		case NUBUS_DRHW_INTERLAN:
			vendor = DP8390_VENDOR_INTERLAN;
			break;
		case NUBUS_DRHW_KINETICS:
			if (strncmp(nubus_get_card_name(bst, bsh, na->fmt),
			    "EtherPort", 9) == 0)
				vendor = DP8390_VENDOR_KINETICS;
			else
				vendor = DP8390_VENDOR_DAYNA;
			break;
		}
		break;
	default:
		vendor = DP8390_VENDOR_UNKNOWN;
	}
	return vendor;
}

static int
ae_nb_get_enaddr(bst, bsh, na, ep)
	bus_space_tag_t bst;
	bus_space_handle_t bsh;
	struct nubus_attach_args *na;
	u_int8_t *ep;
{
	nubus_dir dir;
	nubus_dirent dirent;
	int rv;

	/*
	 * XXX - note hardwired resource IDs here (0x80); these are
	 * assumed to be used by all cards, but should be fixed when
	 * we find out more about Ethernet card resources.
	 */
	nubus_get_main_dir(na->fmt, &dir);
	switch (ae_nb_card_vendor(bst, bsh, na)) {
	case DP8390_VENDOR_APPLE:
		if (na->drsw == NUBUS_DRSW_TFLLAN) {	/* TFL LAN E410/E420 */
			rv = nubus_find_rsrc(bst, bsh, na->fmt,
			    &dir, 0x80, &dirent);
			break;
		}
		/* FALLTHROUGH */
	default:
		rv = nubus_find_rsrc(bst, bsh, na->fmt, &dir, 0x80, &dirent);
		break;
	}
	if (rv <= 0)
		return 1;
	nubus_get_dir_from_rsrc(na->fmt, &dirent, &dir);
	if (nubus_find_rsrc(bst, bsh, na->fmt, &dir, 0x80, &dirent) <= 0)
		return 1;
	if (nubus_get_ind_data(bst, bsh,
	    na->fmt, &dirent, ep, ETHER_ADDR_LEN) <= 0)
		return 1;

	return 0;
}

#ifdef DEBUG
static void
ae_nb_watchdog(ifp)
	struct ifnet *ifp;
{
	struct dp8390_softc *sc = ifp->if_softc;
	extern via2hand_t via2intrs[7];

/*
 * This is a kludge!  The via code seems to miss slot interrupts
 * sometimes.  This kludges around that by calling the handler
 * by hand if the watchdog is activated. -- XXX (akb)
 * XXX note that this assumes the nubus handler is first in the chain.
 */
	if (!SLIST_EMPTY(&via2intrs[1])) {
		struct via2hand *vh = SLIST_FIRST(&via2intrs[1]);
		(void)(*vh->vh_fn)(vh->vh_arg);
	}

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	dp8390_reset(sc);
}
#endif

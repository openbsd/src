/*	$OpenBSD: if_sn_nubus.c,v 1.5 1997/04/08 04:14:46 briggs Exp $	*/

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

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/bus.h>
#include <machine/viareg.h>

#include "nubus.h"
#include "if_aereg.h"	/* For AE_VENDOR values */
#include "if_snreg.h"
#include "if_snvar.h"

static int	sn_nubus_match __P((struct device *, void *, void *));
static void	sn_nubus_attach __P((struct device *, struct device *, void *));
static int	sn_nb_card_vendor __P((struct nubus_attach_args *));
static int	sn_nb_get_enaddr __P((struct nubus_attach_args *,
					u_int8_t *, int));

struct cfattach sn_nubus_ca = {
	sizeof(struct sn_softc), sn_nubus_match, sn_nubus_attach
};

static int
sn_nubus_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct nubus_attach_args *na = (struct nubus_attach_args *) aux;
	bus_space_handle_t bsh;
	int rv;

	if (bus_space_map(na->na_tag, NUBUS_SLOT2PA(na->slot), NBMEMSIZE,
	    0, &bsh))
		return (0);

	rv = 0;

	if (na->category == NUBUS_CATEGORY_NETWORK &&
	    na->type == NUBUS_TYPE_ETHERNET) {
		switch (sn_nb_card_vendor(na)) {

		case AE_VENDOR_APPLE:
		case AE_VENDOR_DAYNA:
			rv = 1;
			break;

		default:
			break;
		}
	}

	bus_space_unmap(na->na_tag, bsh, NBMEMSIZE);

	return rv;
}

/*
 * Install interface into kernel networking data structures
 */
static void
sn_nubus_attach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;
{
        struct sn_softc *sc = (void *)self;
        struct nubus_attach_args *na = (struct nubus_attach_args *)aux;
	int		i, success;
	bus_space_tag_t	bst;
	bus_space_handle_t	bsh, tmp_bsh;

	bst = na->na_tag;
	if (bus_space_map(bst, NUBUS_SLOT2PA(na->slot), NBMEMSIZE, 0, &bsh)) {
		printf(": failed to map memory space.\n");
		return;
	}

	sc->sc_regt = bst;
	sc->bitmode = 1;

	success = 0;

        sc->bitmode = 1;		/* 32-bit card */
        sc->slotno = na->slot;

        switch (sn_nb_card_vendor(na)) {
	case AE_VENDOR_DAYNA:
                sc->snr_dcr = DCR_ASYNC | DCR_WAIT0 | DCR_DW32 |
			DCR_DMABLOCK | DCR_RFT16 | DCR_TFT16;
		sc->snr_dcr2 = 0;

		if (bus_space_subregion(bst, bsh, 0x00180000, SN_REGSIZE,
					&sc->sc_regh)) {
			printf(": failed to map register space.\n");
			break;
		}

		if (bus_space_subregion(bst, bsh, 0x00ffe004, ETHER_ADDR_LEN,
					&tmp_bsh)) {
			printf(": failed to map ROM space.\n");
			break;
		}

		/*
		 * Copy out the ethernet address from the card's ROM
		 *
		 * See if_sn_obio.c for a discussion of bit reversal
		 * in Apple's MAC address PROMs. As far as I can tell
		 * Dayna stores their Mac address in ethernet format,
		 * not Token Ring.
		 */
		for (i = 0; i < ETHER_ADDR_LEN; ++i)
			sc->sc_arpcom.ac_enaddr[i] =
					bus_space_read_1(bst, tmp_bsh, i);

		success = 1;
                break;

	case AE_VENDOR_APPLE:
                sc->snr_dcr = DCR_ASYNC | DCR_WAIT0 | DCR_DW32 |
			DCR_DMABLOCK | DCR_RFT16 | DCR_TFT16;
		sc->snr_dcr2 = 0;

		if (bus_space_subregion(bst, bsh, 0x00180000, SN_REGSIZE,
					&sc->sc_regh)) {
			printf(": failed to map register space.\n");
			break;
		}

		sn_nb_get_enaddr(na, sc->sc_arpcom.ac_enaddr, na, 0x8);

		success = 1;
                break;

        default:
                /*
                 * You can't actually get this default, the snmatch
                 * will fail for unknown hardware. If you're adding support
                 * for a new card, the following defaults are a
                 * good starting point.
                 */
                sc->snr_dcr = DCR_SYNC | DCR_WAIT0 | DCR_DW32 |
			DCR_DMABLOCK | DCR_RFT16 | DCR_TFT16;
		sc->snr_dcr2 = 0;
		printf(": attachment incomplete.\n");
                return;
        }

	if (!success) {
		bus_space_unmap(bst, bsh, NBMEMSIZE);
		return;
	}

	snsetup(sc);
	/* Regs are addressed as words, big endian. */
	for (i = 0; i < SN_NREGS; i++) {
		sc->sc_reg_map[i] = (bus_size_t)((i * 4) + 2);
	}

	/* snsetup returns 1 if something fails */
	if (snsetup(sc)) {
		bus_space_unmap(bst, bsh, NBMEMSIZE);
		return;
	}

	add_nubus_intr(sc->slotno, snintr, (void *)sc);

	return;
}

static int
sn_nb_card_vendor(na)
	struct nubus_attach_args *na;
{
	int vendor;

	switch (na->drsw) {
	case NUBUS_DRSW_3COM:
		switch (na->drhw) {
		case NUBUS_DRHW_APPLE_SN:
			vendor = AE_VENDOR_APPLE;
			break;
		default:
			vendor = AE_VENDOR_UNKNOWN;
			break;
		}
		break;
	case NUBUS_DRSW_APPLE:
	case NUBUS_DRSW_TECHWORKS:
		vendor = AE_VENDOR_APPLE;
		break;
	case NUBUS_DRSW_ASANTE:
		vendor = AE_VENDOR_ASANTE;
		break;
	case NUBUS_DRSW_FARALLON:
		vendor = AE_VENDOR_FARALLON;
		break;
	case NUBUS_DRSW_FOCUS:
		vendor = AE_VENDOR_FOCUS;
		break;
	case NUBUS_DRSW_GATOR:
		switch (na->drhw) {
		default:
		case NUBUS_DRHW_INTERLAN:
			vendor = AE_VENDOR_INTERLAN;
			break;
		case NUBUS_DRHW_KINETICS:
			if (strncmp(
			    nubus_get_card_name(na->fmt), "EtherPort", 9) == 0)
				vendor = AE_VENDOR_KINETICS;
			else
				vendor = AE_VENDOR_DAYNA;
			break;
		}
		break;
	case NUBUS_DRSW_DAYNA:
		vendor = AE_VENDOR_DAYNA;
		break;
	default:
#ifdef DIAGNOSTIC
		printf("Unknown ethernet drsw: %x\n", na->drsw);
#endif
		vendor = AE_VENDOR_UNKNOWN;
	}
	return vendor;
}

static int
sn_nb_get_enaddr(na, ep, rsrc1)
	struct nubus_attach_args *na;
	u_int8_t *ep;
	int	rsrc1;
{
	nubus_dir dir;
	nubus_dirent dirent;

	nubus_get_main_dir(na->fmt, &dir);
	if (nubus_find_rsrc(na->fmt, &dir, na->rsrcid, &dirent) <= 0)
		return 1;
	nubus_get_dir_from_rsrc(na->fmt, &dirent, &dir);
	if (nubus_find_rsrc(na->fmt, &dir, rsrc1, &dirent) <= 0)
		return 1;
	if (nubus_get_ind_data(na->fmt, &dirent, ep, ETHER_ADDR_LEN) <= 0)
		return 1;

	return 0;
}

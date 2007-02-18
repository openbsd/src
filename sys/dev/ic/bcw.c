/*	$OpenBSD: bcw.c,v 1.38 2007/02/18 09:37:21 mglocker Exp $ */

/*
 * Copyright (c) 2006 Jon Simola <jsimola@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Broadcom BCM43xx Wireless network chipsets (broadcom.com)
 * SiliconBackplane is technology from Sonics, Inc.(sonicsinc.com)
 */
 
/* standard includes, probably some extras */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/cardbus/cardbusvar.h>

#include <dev/ic/bcwreg.h>
#include <dev/ic/bcwvar.h>

#include <uvm/uvm_extern.h>

void		bcw_shm_ctl_word(struct bcw_softc *, uint16_t, uint16_t);
uint16_t	bcw_shm_read16(struct bcw_softc *, uint16_t, uint16_t);
void		bcw_radio_write16(struct bcw_softc *, uint16_t, uint16_t);
int		bcw_radio_read16(struct bcw_softc *, uint16_t);
void		bcw_phy_write16(struct bcw_softc *, uint16_t, uint16_t);
int		bcw_phy_read16(struct bcw_softc *, uint16_t);

void		bcw_reset(struct bcw_softc *);
int		bcw_init(struct ifnet *);
void		bcw_start(struct ifnet *);
void		bcw_stop(struct ifnet *, int);

void		bcw_watchdog(struct ifnet *);
void		bcw_rxintr(struct bcw_softc *);
void		bcw_txintr(struct bcw_softc *);
//void		bcw_add_mac(struct bcw_softc *, uint8_t *, unsigned long);
int		bcw_add_rxbuf(struct bcw_softc *, int);
void		bcw_rxdrain(struct bcw_softc *);
void		bcw_set_filter(struct ifnet *); 
void		bcw_tick(void *);
int		bcw_ioctl(struct ifnet *, u_long, caddr_t);

int		bcw_alloc_rx_ring(struct bcw_softc *, struct bcw_rx_ring *,
		    int);
void		bcw_reset_rx_ring(struct bcw_softc *, struct bcw_rx_ring *);
void		bcw_free_rx_ring(struct bcw_softc *, struct bcw_rx_ring *);
int		bcw_alloc_tx_ring(struct bcw_softc *, struct bcw_tx_ring *,
		    int);
void		bcw_reset_tx_ring(struct bcw_softc *, struct bcw_tx_ring *);
void		bcw_free_tx_ring(struct bcw_softc *, struct bcw_tx_ring *);

/* 80211 functions copied from iwi */
int		bcw_newstate(struct ieee80211com *, enum ieee80211_state, int);
int		bcw_media_change(struct ifnet *);
void		bcw_media_status(struct ifnet *, struct ifmediareq *);
/* fashionably new functions */
int		bcw_validatechipaccess(struct bcw_softc *);
void		bcw_powercontrol_crystal_off(struct bcw_softc *);
int		bcw_change_core(struct bcw_softc *, int);
void		bcw_radio_off(struct bcw_softc *);
void		bcw_radio_on(struct bcw_softc *);
int		bcw_radio_channel(struct bcw_softc *, uint8_t, int );
void		bcw_spw(struct bcw_softc *, uint8_t);
int		bcw_chan2freq_bg(uint8_t);
int		bcw_reset_core(struct bcw_softc *, uint32_t);
int		bcw_get_firmware(const char *, const uint8_t *, size_t,
		    size_t *, size_t *);
int		bcw_load_firmware(struct bcw_softc *);
int		bcw_write_initvals(struct bcw_softc *,
		    const struct bcw_initval *, const unsigned int);
int		bcw_load_initvals(struct bcw_softc *);
void		bcw_leds_switch_all(struct bcw_softc *, int);
int		bcw_gpio_init(struct bcw_softc *);

int		bcw_phy_init(struct bcw_softc *);
void		bcw_phy_initg(struct bcw_softc *);
void		bcw_phy_initb5(struct bcw_softc *);
void		bcw_phy_initb6(struct bcw_softc *);

struct cfdriver bcw_cd = {
	NULL, "bcw", DV_IFNET
};

void
bcw_shm_ctl_word(struct bcw_softc *sc, uint16_t routing, uint16_t offset)
{
	uint32_t control;

	control = routing;
	control <<= 16;
	control |= offset;

	BCW_WRITE(sc, BCW_SHM_CONTROL, control);
}

uint16_t
bcw_shm_read16(struct bcw_softc *sc, uint16_t routing, uint16_t offset)
{
	if (routing == BCW_SHM_CONTROL_SHARED) {
		if (offset & 0x0003) {
			bcw_shm_ctl_word(sc, routing, offset >> 2);

			return (BCW_READ16(sc, BCW_SHM_DATAHIGH));
		}
		offset >>= 2;
	}
	bcw_shm_ctl_word(sc, routing, offset);

	return (BCW_READ16(sc, BCW_SHM_DATA));
}

void
bcw_radio_write16(struct bcw_softc *sc, uint16_t offset, uint16_t val)
{
	BCW_WRITE16(sc, BCW_RADIO_CONTROL, offset);
	BCW_WRITE16(sc, BCW_RADIO_DATALOW, val);
}

int
bcw_radio_read16(struct bcw_softc *sc, uint16_t offset)
{
	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
		offset |= 0x0040;
		break;
	case BCW_PHY_TYPEB:
		if (sc->sc_radio_ver == 0x2053) {
			if (offset < 0x70)
				offset += 0x80;
			else if (offset < 0x80)
				offset += 0x70;
		} else if (sc->sc_radio_ver == 0x2050)
			offset |= 0x80;
		else
			return (0);
		break;
	case BCW_PHY_TYPEG:
		offset |= 0x80;
		break;
	}

	BCW_WRITE16(sc, BCW_RADIO_CONTROL, offset);

	return (BCW_READ16(sc, BCW_RADIO_DATALOW));
}

void
bcw_phy_write16(struct bcw_softc *sc, uint16_t offset, uint16_t val)
{
	BCW_WRITE16(sc, BCW_PHY_CONTROL, offset);
	BCW_WRITE16(sc, BCW_PHY_DATA, val);
}

int
bcw_phy_read16(struct bcw_softc *sc, uint16_t offset)
{
	BCW_WRITE16(sc, BCW_PHY_CONTROL, offset);

	return (BCW_READ16(sc, BCW_PHY_DATA));
}

void
bcw_attach(struct bcw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet	*ifp = &ic->ic_if;
	int		error;
	int		i;
	uint32_t	sbval;
//	uint16_t	sbval16;

	/*
	 * Don't reset the chip here, we can only reset each core and we
	 * haven't identified the cores yet.
	 */
//	bcw_reset(sc);

	/*
	 * Attach to the Backplane and start the card up
	 */

	/*
	 * Get a copy of the BoardFlags and fix for broken boards
	 * This needs to be done as soon as possible to determine if the
	 * board supports power control settings. If so, the board has to
	 * be powered on and the clock started. This may even need to go
	 * before the initial chip reset above.
	 */
	sc->sc_boardflags = BCW_READ16(sc, BCW_SPROM_BOARDFLAGS);

	/*
	 * Dell, Product ID 0x4301 Revision 0x74, set BCW_BF_BTCOEXIST
	 * Apple Board Type 0x4e Revision > 0x40, set BCW_BF_PACTRL
	 */

	/*
	 * Should just about everything below here be moved to external files
	 * to keep this file sane? The BCM43xx chips have so many exceptions
	 * based on the version of the chip, the radio, cores and phys that
	 * it would be a huge mess to inline it all here. See the 100 lines
	 * below for an example of just figuring out what the chip id is and
	 * how many cores it has.
	 */

	/*
	 * Try and change to the ChipCommon Core
	 */
	if (bcw_change_core(sc, 0))
		DPRINTF(("\n%s: Selected ChipCommon Core\n",
		    sc->sc_dev.dv_xname));

	/*
	 * Core ID REG, this is either the default wireless core (0x812) or
	 * a ChipCommon core that was successfully selected above
	 */
	sbval = BCW_READ(sc, BCW_CIR_SBID_HI);
	DPRINTF(("%s: Got Core ID Reg 0x%x, type is 0x%x\n",
	    sc->sc_dev.dv_xname, sbval, (sbval & 0x8ff0) >> 4));

	/*
	 * If we successfully got a commoncore, and the corerev=4 or >=6
	 * get the number of cores from the chipid reg
	 */
	if (((sbval & 0x00008ff0) >> 4) == BCW_CORE_COMMON) {
		sc->sc_havecommon = 1;
		/* XXX do early init of sc_core[0] here */
		sbval = BCW_READ(sc, BCW_CORE_COMMON_CHIPID);
		sc->sc_chipid = (sbval & 0x0000ffff);
		sc->sc_chiprev =
		    ((sbval & 0x00007000) >> 8 | (sbval & 0x0000000f));
		if ((sc->sc_chiprev == 4) || (sc->sc_chiprev >= 6))
			sc->sc_numcores = (sbval & 0x0f000000) >> 24;
		else
			switch (sc->sc_chipid) {
			case 0x4710:
			case 0x4610:
			case 0x4704:
				sc->sc_numcores = 9;
				break;
			case 0x4310:
				sc->sc_numcores = 8;
				break;
			case 0x5365:
				sc->sc_numcores = 7;
				break;
			case 0x4306:
				sc->sc_numcores = 6;
				break;
			case 0x4307:
			case 0x4301:
				sc->sc_numcores = 5;
				break;
			case 0x4402:
				sc->sc_numcores = 3;
				break;
			default:
				/* set to max */
				sc->sc_numcores = BCW_MAX_CORES;
			} /* end of switch */
	} else { /* No CommonCore, set chipid,cores,rev based on product id */
		sc->sc_core_common = NULL;
		sc->sc_havecommon = 0;
		switch (sc->sc_prodid) {
		case 0x4710:
		case 0x4711:
		case 0x4712:
		case 0x4713:
		case 0x4714:
		case 0x4715:
			sc->sc_chipid = 0x4710;
			sc->sc_numcores = 9;
			break;
		case 0x4610:
		case 0x4611:
		case 0x4612:
		case 0x4613:
		case 0x4614:
		case 0x4615:
			sc->sc_chipid = 0x4610;
			sc->sc_numcores = 9;
			break;
		case 0x4402:
		case 0x4403:
			sc->sc_chipid = 0x4402;
			sc->sc_numcores = 3;
			break;
		case 0x4305:
		case 0x4306:
		case 0x4307:
			sc->sc_chipid = 0x4307;
			sc->sc_numcores = 5;
			break;
		case 0x4301:
			sc->sc_chipid = 0x4301;
			sc->sc_numcores = 5;
			break;
		default:
			sc->sc_chipid = sc->sc_prodid;
			/* Set to max */
			sc->sc_numcores = BCW_MAX_CORES;
		} /* end of switch */
	} /* End of if/else */

	DPRINTF(("%s: ChipID=0x%x, ChipRev=0x%x, NumCores=%d\n",
	    sc->sc_dev.dv_xname, sc->sc_chipid,
	    sc->sc_chiprev, sc->sc_numcores));

       /* Reset and Identify each core */
       for (i = 0; i < sc->sc_numcores; i++) {
		if (bcw_change_core(sc, i)) {
			sbval = BCW_READ(sc, BCW_CIR_SBID_HI);

			sc->sc_core[i].id = (sbval & 0x00008ff0) >> 4;
			sc->sc_core[i].rev =
			    ((sbval & 0x00007000) >> 8 | (sbval & 0x0000000f));

			switch (sc->sc_core[i].id) {
			case BCW_CORE_COMMON:
				bcw_reset_core(sc, 0);
				sc->sc_core_common = &sc->sc_core[i];
				break;
			case BCW_CORE_PCI:
#if 0
				bcw_reset_core(sc,0);
				(sc->sc_ca == NULL)
#endif
				sc->sc_core_bus = &sc->sc_core[i];
				break;
#if 0
			case BCW_CORE_PCMCIA:
				bcw_reset_core(sc,0);
				if (sc->sc_pa == NULL)
					sc->sc_core_bus = &sc->sc_core[i];
				break;
#endif
			case BCW_CORE_80211:
				bcw_reset_core(sc,
				    SBTML_80211FLAG | SBTML_80211PHY);
				sc->sc_core_80211 = &sc->sc_core[i];
				break;
			case BCW_CORE_NONEXIST:
				sc->sc_numcores = i + 1;
				break;
			default:
				/* Ignore all other core types */
				break;
			}
			DPRINTF(("%s: core %d is type 0x%x rev %d\n",
			    sc->sc_dev.dv_xname, i, 
			    sc->sc_core[i].id, sc->sc_core[i].rev));
			/* XXX Fill out the core location vars */
			sbval = BCW_READ(sc, BCW_SBTPSFLAG);
			sc->sc_core[i].backplane_flag =
			sbval & SBTPS_BACKPLANEFLAGMASK;
			sc->sc_core[i].num = i;
		} else
			DPRINTF(("%s: Failed change to core %d",
			    sc->sc_dev.dv_xname, i));
	} /* End of For loop */

	/* Now that we have cores identified, finish the reset */
	bcw_reset(sc);

	/*
	 * XXX Select the 802.11 core, then
	 * Get and display the PHY info from the MIMO
	 * This probably won't work for cards with multiple radio cores, as
	 * the spec suggests that there is one PHY for each core
	 */
	bcw_change_core(sc, sc->sc_core_80211->num);
	sbval = BCW_READ16(sc, 0x3E0);
	sc->sc_phy_version = (sbval & 0xf000) >> 12;
	sc->sc_phy_rev = sbval & 0xf;
	sc->sc_phy_type = (sbval & 0xf00) >> 8;
	DPRINTF(("%s: PHY version %d revision %d ",
	    sc->sc_dev.dv_xname, sc->sc_phy_version, sc->sc_phy_rev));
	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
		DPRINTF(("PHY %d (A)\n", sc->sc_phy_type));
		break;
	case BCW_PHY_TYPEB:
		DPRINTF(("PHY %d (B)\n", sc->sc_phy_type));
		break;
	case BCW_PHY_TYPEG:
		DPRINTF(("PHY %d (G)\n", sc->sc_phy_type));
		break;
	case BCW_PHY_TYPEN:
		DPRINTF(("PHY %d (N)\n", sc->sc_phy_type));
		break;
	default:
		DPRINTF(("Unrecognizeable PHY type %d\n",
		    sc->sc_phy_type));
		break;
	} /* end of switch */

	/*
	 * Query the RadioID register, on a 4317 use a lookup instead
	 * XXX Different PHYs have different radio register layouts, so
	 * a wrapper func should be written.
	 * Getting the RadioID is the only 32bit operation done with the
	 * Radio registers, and requires seperate 16bit reads from the low
	 * and the high data addresses.
	 */
	if (sc->sc_chipid != 0x4317) {
		BCW_WRITE16(sc, BCW_RADIO_CONTROL, BCW_RADIO_ID);
		sbval = BCW_READ16(sc, BCW_RADIO_DATAHIGH);
		sbval <<= 16;
		BCW_WRITE16(sc, BCW_RADIO_CONTROL, BCW_RADIO_ID);
		sc->sc_radio_mnf = sbval | BCW_READ16(sc, BCW_RADIO_DATALOW);
	} else {
		switch (sc->sc_chiprev) {
		case 0:	
			sc->sc_radio_mnf = 0x3205017F;
			break;
		case 1:
			sc->sc_radio_mnf = 0x4205017f;
			break;
		default:
			sc->sc_radio_mnf = 0x5205017f;
		}
	}

	sc->sc_radio_rev = (sc->sc_radio_mnf & 0xf0000000) >> 28;
	sc->sc_radio_ver = (sc->sc_radio_mnf & 0x0ffff000) >> 12;

	DPRINTF(("%s: Radio Rev %d, Ver 0x%x, Manuf 0x%x\n",
	    sc->sc_dev.dv_xname, sc->sc_radio_rev, sc->sc_radio_ver,
	    sc->sc_radio_mnf & 0xfff));

	error = bcw_validatechipaccess(sc);
	if (error) {
		printf("%s: failed Chip Access Validation at %d\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}

	/* Test for valid PHY/revision combinations, probably a simpler way */
	if (sc->sc_phy_type == BCW_PHY_TYPEA) {
		switch (sc->sc_phy_rev) {
		case 2:
		case 3:
		case 5:
		case 6:
		case 7:
			break;
		default:
			printf("%s: invalid PHY A revision %d\n",
			    sc->sc_dev.dv_xname, sc->sc_phy_rev);
			return;
		}
	}
	if (sc->sc_phy_type == BCW_PHY_TYPEB) {
		switch (sc->sc_phy_rev) {
		case 2:
		case 4:
		case 7:
			break;
		default:
			printf("%s: invalid PHY B revision %d\n",
			    sc->sc_dev.dv_xname, sc->sc_phy_rev);
			return;
		}
	}
	if (sc->sc_phy_type == BCW_PHY_TYPEG) {
		switch(sc->sc_phy_rev) {
		case 1:
		case 2:
		case 4:
		case 6:
		case 7:
		case 8:
			break;
		default:
			printf("%s: invalid PHY G revision %d\n",
			    sc->sc_dev.dv_xname, sc->sc_phy_rev);
			return;
		}
	}

	/* test for valid radio revisions */
	if ((sc->sc_phy_type == BCW_PHY_TYPEA) &
	    (sc->sc_radio_ver != 0x2060)) {
		    	printf("%s: invalid PHY A radio 0x%x\n",
		    	    sc->sc_dev.dv_xname, sc->sc_radio_ver);
		    	return;
	}
	if ((sc->sc_phy_type == BCW_PHY_TYPEB) &
	    ((sc->sc_radio_ver & 0xfff0) != 0x2050)) {
		    	printf("%s: invalid PHY B radio 0x%x\n",
		    	    sc->sc_dev.dv_xname, sc->sc_radio_ver);
		    	return;
	}
	if ((sc->sc_phy_type == BCW_PHY_TYPEG) &
	    (sc->sc_radio_ver != 0x2050)) {
		    	printf("%s: invalid PHY G radio 0x%x\n",
		    	    sc->sc_dev.dv_xname, sc->sc_radio_ver);
		    	return;
	}

	bcw_radio_off(sc);

	/* Read antenna gain from SPROM and multiply by 4 */
	sbval = BCW_READ16(sc, BCW_SPROM_ANTGAIN);
	/* If unset, assume 2 */
	if ((sbval == 0) || (sbval == 0xffff))
		sbval = 0x0202;
	if (sc->sc_phy_type == BCW_PHY_TYPEA)
		sc->sc_radio_gain = (sbval & 0xff);
	else
		sc->sc_radio_gain = ((sbval & 0xff00) >> 8);
	sc->sc_radio_gain *= 4;

	/*
	 * Set the paXbY vars, X=0 for PHY A, X=1 for B/G, but we'll
	 * just grab them all while we're here
	 */
	sc->sc_radio_pa0b0 = BCW_READ16(sc, BCW_SPROM_PA0B0);
	sc->sc_radio_pa0b1 = BCW_READ16(sc, BCW_SPROM_PA0B1);
	    
	sc->sc_radio_pa0b2 = BCW_READ16(sc, BCW_SPROM_PA0B2);
	sc->sc_radio_pa1b0 = BCW_READ16(sc, BCW_SPROM_PA1B0);
	    
	sc->sc_radio_pa1b1 = BCW_READ16(sc, BCW_SPROM_PA1B1);
	sc->sc_radio_pa1b2 = BCW_READ16(sc, BCW_SPROM_PA1B2);

	/* Get the idle TSSI */
	sbval = BCW_READ16(sc, BCW_SPROM_IDLETSSI);
	if (sc->sc_phy_type == BCW_PHY_TYPEA)
		sc->sc_idletssi = (sbval & 0xff);
	else
		sc->sc_idletssi = ((sbval & 0xff00) >> 8);
	
	/* Init the Microcode Flags Bitfield */
	/* http://bcm-specs.sipsolutions.net/MicrocodeFlagsBitfield */
	sbval = 0;
	if ((sc->sc_phy_type == BCW_PHY_TYPEA) ||
	    (sc->sc_phy_type == BCW_PHY_TYPEB) ||
	    (sc->sc_phy_type == BCW_PHY_TYPEG))
		sbval |= 2; /* Turned on during init for non N phys */
	if ((sc->sc_phy_type == BCW_PHY_TYPEG) &&
	    (sc->sc_phy_rev == 1))
		sbval |= 0x20;
	if ((sc->sc_phy_type == BCW_PHY_TYPEG) &&
	    ((sc->sc_boardflags & BCW_BF_PACTRL) == BCW_BF_PACTRL))
		sbval |= 0x40;
	if ((sc->sc_phy_type == BCW_PHY_TYPEG) &&
	    (sc->sc_phy_rev < 3))
		sbval |= 0x8; /* MAGIC */
	if ((sc->sc_boardflags & BCW_BF_XTAL) == BCW_BF_XTAL)
		sbval |= 0x400;
	if (sc->sc_phy_type == BCW_PHY_TYPEB)
		sbval |= 0x4;
	if ((sc->sc_radio_ver == 0x2050) &&
	    (sc->sc_radio_rev <= 5))
	    	sbval |= 0x40000;
	/*
	 * XXX If the device isn't up and this is a PCI bus with revision
	 * 10 or less set bit 0x80000
	 */

	/*
	 * Now, write the value into the regster
	 *
	 * The MicrocodeBitFlags is an unaligned 32bit value in SHM, so the
	 * strategy is to select the aligned word for the lower 16 bits,
	 * but write to the unaligned address. Then, because the SHM
	 * pointer is automatically incremented to the next aligned word,
	 * we can just write the remaining bits as a 16 bit write.
	 * This explanation could make more sense, but an SHM read/write
	 * wrapper of some sort would be better.
	 */
	BCW_WRITE(sc, BCW_SHM_CONTROL,
	    (BCW_SHM_CONTROL_SHARED << 16) + BCW_SHM_MICROCODEFLAGSLOW - 2);
	BCW_WRITE16(sc, BCW_SHM_DATAHIGH, sbval & 0x00ff);
	BCW_WRITE16(sc, BCW_SHM_DATALOW, (sbval & 0xff00)>>16);

	/*
	 * Initialize the TSSI to DBM table
	 * The method is described at
	 * http://bcm-specs.sipsolutions.net/TSSI_to_DBM_Table
	 * but I suspect there's a standard way to do it in the 80211 stuff
	 */

	/*
	 * XXX TODO still for the card attach:
	 * - Disable the 80211 Core (and wrapper for on/off)
	 * - Setup LEDs to blink in whatever fashionable manner
	 */
	//bcw_powercontrol_crystal_off(sc);	/* TODO Fix panic! */

	/*
	 * Allocate DMA-safe memory for ring descriptors.
	 * The receive, and transmit rings are 4k aligned
	 */
	bcw_alloc_rx_ring(sc, &sc->sc_rxring, BCW_RX_RING_COUNT);
	bcw_alloc_tx_ring(sc, &sc->sc_txring, BCW_TX_RING_COUNT);

	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA; /* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities - keep it simple */
	ic->ic_caps = IEEE80211_C_IBSS; /* IBSS mode supported */

	/* MAC address */
	if (sc->sc_phy_type == BCW_PHY_TYPEA) {
		i = BCW_READ16(sc, BCW_SPROM_ET1MACADDR);
		ic->ic_myaddr[0] = (i & 0xff00) >> 8;
		ic->ic_myaddr[1] = i & 0xff;
		i = BCW_READ16(sc, BCW_SPROM_ET1MACADDR + 2);
		    
		ic->ic_myaddr[2] = (i & 0xff00) >> 8;
		ic->ic_myaddr[3] = i & 0xff;
		i = BCW_READ16(sc, BCW_SPROM_ET1MACADDR + 4);
		    
		ic->ic_myaddr[4] = (i & 0xff00) >> 8;
		ic->ic_myaddr[5] = i & 0xff;
	} else { /* assume B or G PHY */
		i = BCW_READ16(sc, BCW_SPROM_IL0MACADDR);
		    
		ic->ic_myaddr[0] = (i & 0xff00) >> 8;
		ic->ic_myaddr[1] = i & 0xff;
		i = BCW_READ16(sc, BCW_SPROM_IL0MACADDR + 2);
		    
		ic->ic_myaddr[2] = (i & 0xff00) >> 8;
		ic->ic_myaddr[3] = i & 0xff;
		i = BCW_READ16(sc, BCW_SPROM_IL0MACADDR + 4);
		    
		ic->ic_myaddr[4] = (i & 0xff00) >> 8;
		ic->ic_myaddr[5] = i & 0xff;
	}
	
	printf(", address %s\n", ether_sprintf(ic->ic_myaddr));

	/* Set supported rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	/* Set supported channels */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	/* IBSS channel undefined for now */
	ic->ic_ibss_chan = &ic->ic_channels[0];

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = bcw_init;
	ifp->if_ioctl = bcw_ioctl;
	ifp->if_start = bcw_start;
	ifp->if_watchdog = bcw_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	/* Attach the interface */
	if_attach(ifp);
	ieee80211_ifattach(ifp);

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = bcw_newstate;
	ieee80211_media_init(ifp, bcw_media_change, bcw_media_status);

	timeout_set(&sc->sc_timeout, bcw_tick, sc);
}

/* handle media, and ethernet requests */
int
bcw_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct bcw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifreq   *ifr = (struct ifreq *) data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			//bcw_init(ifp);
			/* XXX arp_ifinit(&sc->bcw_ac, ifa); */
			break;
#endif /* INET */
		default:
			//bcw_init(ifp);
			break;
		}
		break;
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) &&
		    (!(ifp->if_flags & IFF_RUNNING)))
				bcw_init(ifp);
		else if (ifp->if_flags & IFF_RUNNING)
			bcw_stop(ifp, 1);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &ic->ic_ac) :
		    ether_delmulti(ifr, &ic->ic_ac);

		if (error == ENETRESET)
			error = 0;
		break;

	case SIOCG80211TXPOWER:
		/*
		 * If the hardware radio transmitter switch is off, report a
		 * tx power of IEEE80211_TXPOWER_MIN to indicate that radio
		 * transmitter is killed.
		 */
		break;

	default:
		error = ieee80211_ioctl(ifp, cmd, data);
		break;
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			bcw_init(ifp);
		error = 0;
	}

	splx(s);
	return error;
}

/* Start packet transmission on the interface. */
void
bcw_start(struct ifnet *ifp)
{
#if 0
	struct bcw_softc *sc = ifp->if_softc;
	struct mbuf    *m0;
	bus_dmamap_t    dmamap;
	int		txstart;
	int		txsfree;
	int		error;
#endif
	int		newpkts = 0;

	/*
	 * do not start another if currently transmitting, and more
	 * descriptors(tx slots) are needed for next packet.
	 */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

#if 0   /* FIXME */
	/* determine number of descriptors available */
	if (sc->sc_txsnext >= sc->sc_txin)
		txsfree = BCW_NTXDESC - 1 + sc->sc_txin - sc->sc_txsnext;
	else
		txsfree = sc->sc_txin - sc->sc_txsnext - 1;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	while (txsfree > 0) {
		int	seg;

		/* Grab a packet off the queue. */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		/* get the transmit slot dma map */
		dmamap = sc->sc_cdata.bcw_tx_map[sc->sc_txsnext];

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we
		 * were short on resources. If the packet will not fit,
		 * it will be dropped. If short on resources, it will
		 * be tried again later.
		 */
		error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_WRITE | BUS_DMA_NOWAIT);
		if (error == EFBIG) {
			printf("%s: Tx packet consumes too many DMA segments, "
			    "dropping...\n", sc->sc_dev.dv_xname);
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			m_freem(m0);
			ifp->if_oerrors++;
			continue;
		} else if (error) {
			/* short on resources, come back later */
			printf("%s: unable to load Tx buffer, error = %d\n",
			    sc->sc_dev.dv_xname, error);
			break;
		}
		/* If not enough descriptors available, try again later */
		if (dmamap->dm_nsegs > txsfree) {
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			break;
		}
		/* WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET. */

		/* So take it off the queue */
		IFQ_DEQUEUE(&ifp->if_snd, m0);

		/* save the pointer so it can be freed later */
		sc->sc_cdata.bcw_tx_chain[sc->sc_txsnext] = m0;

		/* Sync the data DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/* Initialize the transmit descriptor(s). */
		txstart = sc->sc_txsnext;
		for (seg = 0; seg < dmamap->dm_nsegs; seg++) {
			uint32_t ctrl;

			ctrl = dmamap->dm_segs[seg].ds_len & CTRL_BC_MASK;
			if (seg == 0)
				ctrl |= CTRL_SOF;
			if (seg == dmamap->dm_nsegs - 1)
				ctrl |= CTRL_EOF;
			if (sc->sc_txsnext == BCW_NTXDESC - 1)
				ctrl |= CTRL_EOT;
			ctrl |= CTRL_IOC;
			sc->bcw_tx_ring[sc->sc_txsnext].ctrl = htole32(ctrl);
			/* MAGIC */
			sc->bcw_tx_ring[sc->sc_txsnext].addr =
			    htole32(dmamap->dm_segs[seg].ds_addr + 0x40000000);
			if (sc->sc_txsnext + 1 > BCW_NTXDESC - 1)
				sc->sc_txsnext = 0;
			else
				sc->sc_txsnext++;
			txsfree--;
		}
		/* sync descriptors being used */
		bus_dmamap_sync(sc->sc_dmat, sc->sc_ring_map,
		    sizeof(struct bcw_dma_slot) * txstart + PAGE_SIZE,
		    sizeof(struct bcw_dma_slot) * dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Give the packet to the chip. */
		BCW_WRITE(sc, BCW_DMA_DPTR,
		    sc->sc_txsnext * sizeof(struct bcw_dma_slot));

		newpkts++;

#if NBPFILTER > 0
		/* Pass the packet to any BPF listeners. */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif				/* NBPFILTER > 0 */
	}
	if (txsfree == 0) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}
#endif /* FIXME */
	if (newpkts) {
		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/* Watchdog timer handler. */
void
bcw_watchdog(struct ifnet *ifp)
{
	struct bcw_softc *sc = ifp->if_softc;

	printf("%s: device timeout\n", sc->sc_dev.dv_xname);
	ifp->if_oerrors++;

	(void) bcw_init(ifp);

	/* Try to get more packets going. */
	bcw_start(ifp);
}

int
bcw_intr(void *xsc)
{
	struct bcw_softc *sc;
	struct ifnet *ifp;
	uint32_t intstatus;
	int wantinit;
	int handled = 0;

	sc = xsc;

	for (wantinit = 0; wantinit == 0;) {
		intstatus = (sc->sc_conf_read)(sc, BCW_INT_STS);

		/* ignore if not ours, or unsolicited interrupts */
		intstatus &= sc->sc_intmask;
		if (intstatus == 0)
			break;

		handled = 1;

		/* Ack interrupt */
		(sc->sc_conf_write)(sc, BCW_INT_STS, intstatus);

		/* Receive interrupts. */
		if (intstatus & I_RI)
			bcw_rxintr(sc);
		/* Transmit interrupts. */
		if (intstatus & I_XI)
			bcw_txintr(sc);
		/* Error interrupts */
		if (intstatus & ~(I_RI | I_XI)) {
			if (intstatus & I_XU)
				printf("%s: transmit fifo underflow\n",
				    sc->sc_dev.dv_xname);
			if (intstatus & I_RO) {
				printf("%s: receive fifo overflow\n",
				    sc->sc_dev.dv_xname);
				ifp->if_ierrors++;
			}
			if (intstatus & I_RU)
				printf("%s: receive descriptor underflow\n",
				    sc->sc_dev.dv_xname);
			if (intstatus & I_DE)
				printf("%s: descriptor protocol error\n",
				    sc->sc_dev.dv_xname);
			if (intstatus & I_PD)
				printf("%s: data error\n",
				    sc->sc_dev.dv_xname);
			if (intstatus & I_PC)
				printf("%s: descriptor error\n",
				    sc->sc_dev.dv_xname);
			if (intstatus & I_TO)
				printf("%s: general purpose timeout\n",
				    sc->sc_dev.dv_xname);
			wantinit = 1;
		}
	}

	if (handled) {
		if (wantinit)
			bcw_init(ifp);
		/* Try to get more packets going. */
		bcw_start(ifp);
	}

	return (handled);
}

/* Receive interrupt handler */
void
bcw_rxintr(struct bcw_softc *sc)
{
#if 0
	struct rx_pph *pph;
	struct mbuf *m;
	int len;
	int i;
#endif
	int curr;

	/* get pointer to active receive slot */
	curr = BCW_READ(sc, BCW_DMA_RXSTATUS(0)) & RS_CD_MASK;
	curr = curr / sizeof(struct bcw_dma_slot);
	if (curr >= BCW_RX_RING_COUNT)
		curr = BCW_RX_RING_COUNT - 1;

#if 0
	/* process packets up to but not current packet being worked on */
	for (i = sc->sc_rxin; i != curr;
	    i + 1 > BCW_NRXDESC - 1 ? i = 0 : i++) {
		/* complete any post dma memory ops on packet */
		bus_dmamap_sync(sc->sc_dmat, sc->sc_cdata.bcw_rx_map[i], 0,
		    sc->sc_cdata.bcw_rx_map[i]->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);

		/*
		 * If the packet had an error, simply recycle the buffer,
		 * resetting the len, and flags.
		 */
		pph = mtod(sc->sc_cdata.bcw_rx_chain[i], struct rx_pph *);
		if (pph->flags & (RXF_NO | RXF_RXER | RXF_CRC | RXF_OV)) {
			/* XXX Increment input error count */
			pph->len = 0;
			pph->flags = 0;
			continue;
		}
		/* receive the packet */
		len = pph->len;
		if (len == 0)
			continue;	/* no packet if empty */
		pph->len = 0;
		pph->flags = 0;
		/* bump past pre header to packet */
		sc->sc_cdata.bcw_rx_chain[i]->m_data +=
		    BCW_PREPKT_HEADER_SIZE;

 		/*
		 * The chip includes the CRC with every packet.  Trim
		 * it off here.
		 */
		len -= ETHER_CRC_LEN;

		/*
		 * If the packet is small enough to fit in a
		 * single header mbuf, allocate one and copy
		 * the data into it.  This greatly reduces
		 * memory consumption when receiving lots
		 * of small packets.
		 *
		 * Otherwise, add a new buffer to the receive
		 * chain.  If this fails, drop the packet and
		 * recycle the old buffer.
		 */
		if (len <= (MHLEN - 2)) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL)
				goto dropit;
			m->m_data += 2;
			memcpy(mtod(m, caddr_t),
			    mtod(sc->sc_cdata.bcw_rx_chain[i], caddr_t), len);
			sc->sc_cdata.bcw_rx_chain[i]->m_data -=
			    BCW_PREPKT_HEADER_SIZE;
		} else {
			m = sc->sc_cdata.bcw_rx_chain[i];
			if (bcw_add_rxbuf(sc, i) != 0) {
		dropit:
				/* XXX increment wireless input error counter */
				/* continue to use old buffer */
				sc->sc_cdata.bcw_rx_chain[i]->m_data -=
				    BCW_PREPKT_HEADER_SIZE;
				bus_dmamap_sync(sc->sc_dmat,
				    sc->sc_cdata.bcw_rx_map[i], 0,
				    sc->sc_cdata.bcw_rx_map[i]->dm_mapsize,
				    BUS_DMASYNC_PREREAD);
				continue;
			}
		}

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;
		/* XXX Increment input packet count */

#if NBPFILTER > 0
		/*
		 * Pass this up to any BPF listeners, but only
		 * pass it up the stack if it's for us.
		 *
		 * if (ifp->if_bpf)
		 *	bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
		 */
#endif				/* NBPFILTER > 0 */

		/* XXX Pass it on. */
		//ether_input_mbuf(ifp, m);

		/* re-check current in case it changed */
		curr = (BCW_READ(sc, BCW_DMA_RXSTATUS) & RS_CD_MASK) /
		    sizeof(struct bcw_dma_slot);
		if (curr >= BCW_NRXDESC)
			curr = BCW_NRXDESC - 1;
	}
	sc->sc_rxin = curr;
#endif
}

/* Transmit interrupt handler */
void
bcw_txintr(struct bcw_softc *sc)
{
//	struct ifnet *ifp = &sc->bcw_ac.ac_if;
	int curr;
//	int i;

//	ifp->if_flags &= ~IFF_OACTIVE;

#if 0
	/*
	 * Go through the Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	curr = BCW_READ(sc, BCW_DMA_TXSTATUS) & RS_CD_MASK;
	curr = curr / sizeof(struct bcw_dma_slot);
	if (curr >= BCW_NTXDESC)
		curr = BCW_NTXDESC - 1;
	for (i = sc->sc_txin; i != curr;
	    i + 1 > BCW_NTXDESC - 1 ? i = 0 : i++) {
		/* do any post dma memory ops on transmit data */
		if (sc->sc_cdata.bcw_tx_chain[i] == NULL)
			continue;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_cdata.bcw_tx_map[i], 0,
		    sc->sc_cdata.bcw_tx_map[i]->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, sc->sc_cdata.bcw_tx_map[i]);
		m_freem(sc->sc_cdata.bcw_tx_chain[i]);
		sc->sc_cdata.bcw_tx_chain[i] = NULL;
		ifp->if_opackets++;
	}
#endif
	sc->sc_txin = curr;

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer
	 */
//	if (sc->sc_txsnext == sc->sc_txin)
//		ifp->if_timer = 0;
}

/* initialize the interface */
int
bcw_init(struct ifnet *ifp)
{
	struct bcw_softc *sc = ifp->if_softc;
	uint16_t val16;
	int error, i;

	BCW_WRITE(sc, BCW_SBF, BCW_SBF_CORE_READY | BCW_SBF_400_MAGIC);

	/* load firmware */
	if ((error = bcw_load_firmware(sc)))
		return (error);

	/*
	 * verify firmware revision
	 */
	BCW_WRITE(sc, BCW_GIR, 0xffffffff);
	BCW_WRITE(sc, BCW_SBF, 0x00020402);
	for (i = 0; i < 50; i++) {
		if (BCW_READ(sc, BCW_GIR) == BCW_INTR_READY)
			break;
		delay(10);
	}
	if (i == 50) {
		printf("%s: interrupt-ready timeout!\n", sc->sc_dev.dv_xname);
		return (1);
	}
	BCW_READ(sc, BCW_GIR);	/* dummy read */

	val16 = bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_REVISION);

	DPRINTF(("%s: Firmware revision 0x%x, patchlevel 0x%x "
            "(20%.2i-%.2i-%.2i %.2i:%.2i:%.2i)\n",
	    sc->sc_dev.dv_xname, val16,
	    bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_PATCHLEVEL),
            (bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_DATE) >> 12)
            & 0xf,
            (bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_DATE) >> 8)
            & 0xf,
            bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_DATE)
            & 0xff,
            (bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_TIME) >> 11)
            & 0x1f,
            (bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_TIME) >> 5)
            & 0x3f,
            bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_TIME)
	    & 0x1f));

	if (val16 > 0x128) {
		printf("%s: no support for this firmware revision!\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}

	/* initialize GPIO */
	if ((error = bcw_gpio_init(sc)))
		return (error);

	/* load init values */
	if ((error = bcw_load_initvals(sc)))
		return (error);

	/* turn radio on */
	bcw_radio_on(sc);

	BCW_WRITE16(sc, 0x03e6, 0);
	//if ((error = bcw_phy_init(sc)))
		//return (error);

	return (0);

	/* Cancel any pending I/O. */
	bcw_stop(ifp, 0);

	/*
	 * Most of this needs to be rewritten to take into account the
	 * possible single/multiple core nature of the BCM43xx, and the
	 * differences from the BCM44xx ethernet chip that if_bce.c is
	 * written for.
	 */

	/* enable pci interrupts, bursts, and prefetch */

	/* remap the pci registers to the Sonics config registers */
#if 0
	/* XXX - use (sc->sc_conf_read/write) */
	/* save the current map, so it can be restored */
	reg_win = BCW_READ(sc, BCW_REG0_WIN);

	/* set register window to Sonics registers */
	BCW_WRITE(sc, BCW_REG0_WIN, BCW_SONICS_WIN);

	/* enable SB to PCI interrupt */
	BCW_WRITE(sc, BCW_SBINTVEC, BCW_READ(sc, BCW_SBINTVEC) | SBIV_ENET0);

	/* enable prefetch and bursts for sonics-to-pci translation 2 */
	BCW_WRITE(sc, BCW_SPCI_TR2,
	    BCW_READ(sc, BCW_SPCI_TR2) | SBTOPCI_PREF | SBTOPCI_BURST);

	/* restore to ethernet register space */
	BCW_WRITE(sc, BCW_REG0_WIN, reg_win);

	/* Reset the chip to a known state. */
	bcw_reset(sc);
#endif

#if 0 /* FIXME */
	/* Initialize transmit descriptors */
	memset(sc->bcw_tx_ring, 0, BCW_NTXDESC * sizeof(struct bcw_dma_slot));
	sc->sc_txsnext = 0;
	sc->sc_txin = 0;
#endif

	/* enable crc32 generation and set proper LED modes */
	BCW_WRITE(sc, BCW_MACCTL,
	    BCW_READ(sc, BCW_MACCTL) | BCW_EMC_CRC32_ENAB | BCW_EMC_LED);
	    
	/* reset or clear powerdown control bit  */
	BCW_WRITE(sc, BCW_MACCTL, BCW_READ(sc, BCW_MACCTL) & ~BCW_EMC_PDOWN);

	/* setup DMA interrupt control */
	BCW_WRITE(sc, BCW_DMAI_CTL, 1 << 24);	/* MAGIC */

	/* setup packet filter */
	bcw_set_filter(ifp);

	/* set max frame length, account for possible VLAN tag */
	BCW_WRITE(sc, BCW_RX_MAX, ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN);
	BCW_WRITE(sc, BCW_TX_MAX, ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN);

	/* set tx watermark */
	BCW_WRITE(sc, BCW_TX_WATER, 56);

	/* enable transmit */
	BCW_WRITE(sc, BCW_DMA_TXCTL, XC_XE);

	/*
	 * Give the receive ring to the chip, and
	 * start the receive DMA engine.
	 */
	sc->sc_rxin = 0;

	/* enable receive */
	BCW_WRITE(sc, BCW_DMA_RXCTL, BCW_PREPKT_HEADER_SIZE << 1 | 1);

	/* Enable interrupts */
	sc->sc_intmask =
	    I_XI | I_RI | I_XU | I_RO | I_RU | I_DE | I_PD | I_PC | I_TO;
	BCW_WRITE(sc, BCW_INT_MASK, sc->sc_intmask);
	    
#if 0
	/* FIXME */
	/* start the receive dma */
	BCW_WRITE(sc, BCW_DMA_RXDPTR,
	    BCW_NRXDESC * sizeof(struct bcw_dma_slot));
#endif

	/* set media */
	//mii_mediachg(&sc->bcw_mii);
#if 0
	/* turn on the ethernet mac */
	BCW_WRITE(sc, BCW_ENET_CTL, BCW_READ(sc, BCW_ENET_CTL) | EC_EE);
	    
#endif
	/* start timer */
	timeout_add(&sc->sc_timeout, hz);

	/* mark as running, and no outputs active */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return (0);
}

/* Add a receive buffer to the indiciated descriptor. */
int
bcw_add_rxbuf(struct bcw_softc *sc, int idx)
{
#if 0
	struct mbuf *m;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}
	if (sc->sc_cdata.bcw_rx_chain[idx] != NULL)
		bus_dmamap_unload(sc->sc_dmat,
		    sc->sc_cdata.bcw_rx_map[idx]);

	sc->sc_cdata.bcw_rx_chain[idx] = m;

	error = bus_dmamap_load(sc->sc_dmat, sc->sc_cdata.bcw_rx_map[idx],
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ | BUS_DMA_NOWAIT);
	if (error)
		return (error);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_cdata.bcw_rx_map[idx], 0,
	    sc->sc_cdata.bcw_rx_map[idx]->dm_mapsize, BUS_DMASYNC_PREREAD);

	BCW_INIT_RXDESC(sc, idx);

	return (0);
#endif
	return (1);

}

/* Drain the receive queue. */
void
bcw_rxdrain(struct bcw_softc *sc)
{
#if 0
	/* FIXME */
	int i;

	for (i = 0; i < BCW_NRXDESC; i++) {
		if (sc->sc_cdata.bcw_rx_chain[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->sc_cdata.bcw_rx_map[i]);
			m_freem(sc->sc_cdata.bcw_rx_chain[i]);
			sc->sc_cdata.bcw_rx_chain[i] = NULL;
		}
	}
#endif
}

/* Stop transmission on the interface */
void
bcw_stop(struct ifnet *ifp, int disable)
{
	struct bcw_softc *sc = ifp->if_softc;
	//uint32_t val;

	/* Stop the 1 second timer */
	timeout_del(&sc->sc_timeout);

	/* Mark the interface down and cancel the watchdog timer. */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	/* Disable interrupts. */
	BCW_WRITE(sc, BCW_INT_MASK, 0);
	sc->sc_intmask = 0;
	delay(10);

	/* Disable emac */
#if 0
	BCW_WRITE(sc, BCW_ENET_CTL, EC_ED);
	for (i = 0; i < 200; i++) {
		val = BCW_READ(sc, BCW_ENET_CTL);
		    
		if (!(val & EC_ED))
			break;
		delay(10);
	}
#endif
	/* Stop the DMA */
	BCW_WRITE(sc, BCW_DMA_RXCONTROL(0), 0);
	BCW_WRITE(sc, BCW_DMA_TXCONTROL(0), 0);
	delay(10);

#if 0	/* FIXME */
	/* Release any queued transmit buffers. */
	for (i = 0; i < BCW_NTXDESC; i++) {
		if (sc->sc_cdata.bcw_tx_chain[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->sc_cdata.bcw_tx_map[i]);
			m_freem(sc->sc_cdata.bcw_tx_chain[i]);
			sc->sc_cdata.bcw_tx_chain[i] = NULL;
		}
	}
#endif

	/* drain receive queue */
	if (disable)
		bcw_rxdrain(sc);
}

/* reset the chip */
void
bcw_reset(struct bcw_softc *sc)
{
	uint32_t sbval;
	uint32_t reject;

	/*
	 * Figure out what revision the Sonic Backplane is, as the position
	 * of the Reject bit changes.
	 */
	sbval = BCW_READ(sc, BCW_CIR_SBID_LO);
	sc->sc_sbrev = (sbval & SBREV_MASK) >> SBREV_MASK_SHIFT;

	switch (sc->sc_sbrev) {
	case 0:
		reject = SBTML_REJ22;
		break;
	case 1:
		reject = SBTML_REJ23;
		break;
	default:
		reject = SBTML_REJ22 | SBTML_REJ23;
	}

	sbval = BCW_READ(sc, BCW_SBTMSTATELOW);

	/*
	 * If the 802.11 core is enabled, only clock of clock,reset,reject
	 * will be set, and we need to reset all the DMA engines first.
	 */
	bcw_change_core(sc, sc->sc_core_80211->num);

	sbval = BCW_READ(sc, BCW_SBTMSTATELOW);
#if 0
	if ((sbval & (SBTML_RESET | reject | SBTML_CLK)) == SBTML_CLK) {
		/* XXX Stop all DMA */
		/* XXX reset the dma engines */
	}
	/* XXX Cores are reset manually elsewhere for now */
	/* Reset the wireless core, attaching the PHY */
	bcw_reset_core(sc, SBTML_80211FLAG | SBTML_80211PHY );
	bcw_change_core(sc, sc->sc_core_common->num);
	bcw_reset_core(sc, 0);
	bcw_change_core(sc, sc->sc_core_bus->num);
	bcw_reset_core(sc, 0);
#endif
	/* XXX update PHYConnected to requested value */

	/* Clear Baseband Attenuation, might only work for B/G rev < 0 */
	BCW_WRITE16(sc, BCW_RADIO_BASEBAND, 0);

	/* Set 0x400 in the MMIO StatusBitField reg */
	sbval = BCW_READ(sc, BCW_SBF);
	sbval |= BCW_SBF_400_MAGIC;
	BCW_WRITE(sc, BCW_SBF, sbval);

	/* XXX Clear saved interrupt status for DMA controllers */

	/*
	 * XXX Attach cores to the backplane, if we have more than one
	 * Don't attach PCMCIA cores on a PCI card, and reverse?
	 * OR together the bus flags of the 3 cores and write to PCICR
	 */
#if 0
	if (sc->sc_havecommon == 1) {
		sbval = (sc->sc_conf_read)(sc, BCW_PCICR);
		sbval |= 0x1 << 8; /* XXX hardcoded bitmask of single core */
		(sc->sc_conf_write)(sc, BCW_PCICR, sbval);
	}
#endif
	/* Change back to the Wireless core */
	bcw_change_core(sc, sc->sc_core_80211->num);

}

/* Set up the receive filter. */
void
bcw_set_filter(struct ifnet *ifp)
{
#if 0
	struct bcw_softc *sc = ifp->if_softc;

	if (ifp->if_flags & IFF_PROMISC) {
		ifp->if_flags |= IFF_ALLMULTI;
		BCW_WRITE(sc, BCW_RX_CTL, BCW_READ(sc, BCW_RX_CTL) | ERC_PE);
	} else {
		ifp->if_flags &= ~IFF_ALLMULTI;

		/* turn off promiscuous */
		BCW_WRITE(sc, BCW_RX_CTL, BCW_READ(sc, BCW_RX_CTL) & ~ERC_PE);

		/* enable/disable broadcast */
		if (ifp->if_flags & IFF_BROADCAST)
			BCW_WRITE(sc, BCW_RX_CTL,
			    BCW_READ(sc, BCW_RX_CTL) & ~ERC_DB);
		else
			BCW_WRITE(sc, BCW_RX_CTL,
			    BCW_READ(sc, BCW_RX_CTL) | ERC_DB);

		/* disable the filter */
		BCW_WRITE(sc, BCW_FILT_CTL, 0);

		/* add our own address */
		// bcw_add_mac(sc, sc->bcw_ac.ac_enaddr, 0);

		/* for now accept all multicast */
		BCW_WRITE(sc, BCW_RX_CTL, BCW_READ(sc, BCW_RX_CTL) | ERC_AM);

		ifp->if_flags |= IFF_ALLMULTI;

		/* enable the filter */
		BCW_WRITE(sc, BCW_FILT_CTL, BCW_READ(sc, BCW_FILT_CTL) | 1);
	}
#endif
}

int
bcw_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
#if 0
	struct bcw_softc *sc = ic->ic_softc;
	enum ieee80211_state ostate;
	uint32_t tmp;

	ostate = ic->ic_state;
#endif
	return (0);
}

int
bcw_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		bcw_init(ifp);

	return (0);
}

	void
bcw_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct bcw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	//uint32_t val;
	int rate;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->ifm_status |= IFM_ACTIVE;

	/*
	 * XXX Read current transmission rate from the adapter.
	 */
	//val = CSR_READ_4(sc, IWI_CSR_CURRENT_TX_RATE);
	/* convert PLCP signal to 802.11 rate */
	//rate = bcw_rate(val);
	rate = 0;

	imr->ifm_active |= ieee80211_rate2media(ic, rate, ic->ic_curmode);
	switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			break;
		case IEEE80211_M_IBSS:
			imr->ifm_active |= IFM_IEEE80211_ADHOC;
			break;
		case IEEE80211_M_MONITOR:
			imr->ifm_active |= IFM_IEEE80211_MONITOR;
			break;
		case IEEE80211_M_AHDEMO:
		case IEEE80211_M_HOSTAP:
			/* should not get there */
			break;
	}
}

/* One second timer, checks link status */
void
bcw_tick(void *v)
{
#if 0
	struct bcw_softc *sc = v;
	/* http://bcm-specs.sipsolutions.net/PeriodicTasks */
	timeout_add(&sc->bcw_timeout, hz);
#endif
}

/*
 * Validate Chip Access
 */
int
bcw_validatechipaccess(struct bcw_softc *sc)
{
	uint32_t save,val;

	/* Make sure we're dealing with the wireless core */
	bcw_change_core(sc, sc->sc_core_80211->num);

	/*
	 * We use the offset of zero a lot here to reset the SHM pointer to the
	 * beginning of it's memory area, as it automatically moves on every
	 * access to the SHM DATA registers
	 */

	/* Backup SHM uCode Revision before we clobber it */
	BCW_WRITE(sc, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	save = BCW_READ(sc, BCW_SHM_DATA);

	/* write test value */
	BCW_WRITE(sc, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	BCW_WRITE(sc, BCW_SHM_DATA, 0xaa5555aa);
	/* Read it back */
	BCW_WRITE(sc, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);

	val = BCW_READ(sc, BCW_SHM_DATA);
	if (val != 0xaa5555aa)
		return (1);

	/* write 2nd test value */
	BCW_WRITE(sc, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	BCW_WRITE(sc, BCW_SHM_DATA, 0x55aaaa55);
	/* Read it back */
	BCW_WRITE(sc, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);

	val = BCW_READ(sc, BCW_SHM_DATA);
	if (val != 0x55aaaa55)
		return 2;

	/* Restore the saved value now that we're done */
	BCW_WRITE(sc, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	BCW_WRITE(sc, BCW_SHM_DATA, save);
	if (sc->sc_core_80211->rev >= 3) {
		/* do some test writes and reads against the TSF */
		/*
		 * This works during the attach, but the spec at
		 * http://bcm-specs.sipsolutions.net/Timing
		 * say that we're reading/writing silly places, so these regs
		 * are not quite documented yet
		 */
		BCW_WRITE16(sc, 0x18c, 0xaaaa);
		BCW_WRITE(sc, 0x18c, 0xccccbbbb);
		val = BCW_READ16(sc, 0x604);
		if (val != 0xbbbb) return 3;
		val = BCW_READ16(sc, 0x606);
		if (val != 0xcccc) return 4;
		/* re-clear the TSF since we just filled it with garbage */
		BCW_WRITE(sc, 0x18c, 0x0);
	}

	/* Check the Status Bit Field for some unknown bits */
	val = BCW_READ(sc, BCW_SBF);
	if ((val | 0x80000000) != 0x80000400 ) {
		printf("%s: Warning, SBF is 0x%x, expected 0x80000400\n",
		    sc->sc_dev.dv_xname, val);
		/* May not be a critical failure, just warn for now */
		//return (5);
	}
	/* Verify there are no interrupts active on the core */
	val = BCW_READ(sc, BCW_GIR);
	if (val != 0) {
		DPRINTF(("Failed Pending Interrupt test with val=0x%x\n", val));
		return (6);
	}

	/* Above G means it's unsupported currently, like N */
	if (sc->sc_phy_type > BCW_PHY_TYPEG) {
		DPRINTF(("PHY type %d greater than supported type %d\n",
		    sc->sc_phy_type, BCW_PHY_TYPEG));
		return (7);
	}
	
	return (0);
}

int
bcw_detach(void *arg)
{
	struct bcw_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	timeout_del(&sc->sc_scan_to);

	bcw_stop(ifp, 1);
	ieee80211_ifdetach(ifp);
	if_detach(ifp);
	bcw_free_rx_ring(sc, &sc->sc_rxring);
	bcw_free_tx_ring(sc, &sc->sc_txring);

	return (0);
}

#if 0
void
bcw_free_ring(struct bcw_softc *sc, struct bcw_dma_slot *ring)
{
	struct bcw_chain data *data;
	struct bcw_dma_slot *bcwd;
	int i;

	if (sc->bcw_rx_chain != NULL) {
		for (i = 0; i < BCW_NRXDESC; i++) {
			bcwd = &sc->bcw_rx_ring[i];

			if (sc->bcw_rx_chain[i] != NULL) {
				bus_dmamap_sync(sc->sc_dmat,
				    sc->bcw_ring_map,
				    sizeof(struct bcw_dma_slot) * x,
				    sizeof(struct bcw_dma_slot),
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->sc_dmat,
				    sc->bcw_ring_map);
				m_freem(sc->bcw_rx_chain[i]);
			}

			if (sc->bcw_ring_map != NULL)
				bus_dmamap_destroy(sc->sc_dmat,
				    sc->bcw_ring_map);
		}
	}
}
#endif

int
bcw_alloc_rx_ring(struct bcw_softc *sc, struct bcw_rx_ring *ring, int count)
{
	struct bcw_desc *desc;
	struct bcw_rx_data *data;
	int i, nsegs, error;

	ring->count = count;
	ring->cur = ring->next = 0;

	error = bus_dmamap_create(sc->sc_dmat,
	    count * sizeof(struct bcw_desc), 1,
	    count * sizeof(struct bcw_desc), 0,
	    BUS_DMA_NOWAIT, &ring->map);
	if (error != 0) {
		printf("%s: could not create desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat,
	    count * sizeof(struct bcw_desc),
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    count * sizeof(struct bcw_desc), (caddr_t *)&ring->desc,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map desc DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, ring->desc,
	    count * sizeof(struct bcw_desc), NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	bzero(ring->desc, count * sizeof(struct bcw_desc));
	ring->physaddr = ring->map->dm_segs->ds_addr;

	ring->data = malloc(count * sizeof (struct bcw_rx_data), M_DEVBUF,
	    M_NOWAIT);
	if (ring->data == NULL) {
		printf("%s: could not allocate soft data\n",
		    sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto fail;
	}

	BCW_WRITE(sc, BCW_DMA_RXADDR, ring->physaddr + 0x40000000);

	/*
	 * Pre-allocate Rx buffers and populate Rx ring.
	 */
	bzero(ring->data, count * sizeof (struct bcw_rx_data));
	for (i = 0; i < count; i++) {
		desc = &ring->desc[i];
		data = &ring->data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &data->map);
		if (error != 0) {
			printf("%s: could not create DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		MCLGET(data->m, M_DONTWAIT);
		if (!(data->m->m_flags & M_EXT)) {
			printf("%s: could not allocate rx mbuf cluster\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("%s: could not load rx buf DMA map",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	
		desc->addr = htole32(data->map->dm_segs->ds_addr);

		if (i != (count - 1))
			desc->ctrl = htole32(BCW_RXBUF_LEN);
		else
			desc->ctrl = htole32(BCW_RXBUF_LEN | CTRL_EOT);
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	return (0);

fail:	bcw_free_rx_ring(sc, ring);
	return (error);
}

void
bcw_reset_rx_ring(struct bcw_softc *sc, struct bcw_rx_ring *ring)
{
	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	ring->cur = ring->next = 0;
}

void
bcw_free_rx_ring(struct bcw_softc *sc, struct bcw_rx_ring *ring)
{
	struct bcw_rx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)ring->desc,
		    ring->count * sizeof(struct bcw_desc));
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(sc->sc_dmat, data->map, 0,
				    data->map->dm_mapsize,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}

			if (data->map != NULL)
				bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
		free(ring->data, M_DEVBUF);
	}
}

int
bcw_alloc_tx_ring(struct bcw_softc *sc, struct bcw_tx_ring *ring,
    int count)
{
	int i, nsegs, error;

	ring->count = count;
	ring->queued = 0;
	ring->cur = ring->next = ring->stat = 0;

	error = bus_dmamap_create(sc->sc_dmat,
	    count * sizeof(struct bcw_desc), 1,
	    count * sizeof(struct bcw_desc), 0, BUS_DMA_NOWAIT, &ring->map);
	if (error != 0) {
		printf("%s: could not create desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat,
	    count * sizeof(struct bcw_desc),
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    count * sizeof(struct bcw_desc), (caddr_t *)&ring->desc,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map desc DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, ring->desc,
	    count * sizeof(struct bcw_desc), NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	memset(ring->desc, 0, count * sizeof(struct bcw_desc));
	ring->physaddr = ring->map->dm_segs->ds_addr;

	/* MAGIC */
	BCW_WRITE(sc, BCW_DMA_TXADDR, ring->physaddr + 0x40000000);

	ring->data = malloc(count * sizeof(struct bcw_tx_data), M_DEVBUF,
	    M_NOWAIT);
	if (ring->data == NULL) {
		printf("%s: could not allocate soft data\n",
		    sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto fail;
	}

	memset(ring->data, 0, count * sizeof(struct bcw_tx_data));
	for (i = 0; i < count; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    BCW_MAX_SCATTER, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &ring->data[i].map);
		if (error != 0) {
			printf("%s: could not create DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	}

	return (0);

fail:	bcw_free_tx_ring(sc, ring);
	return (error);
}

void
bcw_reset_tx_ring(struct bcw_softc *sc, struct bcw_tx_ring *ring)
{
	struct bcw_desc *desc;
	struct bcw_tx_data *data;
	int i;

	for (i = 0; i < ring->count; i++) {
		desc = &ring->desc[i];
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}

		/*
		 * The node has already been freed at that point so don't call
		 * ieee80211_release_node() here.
		 */
		data->ni = NULL;
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	ring->queued = 0;
	ring->cur = ring->next = ring->stat = 0;
}

void
bcw_free_tx_ring(struct bcw_softc *sc, struct bcw_tx_ring *ring)
{
	struct bcw_tx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)ring->desc,
		    ring->count * sizeof(struct bcw_desc));
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(sc->sc_dmat, data->map, 0,
				    data->map->dm_mapsize,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}

			/*
			 * The node has already been freed at that point so
			 * don't call ieee80211_release_node() here.
			 */
			data->ni = NULL;

			if (data->map != NULL)
				bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
		free(ring->data, M_DEVBUF);
	}
}

void
bcw_powercontrol_crystal_on(struct bcw_softc *sc)
{
	uint32_t sbval;

	sbval = (sc->sc_conf_read)(sc, BCW_GPIOI);
	if ((sbval & BCW_XTALPOWERUP) != BCW_XTALPOWERUP) {
		sbval = (sc->sc_conf_read)(sc, BCW_GPIOO);
		sbval |= (BCW_XTALPOWERUP & BCW_PLLPOWERDOWN);
		(sc->sc_conf_write)(sc, BCW_GPIOO, sbval);
		delay(1000);
		sbval = (sc->sc_conf_read)(sc, BCW_GPIOO);
		sbval &= ~BCW_PLLPOWERDOWN;
		(sc->sc_conf_write)(sc, BCW_GPIOO, sbval);
		delay(5000);
	}
}

void
bcw_powercontrol_crystal_off(struct bcw_softc *sc)
{
	uint32_t sbval;

	/* XXX Return if radio is hardware disabled */
	if (sc->sc_chiprev < 5)
		return;
	if ((sc->sc_boardflags & BCW_BF_XTAL) == BCW_BF_XTAL)
		return;

	/* XXX bcw_powercontrol_clock_slow() */

	sbval = (sc->sc_conf_read)(sc, BCW_GPIOO);
	sbval |= BCW_PLLPOWERDOWN;
	sbval &= ~BCW_XTALPOWERUP;
	(sc->sc_conf_write)(sc, BCW_GPIOO, sbval);
	sbval = (sc->sc_conf_read)(sc, BCW_GPIOE);
	sbval |= BCW_PLLPOWERDOWN | BCW_XTALPOWERUP;
	(sc->sc_conf_write)(sc, BCW_GPIOE, sbval);
}

int
bcw_change_core(struct bcw_softc *sc, int changeto)
{
	uint32_t	sbval;
	int		i;

	(sc->sc_conf_write)(sc, BCW_ADDR_SPACE0,
	    BCW_CORE_SELECT(changeto));
	/* loop to see if the selected core shows up */
	for (i = 0; i < 10; i++) {
		sbval = (sc->sc_conf_read)(sc, BCW_ADDR_SPACE0);
		if (sbval == BCW_CORE_SELECT(changeto))
			break;
		delay(10);
	}
	if (i < 10) {
		sc->sc_lastcore = sc->sc_currentcore;
		sc->sc_currentcore = changeto;
		return 1;
	} else
		return 0;
}

void
bcw_radio_off(struct bcw_softc *sc)
{
	/* Magic unexplained values */
	if (sc->sc_phy_type == BCW_PHY_TYPEA) {
		bcw_radio_write16(sc, 0x0004, 0x00ff);
		bcw_radio_write16(sc, 0x0005, 0x00fb);
		bcw_phy_write16(sc, 0x0010, bcw_phy_read16(sc, 0x0010) |
		    0x0008);
		bcw_phy_write16(sc, 0x0011, bcw_phy_read16(sc, 0x0011) |
		    0x0008);
	}
	if (sc->sc_phy_type == BCW_PHY_TYPEB && sc->sc_core_80211->rev >= 5) {
		bcw_phy_write16(sc, 0x0811, bcw_phy_read16(sc, 0x0811) |
		    0x008c);
		bcw_phy_write16(sc, 0x0812, bcw_phy_read16(sc, 0x0812) &
		    0xff73);
	} else
		bcw_phy_write16(sc, 0x0015, 0xaa00);

	DPRINTF(("%s: Radio turned off\n", sc->sc_dev.dv_xname));
}

void
bcw_radio_on(struct bcw_softc *sc)
{
	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
		bcw_radio_write16(sc, 0x0004, 0x00c0);
		bcw_radio_write16(sc, 0x0005, 0x0008);
		bcw_phy_write16(sc, 0x0010, bcw_phy_read16(sc, 0x0010) &
		    0xfff7);
		bcw_phy_write16(sc, 0x0011, bcw_phy_read16(sc, 0x0011) &
		    0xfff7);
		/* TODO bcw_radio_init_2060() */
		break;
	case BCW_PHY_TYPEB:
	case BCW_PHY_TYPEG:
		bcw_phy_write16(sc, 0x0015, 0x8000);
		bcw_phy_write16(sc, 0x0015, 0xcc00);
		bcw_phy_write16(sc, 0x0015, 0); /* XXX check for phy connect */
		if (bcw_radio_channel(sc, BCW_RADIO_DEFAULT_CHANNEL, 1))
			return;
		break;
	default:
		return;
	}

	DPRINTF(("%s: Radio turned on\n", sc->sc_dev.dv_xname)); 
}

int
bcw_radio_channel(struct bcw_softc *sc, uint8_t channel, int spw)
{
	uint16_t freq, tmp, r8;

	freq = 1; /* TODO */

	r8 = bcw_radio_read16(sc, 0x0008);
	BCW_WRITE16(sc, 0x3f0, freq);
	bcw_radio_write16(sc, 0x0008, r8);

	tmp = bcw_radio_read16(sc, 0x002e);
	tmp &= 0x0080;

	bcw_radio_write16(sc, 0x002e, tmp);

	if (freq >= 4920 && freq <= 5500)
		r8 = 3 * freq / 116;

	if (sc->sc_radio_mnf == 0x17f && sc->sc_radio_ver == 0x2060 &&
	    sc->sc_radio_rev == 1) {
		bcw_radio_write16(sc, 0x0007, (r8 << 4) | r8);
		bcw_radio_write16(sc, 0x0020, (r8 << 4) | r8);
		bcw_radio_write16(sc, 0x0021, (r8 << 4) | r8);
		bcw_radio_write16(sc, 0x0022, (bcw_radio_read16(sc, 0x0022) &
		    0x000f) | (r8 << 4));
		bcw_radio_write16(sc, 0x002a, (r8 << 4));
		bcw_radio_write16(sc, 0x002b, (r8 << 4));
		bcw_radio_write16(sc, 0x0008, (bcw_radio_read16(sc, 0x0008) &
		    0x00f0) | (r8 << 4));
		bcw_radio_write16(sc, 0x0029, (bcw_radio_read16(sc, 0x0029) &
		    0xff0f) | 0x00b0);
		bcw_radio_write16(sc, 0x0035, 0x00aa);
		bcw_radio_write16(sc, 0x0036, 0x0085);
		bcw_radio_write16(sc, 0x003a, (bcw_radio_read16(sc, 0x003a) &
		    0xff20) | 0); /* TODO */
		bcw_radio_write16(sc, 0x003d, bcw_radio_read16(sc, 0x003d) &
		    0x00ff);
		bcw_radio_write16(sc, 0x0081, (bcw_radio_read16(sc, 0x0081) &
		    0xff7f) | 0x0080);
		bcw_radio_write16(sc, 0x0035, bcw_radio_read16(sc, 0x0035) &
		    0xffef);
		bcw_radio_write16(sc, 0x0035, (bcw_radio_read16(sc, 0x0035) &
		    0xffef) | 0x0010);

		/* TODO bcw_radio_set_tx_iq() */

		/* TODO bcw_radio_phy_xmitpower() */
	} else {
		if (spw)
			bcw_spw(sc, channel);
		
		BCW_WRITE16(sc, BCW_MMIO_CHANNEL, bcw_chan2freq_bg(channel));

		/* TODO more stuff if channel = 14 */
	}

	delay(8000);

	return (0);
}

void
bcw_spw(struct bcw_softc *sc, uint8_t channel)
{
	if (sc->sc_radio_ver != 0x2050 || sc->sc_radio_rev >= 6)
		/* we do not need the workaround */
		return;

	if (channel <= 10)
		BCW_WRITE16(sc, BCW_MMIO_CHANNEL,
		    bcw_chan2freq_bg(channel + 4));
	else
		BCW_WRITE16(sc, BCW_MMIO_CHANNEL, bcw_chan2freq_bg(1));

	delay(100);

	BCW_WRITE16(sc, BCW_MMIO_CHANNEL, bcw_chan2freq_bg(channel));
}

int
bcw_chan2freq_bg(uint8_t channel)
{
	static const uint16_t freqs_bg[14] = {
	    12, 17, 22, 27,
	    32, 37, 42, 47,
	    52, 57, 62, 67,
	    72, 84 };

	if (channel < 1 && channel > 14)
		return (0);

	return (freqs_bg[channel - 1]);
}

int
bcw_reset_core(struct bcw_softc *sc, uint32_t flags)
{
	uint32_t	sbval, reject, val;
	int		i;

	/*
	 * Figure out what revision the Sonic Backplane is, as the position
	 * of the Reject bit changes.
	 */
	switch (sc->sc_sbrev) {
	case 0:
		reject = SBTML_REJ22;
		break;
	case 1:
		reject = SBTML_REJ23;
		break;
	default:
		reject = SBTML_REJ22 | SBTML_REJ23;
	}

	/* disable core if not in reset */
	if (!(sbval & SBTML_RESET)) {
		/* if the core is not enabled, the clock won't be enabled */
		if (!(sbval & SBTML_CLK)) {
			BCW_WRITE(sc, BCW_SBTMSTATELOW,
			    SBTML_RESET | reject | flags);
			delay(1);
			sbval = BCW_READ(sc, BCW_SBTMSTATELOW);
			goto disabled;

			BCW_WRITE(sc, BCW_SBTMSTATELOW, reject);
			delay(1);
			/* wait until busy is clear */
			for (i = 0; i < 10000; i++) {
				val = BCW_READ(sc, BCW_SBTMSTATEHI);
				if (!(val & SBTMH_BUSY))
					break;
				delay(10);
			}
			if (i == 10000)
				printf("%s: while resetting core, busy did "
				    "not clear\n", sc->sc_dev.dv_xname);

			val = BCW_READ(sc, BCW_CIR_SBID_LO);
			if (val & BCW_CIR_SBID_LO_INITIATOR) {
				sbval = BCW_READ(sc, BCW_SBIMSTATE);
				BCW_WRITE(sc, BCW_SBIMSTATE,
				    sbval | SBIM_REJECT);
				sbval = BCW_READ(sc, BCW_SBIMSTATE);
				delay(1);

				/* wait until busy is clear */
				for (i = 0; i < 10000; i++) {
					val = BCW_READ(sc, BCW_SBTMSTATEHI);
					if (!(val & SBTMH_BUSY))
						break;
					delay(10);
				}
				if (i == 10000)
					printf("%s: while resetting core, busy "
					    "did not clear\n",
					    sc->sc_dev.dv_xname);
			} /* end initiator check */

			/* set reset and reject while enabling the clocks */
			/* XXX why isn't reject in here? */
			BCW_WRITE(sc, BCW_SBTMSTATELOW,
			    SBTML_FGC | SBTML_CLK | SBTML_RESET | flags);
			val = BCW_READ(sc, BCW_SBTMSTATELOW);
			delay(10);

			val = BCW_READ(sc, BCW_CIR_SBID_LO);
			if (val & BCW_CIR_SBID_LO_INITIATOR) {
				sbval = BCW_READ(sc, BCW_SBIMSTATE);
				BCW_WRITE(sc, BCW_SBIMSTATE,
				    sbval & ~SBIM_REJECT);
				sbval = BCW_READ(sc, BCW_SBIMSTATE);
				delay(1);

				/* wait until busy is clear */
				for (i = 0; i < 10000; i++) {
					val = BCW_READ(sc, BCW_SBTMSTATEHI);
					if (!(val & SBTMH_BUSY))
						break;
					delay(10);
				}
				if (i == 10000)
					printf("%s: while resetting core, busy "
					    "did not clear\n",
					    sc->sc_dev.dv_xname);
			} /* end initiator check */

			BCW_WRITE(sc, BCW_SBTMSTATELOW,
			    SBTML_RESET | reject | flags);
			delay(1);
		}
	}

disabled:

	/* This is enabling/resetting the core */
	/* enable clock */
	BCW_WRITE(sc, BCW_SBTMSTATELOW,
	    SBTML_FGC | SBTML_CLK | SBTML_RESET | flags);
	val = BCW_READ(sc, BCW_SBTMSTATELOW);
	delay(1);

	/* clear any error bits that may be on */
	val = BCW_READ(sc, BCW_SBTMSTATEHI);
	if (val & SBTMH_SERR)
		BCW_WRITE(sc, BCW_SBTMSTATEHI, 0);
	val = BCW_READ(sc, BCW_SBIMSTATE);
	if (val & (SBIM_INBANDERR | SBIM_TIMEOUT))
		BCW_WRITE(sc, BCW_SBIMSTATE,
		    val & ~(SBIM_INBANDERR | SBIM_TIMEOUT));

	/* clear reset and allow it to propagate throughout the core */
	BCW_WRITE(sc, BCW_SBTMSTATELOW,
	    SBTML_FGC | SBTML_CLK | flags);
	val = BCW_READ(sc, BCW_SBTMSTATELOW);
	delay(1);

	/* leave clock enabled */
	BCW_WRITE(sc, BCW_SBTMSTATELOW, SBTML_CLK | flags);
	val = BCW_READ(sc, BCW_SBTMSTATELOW);
	delay(1);

	return 0;
}

int
bcw_get_firmware(const char *name, const uint8_t *ucode, size_t size_ucode,
    size_t *size, size_t *offset)
{
	int i, nfiles, off = 0, ret = 1;
	struct fwheader *h;

	if ((h = malloc(sizeof(struct fwheader), M_DEVBUF, M_NOWAIT)) == NULL)
		return (ret);

	/* get number of firmware files */
	bcopy(ucode, &nfiles, sizeof(nfiles));
	nfiles = ntohl(nfiles);
	off += sizeof(nfiles);

	/* parse header and search the firmware */
	for (i = 0; i < nfiles && off < size_ucode; i++) {
		bzero(h, sizeof(struct fwheader));
		bcopy(ucode + off, h, sizeof(struct fwheader));
		off += sizeof(struct fwheader);

		if (strcmp(name, h->filename) == 0) {
			ret = 0;
			*size = ntohl(h->filesize);
			*offset = ntohl(h->fileoffset);
			break;
		}
	}

	free(h, M_DEVBUF);

	return (ret);
}

int
bcw_load_firmware(struct bcw_softc *sc)
{
	int rev = sc->sc_core[sc->sc_currentcore].rev;
	int error, len, i;
	uint32_t *data;
	uint8_t *ucode;
	size_t size_ucode, size_micro, size_pcm, off_micro, off_pcm;
	char *name = "bcw-bcm43xx";
	char filename[64];

	/* load firmware */
	if ((error = loadfirmware(name, &ucode, &size_ucode)) != 0) {
		printf("%s: error %d, could not read microcode %s!\n",
		    sc->sc_dev.dv_xname, error, name);
		return (EIO);
	}
	DPRINTF(("%s: successfully read %s\n", sc->sc_dev.dv_xname, name));

	/* get microcode file offset */
	snprintf(filename, sizeof(filename), "bcm43xx_microcode%d.fw",
	    rev >= 5 ? 5 : rev);

	if (bcw_get_firmware(filename, ucode, size_ucode, &size_micro,
	    &off_micro) != 0) {
		printf("%s: get offset for firmware file %s failed!\n",
		    sc->sc_dev.dv_xname, filename);
		goto fail;
	}

	/* get pcm file offset */
	snprintf(filename, sizeof(filename), "bcm43xx_pcm%d.fw",
	    rev < 5 ? 4 : 5);

	if (bcw_get_firmware(filename, ucode, size_ucode, &size_pcm,
	    &off_pcm) != 0) {
		printf("%s: get offset for firmware file %s failed!\n",
		    sc->sc_dev.dv_xname, filename);
		goto fail;
	}

	/* upload microcode */
	data = (uint32_t *)(ucode + off_micro);
	len = size_micro / sizeof(uint32_t);
	bcw_shm_ctl_word(sc, BCW_SHM_CONTROL_MCODE, 0);
	for (i = 0; i < len; i++) {
		BCW_WRITE(sc, BCW_SHM_DATA, betoh32(data[i]));
		delay(10);
	}
	DPRINTF(("%s: uploaded microcode\n", sc->sc_dev.dv_xname));

	/* upload pcm */
	data = (uint32_t *)(ucode + off_pcm);
	len = size_pcm / sizeof(uint32_t);
	bcw_shm_ctl_word(sc, BCW_SHM_CONTROL_PCM, 0x01ea);
	BCW_WRITE(sc, BCW_SHM_DATA, 0x00004000);
	bcw_shm_ctl_word(sc, BCW_SHM_CONTROL_PCM, 0x01eb);
	for (i = 0; i < len; i++) {
		BCW_WRITE(sc, BCW_SHM_DATA, betoh32(data[i]));
		delay(10);
	}
	DPRINTF(("%s: uploaded pcm\n", sc->sc_dev.dv_xname));

	free(ucode, M_DEVBUF);

	return (0);

fail:	free(ucode, M_DEVBUF);
	return (EIO);
}

int
bcw_write_initvals(struct bcw_softc *sc, const struct bcw_initval *data,
    const unsigned int len)
{
	int i;
	uint16_t offset, size;
	uint32_t value;

	for (i = 0; i < len; i++) {
		offset = betoh16(data[i].offset);
		size = betoh16(data[i].size);
		value = betoh32(data[i].value);

		if (offset >= 0x1000)
			goto bad_format;
		if (size == 2) {
			if (value & 0xffff0000)
				goto bad_format;
			BCW_WRITE16(sc, offset, (uint16_t)value);
		} else if (size == 4)
			BCW_WRITE(sc, offset, value);
		else
			goto bad_format;
	}

	return (0);

bad_format:
	printf("%s: initvals file-format error!\n", sc->sc_dev.dv_xname);
	return (EIO);
}

int
bcw_load_initvals(struct bcw_softc *sc)
{
	int rev = sc->sc_core[sc->sc_currentcore].rev;
	int error, nr;
	uint32_t val;
	uint8_t *ucode;
	size_t size_ucode, size_ival0, size_ival1, off_ival0, off_ival1;
	char *name = "bcw-bcm43xx";
	char filename[64];

	/* load firmware */
	if ((error = loadfirmware(name, &ucode, &size_ucode)) != 0) {
		printf("%s: error %d, could not read microcode %s!\n",
		    sc->sc_dev.dv_xname, error, name);
		return (EIO);
	}
	DPRINTF(("%s: successfully read %s\n", sc->sc_dev.dv_xname, name));

	/* get initval0 file offset */
	if (rev == 2 || rev == 4) {
		switch (sc->sc_phy_type) {
		case BCW_PHY_TYPEA:
			nr = 3;
			break;
		case BCW_PHY_TYPEB:
		case BCW_PHY_TYPEG:
			nr = 1;
			break;
		default:
			printf("%s: no initvals available!\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	} else if (rev >= 5) {
		switch (sc->sc_phy_type) {
		case BCW_PHY_TYPEA:
			nr = 7;
			break;
		case BCW_PHY_TYPEB:
		case BCW_PHY_TYPEG:
			nr = 5;
			break;
		default:
			printf("%s: no initvals available!\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	} else {
		printf("%s: no initvals available!\n", sc->sc_dev.dv_xname);
		goto fail;
	}

	snprintf(filename, sizeof(filename), "bcm43xx_initval%02d.fw", nr);

	if (bcw_get_firmware(filename, ucode, size_ucode, &size_ival0,
	    &off_ival0) != 0) {
		printf("%s: get offset for initval0 file %s failed\n",
		    sc->sc_dev.dv_xname, filename);
		goto fail;
	}

	/* get initval1 file offset */
	if (rev >= 5) {
		switch (sc->sc_phy_type) {
		case BCW_PHY_TYPEA:
			val = BCW_READ(sc, BCW_SBTMSTATEHI);
			if (val & 0x00010000)
				nr = 9;
			else
				nr = 10;
			break;
		case BCW_PHY_TYPEB:
		case BCW_PHY_TYPEG:
			nr = 6;
			break;
		default:
			printf("%s: no initvals available!\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		snprintf(filename, sizeof(filename), "bcm43xx_initval%02d.fw",
		    nr);

		if (bcw_get_firmware(filename, ucode, size_ucode, &size_ival1,
		    &off_ival1) != 0) {
			printf("%s: get offset for initval1 file %s failed\n",
			    sc->sc_dev.dv_xname, filename);
			goto fail;
		}
	}

	/* upload initval0 */
	if (bcw_write_initvals(sc, (struct bcw_initval *)(ucode + off_ival0),
	    size_ival0 / sizeof(struct bcw_initval)))
		goto fail;
	DPRINTF(("%s: uploaded initval0\n", sc->sc_dev.dv_xname));

	/* upload initval1 */
	if (off_ival1 != 0) {
		if (bcw_write_initvals(sc,
		    (struct bcw_initval *)(ucode + off_ival1),
		    size_ival1 / sizeof(struct bcw_initval)))
			goto fail;
		DPRINTF(("%s: uploaded initval1\n", sc->sc_dev.dv_xname));
	}

	free(ucode, M_DEVBUF);

	return (0);

fail:	free(ucode, M_DEVBUF);
	return (EIO);
}

void
bcw_leds_switch_all(struct bcw_softc *sc, int on)
{
	struct bcw_led *led;
	uint16_t ledctl;
	int i, bit_on;

	ledctl = BCW_READ16(sc, BCW_GPIO_CTRL);

	for (i = 0; i < BCW_NR_LEDS; i++) {
		led = &(sc->leds[i]);
		if (led->behaviour == BCW_LED_INACTIVE)
			continue;
		if (on)
			bit_on = led->activelow ? 0 : 1;
		else
			bit_on = led->activelow ? 0 : 1;
		if (bit_on)
			ledctl |= (1 << i);
		else
			ledctl &= ~(1 << i);
	}

	BCW_WRITE16(sc, BCW_GPIO_CTRL, ledctl);
}

int
bcw_gpio_init(struct bcw_softc *sc)
{
	uint32_t mask, set;

	BCW_WRITE(sc, BCW_SBF, BCW_READ(sc, BCW_SBF) & 0xffff3fff);

	bcw_leds_switch_all(sc, 0);

	BCW_WRITE16(sc, BCW_GPIO_MASK, BCW_READ16(sc, BCW_GPIO_MASK) | 0x000f);

	mask = 0x0000001f;
	set = 0x0000000f;

	if (sc->sc_chipid == 0x4301) {
		mask |= 0x0060;
		set |= 0x0060;
	}
	if (0) { /* FIXME conditional unknown */
		BCW_WRITE16(sc, BCW_GPIO_MASK, BCW_READ16(sc, BCW_GPIO_MASK) |
		    0x0100);
		mask |= 0x0180;
		set |= 0x0180;
	}
	if (sc->sc_boardflags & BCW_BF_PACTRL) {
		BCW_WRITE16(sc, BCW_GPIO_MASK, BCW_READ16(sc, BCW_GPIO_MASK) |
		    0x0200);
		mask |= 0x0200;
		set |= 0x0200;
	}
	if (sc->sc_chiprev >= 2)
		mask |= 0x0010; /* FIXME this is redundant */

	BCW_WRITE(sc, BCW_GPIO_CTRL, (BCW_READ(sc, BCW_GPIO_CTRL) & mask) |
	    set);

	return (0);
}

/*
 * PHY
 */
int
bcw_phy_init(struct bcw_softc *sc)
{
	int error = ENODEV;

	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
		if (sc->sc_phy_rev == 2 || sc->sc_phy_rev == 3) {
			error = 0;
		}
		break;
	case BCW_PHY_TYPEB:
		switch (sc->sc_phy_rev) {
		case 2:
			// bcw_phy_initb2(sc);
			error = 0;
			break;
		case 4:
			// bcw_phy_initb4(sc);
			error = 0;
			break;
		case 5:
			// bcw_phy_initb5(sc);
			error = 0;
			break;
		case 6:
			// bcw_phy_initb6(sc);
			error = 0;
			break;
		}
		break;
	case BCW_PHY_TYPEG:
		// bcw_phy_initg(sc);
		error = 0;
		break;
	}

	if (error)
		printf("%s: Unknown PHYTYPE found!\n", sc->sc_dev.dv_xname);

	return (error);
}

void
bcw_phy_initg(struct bcw_softc *sc)
{
	if (sc->sc_phy_rev == 1)
		bcw_phy_initb5(sc);
	else
		bcw_phy_initb6(sc);
}

void
bcw_phy_initb5(struct bcw_softc *sc)
{
	uint16_t offset;

	if (sc->sc_phy_rev == 1 && sc->sc_radio_rev == 0x2050)
		bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) |
		    0x0050);

	if (1) { /* XXX bcw->board_vendor */
		for (offset = 0x00a8; offset < 0x00c7; offset++)
			bcw_phy_write16(sc, offset,
			    (bcw_phy_read16(sc, offset) + 0x02020) & 0x3f3f);
	}

	bcw_phy_write16(sc, 0x0035, (bcw_phy_read16(sc, 0x0035) & 0xf0ff) |
	    0x0700);

	if (sc->sc_radio_rev == 0x2050)
		bcw_phy_write16(sc, 0x0038, 0x0667);

	if (1) { /* XXX phy->connected */
		if (sc->sc_radio_rev == 0x2050) {
			bcw_radio_write16(sc, 0x007a,
			    bcw_radio_read16(sc, 0x007a) | 0x0020);
			bcw_radio_write16(sc, 0x0051,
			    bcw_radio_read16(sc, 0x0051) | 0x0004);
		}
		BCW_WRITE16(sc, BCW_MMIO_PHY_RADIO, 0);
		bcw_phy_write16(sc, 0x0802, bcw_phy_read16(sc, 0x0802) |
		    0x0100);
		bcw_phy_write16(sc, 0x042b, bcw_phy_read16(sc, 0x042b) |
		    0x2000);
		bcw_phy_write16(sc, 0x001c, 0x186a);
		bcw_phy_write16(sc, 0x0013,
		    (bcw_phy_read16(sc, 0x0013) & 0x00ff) | 0x1900);
		bcw_phy_write16(sc, 0x0035,
		    (bcw_phy_read16(sc, 0x0035) & 0xffc0) | 0x0064);
		bcw_phy_write16(sc, 0x005d,
		    (bcw_phy_read16(sc, 0x005d) & 0xff80) | 0x000a);
	}

	if (sc->sc_phy_rev == 1 && sc->sc_radio_rev == 0x2050) {
		bcw_phy_write16(sc, 0x0026, 0xce00);
		bcw_phy_write16(sc, 0x0021, 0x3763);
		bcw_phy_write16(sc, 0x0022, 0x1bc3);
		bcw_phy_write16(sc, 0x0023, 0x06f9);
		bcw_phy_write16(sc, 0x0024, 0x037e);
	} else
		bcw_phy_write16(sc, 0x0026, 0xcc00);
	bcw_phy_write16(sc, 0x0030, 0x00c6);
	BCW_WRITE16(sc, 0x3ec, 0x3f22);

	if (sc->sc_phy_rev == 1 && sc->sc_radio_rev == 0x2050)
		bcw_phy_write16(sc, 0x0020, 0x3e1c);
	else
		bcw_phy_write16(sc, 0x0020, 0x301c);

	if (sc->sc_phy_rev == 0)
		BCW_WRITE16(sc, 0x03e4, 0x3000);

	/* force to channel 7, even if not supported */
	bcw_radio_channel(sc, 7, 0);

	if (sc->sc_radio_rev != 0x2050) {
		bcw_radio_write16(sc, 0x0075, 0x0080);
		bcw_radio_write16(sc, 0x0079, 0x0081);
	}

	bcw_radio_write16(sc, 0x0050, 0x0020);
	bcw_radio_write16(sc, 0x0050, 0x0023);

	if (sc->sc_radio_rev == 0x2050) {
		bcw_radio_write16(sc, 0x0050, 0x0020);
		bcw_radio_write16(sc, 0x005a, 0x0070);
	}

	bcw_radio_write16(sc, 0x005b, 0x007b);
	bcw_radio_write16(sc, 0x005c, 0x00b0);

	bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) | 0x0007);

	bcw_radio_channel(sc, BCW_RADIO_DEFAULT_CHANNEL, 0);

	bcw_phy_write16(sc, 0x0014, 0x0080);
	bcw_phy_write16(sc, 0x0032, 0x00ca);
	bcw_phy_write16(sc, 0x88a3, 0x002a);

	/* TODO bcw_radio_set_tx_power() */

	if (sc->sc_radio_rev == 0x2050)
		bcw_radio_write16(sc, 0x005d, 0x000d);

	BCW_WRITE16(sc, 0x03e4, (BCW_READ16(sc, 0x03e4) & 0xffc0) | 0x0004);
}

void
bcw_phy_initb6(struct bcw_softc *sc)
{
	uint16_t offset, val;

	bcw_phy_write16(sc, 0x003e, 0x817a);
	bcw_radio_write16(sc, 0x007a, (bcw_radio_read16(sc, 0x007a) | 0x0058));

	if (sc->sc_radio_mnf == 0x17f && sc->sc_radio_ver == 0x2050 &&
	    (sc->sc_radio_rev == 3 || sc->sc_radio_rev == 4 ||
	    sc->sc_radio_rev == 5)) {
		bcw_radio_write16(sc, 0x0051, 0x001f);
		bcw_radio_write16(sc, 0x0052, 0x0040);
		bcw_radio_write16(sc, 0x0053, 0x005b);
		bcw_radio_write16(sc, 0x0054, 0x0098);
		bcw_radio_write16(sc, 0x005a, 0x0088);
		bcw_radio_write16(sc, 0x005b, 0x0088);
		bcw_radio_write16(sc, 0x005d, 0x0088);
		bcw_radio_write16(sc, 0x005e, 0x0088);
		bcw_radio_write16(sc, 0x007d, 0x0088);
	}

	if (sc->sc_radio_mnf == 0x17f && sc->sc_radio_ver == 0x2050 &&
	    sc->sc_radio_rev == 6) {
		bcw_radio_write16(sc, 0x0051, 0);
		bcw_radio_write16(sc, 0x0052, 0x0040);
		bcw_radio_write16(sc, 0x0053, 0x00b7);
		bcw_radio_write16(sc, 0x0054, 0x0098);
		bcw_radio_write16(sc, 0x005a, 0x0088);
		bcw_radio_write16(sc, 0x005b, 0x008b);
		bcw_radio_write16(sc, 0x005c, 0x00b5);
		bcw_radio_write16(sc, 0x005d, 0x0088);
		bcw_radio_write16(sc, 0x005e, 0x0088);
		bcw_radio_write16(sc, 0x007d, 0x0088);
		bcw_radio_write16(sc, 0x007c, 0x0001);
		bcw_radio_write16(sc, 0x007e, 0x0008);
	}

	if (sc->sc_radio_mnf == 0x017f && sc->sc_radio_ver == 0x2050 &&
	    sc->sc_radio_rev == 7) {
		bcw_radio_write16(sc, 0x0051, 0);
		bcw_radio_write16(sc, 0x0052, 0x0040);
		bcw_radio_write16(sc, 0x0053, 0x00b7);
		bcw_radio_write16(sc, 0x0054, 0x0098);
		bcw_radio_write16(sc, 0x005a, 0x0088);
		bcw_radio_write16(sc, 0x005b, 0x00a8);
		bcw_radio_write16(sc, 0x005c, 0x0075);
		bcw_radio_write16(sc, 0x005d, 0x00f5);
		bcw_radio_write16(sc, 0x005e, 0x00b8);
		bcw_radio_write16(sc, 0x007d, 0x00e8);
		bcw_radio_write16(sc, 0x007c, 0x0001);
		bcw_radio_write16(sc, 0x007e, 0x0008);
		bcw_radio_write16(sc, 0x007b, 0);
	}

	if (sc->sc_radio_mnf == 0x17f && sc->sc_radio_ver == 0x2050 &&
	    sc->sc_radio_rev == 8) {
		bcw_radio_write16(sc, 0x0051, 0);
		bcw_radio_write16(sc, 0x0052, 0x0040);
		bcw_radio_write16(sc, 0x0053, 0x00b7);
		bcw_radio_write16(sc, 0x0054, 0x0098);
		bcw_radio_write16(sc, 0x005a, 0x0088);
		bcw_radio_write16(sc, 0x005b, 0x006b);
		bcw_radio_write16(sc, 0x005c, 0x000f);
		if (sc->sc_boardflags & 0x8000) {
			bcw_radio_write16(sc, 0x005d, 0x00fa);
			bcw_radio_write16(sc, 0x005e, 0x00d8);
		} else {
			bcw_radio_write16(sc, 0x005d, 0x00f5);
			bcw_radio_write16(sc, 0x005e, 0x00b8);
		}
		bcw_radio_write16(sc, 0x0073, 0x0003);
		bcw_radio_write16(sc, 0x007d, 0x00a8);
		bcw_radio_write16(sc, 0x007c, 0x0001);
		bcw_radio_write16(sc, 0x007e, 0x0008);
	}

	val = 0x1e1f;
	for (offset = 0x0088; offset < 0x0098; offset++) {
		bcw_phy_write16(sc, offset, val);
		val -= 0x0202;
	}
	val = 0x3e3f;
	for (offset = 0x0098; offset < 0x00a8; offset++) {
		bcw_phy_write16(sc, offset, val);
		val -= 0x0202;
	}
	val = 0x2120;
	for (offset = 0x00a8; offset < 0x00c8; offset++) {
		bcw_phy_write16(sc, offset, (val & 0x3f3f));
		val += 0x0202;
	}

	if (sc->sc_phy_type == BCW_PHY_TYPEG) {
		bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) |
		    0x0020);
		bcw_radio_write16(sc, 0x0051, bcw_radio_read16(sc, 0x0051) |
		    0x0004);
		bcw_phy_write16(sc, 0x0802, bcw_phy_read16(sc, 0x0802) |
		    0x0100);
		bcw_phy_write16(sc, 0x042b, bcw_phy_read16(sc, 0x042b) |
		    0x2000);
	}

	/* force to channel 7, even if not supported */
	bcw_radio_channel(sc, 7, 0);

	bcw_radio_write16(sc, 0x0050, 0x0020);
	bcw_radio_write16(sc, 0x0050, 0x0023);
	delay(40);
	bcw_radio_write16(sc, 0x007c, (bcw_radio_read16(sc, 0x007c) |
	    0x0002));
	bcw_radio_write16(sc, 0x0050, 0x0020);
	if (sc->sc_radio_mnf == 0x17f && sc->sc_radio_ver == 0x2050 &&
	    sc->sc_radio_rev <= 2) {
		bcw_radio_write16(sc, 0x0050, 0x0020);
		bcw_radio_write16(sc, 0x005a, 0x0070);
		bcw_radio_write16(sc, 0x005b, 0x007b);
		bcw_radio_write16(sc, 0x005c, 0x00b0);
	}
	bcw_radio_write16(sc, 0x007a, (bcw_radio_read16(sc, 0x007a) & 0x00f8) |
	    0x0007);

	bcw_radio_channel(sc, BCW_RADIO_DEFAULT_CHANNEL, 0);

	bcw_phy_write16(sc, 0x0014, 0x0200);
	if (sc->sc_radio_ver == 0x2050) {
		if (sc->sc_radio_rev == 3 || sc->sc_radio_rev == 4 ||
		    sc->sc_radio_rev == 5)
			bcw_phy_write16(sc, 0x002a, 0x8ac0);
		else
			bcw_phy_write16(sc, 0x002a, 0x88c2);
	}
	bcw_phy_write16(sc, 0x0038, 0x0668);
	/* TODO bcw_radio_set_tx_power() */
	if (sc->sc_radio_ver == 0x2050) {
		if (sc->sc_radio_rev == 3 || sc->sc_radio_rev == 4 ||
		    sc->sc_radio_rev == 5)
			bcw_phy_write16(sc, 0x005d,
			    bcw_phy_read16(sc, 0x005d) | 0x0003);
		else if (sc->sc_radio_rev <= 2)
			bcw_phy_write16(sc, 0x005d, 0x000d);
	}

	if (sc->sc_phy_rev == 4)
		bcw_phy_write16(sc, 0x0002, (bcw_phy_read16(sc, 0x0002) &
		    0xffc0) | 0x0004);
	else
		BCW_WRITE16(sc, 0x03e4, 0x0009);
	if (sc->sc_phy_type == BCW_PHY_TYPEG) {
		BCW_WRITE16(sc, 0x03e6, 0x8140);
		bcw_phy_write16(sc, 0x0016, 0x0410);
		bcw_phy_write16(sc, 0x0017, 0x0820);
		bcw_phy_write16(sc, 0x0062, 0x0007);
		/* TODO */
	} else
		BCW_WRITE16(sc, 0x03e6, 0);
}

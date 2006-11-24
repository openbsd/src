/*	$OpenBSD: bcw.c,v 1.6 2006/11/24 20:27:41 mglocker Exp $ */

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

#include <dev/ic/bcwreg.h>
#include <dev/ic/bcwvar.h>

#include <uvm/uvm_extern.h>

void	bcw_reset(struct bcw_softc *);
int	bcw_init(struct ifnet *);
void	bcw_start(struct ifnet *);
void	bcw_stop(struct ifnet *, int);

void	bcw_watchdog(struct ifnet *);
void	bcw_rxintr(struct bcw_softc *);
void	bcw_txintr(struct bcw_softc *);
//void	bcw_add_mac(struct bcw_softc *, u_int8_t *, unsigned long);
int	bcw_add_rxbuf(struct bcw_softc *, int);
void	bcw_rxdrain(struct bcw_softc *);
void	bcw_set_filter(struct ifnet *); 
void	bcw_tick(void *);
int	bcw_ioctl(struct ifnet *, u_long, caddr_t);

int	bcw_alloc_rx_ring(struct bcw_softc *, struct bcw_rx_ring *, int);
void	bcw_reset_rx_ring(struct bcw_softc *, struct bcw_rx_ring *);
void	bcw_free_rx_ring(struct bcw_softc *, struct bcw_rx_ring *);
int	bcw_alloc_tx_ring(struct bcw_softc *, struct bcw_tx_ring *, int);
void	bcw_reset_tx_ring(struct bcw_softc *, struct bcw_tx_ring *);
void	bcw_free_tx_ring(struct bcw_softc *, struct bcw_tx_ring *);

/* 80211 functions copied from iwi */
int	bcw_newstate(struct ieee80211com *, enum ieee80211_state, int);
int	bcw_media_change(struct ifnet *);
void	bcw_media_status(struct ifnet *, struct ifmediareq *);
/* fashionably new functions */
int	bcw_validatechipaccess(struct bcw_softc *);

struct cfdriver bcw_cd = {
	NULL, "bcw", DV_IFNET
};

void
bcw_attach(struct bcw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet   *ifp = &ic->ic_if;
	int             error;
	int             i,j;
	u_int32_t	sbval;
	u_int16_t	sbval16;
	

	/*
	 * Reset the chip
	 */
	bcw_reset(sc);

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
	sc->sc_boardflags = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
	    BCW_SPROM_BOARDFLAGS);
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
	 * XXX Can we read BCW_ADDR_SPACE0 and see if it returns a likely
	 * Core? On the 4318 that only has an 802.11 core, it always reads as
	 * some garbage, so if we do a read first we could set a "singlecore"
	 * flag instead of thrashing around trying to select non-existant
	 * cores. Testing requires cards that do have multiple cores. This
	 * would also simplify identifying cards into 3 classes early:
	 *   - Multiple Cores without a Chip Common Core
	 *   - Multiple Cores with a Chip Common Core
	 *   - Single Core
	 */

	sbval = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_ADDR_SPACE0);
	if ((sbval & 0xffff0000) != 0x18000000) {
		DPRINTF(("\n%s: Trial Core read was 0x%x, single core only?\n",
		    sc->sc_dev.dv_xname, sbval));
	 	//sc->sc_singlecore=1;
	} else
		DPRINTF(("\n%s: Trial Core read was 0x%x\n",
		    sc->sc_dev.dv_xname, sbval));

	/* 
	 * Try and change to the ChipCommon Core
	 */
	for (i = 0; i < 10; i++) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_ADDR_SPACE0,
		    BCW_CORE_SELECT(0));
		delay(10);
		sbval = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    BCW_ADDR_SPACE0);
		if (sbval == BCW_CORE_SELECT(0)) {
			DPRINTF(("%s: Selected ChipCommon Core\n"));
			break;
		}
		delay(10);
	}

	/*
	 * Core ID REG, this is either the default wireless core (0x812) or
	 * a ChipCommon core that was successfully selected above
	 */
	sbval = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_CIR_SBID_HI);
	DPRINTF(("%s: Got Core ID Reg 0x%x, type is 0x%x\n",
	    sc->sc_dev.dv_xname, sbval, (sbval & 0x8ff0) >> 4));
	
	/* If we successfully got a commoncore, and the corerev=4 or >=6
	   get the number of cores from the chipid reg */
	if (((sbval & 0x00008ff0) >> 4) == BCW_CORE_COMMON) {
		sc->sc_havecommon = 1;
		sbval = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    BCW_CORE_COMMON_CHIPID);
		sc->sc_chipid = (sbval & 0x0000ffff);
		sc->sc_corerev =
		    ((sbval & 0x00007000) >> 8 | (sbval & 0x0000000f));
		if ((sc->sc_corerev == 4) || (sc->sc_corerev >= 6))
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
					/* XXX Educated Guess */
					sc->sc_numcores = 0;
			} /* end of switch */
	} else { /* No CommonCore, set chipid,cores,rev based on product id */
		sc->sc_havecommon = 0;
		switch(sc->sc_prodid) {
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
				/* XXX educated guess */
				sc->sc_numcores = 1;
		} /* end of switch */
	} /* End of if/else */

	DPRINTF(("%s: ChipID=0x%x, ChipRev=0x%x, NumCores=%d\n",
	    sc->sc_dev.dv_xname, sc->sc_chipid,
	    sc->sc_chiprev, sc->sc_numcores));

	/* Identify each core */
	if (sc->sc_numcores >= 2) { /* Exclude single core chips */
 		for (i = 0; i <= sc->sc_numcores; i++) {
 			DPRINTF(("%s: Trying core %d -\n",
 			    sc->sc_dev.dv_xname, i));
 			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    BCW_ADDR_SPACE0, BCW_CORE_SELECT(i));
			/* loop to see if the selected core shows up */
			for (j = 0; j < 10; j++) {
				sbval=bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    BCW_ADDR_SPACE0);
				DPRINTF(("%s: read %d for core %d = 0x%x\n",
				    sc->sc_dev.dv_xname, j, i, sbval));
				if (sbval == BCW_CORE_SELECT(i)) break;
				delay(10);
			}
			if (j < 10) 
				DPRINTF(("%s: Found core %d of type 0x%x\n",
				    sc->sc_dev.dv_xname, i, 
				    (sbval & 0x00008ff0) >> 4));
			//sc->sc_core[i].id = (sbval & 0x00008ff0) >> 4;
		} /* End of For loop */
	}

	/*
	 * XXX Attach cores to the backplane, if we have more than one
	 */
	// ??? if (!sc->sc_singlecore) {
	if (sc->sc_havecommon == 1) {
		sbval = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_PCICR);
		sbval |= 0x1 << 8; /* XXX hardcoded bitmask of single core */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_PCICR, sbval);
	}

	/*
	 * Get and display the PHY info from the MIMO
	 */
	sbval = bus_space_read_2(sc->sc_iot, sc->sc_ioh, 0x3E0);
	sc->sc_phy_version = (sbval&0xf000)>>12;
	sc->sc_phy_rev = sbval&0xf;
	sc->sc_phy_type = (sbval&0xf00)>>8;
	DPRINTF(("%s: PHY version %d revision %d ",
	    sc->sc_dev.dv_xname, sc->sc_phy_version, sc->sc_phy_rev));
	switch (sc->sc_phy_type) {
		case BCW_PHY_TYPEA:
			DPRINTF(("PHY %d (A)\n",sc->sc_phy_type));
			break;
		case BCW_PHY_TYPEB:
			DPRINTF(("PHY %d (B)\n",sc->sc_phy_type));
			break;
		case BCW_PHY_TYPEG:
			DPRINTF(("PHY %d (G)\n",sc->sc_phy_type));
			break;
		case BCW_PHY_TYPEN:
			DPRINTF(("PHY %d (N)\n",sc->sc_phy_type));
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
		bus_space_write_2(sc->sc_iot, sc->sc_ioh, BCW_RADIO_CONTROL,
		    BCW_RADIO_ID);
		sbval=bus_space_read_2(sc->sc_iot, sc->sc_ioh,
		    BCW_RADIO_DATAHIGH);
		sbval <<= 16;
		bus_space_write_2(sc->sc_iot, sc->sc_ioh, BCW_RADIO_CONTROL,
		    BCW_RADIO_ID);
		sc->sc_radioid = sbval | bus_space_read_2(sc->sc_iot, 
		    sc->sc_ioh, BCW_RADIO_DATALOW);
	} else {
		switch(sc->sc_corerev) {
			case 0:	
				sc->sc_radioid = 0x3205017F;
				break;
			case 1:
				sc->sc_radioid = 0x4205017f;
				break;
			default:
				sc->sc_radioid = 0x5205017f;
		}
	}

	sc->sc_radiorev = (sc->sc_radioid & 0xf0000000) >> 28;
	sc->sc_radiotype = (sc->sc_radioid & 0x0ffff000) >> 12;

	DPRINTF(("%s: Radio Rev %d, Ver 0x%x, Manuf 0x%x\n",
	    sc->sc_dev.dv_xname, sc->sc_radiorev, sc->sc_radiotype,
	    sc->sc_radioid & 0xfff));

	error = bcw_validatechipaccess(sc);
	if (error) {
		printf("%s: failed Chip Access Validation at %d\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}

	/* Test for valid PHY/revision combinations, probably a simpler way */
	if (sc->sc_phy_type == BCW_PHY_TYPEA) {
		switch(sc->sc_phy_rev) {
			case 2:
			case 3:
			case 5:
			case 6:
			case 7:	break;
			default: 
				printf("%s: invalid PHY A revision %d\n",
				    sc->sc_dev.dv_xname, sc->sc_phy_rev);
				return;
		}
	}
	if (sc->sc_phy_type == BCW_PHY_TYPEB) {
		switch(sc->sc_phy_rev) {
			case 2:
			case 4:
			case 7:	break;
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
			case 8:	break;
			default: 
				printf("%s: invalid PHY G revision %d\n",
				    sc->sc_dev.dv_xname, sc->sc_phy_rev);
				return;
		}
	}

	/* test for valid radio revisions */
	if ((sc->sc_phy_type == BCW_PHY_TYPEA) &
	    (sc->sc_radiotype != 0x2060)) {
		    	printf("%s: invalid PHY A radio 0x%x\n",
		    	    sc->sc_dev.dv_xname, sc->sc_radiotype);
		    	return;
	}
	if ((sc->sc_phy_type == BCW_PHY_TYPEB) &
	    ((sc->sc_radiotype & 0xfff0) != 0x2050)) {
		    	printf("%s: invalid PHY B radio 0x%x\n",
		    	    sc->sc_dev.dv_xname, sc->sc_radiotype);
		    	return;
	}
	if ((sc->sc_phy_type == BCW_PHY_TYPEG) &
	    (sc->sc_radiotype != 0x2050)) {
		    	printf("%s: invalid PHY G radio 0x%x\n",
		    	    sc->sc_dev.dv_xname, sc->sc_radiotype);
		    	return;
	}

	/*
	 * Switch the radio off - candidate for seperate function
	 */
	switch(sc->sc_phy_type) {
		case BCW_PHY_TYPEA:
			/* Magic unexplained values */
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    BCW_RADIO_CONTROL, 0x04);
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    BCW_RADIO_DATALOW, 0xff);
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    BCW_RADIO_CONTROL, 0x05);
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    BCW_RADIO_DATALOW, 0xfb);
			/* 
			 * "When the term MaskSet is used, it is shorthand 
			 * for reading a value from a register, applying a 
			 * bitwise AND mask to the value and then applying 
			 * a bitwise OR to set a value. This value is then 
			 * written back to the register."
			 *  - This makes little sense when the docs say that
			 *    here we read a value, AND it with 0xffff, then
			 *    OR with 0x8. Why not just set the 0x8 bit?
			 */
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    BCW_PHY_CONTROL, 0x10);
			sbval16 = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
			    BCW_PHY_DATA);
			sbval16 |= 0x8;
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    BCW_PHY_CONTROL, 0x10);
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    BCW_PHY_DATA, sbval16);

			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    BCW_PHY_CONTROL, 0x11);
			sbval16 = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
			    BCW_PHY_DATA);
			sbval16 |= 0x8;
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    BCW_PHY_CONTROL, 0x11);
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    BCW_PHY_DATA, sbval16);
			break;
		case BCW_PHY_TYPEG:
			if (sc->sc_corerev >= 5) {
				bus_space_write_2(sc->sc_iot, sc->sc_ioh,
				    BCW_PHY_CONTROL, 0x811);
				sbval16 = bus_space_read_2(sc->sc_iot,
				    sc->sc_ioh, BCW_PHY_DATA);
				sbval16 |= 0x8c;
				bus_space_write_2(sc->sc_iot, sc->sc_ioh,
				    BCW_PHY_CONTROL, 0x811);
				bus_space_write_2(sc->sc_iot, sc->sc_ioh,
				    BCW_PHY_DATA, sbval16);

				bus_space_write_2(sc->sc_iot, sc->sc_ioh,
				    BCW_PHY_CONTROL, 0x812);
				sbval16 = bus_space_read_2(sc->sc_iot,
				    sc->sc_ioh, BCW_PHY_DATA);
				sbval16 &= 0xff73;
				bus_space_write_2(sc->sc_iot, sc->sc_ioh,
				    BCW_PHY_CONTROL, 0x812);
				bus_space_write_2(sc->sc_iot, sc->sc_ioh,
				    BCW_PHY_DATA, sbval16);
			}
			/* FALL-THROUGH */
		default:
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    BCW_PHY_CONTROL, 0x15);
			bus_space_write_2(sc->sc_iot, sc->sc_ioh,
			    BCW_PHY_DATA, 0xaa00);
	} /* end of switch statement to turn off radio */

	/* Read antenna gain from SPROM and multiply by 4 */
	sbval = bus_space_read_2(sc->sc_iot, sc->sc_ioh, BCW_SPROM_ANTGAIN);
	/* If unset, assume 2 */
	if((sbval == 0) || (sbval == 0xffff)) sbval = 0x0202;
	if(sc->sc_phy_type == BCW_PHY_TYPEA)
		sc->sc_radio_gain = (sbval & 0xff);
	else
		sc->sc_radio_gain = ((sbval & 0xff00) >> 8);
	sc->sc_radio_gain *= 4;

	/*
	 * Set the paXbY vars, X=0 for PHY A, X=1 for B/G, but we'll
	 * just grab them all while we're here
	 */
	sc->sc_radio_pa0b0 = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
	    BCW_SPROM_PA0B0);
	sc->sc_radio_pa0b1 = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
	    BCW_SPROM_PA0B1);
	sc->sc_radio_pa0b2 = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
	    BCW_SPROM_PA0B2);
	sc->sc_radio_pa1b0 = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
	    BCW_SPROM_PA1B0);
	sc->sc_radio_pa1b1 = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
	    BCW_SPROM_PA1B1);
	sc->sc_radio_pa1b2 = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
	    BCW_SPROM_PA1B2);

	/* Get the idle TSSI */
	sbval = bus_space_read_2(sc->sc_iot, sc->sc_ioh, BCW_SPROM_IDLETSSI);
	if(sc->sc_phy_type == BCW_PHY_TYPEA)
		sc->sc_idletssi = (sbval & 0xff);
	else
		sc->sc_idletssi = ((sbval & 0xff00) >> 8);
	
	
	/* Init the Microcode Flags Bitfield */
	/* http://bcm-specs.sipsolutions.net/MicrocodeFlagsBitfield */
	
	sbval = 0;
	if((sc->sc_phy_type == BCW_PHY_TYPEA) ||
	    (sc->sc_phy_type == BCW_PHY_TYPEB) ||
	    (sc->sc_phy_type == BCW_PHY_TYPEG))
		sbval |= 2; /* Turned on during init for non N phys */
	if((sc->sc_phy_type == BCW_PHY_TYPEG) &&
	    (sc->sc_phy_rev == 1))
		sbval |= 0x20;
	if ((sc->sc_phy_type == BCW_PHY_TYPEG) &&
	    ((sc->sc_boardflags & BCW_BF_PACTRL) == BCW_BF_PACTRL))
		sbval |= 0x40;
	if((sc->sc_phy_type == BCW_PHY_TYPEG) &&
	    (sc->sc_phy_rev < 3))
		sbval |= 0x8; /* MAGIC */
	if ((sc->sc_boardflags & BCW_BF_XTAL) == BCW_BF_XTAL)
		sbval |= 0x400;
	if(sc->sc_phy_type == BCW_PHY_TYPEB)
		sbval |= 0x4;
	if((sc->sc_radiotype == 0x2050) &&
	    (sc->sc_radiorev <= 5))
	    	sbval |= 0x40000;
	/*
	 * XXX If the device isn't up and this is a PCI bus with revision 
	 * 10 or less set bit 0x80000
	 */

	/* Now, write the value into the regster
	 *
	 * The MicrocodeBitFlags is an unaligned 32bit value in SHM, so the
	 * strategy is to select the aligned word for the lower 16 bits,
	 * but write to the unaligned address. Then, because the SHM
	 * pointer is automatically incremented to the next aligned word,
	 * we can just write the remaining bits as a 16 bit write.
	 * This explanation could make more sense, but an SHM read/write
	 * wrapper of some sort would be better.
	 */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_CONTROL, 
	    (BCW_SHM_CONTROL_SHARED << 16) + BCW_SHM_MICROCODEFLAGSLOW - 2);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, BCW_SHM_DATAHIGH,
	    sbval & 0x00ff);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, BCW_SHM_DATALOW,
	    (sbval & 0xff00)>>16);

	/* 
	 * Initialize the TSSI to DBM table
	 * The method is described at 
	 * http://bcm-specs.sipsolutions.net/TSSI_to_DBM_Table
	 * but I suspect there's a standard way to do it in the 80211 stuff
	 */
	
	/* 
	 * XXX TODO still for the card attach: 
	 * - Disable the 80211 Core (and wrapper for on/off)
	 * - Powercontrol Crystal Off (and write a wrapper for on/off)
	 * - Setup LEDs to blink in whatever fashionable manner
	 */

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
	if(sc->sc_phy_type == BCW_PHY_TYPEA) {
		i = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
		    BCW_SPROM_ET1MACADDR);
		ic->ic_myaddr[0] = (i & 0xff00) >> 8;
		ic->ic_myaddr[1] = i & 0xff;
		i = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
		    BCW_SPROM_ET1MACADDR + 2);
		ic->ic_myaddr[2] = (i & 0xff00) >> 8;
		ic->ic_myaddr[3] = i & 0xff;
		i = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
		    BCW_SPROM_ET1MACADDR + 4);
		ic->ic_myaddr[4] = (i & 0xff00) >> 8;
		ic->ic_myaddr[5] = i & 0xff;
	} else { /* assume B or G PHY */
		i = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
		    BCW_SPROM_IL0MACADDR);
		ic->ic_myaddr[0] = (i & 0xff00) >> 8;
		ic->ic_myaddr[1] = i & 0xff;
		i = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
		    BCW_SPROM_IL0MACADDR + 2);
		ic->ic_myaddr[2] = (i & 0xff00) >> 8;
		ic->ic_myaddr[3] = i & 0xff;
		i = bus_space_read_2(sc->sc_iot, sc->sc_ioh,
		    BCW_SPROM_IL0MACADDR + 4);
		ic->ic_myaddr[4] = (i & 0xff00) >> 8;
		ic->ic_myaddr[5] = i & 0xff;
	}
	
	printf(", address %s\n", ether_sprintf(ic->ic_myaddr));

	/* Set supported rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = bcw_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = bcw_rateset_11g;

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
	int             s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			bcw_init(ifp);
			/* XXX arp_ifinit(&sc->bcw_ac, ifa); */
			break;
#endif /* INET */
		default:
			bcw_init(ifp);
			break;
		}
		break;
	case SIOCSIFFLAGS:
		if((ifp->if_flags & IFF_UP)&&(!(ifp->if_flags & IFF_RUNNING)))
				bcw_init(ifp);
		else if(ifp->if_flags & IFF_RUNNING)
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
	int             txstart;
	int             txsfree;
	int		error;
#endif
	int             newpkts = 0;

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
			u_int32_t ctrl;

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
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_DMA_DPTR,
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
	u_int32_t intstatus;
	int wantinit;
	int handled = 0;

	sc = xsc;

	for (wantinit = 0; wantinit == 0;) {
		intstatus = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    BCW_INT_STS);

		/* ignore if not ours, or unsolicited interrupts */
		intstatus &= sc->sc_intmask;
		if (intstatus == 0)
			break;

		handled = 1;

		/* Ack interrupt */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_INT_STS,
		    intstatus);

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
	curr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_DMA_RXSTATUS(0)) & 
	    RS_CD_MASK;
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
		curr = (bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    BCW_DMA_RXSTATUS) & RS_CD_MASK) /
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
	curr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_DMA_TXSTATUS) & RS_CD_MASK;
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
	u_int32_t reg_win;

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

	/* save the current map, so it can be restored */
	reg_win = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_REG0_WIN);

	/* set register window to Sonics registers */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_REG0_WIN, BCW_SONICS_WIN);

	/* enable SB to PCI interrupt */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SBINTVEC,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SBINTVEC) |
	    SBIV_ENET0);

	/* enable prefetch and bursts for sonics-to-pci translation 2 */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SPCI_TR2,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SPCI_TR2) |
	    SBTOPCI_PREF | SBTOPCI_BURST);

	/* restore to ethernet register space */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_REG0_WIN, reg_win);

	/* Reset the chip to a known state. */
	bcw_reset(sc);

#if 0 /* FIXME */
	/* Initialize transmit descriptors */
	memset(sc->bcw_tx_ring, 0, BCW_NTXDESC * sizeof(struct bcw_dma_slot));
	sc->sc_txsnext = 0;
	sc->sc_txin = 0;
#endif

	/* enable crc32 generation and set proper LED modes */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_MACCTL,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_MACCTL) |
	    BCW_EMC_CRC32_ENAB | BCW_EMC_LED);

	/* reset or clear powerdown control bit  */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_MACCTL,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_MACCTL) &
	    ~BCW_EMC_PDOWN);

	/* setup DMA interrupt control */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_DMAI_CTL, 1 << 24);	/* MAGIC */

	/* setup packet filter */
	bcw_set_filter(ifp);

	/* set max frame length, account for possible VLAN tag */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_RX_MAX,
	    ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_TX_MAX,
	    ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN);

	/* set tx watermark */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_TX_WATER, 56);

	/* enable transmit */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_DMA_TXCTL, XC_XE);

	/*
         * Give the receive ring to the chip, and
         * start the receive DMA engine.
         */
	sc->sc_rxin = 0;

	/* enable receive */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_DMA_RXCTL,
	    BCW_PREPKT_HEADER_SIZE << 1 | 1);

	/* Enable interrupts */
	sc->sc_intmask =
	    I_XI | I_RI | I_XU | I_RO | I_RU | I_DE | I_PD | I_PC | I_TO;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_INT_MASK,
	    sc->sc_intmask);

#if 0 /* FIXME */
	/* start the receive dma */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_DMA_RXDPTR,
	    BCW_NRXDESC * sizeof(struct bcw_dma_slot));
#endif

	/* set media */
	//mii_mediachg(&sc->bcw_mii);

#if 0
	/* turn on the ethernet mac */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_ENET_CTL,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    BCW_ENET_CTL) | EC_EE);
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
#if 0 /* FIXME */
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
	//u_int32_t val;

	/* Stop the 1 second timer */
	timeout_del(&sc->sc_timeout);

	/* Mark the interface down and cancel the watchdog timer. */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	/* Disable interrupts. */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_INT_MASK, 0);
	sc->sc_intmask = 0;
	delay(10);

	/* Disable emac */
#if 0
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_ENET_CTL, EC_ED);
	for (i = 0; i < 200; i++) {
		val = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    BCW_ENET_CTL);
		if (!(val & EC_ED))
			break;
		delay(10);
	}
#endif
	/* Stop the DMA */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_DMA_RXCONTROL(0), 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_DMA_TXCONTROL(0), 0);
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
	int i;
	u_int32_t sbval;
	u_int32_t val;
	u_int32_t reject;

	/*
	 * Figure out what revision the Sonic Backplane is, as the position
	 * of the Reject bit changed. Save the revision in the softc, and
	 * use the local variable 'reject' in all the bit banging.
	 */
	sbval = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_CIR_SBID_LO);
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

	sbval = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SBTMSTATELOW);

	/*
	 * If the 802.11 core is enabled, only clock of clock,reset,reject
	 * will be set, and we need to reset all the DMA engines first.
	 */
#if 0
	if ((sbval & (SBTML_RESET | reject | SBTML_CLK)) == SBTML_CLK) {
		/* Stop all DMA */
		/* reset the dma engines */
	}
#endif
	/* disable 802.11 core if not in reset */
	if (!(sbval & SBTML_RESET)) {
		/* if the core is not enabled, the clock won't be enabled */
		if (!(sbval & SBTML_CLK)) {
			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    BCW_SBTMSTATELOW, SBTML_RESET | reject |
			    SBTML_80211FLAG | SBTML_80211PHY );
			delay(1);
			sbval = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    BCW_SBTMSTATELOW);
			goto disabled;

			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    BCW_SBTMSTATELOW, reject);
			delay(1);
			/* wait until busy is clear */
			for (i = 0; i < 10000; i++) {
				val = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    BCW_SBTMSTATEHI);
				if (!(val & SBTMH_BUSY))
					break;
				delay(10);
			}
			if (i == 10000)
				printf("%s: while resetting core, busy did "
				    "not clear\n", sc->sc_dev.dv_xname);

			val = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    BCW_CIR_SBID_LO);
			if (val & BCW_CIR_SBID_LO_INITIATOR) {
				sbval = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    BCW_SBIMSTATE);
				bus_space_write_4(sc->sc_iot, sc->sc_ioh,
				    BCW_SBIMSTATE, sbval | SBIM_REJECT);
				sbval = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    BCW_SBIMSTATE);
				delay(1);

				/* wait until busy is clear */
				for (i = 0; i < 10000; i++) {
					val = bus_space_read_4(sc->sc_iot,
					sc->sc_ioh, BCW_SBTMSTATEHI);
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
			bus_space_write_4(sc->sc_iot, sc->sc_ioh, 
			    BCW_SBTMSTATELOW, SBTML_FGC | SBTML_CLK | 
			    SBTML_RESET | SBTML_80211FLAG | SBTML_80211PHY);
			val = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    BCW_SBTMSTATELOW);
			delay(10);

			val = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    BCW_CIR_SBID_LO);
			if (val & BCW_CIR_SBID_LO_INITIATOR) {
				sbval = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    BCW_SBIMSTATE);
				bus_space_write_4(sc->sc_iot, sc->sc_ioh,
				    BCW_SBIMSTATE, sbval & ~SBIM_REJECT);
				sbval = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				    BCW_SBIMSTATE);
				delay(1);

				/* wait until busy is clear */
				for (i = 0; i < 10000; i++) {
					val = bus_space_read_4(sc->sc_iot,
					sc->sc_ioh, BCW_SBTMSTATEHI);
					if (!(val & SBTMH_BUSY))
						break;
					delay(10);
				}
				if (i == 10000)
					printf("%s: while resetting core, busy "
					    "did not clear\n",
					    sc->sc_dev.dv_xname);
			} /* end initiator check */

			bus_space_write_4(sc->sc_iot, sc->sc_ioh,
			    BCW_SBTMSTATELOW, SBTML_RESET | reject |
			    SBTML_80211FLAG | SBTML_80211PHY);
			delay(1);
		}
	}

disabled:

	/* This is enabling/resetting the core */
	/* enable clock */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SBTMSTATELOW,
	    SBTML_FGC | SBTML_CLK | SBTML_RESET | 
	    SBTML_80211FLAG | SBTML_80211PHY );
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SBTMSTATELOW);
	delay(1);

	/* clear any error bits that may be on */
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SBTMSTATEHI);
	if (val & SBTMH_SERR)
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SBTMSTATEHI, 0);
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SBIMSTATE);
	if (val & (SBIM_INBANDERR | SBIM_TIMEOUT))
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SBIMSTATE,
		    val & ~(SBIM_INBANDERR | SBIM_TIMEOUT));

	/* clear reset and allow it to propagate throughout the core */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SBTMSTATELOW,
	    SBTML_FGC | SBTML_CLK | SBTML_80211FLAG | SBTML_80211PHY );
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SBTMSTATELOW);
	delay(1);

	/* leave clock enabled */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SBTMSTATELOW,
	    SBTML_CLK | SBTML_80211FLAG | SBTML_80211PHY);
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SBTMSTATELOW);
	delay(1);

	/* XXX update PHYConnected to requested value */

	/* Clear Baseband Attenuation, might only work for B/G rev < 0 */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, BCW_RADIO_BASEBAND, 0);

	/* Set 0x400 in the MMIO StatusBitField reg */
	sbval=bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SBF);
	sbval |= BCW_SBF_400_MAGIC;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SBF, sbval);

	/* XXX Clear saved interrupt status for DMA controllers */

}

/* Set up the receive filter. */
void
bcw_set_filter(struct ifnet *ifp)
{
#if 0
	struct bcw_softc *sc = ifp->if_softc;

	if (ifp->if_flags & IFF_PROMISC) {
		ifp->if_flags |= IFF_ALLMULTI;
		bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle, BCW_RX_CTL,
		    bus_space_read_4(sc->bcw_btag, sc->bcw_bhandle, BCW_RX_CTL)
		    | ERC_PE);
	} else {
		ifp->if_flags &= ~IFF_ALLMULTI;

		/* turn off promiscuous */
		bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle, BCW_RX_CTL,
		    bus_space_read_4(sc->bcw_btag, sc->bcw_bhandle,
		    BCW_RX_CTL) & ~ERC_PE);

		/* enable/disable broadcast */
		if (ifp->if_flags & IFF_BROADCAST)
			bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle,
			    BCW_RX_CTL, bus_space_read_4(sc->bcw_btag,
			    sc->bcw_bhandle, BCW_RX_CTL) & ~ERC_DB);
		else
			bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle,
			    BCW_RX_CTL, bus_space_read_4(sc->bcw_btag,
			    sc->bcw_bhandle, BCW_RX_CTL) | ERC_DB);

		/* disable the filter */
		bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle, BCW_FILT_CTL,
		    0);

		/* add our own address */
		// bcw_add_mac(sc, sc->bcw_ac.ac_enaddr, 0);

		/* for now accept all multicast */
		bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle, BCW_RX_CTL,
		bus_space_read_4(sc->bcw_btag, sc->bcw_bhandle, BCW_RX_CTL) |
		    ERC_AM);
		ifp->if_flags |= IFF_ALLMULTI;

		/* enable the filter */
		bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle, BCW_FILT_CTL,
		    bus_space_read_4(sc->bcw_btag, sc->bcw_bhandle,
		    BCW_FILT_CTL) | 1);
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
	u_int32_t save,val;

	/*
	 * We use the offset of zero a lot here to reset the SHM pointer to the
	 * beginning of it's memory area, as it automatically moves on every
	 * access to the SHM DATA registers
	 */
	 
	/* Backup SHM uCode Revision before we clobber it */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_CONTROL,
	    (BCW_SHM_CONTROL_SHARED << 16) + 0);
	save = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_DATA);
	
	/* write test value */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_CONTROL,
	    (BCW_SHM_CONTROL_SHARED << 16) + 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_DATA, 0xaa5555aa);
	/* Read it back */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_CONTROL,
	    (BCW_SHM_CONTROL_SHARED << 16) + 0);
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_DATA);
	if (val != 0xaa5555aa)
		return (1);
	
	/* write 2nd test value */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_CONTROL,
	    (BCW_SHM_CONTROL_SHARED << 16) + 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_DATA, 0x55aaaa55);
	/* Read it back */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_CONTROL,
	    (BCW_SHM_CONTROL_SHARED << 16) + 0);
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_DATA);
	if (val != 0x55aaaa55)
		return 2;

	/* Restore the saved value now that we're done */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_CONTROL,
	    (BCW_SHM_CONTROL_SHARED << 16) + 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_SHM_DATA, save);
	
	if (sc->sc_corerev >= 3) {
		/* do some test writes and reads against the TSF */
		/* 
		 * This works during the attach, but the spec at
		 * http://bcm-specs.sipsolutions.net/Timing
		 * say that we're reading/writing silly places, so these regs
		 * are not quite documented yet 
		 */
		bus_space_write_2(sc->sc_iot, sc->sc_ioh, 0x18c, 0xaaaa);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, 0x18c, 0xccccbbbb);
		val = bus_space_read_2(sc->sc_iot, sc->sc_ioh, 0x604);
		if (val != 0xbbbb) return 3;
		val = bus_space_read_2(sc->sc_iot, sc->sc_ioh, 0x606);
		if (val != 0xcccc) return 4;
		/* re-clear the TSF since we just filled it with garbage */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, 0x18c, 0x0);
	}	
	
	/* Check the Status Bit Field for some unknown bits */
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_SBF);
	if ((val | 0x80000000) != 0x80000400 ) {
		printf("%s: Warning, SBF is 0x%x, expected 0x80000400\n",
		    sc->sc_dev.dv_xname, val);
		/* May not be a critical failure, just warn for now */
		//return (5);
	}
	/* Verify there are no interrupts active on the core */
	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, BCW_GIR);
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

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_DMA_RXADDR,
	    ring->physaddr + 0x40000000);

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
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, BCW_DMA_TXADDR,
	    ring->physaddr + 0x40000000);

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


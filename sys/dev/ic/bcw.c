/*	$OpenBSD: bcw.c,v 1.2 2006/11/17 20:04:52 mglocker Exp $ */

/*
 * Copyright (c) 2006 Jon Simola <jsimola@gmail.com>
 * Copyright (c) 2003 Clifford Wright. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Broadcom BCM43xx Wireless network chipsets (broadcom.com)
 * SiliconBackplane is technology from Sonics, Inc.(sonicsinc.com)
 *
 * Cliff Wright cliff@snipe444.org
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
/* Functions copied from bce - */
void	bcw_watchdog(struct ifnet *);
void	bcw_rxintr(struct bcw_softc *);
void	bcw_txintr(struct bcw_softc *);
//void	bcw_add_mac(struct bcw_softc *, u_int8_t *, unsigned long);
int	bcw_add_rxbuf(struct bcw_softc *, int);
void	bcw_rxdrain(struct bcw_softc *);
void	bcw_set_filter(struct ifnet *); 
void	bcw_tick(void *);
int	bcw_ioctl(struct ifnet *, u_long, caddr_t);
/* 80211 functions copied from iwi */
int	bcw_newstate(struct ieee80211com *, enum ieee80211_state, int);
int	bcw_media_change(struct ifnet *);
void	bcw_media_status(struct ifnet *, struct ifmediareq *);
/* fashionably new functions */
int	bcw_validatechipaccess(struct bcw_softc *ifp);

/* read/write functions */
u_int8_t	bcw_read8(void *, u_int32_t);
u_int16_t	bcw_read16(void *, u_int32_t);
u_int32_t	bcw_read32(void *, u_int32_t);
void		bcw_write8(void *, u_int32_t, u_int8_t);
void		bcw_write16(void *, u_int32_t, u_int16_t);
void		bcw_write32(void *, u_int32_t, u_int32_t);
void		bcw_barrier(void *, u_int32_t, u_int32_t, int);

struct cfdriver bcw_cd = {
	NULL, "bcw", DV_IFNET
};

void
bcw_attach(struct bcw_softc *sc)
{
	struct ieee80211com *ic = &sc->bcw_ic;
	struct ifnet   *ifp = &ic->ic_if;
	struct bcw_regs *regs = &sc->bcw_regs;
#if 0
	struct pci_attach_args *pa = &sc->bcw_pa;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char     *intrstr = NULL;
#endif
	caddr_t         kva;
	bus_dma_segment_t seg;
	int             rseg;
#if 0
	pcireg_t        memtype;
	bus_addr_t      memaddr;
	bus_size_t      memsize;
	int             pmreg;
	pcireg_t        pmode;
#endif
	int             error;
	int             i,j;
	u_int32_t	sbval;
	u_int16_t	sbval16;
	
	//sc->bcw_pa = *pa;
	//sc->bcw_dmatag = pa->pa_dmat;

	if (sc->bcw_regs.r_read8 == NULL) {
		sc->bcw_regs.r_read8 = bcw_read8;
		sc->bcw_regs.r_read16 = bcw_read16;
		sc->bcw_regs.r_read32 = bcw_read32;
		sc->bcw_regs.r_write8 = bcw_write8;
		sc->bcw_regs.r_write16 = bcw_write16;
		sc->bcw_regs.r_write32 = bcw_write32;
		sc->bcw_regs.r_barrier = bcw_barrier;
	}

	/*
	 * Reset the chip
	 */
	bcw_reset(sc);

	/*
	 * Attach to the Backplane and start the card up
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

	sbval = BCW_READ32(regs, BCW_ADDR_SPACE0);
	if ((sbval & 0xffff0000) != 0x18000000) {
		DPRINTF(("%s: Trial Core read was 0x%x, single core only?\n",
		    sc->bcw_dev.dv_xname, sbval));
	 	//sc->bcw_singlecore=1;
	}

	/* 
	 * Try and change to the ChipCommon Core
	 */
	for (i = 0; i < 10; i++) {
		BCW_WRITE32(regs, BCW_ADDR_SPACE0, BCW_CORE_SELECT(0));
		delay(10);
		sbval = BCW_READ32(regs, BCW_ADDR_SPACE0);
		DPRINTF(("%s: Core change read %d = 0x%x\n",
		    sc->bcw_dev.dv_xname,i,sbval));
		if (sbval == BCW_CORE_SELECT(0)) break;
		delay(10);
	}
	//DPRINTF(("\n")); /* Pretty print so the debugs start on new lines */
	
	/*
	 * Core ID REG, this is either the default wireless core (0x812) or
	 * a ChipCommon core
	 */
	sbval = BCW_READ32(regs, BCW_CIR_SBID_HI);
	DPRINTF(("%s: Got Core ID Reg 0x%x, type is 0x%x\n",
	    sc->bcw_dev.dv_xname, sbval, (sbval & 0x8ff0) >> 4));
	
	/* If we successfully got a commoncore, and the corerev=4 or >=6
	   get the number of cores from the chipid reg */
	if (((sbval & 0x00008ff0) >> 4) == BCW_CORE_COMMON) {
		sc->bcw_havecommon = 1;
		sbval = BCW_READ32(regs, BCW_CORE_COMMON_CHIPID);
		sc->bcw_chipid = (sbval & 0x0000ffff);
		sc->bcw_corerev =
		    ((sbval & 0x00007000) >> 8 | (sbval & 0x0000000f));
		if ((sc->bcw_corerev == 4) || (sc->bcw_corerev >= 6))
			sc->bcw_numcores = (sbval & 0x0f000000) >> 24;
		else
			switch (sc->bcw_chipid) {
				case 0x4710:
				case 0x4610:
				case 0x4704:
					sc->bcw_numcores = 9;
					break;
				case 0x4310:
					sc->bcw_numcores = 8;
					break;
				case 0x5365:
					sc->bcw_numcores = 7;
					break;
				case 0x4306:
					sc->bcw_numcores = 6;
					break;
				case 0x4307:
				case 0x4301:
					sc->bcw_numcores = 5;
					break;
				case 0x4402:
					sc->bcw_numcores = 3;
					break;
				default:
					/* XXX Educated Guess */
					sc->bcw_numcores = 0;
			} /* end of switch */
	} else { /* No CommonCore, set chipid,cores,rev based on product id */
		sc->bcw_havecommon = 0;
		switch(sc->bcw_prodid) {
			case 0x4710:
			case 0x4711:
			case 0x4712:
			case 0x4713:
			case 0x4714:
			case 0x4715:
				sc->bcw_chipid = 0x4710;
				sc->bcw_numcores = 9;
				break;
			case 0x4610:
			case 0x4611:
			case 0x4612:
			case 0x4613:
			case 0x4614:
			case 0x4615:
				sc->bcw_chipid = 0x4610;
				sc->bcw_numcores = 9;
				break;
			case 0x4402:
			case 0x4403:
				sc->bcw_chipid = 0x4402;
				sc->bcw_numcores = 3;
				break;
			case 0x4305:
			case 0x4306:
			case 0x4307:
				sc->bcw_chipid = 0x4307;
				sc->bcw_numcores = 5;
				break;
			case 0x4301:
				sc->bcw_chipid = 0x4301;
				sc->bcw_numcores = 5;
				break;
			default:
				sc->bcw_chipid = sc->bcw_prodid;
				/* XXX educated guess */
				sc->bcw_numcores = 1;
		} /* end of switch */
	} /* End of if/else */

	DPRINTF(("%s: ChipID=0x%x, ChipRev=0x%x, NumCores=%d\n",
	    sc->bcw_dev.dv_xname, sc->bcw_chipid,
	    sc->bcw_chiprev, sc->bcw_numcores));

	/* Identify each core */
	if (sc->bcw_numcores >= 2) { /* Exclude single core chips */
 		for (i = 0; i <= sc->bcw_numcores; i++) {
 			DPRINTF(("%s: Trying core %d - ", 
 			    sc->bcw_dev.dv_xname, i));
 			BCW_WRITE32(regs, BCW_ADDR_SPACE0, BCW_CORE_SELECT(i));
			/* loop to see if the selected core shows up */
			for (j = 0; j < 10; j++) {
				sbval=BCW_READ32(regs, BCW_ADDR_SPACE0);
				DPRINTF(("%s: read %d for core %d = 0x%x ",
				    sc->bcw_dev.dv_xname, j, i, sbval));
				if (sbval == BCW_CORE_SELECT(i)) break;
				delay(10);
			}
			if (j < 10) 
				DPRINTF(("%s: Found core %d of type 0x%x\n",
				    sc->bcw_dev.dv_xname, i, 
				    (sbval & 0x00008ff0) >> 4));
			//sc->bcw_core[i].id = (sbval & 0x00008ff0) >> 4;
		} /* End of For loop */
		DPRINTF(("\n")); /* Make pretty debug output */
	}

	/*
	 * Attach cores to the backplane, if we have more than one
	 */
	// ??? if (!sc->bcw_singlecore) {
	if (sc->bcw_havecommon == 1) {
		sbval = BCW_READ32(regs, BCW_PCICR);
		sbval |= 0x1 << 8; /* XXX hardcoded bitmask of single core */
		BCW_WRITE32(regs, BCW_PCICR, sbval);
	}

	/*
	 * Get and display the PHY info from the MIMO
	 */
	sbval = BCW_READ16(regs, 0x3E0);
	sc->bcw_phy_version = (sbval&0xf000)>>12;
	sc->bcw_phy_rev = sbval&0xf;
	sc->bcw_phy_type = (sbval&0xf00)>>8;
	DPRINTF(("%s: PHY version %d revision %d ",
	    sc->bcw_dev.dv_xname, sc->bcw_phy_version, sc->bcw_phy_rev));
	switch (sc->bcw_phy_type) {
		case BCW_PHY_TYPEA:
			DPRINTF(("PHY %d (A)\n",sc->bcw_phy_type));
			break;
		case BCW_PHY_TYPEB:
			DPRINTF(("PHY %d (B)\n",sc->bcw_phy_type));
			break;
		case BCW_PHY_TYPEG:
			DPRINTF(("PHY %d (G)\n",sc->bcw_phy_type));
			break;
		case BCW_PHY_TYPEN:
			DPRINTF(("PHY %d (N)\n",sc->bcw_phy_type));
			break;
		default:
			DPRINTF(("Unrecognizeable PHY type %d\n",
			    sc->bcw_phy_type));
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
	if (sc->bcw_chipid != 0x4317) {
		BCW_WRITE16(regs, BCW_RADIO_CONTROL, BCW_RADIO_ID);
		sbval=BCW_READ16(regs, BCW_RADIO_DATAHIGH);
		sbval <<= 16;
		BCW_WRITE16(regs, BCW_RADIO_CONTROL, BCW_RADIO_ID);
		sc->bcw_radioid = sbval | BCW_READ16(regs, BCW_RADIO_DATALOW);
	} else {
		switch(sc->bcw_corerev) {
			case 0:	
				sc->bcw_radioid = 0x3205017F;
				break;
			case 1:
				sc->bcw_radioid = 0x4205017f;
				break;
			default:
				sc->bcw_radioid = 0x5205017f;
		}
	}

	sc->bcw_radiorev = (sc->bcw_radioid & 0xf0000000) >> 28;
	sc->bcw_radiotype = (sc->bcw_radioid & 0x0ffff000) >> 12;

	DPRINTF(("%s: Radio Rev %d, Ver 0x%x, Manuf 0x%x\n",
	    sc->bcw_dev.dv_xname, sc->bcw_radiorev, sc->bcw_radiotype,
	    sc->bcw_radioid & 0xfff));

	error = bcw_validatechipaccess(sc);
	if (error) {
		printf("%s: failed Chip Access Validation at %d\n",
		    sc->bcw_dev.dv_xname, error);
		return;
	}

	/* Test for valid PHY/revision combinations, probably a simpler way */
	if (sc->bcw_phy_type == BCW_PHY_TYPEA) {
		switch(sc->bcw_phy_rev) {
			case 2:
			case 3:
			case 5:
			case 6:
			case 7:	break;
			default: 
				printf("%s: invalid PHY A revision %d\n",
				    sc->bcw_dev.dv_xname, sc->bcw_phy_rev);
				return;
		}
	}
	if (sc->bcw_phy_type == BCW_PHY_TYPEB) {
		switch(sc->bcw_phy_rev) {
			case 2:
			case 4:
			case 7:	break;
			default: 
				printf("%s: invalid PHY B revision %d\n",
				    sc->bcw_dev.dv_xname, sc->bcw_phy_rev);
				return;
		}
	}
	if (sc->bcw_phy_type == BCW_PHY_TYPEG) {
		switch(sc->bcw_phy_rev) {
			case 1:
			case 2:
			case 4:
			case 6:
			case 7:
			case 8:	break;
			default: 
				printf("%s: invalid PHY G revision %d\n",
				    sc->bcw_dev.dv_xname, sc->bcw_phy_rev);
				return;
		}
	}

	/* test for valid radio revisions */
	if ((sc->bcw_phy_type == BCW_PHY_TYPEA) &
	    (sc->bcw_radiotype != 0x2060)) {
		    	printf("%s: invalid PHY A radio 0x%x\n",
		    	    sc->bcw_dev.dv_xname, sc->bcw_radiotype);
		    	return;
	}
	if ((sc->bcw_phy_type == BCW_PHY_TYPEB) &
	    ((sc->bcw_radiotype & 0xfff0) != 0x2050)) {
		    	printf("%s: invalid PHY B radio 0x%x\n",
		    	    sc->bcw_dev.dv_xname, sc->bcw_radiotype);
		    	return;
	}
	if ((sc->bcw_phy_type == BCW_PHY_TYPEG) &
	    (sc->bcw_radiotype != 0x2050)) {
		    	printf("%s: invalid PHY G radio 0x%x\n",
		    	    sc->bcw_dev.dv_xname, sc->bcw_radiotype);
		    	return;
	}

	/*
	 * Switch the radio off - candidate for seperate function
	 */
	switch(sc->bcw_phy_type) {
		case BCW_PHY_TYPEA:
			/* Magic unexplained values */
			BCW_WRITE16(regs, BCW_RADIO_CONTROL, 0x04);
			BCW_WRITE16(regs, BCW_RADIO_DATALOW, 0xff);
			BCW_WRITE16(regs, BCW_RADIO_CONTROL, 0x05);
			BCW_WRITE16(regs, BCW_RADIO_DATALOW, 0xfb);
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
			BCW_WRITE16(regs, BCW_PHY_CONTROL, 0x10);
			sbval16 = BCW_READ16(regs, BCW_PHY_DATA);
			sbval16 |= 0x8;
			BCW_WRITE16(regs, BCW_PHY_CONTROL, 0x10);
			BCW_WRITE16(regs, BCW_PHY_DATA, sbval16);

			BCW_WRITE16(regs, BCW_PHY_CONTROL, 0x11);
			sbval16 = BCW_READ16(regs, BCW_PHY_DATA);
			sbval16 |= 0x8;
			BCW_WRITE16(regs, BCW_PHY_CONTROL, 0x11);
			BCW_WRITE16(regs, BCW_PHY_DATA, sbval16);
			break;
		case BCW_PHY_TYPEG:
			if (sc->bcw_corerev >= 5) {
				BCW_WRITE16(regs, BCW_PHY_CONTROL, 0x811);
				sbval16 = BCW_READ16(regs, BCW_PHY_DATA);
				sbval16 |= 0x8c;
				BCW_WRITE16(regs, BCW_PHY_CONTROL, 0x811);
				BCW_WRITE16(regs, BCW_PHY_DATA, sbval16);

				BCW_WRITE16(regs, BCW_PHY_CONTROL, 0x812);
				sbval16 = BCW_READ16(regs, BCW_PHY_DATA);
				sbval16 &= 0xff73;
				BCW_WRITE16(regs, BCW_PHY_CONTROL, 0x812);
				BCW_WRITE16(regs, BCW_PHY_DATA, sbval16);
			}
			/* FALL-THROUGH */
		default:
			BCW_WRITE16(regs, BCW_PHY_CONTROL, 0x15);
			BCW_WRITE16(regs, BCW_PHY_DATA, 0xaa00);
	} /* end of switch statement to turn off radio */

	// XXX - Get a copy of the BoardFlags to keep in RAM

	/* Read antenna gain from SPROM and multiply by 4 */
	sbval = BCW_READ16(regs, BCW_SPROM_ANTGAIN);
	/* If unset, assume 2 */
	if((sbval == 0) || (sbval == 0xffff)) sbval = 0x0202;
	if(sc->bcw_phy_type == BCW_PHY_TYPEA)
		sc->bcw_radio_gain = (sbval & 0xff);
	else
		sc->bcw_radio_gain = ((sbval & 0xff00) >> 8);
	sc->bcw_radio_gain *= 4;

	/*
	 * Set the paXbY vars, X=0 for PHY A, X=1 for B/G, but we'll
	 * just grab them all while we're here
	 */
	sc->bcw_radio_pa0b0 = BCW_READ16(regs, BCW_SPROM_PA0B0);
	sc->bcw_radio_pa0b1 = BCW_READ16(regs, BCW_SPROM_PA0B1);
	sc->bcw_radio_pa0b2 = BCW_READ16(regs, BCW_SPROM_PA0B2);
	sc->bcw_radio_pa1b0 = BCW_READ16(regs, BCW_SPROM_PA1B0);
	sc->bcw_radio_pa1b1 = BCW_READ16(regs, BCW_SPROM_PA1B1);
	sc->bcw_radio_pa1b2 = BCW_READ16(regs, BCW_SPROM_PA1B2);

	/* Get the idle TSSI */
	sbval = BCW_READ16(regs, BCW_SPROM_IDLETSSI);
	if(sc->bcw_phy_type == BCW_PHY_TYPEA)
		sc->bcw_idletssi = (sbval & 0xff);
	else
		sc->bcw_idletssi = ((sbval & 0xff00) >> 8);
	
	
	/* Init the Microcode Flags Bitfield */
	/* http://bcm-specs.sipsolutions.net/MicrocodeFlagsBitfield */
	
	sbval = 0;
	if((sc->bcw_phy_type == BCW_PHY_TYPEA) ||
	    (sc->bcw_phy_type == BCW_PHY_TYPEB) ||
	    (sc->bcw_phy_type == BCW_PHY_TYPEG))
		sbval |= 2; /* Turned on during init for non N phys */
	if((sc->bcw_phy_type == BCW_PHY_TYPEG) &&
	    (sc->bcw_phy_rev == 1))
		sbval |= 0x20;
	/*
	 * XXX If this is a G PHY with BoardFlags BFL_PACTRL set,
	 * set bit 0x40
	 */
	if((sc->bcw_phy_type == BCW_PHY_TYPEG) &&
	    (sc->bcw_phy_rev < 3))
		sbval |= 0x8; /* MAGIC */
	/* XXX If BoardFlags BFL_XTAL is set, set bit 0x400 */
	if(sc->bcw_phy_type == BCW_PHY_TYPEB)
		sbval |= 0x4;
	if((sc->bcw_radiotype == 0x2050) &&
	    (sc->bcw_radiorev <= 5))
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
	BCW_WRITE32(regs, BCW_SHM_CONTROL, 
	    (BCW_SHM_CONTROL_SHARED << 16) + BCW_SHM_MICROCODEFLAGSLOW - 2);
	BCW_WRITE16(regs, BCW_SHM_DATAHIGH, sbval & 0x00ff);
	BCW_WRITE16(regs, BCW_SHM_DATALOW, (sbval & 0xff00)>>16);

	/* 
	 * Initialize the TSSI to DBM table
	 * The method is described at 
	 * http://bcm-specs.sipsolutions.net/TSSI_to_DBM_Table
	 * but I suspect there's a standard way to do it in the 80211 stuff
	 */
	
	/* 
	 * TODO still for the card attach: 
	 * - Disable the 80211 Core
	 * - Powercontrol Crystal Off (and write a wrapper for on/off)
	 * - Setup LEDs to blink in whatever fashionable manner
	 */

	/*
	 * Allocate DMA-safe memory for ring descriptors.
	 * The receive, and transmit rings are 4k aligned
	 * XXX - still not too sure on how this works, it is
	 * some of the remaining untouched code from if_bce
	 */
	if ((error = bus_dmamem_alloc(sc->bcw_dmatag,
	    2 * PAGE_SIZE, PAGE_SIZE, 2 * PAGE_SIZE,
	    &seg, 1, &rseg, BUS_DMA_NOWAIT))) {
		printf("%s: unable to alloc space for ring descriptors, "
		       "error = %d\n", sc->bcw_dev.dv_xname, error);
		return;
	}
	/* map ring space to kernel */
	if ((error = bus_dmamem_map(sc->bcw_dmatag, &seg, rseg,
	    2 * PAGE_SIZE, &kva, BUS_DMA_NOWAIT))) {
		printf("%s: unable to map DMA buffers, error = %d\n",
		    sc->bcw_dev.dv_xname, error);
		bus_dmamem_free(sc->bcw_dmatag, &seg, rseg);
		return;
	}
	/* create a dma map for the ring */
	if ((error = bus_dmamap_create(sc->bcw_dmatag,
	    2 * PAGE_SIZE, 1, 2 * PAGE_SIZE, 0, BUS_DMA_NOWAIT,
	    &sc->bcw_ring_map))) {
		printf("%s: unable to create ring DMA map, error = %d\n",
		    sc->bcw_dev.dv_xname, error);
		bus_dmamem_unmap(sc->bcw_dmatag, kva, 2 * PAGE_SIZE);
		bus_dmamem_free(sc->bcw_dmatag, &seg, rseg);
		return;
	}
	/* connect the ring space to the dma map */
	if (bus_dmamap_load(sc->bcw_dmatag, sc->bcw_ring_map, kva,
	    2 * PAGE_SIZE, NULL, BUS_DMA_NOWAIT)) {
		bus_dmamap_destroy(sc->bcw_dmatag, sc->bcw_ring_map);
		bus_dmamem_unmap(sc->bcw_dmatag, kva, 2 * PAGE_SIZE);
		bus_dmamem_free(sc->bcw_dmatag, &seg, rseg);
		return;
	}
	/* save the ring space in softc */
	sc->bcw_rx_ring = (struct bcw_dma_slot *) kva;
	sc->bcw_tx_ring = (struct bcw_dma_slot *) (kva + PAGE_SIZE);

	/* Create the transmit buffer DMA maps. */
	for (i = 0; i < BCW_NTXDESC; i++) {
		if ((error = bus_dmamap_create(sc->bcw_dmatag, MCLBYTES,
		    BCW_NTXFRAGS, MCLBYTES, 0, 0,
		    &sc->bcw_cdata.bcw_tx_map[i])) != 0) {
			printf("%s: unable to create tx DMA map, error = %d\n",
			    sc->bcw_dev.dv_xname, error);
		}
		sc->bcw_cdata.bcw_tx_chain[i] = NULL;
	}

	/* Create the receive buffer DMA maps. */
	for (i = 0; i < BCW_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->bcw_dmatag, MCLBYTES, 1,
		    MCLBYTES, 0, 0, &sc->bcw_cdata.bcw_rx_map[i])) != 0) {
			printf("%s: unable to create rx DMA map, error = %d\n",
			    sc->bcw_dev.dv_xname, error);
		}
		sc->bcw_cdata.bcw_rx_chain[i] = NULL;
	}

	/* End of the DMA stuff */

	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA; /* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;
	
	/* set device capabilities - keep it simple */
	ic->ic_caps = IEEE80211_C_IBSS; /* IBSS mode supported */

	/* MAC address */
	if(sc->bcw_phy_type == BCW_PHY_TYPEA) {
		i=BCW_READ16(regs, BCW_SPROM_ET1MACADDR);
		ic->ic_myaddr[0] = (i & 0xff00) >> 8;
		ic->ic_myaddr[1] = i & 0xff;
		i=BCW_READ16(regs, BCW_SPROM_ET1MACADDR + 2);
		ic->ic_myaddr[2] = (i & 0xff00) >> 8;
		ic->ic_myaddr[3] = i & 0xff;
		i=BCW_READ16(regs, BCW_SPROM_ET1MACADDR + 4);
		ic->ic_myaddr[4] = (i & 0xff00) >> 8;
		ic->ic_myaddr[5] = i & 0xff;
	} else { /* assume B or G PHY */
		i=BCW_READ16(regs, BCW_SPROM_IL0MACADDR);
		ic->ic_myaddr[0] = (i & 0xff00) >> 8;
		ic->ic_myaddr[1] = i & 0xff;
		i=BCW_READ16(regs, BCW_SPROM_IL0MACADDR + 2);
		ic->ic_myaddr[2] = (i & 0xff00) >> 8;
		ic->ic_myaddr[3] = i & 0xff;
		i=BCW_READ16(regs, BCW_SPROM_IL0MACADDR + 4);
		ic->ic_myaddr[4] = (i & 0xff00) >> 8;
		ic->ic_myaddr[5] = i & 0xff;
	}
	
	printf(": %s, address %s\n", sc->bcw_intrstr,
	    ether_sprintf(ic->ic_myaddr));

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
	bcopy(sc->bcw_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	/* Attach the interface */
	if_attach(ifp);
	ieee80211_ifattach(ifp);
        /* override state transition machine */
        sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = bcw_newstate;
	ieee80211_media_init(ifp, bcw_media_change, bcw_media_status);

	timeout_set(&sc->bcw_timeout, bcw_tick, sc);
}

/* handle media, and ethernet requests */
int
bcw_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct bcw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->bcw_ic;
	//struct bcw_regs *regs = &sc->bcw_regs;
	struct ifreq   *ifr = (struct ifreq *) data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int             s, error = 0;

	s = splnet();
#if 0
	if ((error = ether_ioctl(ifp, &sc->bcw_ac, cmd, data)) > 0) {
		splx(s);
		return (error);
	}
#endif

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			bcw_init(ifp);
			arp_ifinit(&sc->bcw_ac, ifa);
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
	struct bcw_softc *sc = ifp->if_softc;
	struct bcw_regs *regs = &sc->bcw_regs;
	struct mbuf    *m0;
	bus_dmamap_t    dmamap;
	int             txstart;
	int             txsfree;
	int             newpkts = 0;
	int             error;

	/*
         * do not start another if currently transmitting, and more
         * descriptors(tx slots) are needed for next packet.
         */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/* determine number of descriptors available */
	if (sc->bcw_txsnext >= sc->bcw_txin)
		txsfree = BCW_NTXDESC - 1 + sc->bcw_txin - sc->bcw_txsnext;
	else
		txsfree = sc->bcw_txin - sc->bcw_txsnext - 1;

	/*
         * Loop through the send queue, setting up transmit descriptors
         * until we drain the queue, or use up all available transmit
         * descriptors.
         */
	while (txsfree > 0) {
		int             seg;

		/* Grab a packet off the queue. */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		/* get the transmit slot dma map */
		dmamap = sc->bcw_cdata.bcw_tx_map[sc->bcw_txsnext];

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we
		 * were short on resources. If the packet will not fit,
		 * it will be dropped. If short on resources, it will
		 * be tried again later.
		 */
		error = bus_dmamap_load_mbuf(sc->bcw_dmatag, dmamap, m0,
		    BUS_DMA_WRITE | BUS_DMA_NOWAIT);
		if (error == EFBIG) {
			printf("%s: Tx packet consumes too many DMA segments, "
			    "dropping...\n", sc->bcw_dev.dv_xname);
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			m_freem(m0);
			ifp->if_oerrors++;
			continue;
		} else if (error) {
			/* short on resources, come back later */
			printf("%s: unable to load Tx buffer, error = %d\n",
			    sc->bcw_dev.dv_xname, error);
			break;
		}
		/* If not enough descriptors available, try again later */
		if (dmamap->dm_nsegs > txsfree) {
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->bcw_dmatag, dmamap);
			break;
		}
		/* WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET. */

		/* So take it off the queue */
		IFQ_DEQUEUE(&ifp->if_snd, m0);

		/* save the pointer so it can be freed later */
		sc->bcw_cdata.bcw_tx_chain[sc->bcw_txsnext] = m0;

		/* Sync the data DMA map. */
		bus_dmamap_sync(sc->bcw_dmatag, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/* Initialize the transmit descriptor(s). */
		txstart = sc->bcw_txsnext;
		for (seg = 0; seg < dmamap->dm_nsegs; seg++) {
			u_int32_t ctrl;

			ctrl = dmamap->dm_segs[seg].ds_len & CTRL_BC_MASK;
			if (seg == 0)
				ctrl |= CTRL_SOF;
			if (seg == dmamap->dm_nsegs - 1)
				ctrl |= CTRL_EOF;
			if (sc->bcw_txsnext == BCW_NTXDESC - 1)
				ctrl |= CTRL_EOT;
			ctrl |= CTRL_IOC;
			sc->bcw_tx_ring[sc->bcw_txsnext].ctrl = htole32(ctrl);
			/* MAGIC */
			sc->bcw_tx_ring[sc->bcw_txsnext].addr =
			    htole32(dmamap->dm_segs[seg].ds_addr + 0x40000000);
			if (sc->bcw_txsnext + 1 > BCW_NTXDESC - 1)
				sc->bcw_txsnext = 0;
			else
				sc->bcw_txsnext++;
			txsfree--;
		}
		/* sync descriptors being used */
		bus_dmamap_sync(sc->bcw_dmatag, sc->bcw_ring_map,
			  sizeof(struct bcw_dma_slot) * txstart + PAGE_SIZE,
			     sizeof(struct bcw_dma_slot) * dmamap->dm_nsegs,
				BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Give the packet to the chip. */
		BCW_WRITE32(regs, BCW_DMA_DPTR,
			     sc->bcw_txsnext * sizeof(struct bcw_dma_slot));

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

	printf("%s: device timeout\n", sc->bcw_dev.dv_xname);
	ifp->if_oerrors++;

	(void) bcw_init(ifp);

	/* Try to get more packets going. */
	bcw_start(ifp);
}

int
bcw_intr(void *xsc)
{
	struct bcw_softc *sc;
	struct bcw_regs *regs;
	struct ifnet *ifp;
	u_int32_t intstatus;
	int wantinit;
	int handled = 0;

	sc = xsc;
	regs = &sc->bcw_regs;
	ifp = &sc->bcw_ac.ac_if;

	for (wantinit = 0; wantinit == 0;) {
		intstatus = BCW_READ32(regs, BCW_INT_STS);

		/* ignore if not ours, or unsolicited interrupts */
		intstatus &= sc->bcw_intmask;
		if (intstatus == 0)
			break;

		handled = 1;

		/* Ack interrupt */
		BCW_WRITE32(regs, BCW_INT_STS, intstatus);

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
				    sc->bcw_dev.dv_xname);
			if (intstatus & I_RO) {
				printf("%s: receive fifo overflow\n",
				    sc->bcw_dev.dv_xname);
				ifp->if_ierrors++;
			}
			if (intstatus & I_RU)
				printf("%s: receive descriptor underflow\n",
				       sc->bcw_dev.dv_xname);
			if (intstatus & I_DE)
				printf("%s: descriptor protocol error\n",
				       sc->bcw_dev.dv_xname);
			if (intstatus & I_PD)
				printf("%s: data error\n",
				    sc->bcw_dev.dv_xname);
			if (intstatus & I_PC)
				printf("%s: descriptor error\n",
				    sc->bcw_dev.dv_xname);
			if (intstatus & I_TO)
				printf("%s: general purpose timeout\n",
				    sc->bcw_dev.dv_xname);
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
	struct ifnet *ifp = &sc->bcw_ac.ac_if;
	struct bcw_regs *regs = &sc->bcw_regs;
	struct rx_pph *pph;
	struct mbuf *m;
	int curr;
	int len;
	int i;

	/* get pointer to active receive slot */
	curr = BCW_READ32(regs, BCW_DMA_RXSTATUS) & RS_CD_MASK;
	curr = curr / sizeof(struct bcw_dma_slot);
	if (curr >= BCW_NRXDESC)
		curr = BCW_NRXDESC - 1;

	/* process packets up to but not current packet being worked on */
	for (i = sc->bcw_rxin; i != curr;
	    i + 1 > BCW_NRXDESC - 1 ? i = 0 : i++) {
		/* complete any post dma memory ops on packet */
		bus_dmamap_sync(sc->bcw_dmatag, sc->bcw_cdata.bcw_rx_map[i], 0,
		    sc->bcw_cdata.bcw_rx_map[i]->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);

		/*
		 * If the packet had an error, simply recycle the buffer,
		 * resetting the len, and flags.
		 */
		pph = mtod(sc->bcw_cdata.bcw_rx_chain[i], struct rx_pph *);
		if (pph->flags & (RXF_NO | RXF_RXER | RXF_CRC | RXF_OV)) {
			ifp->if_ierrors++;
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
		sc->bcw_cdata.bcw_rx_chain[i]->m_data += 
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
			    mtod(sc->bcw_cdata.bcw_rx_chain[i], caddr_t), len);
			sc->bcw_cdata.bcw_rx_chain[i]->m_data -=
			    BCW_PREPKT_HEADER_SIZE;
		} else {
			m = sc->bcw_cdata.bcw_rx_chain[i];
			if (bcw_add_rxbuf(sc, i) != 0) {
		dropit:
				ifp->if_ierrors++;
				/* continue to use old buffer */
				sc->bcw_cdata.bcw_rx_chain[i]->m_data -=
				    BCW_PREPKT_HEADER_SIZE;
				bus_dmamap_sync(sc->bcw_dmatag,
				    sc->bcw_cdata.bcw_rx_map[i], 0,
				    sc->bcw_cdata.bcw_rx_map[i]->dm_mapsize,
				    BUS_DMASYNC_PREREAD);
				continue;
			}
		}

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;
		ifp->if_ipackets++;

#if NBPFILTER > 0
		/*
		 * Pass this up to any BPF listeners, but only
		 * pass it up the stack if it's for us.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif				/* NBPFILTER > 0 */

		/* Pass it on. */
		ether_input_mbuf(ifp, m);

		/* re-check current in case it changed */
		curr = (BCW_READ32(regs,
		    BCW_DMA_RXSTATUS) & RS_CD_MASK) /
		    sizeof(struct bcw_dma_slot);
		if (curr >= BCW_NRXDESC)
			curr = BCW_NRXDESC - 1;
	}
	sc->bcw_rxin = curr;
}

/* Transmit interrupt handler */
void
bcw_txintr(struct bcw_softc *sc)
{
	struct ifnet *ifp = &sc->bcw_ac.ac_if;
	struct bcw_regs *regs = &sc->bcw_regs;
	int curr;
	int i;

	ifp->if_flags &= ~IFF_OACTIVE;

	/*
         * Go through the Tx list and free mbufs for those
         * frames which have been transmitted.
         */
	curr = BCW_READ32(regs, BCW_DMA_TXSTATUS) & RS_CD_MASK;
	curr = curr / sizeof(struct bcw_dma_slot);
	if (curr >= BCW_NTXDESC)
		curr = BCW_NTXDESC - 1;
	for (i = sc->bcw_txin; i != curr;
	    i + 1 > BCW_NTXDESC - 1 ? i = 0 : i++) {
		/* do any post dma memory ops on transmit data */
		if (sc->bcw_cdata.bcw_tx_chain[i] == NULL)
			continue;
		bus_dmamap_sync(sc->bcw_dmatag, sc->bcw_cdata.bcw_tx_map[i], 0,
		    sc->bcw_cdata.bcw_tx_map[i]->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->bcw_dmatag, sc->bcw_cdata.bcw_tx_map[i]);
		m_freem(sc->bcw_cdata.bcw_tx_chain[i]);
		sc->bcw_cdata.bcw_tx_chain[i] = NULL;
		ifp->if_opackets++;
	}
	sc->bcw_txin = curr;

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer
	 */
	if (sc->bcw_txsnext == sc->bcw_txin)
		ifp->if_timer = 0;
}

/* initialize the interface */
int
bcw_init(struct ifnet *ifp)
{
	struct bcw_softc *sc = ifp->if_softc;
	struct bcw_regs *regs = &sc->bcw_regs;
	u_int32_t reg_win;
	int error;
	int i;

	/* Cancel any pending I/O. */
	bcw_stop(ifp, 0);

	/*
	 * Most of this needs to be rewritten to take into account the
	 * possible single/multiple core nature of the BCM43xx, and the
	 * differences from the BCM44xx ethernet chip that if_bce.c is
	 * written for.
	 */

	/* enable pci inerrupts, bursts, and prefetch */

	/* remap the pci registers to the Sonics config registers */

	/* save the current map, so it can be restored */
	reg_win = BCW_READ32(regs, BCW_REG0_WIN);

	/* set register window to Sonics registers */
	BCW_WRITE32(regs, BCW_REG0_WIN, BCW_SONICS_WIN);

	/* enable SB to PCI interrupt */
	BCW_WRITE32(regs, BCW_SBINTVEC,
	    BCW_READ32(regs, BCW_SBINTVEC) |
	    SBIV_ENET0);

	/* enable prefetch and bursts for sonics-to-pci translation 2 */
	BCW_WRITE32(regs, BCW_SPCI_TR2,
	    BCW_READ32(regs, BCW_SPCI_TR2) |
	    SBTOPCI_PREF | SBTOPCI_BURST);

	/* restore to ethernet register space */
	BCW_WRITE32(regs, BCW_REG0_WIN, reg_win);

	/* Reset the chip to a known state. */
	bcw_reset(sc);

	/* Initialize transmit descriptors */
	memset(sc->bcw_tx_ring, 0, BCW_NTXDESC * sizeof(struct bcw_dma_slot));
	sc->bcw_txsnext = 0;
	sc->bcw_txin = 0;

	/* enable crc32 generation and set proper LED modes */
	BCW_WRITE32(regs, BCW_MACCTL,
	    BCW_READ32(regs, BCW_MACCTL) |
	    BCW_EMC_CRC32_ENAB | BCW_EMC_LED);

	/* reset or clear powerdown control bit  */
	BCW_WRITE32(regs, BCW_MACCTL,
	    BCW_READ32(regs, BCW_MACCTL) &
	    ~BCW_EMC_PDOWN);

	/* setup DMA interrupt control */
	BCW_WRITE32(regs, BCW_DMAI_CTL, 1 << 24);	/* MAGIC */

	/* setup packet filter */
	bcw_set_filter(ifp);

	/* set max frame length, account for possible VLAN tag */
	BCW_WRITE32(regs, BCW_RX_MAX,
	    ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN);
	BCW_WRITE32(regs, BCW_TX_MAX,
	    ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN);

	/* set tx watermark */
	BCW_WRITE32(regs, BCW_TX_WATER, 56);

	/* enable transmit */
	BCW_WRITE32(regs, BCW_DMA_TXCTL, XC_XE);
	/* MAGIC */
	BCW_WRITE32(regs, BCW_DMA_TXADDR,
	    sc->bcw_ring_map->dm_segs[0].ds_addr + PAGE_SIZE + 0x40000000);

	/*
         * Give the receive ring to the chip, and
         * start the receive DMA engine.
         */
	sc->bcw_rxin = 0;

	/* clear the rx descriptor ring */
	memset(sc->bcw_rx_ring, 0, BCW_NRXDESC * sizeof(struct bcw_dma_slot));
	/* enable receive */
	BCW_WRITE32(regs, BCW_DMA_RXCTL,
	    BCW_PREPKT_HEADER_SIZE << 1 | 1);
	BCW_WRITE32(regs, BCW_DMA_RXADDR,
	    sc->bcw_ring_map->dm_segs[0].ds_addr + 0x40000000);	/* MAGIC */

	/* Initalize receive descriptors */
	for (i = 0; i < BCW_NRXDESC; i++) {
		if (sc->bcw_cdata.bcw_rx_chain[i] == NULL) {
			if ((error = bcw_add_rxbuf(sc, i)) != 0) {
				printf("%s: unable to allocate or map rx(%d) "
				    "mbuf, error = %d\n", sc->bcw_dev.dv_xname,
				    i, error);
				bcw_rxdrain(sc);
				return (error);
			}
		} else
			BCW_INIT_RXDESC(sc, i);
	}

	/* Enable interrupts */
	sc->bcw_intmask =
	    I_XI | I_RI | I_XU | I_RO | I_RU | I_DE | I_PD | I_PC | I_TO;
	BCW_WRITE32(regs, BCW_INT_MASK,
	    sc->bcw_intmask);

	/* start the receive dma */
	BCW_WRITE32(regs, BCW_DMA_RXDPTR,
	    BCW_NRXDESC * sizeof(struct bcw_dma_slot));

	/* set media */
	//mii_mediachg(&sc->bcw_mii);

	/* turn on the ethernet mac */
#if 0
	bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle, BCW_ENET_CTL,
	    bus_space_read_4(sc->bcw_btag, sc->bcw_bhandle,
	    BCW_ENET_CTL) | EC_EE);
#endif

	/* start timer */
	timeout_add(&sc->bcw_timeout, hz);

	/* mark as running, and no outputs active */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return (0);
}

/* Add a receive buffer to the indiciated descriptor. */
int
bcw_add_rxbuf(struct bcw_softc *sc, int idx)
{
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
	if (sc->bcw_cdata.bcw_rx_chain[idx] != NULL)
		bus_dmamap_unload(sc->bcw_dmatag,
		    sc->bcw_cdata.bcw_rx_map[idx]);

	sc->bcw_cdata.bcw_rx_chain[idx] = m;

	error = bus_dmamap_load(sc->bcw_dmatag, sc->bcw_cdata.bcw_rx_map[idx],
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ | BUS_DMA_NOWAIT);
	if (error)
		return (error);

	bus_dmamap_sync(sc->bcw_dmatag, sc->bcw_cdata.bcw_rx_map[idx], 0,
	    sc->bcw_cdata.bcw_rx_map[idx]->dm_mapsize, BUS_DMASYNC_PREREAD);

	BCW_INIT_RXDESC(sc, idx);

	return (0);

}

/* Drain the receive queue. */
void
bcw_rxdrain(struct bcw_softc *sc)
{
	int i;

	for (i = 0; i < BCW_NRXDESC; i++) {
		if (sc->bcw_cdata.bcw_rx_chain[i] != NULL) {
			bus_dmamap_unload(sc->bcw_dmatag,
			    sc->bcw_cdata.bcw_rx_map[i]);
			m_freem(sc->bcw_cdata.bcw_rx_chain[i]);
			sc->bcw_cdata.bcw_rx_chain[i] = NULL;
		}
	}
}

/* Stop transmission on the interface */
void
bcw_stop(struct ifnet *ifp, int disable)
{
	struct bcw_softc *sc = ifp->if_softc;
	struct bcw_regs *regs = &sc->bcw_regs;
	int i;
	//u_int32_t val;

	/* Stop the 1 second timer */
	timeout_del(&sc->bcw_timeout);

	/* Mark the interface down and cancel the watchdog timer. */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	/* Disable interrupts. */
	BCW_WRITE32(regs, BCW_INT_MASK, 0);
	sc->bcw_intmask = 0;
	delay(10);

	/* Disable emac */
#if 0
	bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle, BCW_ENET_CTL, EC_ED);
	for (i = 0; i < 200; i++) {
		val = bus_space_read_4(sc->bcw_btag, sc->bcw_bhandle,
		    BCW_ENET_CTL);
		if (!(val & EC_ED))
			break;
		delay(10);
	}
#endif
	/* Stop the DMA */
	BCW_WRITE32(regs, BCW_DMA_RXCTL, 0);
	BCW_WRITE32(regs, BCW_DMA_TXCTL, 0);
	delay(10);

	/* Release any queued transmit buffers. */
	for (i = 0; i < BCW_NTXDESC; i++) {
		if (sc->bcw_cdata.bcw_tx_chain[i] != NULL) {
			bus_dmamap_unload(sc->bcw_dmatag,
			    sc->bcw_cdata.bcw_tx_map[i]);
			m_freem(sc->bcw_cdata.bcw_tx_chain[i]);
			sc->bcw_cdata.bcw_tx_chain[i] = NULL;
		}
	}

	/* drain receive queue */
	if (disable)
		bcw_rxdrain(sc);
}

/* reset the chip */
void
bcw_reset(struct bcw_softc *sc)
{
	struct bcw_regs *regs = &sc->bcw_regs;
	u_int32_t val;
	u_int32_t sbval;
	int i;
	
	/* if SB core is up, only clock of clock,reset,reject will be set */
	sbval = BCW_READ32(regs, BCW_SBTMSTATELOW);

	/* The core isn't running if the if the clock isn't enabled */
	if ((sbval & (SBTML_RESET | SBTML_REJ | SBTML_CLK)) == SBTML_CLK) {

		// Stop all DMA
		BCW_WRITE32(regs, BCW_DMAI_CTL, 0);

		/* reset the dma engines */
		BCW_WRITE32(regs, BCW_DMA_TXCTL, 0);
		val = BCW_READ32(regs, BCW_DMA_RXSTATUS);
		/* if error on receive, wait to go idle */
		if (val & RS_ERROR) {
			for (i = 0; i < 100; i++) {
				val = BCW_READ32(regs, BCW_DMA_RXSTATUS);
				if (val & RS_DMA_IDLE)
					break;
				delay(10);
			}
			if (i == 100)
				printf("%s: receive dma did not go idle after"
				    " error\n", sc->bcw_dev.dv_xname);
		}
		BCW_WRITE32(regs, BCW_DMA_RXSTATUS, 0);

		/* reset ethernet mac */
#if 0
		bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle, BCW_ENET_CTL,
		    EC_ES);
		for (i = 0; i < 200; i++) {
			val = bus_space_read_4(sc->bcw_btag, sc->bcw_bhandle,
			    BCW_ENET_CTL);
			if (!(val & EC_ES))
				break;
			delay(10);
		}
		if (i == 200)
			printf("%s: timed out resetting ethernet mac\n",
			       sc->bcw_dev.dv_xname);
#endif
	} else {
		u_int32_t reg_win;

		/* remap the pci registers to the Sonics config registers */

		/* save the current map, so it can be restored */
		reg_win = BCW_READ32(regs, BCW_REG0_WIN);
		/* set register window to Sonics registers */
		BCW_WRITE32(regs, BCW_REG0_WIN, BCW_SONICS_WIN);

		/* enable SB to PCI interrupt */
		BCW_WRITE32(regs, BCW_SBINTVEC,
		    BCW_READ32(regs,
		        BCW_SBINTVEC) |
		    SBIV_ENET0);

		/* enable prefetch and bursts for sonics-to-pci translation 2 */
		BCW_WRITE32(regs, BCW_SPCI_TR2,
		    BCW_READ32(regs,
			BCW_SPCI_TR2) |
		    SBTOPCI_PREF | SBTOPCI_BURST);

		/* restore to ethernet register space */
		BCW_WRITE32(regs, BCW_REG0_WIN, reg_win);
	}

	/* disable SB core if not in reset */
	if (!(sbval & SBTML_RESET)) {

		/* set the reject bit */
		BCW_WRITE32(regs, BCW_SBTMSTATELOW, SBTML_REJ | SBTML_CLK);
		for (i = 0; i < 200; i++) {
			val = BCW_READ32(regs, BCW_SBTMSTATELOW);
			if (val & SBTML_REJ)
				break;
			delay(1);
		}
		if (i == 200)
			printf("%s: while resetting core, reject did not set\n",
			    sc->bcw_dev.dv_xname);
		/* wait until busy is clear */
		for (i = 0; i < 200; i++) {
			val = BCW_READ32(regs, BCW_SBTMSTATEHI);
			if (!(val & 0x4))
				break;
			delay(1);
		}
		if (i == 200)
			printf("%s: while resetting core, busy did not clear\n",
			    sc->bcw_dev.dv_xname);
		/* set reset and reject while enabling the clocks */
		BCW_WRITE32(regs, BCW_SBTMSTATELOW,
		    SBTML_FGC | SBTML_CLK | SBTML_REJ | SBTML_RESET |
		    SBTML_80211FLAG);
		val = BCW_READ32(regs, BCW_SBTMSTATELOW);
		delay(10);
		BCW_WRITE32(regs, BCW_SBTMSTATELOW, SBTML_REJ | SBTML_RESET);
		delay(1);
	}
	/* This is enabling/resetting the core */
	/* enable clock */
	BCW_WRITE32(regs, BCW_SBTMSTATELOW,
	    SBTML_FGC | SBTML_CLK | SBTML_RESET | 
	    SBTML_80211FLAG | SBTML_80211PHY );
	val = BCW_READ32(regs, BCW_SBTMSTATELOW);
	delay(1);

	/* clear any error bits that may be on */
	val = BCW_READ32(regs, BCW_SBTMSTATEHI);
	if (val & 1)
		BCW_WRITE32(regs, BCW_SBTMSTATEHI, 0);
	val = BCW_READ32(regs, BCW_SBIMSTATE);
	if (val & SBIM_MAGIC_ERRORBITS)
		BCW_WRITE32(regs, BCW_SBIMSTATE,
		    val & ~SBIM_MAGIC_ERRORBITS);

	/* clear reset and allow it to propagate throughout the core */
	BCW_WRITE32(regs, BCW_SBTMSTATELOW,
	    SBTML_FGC | SBTML_CLK | SBTML_80211FLAG | SBTML_80211PHY );
	val = BCW_READ32(regs, BCW_SBTMSTATELOW);
	delay(1);

	/* leave clock enabled */
	BCW_WRITE32(regs, BCW_SBTMSTATELOW,
	    SBTML_CLK | SBTML_80211FLAG | SBTML_80211PHY);
	val = BCW_READ32(regs, BCW_SBTMSTATELOW);
	delay(1);

	/* Write a 0 to MMIO reg 0x3e6, Baseband attenuation */
	BCW_WRITE16(regs, 0x3e6,0);
	
	/* Set 0x400 in the MMIO StatusBitField reg */
	sbval=BCW_READ32(regs, 0x120);
	sbval |= 0x400; 
	BCW_WRITE32(regs, 0x120, sbval);
#if 0
	/* initialize MDC preamble, frequency */
	/* MAGIC */
	bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle, BCW_MI_CTL, 0x8d);

	/* enable phy, differs for internal, and external */
	val = bus_space_read_4(sc->bcw_btag, sc->bcw_bhandle, BCW_DEVCTL);
	if (!(val & BCW_DC_IP)) {
		/* select external phy */
		bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle, BCW_ENET_CTL,
		    EC_EP);
	} else if (val & BCW_DC_ER) {	/* internal, clear reset bit if on */
		bus_space_write_4(sc->bcw_btag, sc->bcw_bhandle, BCW_DEVCTL,
		    val & ~BCW_DC_ER);
		delay(100);
	}
#endif
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
		bcw_add_mac(sc, sc->bcw_ac.ac_enaddr, 0);

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
	struct ieee80211com *ic = &sc->bcw_ic;
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
	struct bcw_regs *regs = &sc->bcw_regs;
	u_int32_t save,val;

	/*
	 * We use the offset of zero a lot here to reset the SHM pointer to the
	 * beginning of it's memory area, as it automatically moves on every
	 * access to the SHM DATA registers
	 */
	 
	/* Backup SHM uCode Revision before we clobber it */
	BCW_WRITE32(regs, BCW_SHM_CONTROL,
	    (BCW_SHM_CONTROL_SHARED << 16) + 0);
	save = BCW_READ32(regs, BCW_SHM_DATA);
	
	/* write test value */
	BCW_WRITE32(regs, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	BCW_WRITE32(regs, BCW_SHM_DATA, 0xaa5555aa);
	DPRINTF(("Write test 1, "));	
	/* Read it back */
	BCW_WRITE32(regs, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	val = BCW_READ32(regs, BCW_SHM_DATA);
	DPRINTF(("Read test 1, "));
	if (val != 0xaa5555aa) {
		DPRINTF(("Failed test 1\n"));
		return (1);
	} else
		DPRINTF(("Passed test 1\n"));
	
	/* write 2nd test value */
	BCW_WRITE32(regs, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	BCW_WRITE32(regs, BCW_SHM_DATA, 0x55aaaa55);
	DPRINTF(("Write test 2, "));
	/* Read it back */
	BCW_WRITE32(regs, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	val = BCW_READ32(regs, BCW_SHM_DATA);
	DPRINTF(("Read test 2, "));
	if (val != 0x55aaaa55) {
		DPRINTF(("Failed test 2\n"));
		return 2;
	} else
		DPRINTF(("Passed test 2\n"));

	/* Restore the saved value now that we're done */
	BCW_WRITE32(regs, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	BCW_WRITE32(regs, BCW_SHM_DATA, save);
	
	if (sc->bcw_corerev >= 3) {
		DPRINTF(("Doing corerev >= 3 tests\n"));
		/* do some test writes and reads against the TSF */
		/* 
		 * This works during the attach, but the spec at
		 * http://bcm-specs.sipsolutions.net/Timing
		 * say that we're reading/writing silly places, so these regs
		 * are not quite documented yet 
		 */
		BCW_WRITE16(regs, 0x18c, 0xaaaa);
		BCW_WRITE32(regs, 0x18c, 0xccccbbbb);
		val = BCW_READ16(regs, 0x604);
		if (val != 0xbbbb) return 3;
		val = BCW_READ16(regs, 0x606);
		if (val != 0xcccc) return 4;
		/* re-clear the TSF since we just filled it with garbage */
		BCW_WRITE32(regs, 0x18c, 0x0);
	}	
	
	/* Check the Status Bit Field for some unknown bits */
	val = BCW_READ32(regs, BCW_SBF);
	if ((val | 0x80000000) != 0x80000400 ) {
		printf("%s: Warning, SBF is 0x%x, expected 0x80000400\n",
		    sc->bcw_dev.dv_xname,val);
		/* May not be a critical failure, just warn for now */
		//return (5);
	}
	/* Verify there are no interrupts active on the core */
	val = BCW_READ32(regs, BCW_GIR);
	if (val!=0) {
		DPRINTF(("Failed Pending Interrupt test with val=0x%x\n",val));
		return (6);
	}

	/* Above G means it's unsupported currently, like N */
	if (sc->bcw_phy_type > BCW_PHY_TYPEG) {
		DPRINTF(("PHY type %d greater than supported type %d\n",
		    sc->bcw_phy_type, BCW_PHY_TYPEG));
		return (7);
	}
	
	return (0);
}


/*
 * Abstracted reads and writes - from rtw
 */
u_int8_t
bcw_read8(void *arg, u_int32_t off)
{
	struct bcw_regs *regs = (struct bcw_regs *)arg;
	return (bus_space_read_1(regs->r_bt, regs->r_bh, off));
}

u_int16_t
bcw_read16(void *arg, u_int32_t off)
{
	struct bcw_regs *regs = (struct bcw_regs *)arg;
	return (bus_space_read_2(regs->r_bt, regs->r_bh, off));
}

u_int32_t
bcw_read32(void *arg, u_int32_t off)
{
	struct bcw_regs *regs = (struct bcw_regs *)arg;
	return (bus_space_read_4(regs->r_bt, regs->r_bh, off));
}

void
bcw_write8(void *arg, u_int32_t off, u_int8_t val)
{
	struct bcw_regs *regs = (struct bcw_regs *)arg;
	bus_space_write_1(regs->r_bt, regs->r_bh, off, val);
}

void
bcw_write16(void *arg, u_int32_t off, u_int16_t val)
{
	struct bcw_regs *regs = (struct bcw_regs *)arg;
	bus_space_write_2(regs->r_bt, regs->r_bh, off, val);
}

void
bcw_write32(void *arg, u_int32_t off, u_int32_t val)
{
	struct bcw_regs *regs = (struct bcw_regs *)arg;
	bus_space_write_4(regs->r_bt, regs->r_bh, off, val);
}

void
bcw_barrier(void *arg, u_int32_t reg0, u_int32_t reg1, int flags)
{
	struct bcw_regs *regs = (struct bcw_regs *)arg;
	bus_space_barrier(regs->r_bh, regs->r_bt, MIN(reg0, reg1),
	    MAX(reg0, reg1) - MIN(reg0, reg1) + 4, flags);
}

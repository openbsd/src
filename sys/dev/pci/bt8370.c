/*	$OpenBSD: bt8370.c,v 1.7 2006/01/31 16:51:13 claudio Exp $ */

/*
 * Copyright (c) 2004,2005  Internet Business Solutions AG, Zurich, Switzerland
 * Written by: Andre Oppermann <oppermann@accoom.net>
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
#include <sys/param.h>
#include <sys/types.h>

#include <sys/device.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_sppp.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include "musyccreg.h"
#include "musyccvar.h"
#include "if_art.h"
#include "bt8370reg.h"

#define	FRAMER_LIU_E1_120	1
#define	FRAMER_LIU_T1_133	2

void	bt8370_set_sbi_clock_mode(struct art_softc *, enum art_sbi_type,
	    u_int, int);
void	bt8370_set_bus_mode(struct art_softc *, enum art_sbi_mode, int);
void	bt8370_set_line_buildout(struct art_softc *, int);
void	bt8370_set_loopback_mode(struct art_softc *, enum art_loopback);
void	bt8370_set_bop_mode(struct art_softc *ac, int);
void	bt8370_set_dl_1_mode(struct art_softc *, int);
void	bt8370_set_dl_2_mode(struct art_softc *, int);
void	bt8370_intr_enable(struct art_softc *ac, int);

#ifndef ACCOOM_DEBUG
#define bt8370_print_status(x)
#define bt8370_print_counters(x)
#define bt8370_dump_registers(x)
#else
void	bt8370_print_status(struct art_softc *);
void	bt8370_print_counters(struct art_softc *);
void	bt8370_dump_registers(struct art_softc *);
#endif

int
bt8370_reset(struct art_softc *ac)
{
	u_int8_t cr0;

	ebus_write(&ac->art_ebus, Bt8370_CR0, 0x00);
	DELAY(10);	/* 10 microseconds */
	ebus_write(&ac->art_ebus, Bt8370_CR0, CR0_RESET);
	DELAY(20);	/* 20 microseconds */
	ebus_write(&ac->art_ebus, Bt8370_CR0, 0x00);
	cr0 = ebus_read(&ac->art_ebus, Bt8370_CR0);
	if (cr0 != 0x0) {
		log(LOG_ERR, "%s: reset not successful\n",
		    ac->art_dev.dv_xname);
		return (-1);
	}
	return (0);
}

int
bt8370_set_frame_mode(struct art_softc *ac, enum art_sbi_type type, u_int mode,
    u_int clockmode)
{
	int channels;

	/* Get into a clean state */
	bt8370_reset(ac);

	/* Disable all interrupts to be sure */
	bt8370_intr_enable(ac, 0);

	switch (mode) {
	case IFM_TDM_E1:	/* 32 payload channels, bit transparent */
		channels = 32;

		/* Global Config */
		ebus_write(&ac->art_ebus, Bt8370_CR0, CR0_E1_FAS);

		/* Primary Config */
		bt8370_set_loopback_mode(ac, ART_NOLOOP);
		ebus_write(&ac->art_ebus, Bt8370_DL3_TS, 0x00);

		/* Timing and Clock Config */
		bt8370_set_sbi_clock_mode(ac, type, clockmode, channels);

		/* Receiver RLIU, RCVR */
		bt8370_set_line_buildout(ac, FRAMER_LIU_E1_120);
		/* This one is critical */
		ebus_write(&ac->art_ebus, Bt8370_RCR0, RCR0_HDB3 |
		    RCR0_RABORT | RCR0_LFA_FAS | RCR0_RZCS_NBPV);
		ebus_write(&ac->art_ebus, Bt8370_RALM, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_LATCH, LATCH_STOPCNT);

		/* Transmitter TLIU, XMTR */
		ebus_write(&ac->art_ebus, Bt8370_TCR0, TCR0_FAS);
		ebus_write(&ac->art_ebus, Bt8370_TCR1, TCR1_TABORT |
		    TCR1_HDB3);
		ebus_write(&ac->art_ebus, Bt8370_TFRM, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TMAN, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TALM, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TPATT, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TLB, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TSA4, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA5, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA6, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA7, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA8, 0xFF);

		/* Bit Oriented Protocol Transceiver BOP disabled */
		bt8370_set_bop_mode(ac, 0);

		/* Data Link #1 disabled */
		bt8370_set_dl_1_mode(ac, 0);

		/* Data Link #2 disabled */
		bt8370_set_dl_2_mode(ac, 0);

		ACCOOM_PRINTF(1, ("%s: set to E1 G.703 unframed, HDB3\n",
		    ac->art_dev.dv_xname));
		break;
	case IFM_TDM_E1_G704:	/* 31 payload channels, byte aligned */
		channels = 32;

		/* Global Config */
		ebus_write(&ac->art_ebus, Bt8370_CR0, CR0_E1_FAS);

		/* Primary Config */
		bt8370_set_loopback_mode(ac, ART_NOLOOP);
		ebus_write(&ac->art_ebus, Bt8370_DL3_TS, 0x00);

		/* Timing and Clock Config */
		bt8370_set_sbi_clock_mode(ac, type, clockmode, channels);

		/* Receiver RLIU, RCVR */
		bt8370_set_line_buildout(ac, FRAMER_LIU_E1_120);
		/* This one is critical */
		ebus_write(&ac->art_ebus, Bt8370_RCR0, RCR0_RFORCE |
		    RCR0_HDB3 | RCR0_LFA_FAS | RCR0_RZCS_NBPV);
		ebus_write(&ac->art_ebus, Bt8370_RALM, RALM_FSNFAS);
		ebus_write(&ac->art_ebus, Bt8370_LATCH, LATCH_STOPCNT);

		/* Transmitter TLIU, XMTR */
		ebus_write(&ac->art_ebus, Bt8370_TCR0, TCR0_FAS);
		/* This one is critical */
		ebus_write(&ac->art_ebus, Bt8370_TCR1, TCR1_TABORT |
		    TCR1_3FAS | TCR1_HDB3);
		ebus_write(&ac->art_ebus, Bt8370_TFRM, TFRM_YEL |
		    TFRM_FBIT);
		ebus_write(&ac->art_ebus, Bt8370_TMAN, TMAN_MALL);
		ebus_write(&ac->art_ebus, Bt8370_TALM, TALM_AYEL);
		ebus_write(&ac->art_ebus, Bt8370_TPATT, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TLB, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TSA4, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA5, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA6, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA7, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA8, 0xFF);

		/* Bit Oriented Protocol Transceiver BOP disabled */
		bt8370_set_bop_mode(ac, 0);

		/* Data Link #1 disabled */
		bt8370_set_dl_1_mode(ac, 0);

		/* Data Link #2 disabled */
		bt8370_set_dl_2_mode(ac, 0);

		ACCOOM_PRINTF(1, ("%s: set to E1 G.704, HDB3\n",
		    ac->art_dev.dv_xname));
		break;
	case IFM_TDM_E1_G704_CRC4:  /* 31 payload channels, byte aligned */
		channels = 32;

		/*
		 * Over normal G.704 the following registers need changes:
		 * CR0 = +CRC
		 * TFRM = +INS_CRC
		 */

		/* Global Config */
		ebus_write(&ac->art_ebus, Bt8370_CR0, CR0_E1_FAS_CRC);

		/* Primary Config */
		bt8370_set_loopback_mode(ac, ART_NOLOOP);
		ebus_write(&ac->art_ebus, Bt8370_DL3_TS, 0x00);

		/* Timing and Clock Config */
		bt8370_set_sbi_clock_mode(ac, type, clockmode, channels);

		/* Receiver RLIU, RCVR */
		bt8370_set_line_buildout(ac, FRAMER_LIU_E1_120);
		/* This one is critical */
		ebus_write(&ac->art_ebus, Bt8370_RCR0, RCR0_RFORCE |
		    RCR0_HDB3 | RCR0_LFA_FASCRC | RCR0_RZCS_NBPV);
		ebus_write(&ac->art_ebus, Bt8370_RALM, RALM_FSNFAS);
		ebus_write(&ac->art_ebus, Bt8370_LATCH, LATCH_STOPCNT);

		/* Transmitter TLIU, XMTR */
		ebus_write(&ac->art_ebus, Bt8370_TCR0, TCR0_MFAS);
		/* This one is critical */
		ebus_write(&ac->art_ebus, Bt8370_TCR1, TCR1_TABORT |
		    TCR1_3FAS | TCR1_HDB3);
		ebus_write(&ac->art_ebus, Bt8370_TFRM, TFRM_YEL |
		    TFRM_MF | TFRM_FE | TFRM_CRC | TFRM_FBIT);
		ebus_write(&ac->art_ebus, Bt8370_TMAN, TMAN_MALL);
		ebus_write(&ac->art_ebus, Bt8370_TALM, TALM_AYEL | TALM_AAIS);
		ebus_write(&ac->art_ebus, Bt8370_TPATT, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TLB, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TSA4, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA5, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA6, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA7, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA8, 0xFF);

		/* Bit Oriented Protocol Transceiver BOP disabled */
		bt8370_set_bop_mode(ac, 0);

		/* Data Link #1 disabled */
		bt8370_set_dl_1_mode(ac, 0);

		/* Data Link #2 disabled */
		bt8370_set_dl_2_mode(ac, 0);

		ACCOOM_PRINTF(1, ("%s: set to E1 G.704 CRC4, HDB3\n",
		    ac->art_dev.dv_xname));
		break;
	case IFM_TDM_T1_AMI:	/* 24 payload channels, byte aligned */
		channels = 25;	/* zero is ignored for T1 */

		/* Global Config */
		ebus_write(&ac->art_ebus, Bt8370_CR0, CR0_T1_SF);

		/* Primary Config */
		bt8370_set_loopback_mode(ac, ART_NOLOOP);
		ebus_write(&ac->art_ebus, Bt8370_DL3_TS, 0x00);

		/* Timing and Clock Config */
		bt8370_set_sbi_clock_mode(ac, type, clockmode, channels);

		/* Receiver RLIU, RCVR */
		bt8370_set_line_buildout(ac, FRAMER_LIU_T1_133);
		/* This one is critical */
		ebus_write(&ac->art_ebus, Bt8370_RCR0, RCR0_RFORCE |
		    RCR0_AMI | RCR0_LFA_26F | RCR0_RZCS_NBPV);
		ebus_write(&ac->art_ebus, Bt8370_RALM, RALM_FSNFAS);
		ebus_write(&ac->art_ebus, Bt8370_LATCH, LATCH_STOPCNT);

		/* Transmitter TLIU, XMTR */
		ebus_write(&ac->art_ebus, Bt8370_TCR0, TCR0_SF);
		/* This one is critical */
		ebus_write(&ac->art_ebus, Bt8370_TCR1, TCR1_TABORT |
		    TCR1_26F | TCR1_AMI);
		ebus_write(&ac->art_ebus, Bt8370_TFRM, TFRM_YEL |
		    TFRM_MF | TFRM_FBIT);
		ebus_write(&ac->art_ebus, Bt8370_TMAN, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TALM, TALM_AYEL);
		ebus_write(&ac->art_ebus, Bt8370_TPATT, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TLB, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TSA4, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA5, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA6, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA7, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA8, 0xFF);

		/* Bit Oriented Protocol Transceiver BOP disabled */
		bt8370_set_bop_mode(ac, 0);

		/* Data Link #1 disabled */
		bt8370_set_dl_1_mode(ac, 0);

		/* Data Link #2 disabled */
		bt8370_set_dl_2_mode(ac, 0);

		ACCOOM_PRINTF(1, ("%s: set to T1 SF, AMI\n",
		    ac->art_dev.dv_xname));
		break;
	case IFM_TDM_T1:	/* 24 payload channels, byte aligned */
		channels = 25;	/* zero is ignored for T1 */

		/* Global Config */
		ebus_write(&ac->art_ebus, Bt8370_CR0, CR0_T1_ESF);

		/* Primary Config */
		bt8370_set_loopback_mode(ac, ART_NOLOOP);
		ebus_write(&ac->art_ebus, Bt8370_DL3_TS, 0x00);

		/* Timing and Clock Config */
		bt8370_set_sbi_clock_mode(ac, type, clockmode, channels);

		/* Receiver RLIU, RCVR */
		bt8370_set_line_buildout(ac, FRAMER_LIU_T1_133);
		/* This one is critical */
		ebus_write(&ac->art_ebus, Bt8370_RCR0, RCR0_RFORCE |
		    RCR0_B8ZS | RCR0_LFA_26F | RCR0_RZCS_NBPV);
		ebus_write(&ac->art_ebus, Bt8370_RLB, 0x09);
		ebus_write(&ac->art_ebus, Bt8370_LBA, 0x08);
		ebus_write(&ac->art_ebus, Bt8370_LBD, 0x24);
		ebus_write(&ac->art_ebus, Bt8370_RALM, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_LATCH, LATCH_STOPCNT);

		/* Transmitter TLIU, XMTR */
		ebus_write(&ac->art_ebus, Bt8370_TCR0, TCR0_ESFCRC);
		/* This one is critical */
		ebus_write(&ac->art_ebus, Bt8370_TCR1, TCR1_TABORT |
		    TCR1_26F | TCR1_B8ZS);
		ebus_write(&ac->art_ebus, Bt8370_TFRM, TFRM_CRC |
		    TFRM_FBIT);
		ebus_write(&ac->art_ebus, Bt8370_TMAN, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TALM, TALM_AYEL);
		ebus_write(&ac->art_ebus, Bt8370_TPATT, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TLB, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TSA4, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA5, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA6, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA7, 0xFF);
		ebus_write(&ac->art_ebus, Bt8370_TSA8, 0xFF);

		/* Bit Oriented Protocol Transceiver BOP setup */
		bt8370_set_bop_mode(ac, ART_BOP_ESF);

		/* Data Link #1 set to BOP mode for FDL */
		bt8370_set_dl_1_mode(ac, ART_DL1_BOP);

		/* Data Link #2 disabled */
		bt8370_set_dl_2_mode(ac, 0);

		ACCOOM_PRINTF(1, ("%s: set to T1 ESF CRC6, B8ZS\n",
		    ac->art_dev.dv_xname));
		break;
	/*
	 * case FAS_BSLIP:
	 * case FAS_CRC_BSLIP:
	 * case FAS_CAS:
	 * case FAS_CAS_BSLIP:
	 * case FAS_CRC_CAS:
	 * case FAS_CRC_CAS_BSLIP:
	 * case FT:
	 * case ESF_NOCRC:
	 * case SF_JYEL:
	 * case SF_T1DM:
	 * case SLC_FSLOF:
	 * case SLC:
	 * case ESF_xx (MimicCRC, ForceCRC?)
	 *
	 * are not yet implemented.
	 * If you need one of them please contact us.
	 */
	default:
		return (-1);
	}
	return (0);
}

void
bt8370_set_sbi_clock_mode(struct art_softc *ac, enum art_sbi_type mode,
    u_int linemode, int channels)
{
	u_int8_t cmux, jatcr;

	/*
	 * mode is either master or slave.
	 * linemode is either T1 (1544) or E1 (2048) external or internal.
	 */
	switch (mode) {
	case ART_SBI_MASTER:
		ACCOOM_PRINTF(1, ("%s: set to MASTER\n",
		    ac->art_dev.dv_xname));
		/*
		 * ONESEC pulse output,
		 * RDL/TDL/INDY ignored,
		 * RFSYNC Receive Frame Sync output,
		 * RMSYNC Reveice MultiFrame Sync output,
		 * TFYNC Transmit Frame Sync output,
		 * TMSYNC Transmit MultiFrame Sync output.
		 */
		ebus_write(&ac->art_ebus, Bt8370_PIO, PIO_ONESEC_IO |
		    PIO_TDL_IO | PIO_RFSYNC_IO | PIO_RMSYNC_IO |
		    PIO_TFSYNC_IO | PIO_TMSYNC_IO);
		/*
		 * TDL/RDL/INDY/TCKO three-stated.
		 * CLADO enabled, drives SBI bus RCLK, TCLK and
		 *  is connected to own TSBCKI and TSBCKI on slave.
		 * RCKO enabled, is connected to TCKI on slave.
		 */
		ebus_write(&ac->art_ebus, Bt8370_POE, POE_TDL_OE |
		    POE_RDL_OE | POE_INDY_OE | POE_TCKO_OE);
		/*
		 * We are the SBI bus master and take clock from our own
		 * CLADO. The TCKI source depends on line or internal
		 * clocking.
		 */
		cmux = CMUX_RSBCKI_CLADO | CMUX_TSBCKI_CLADO |
			CMUX_CLADI_CLADI;
		break;
	case ART_SBI_SLAVE:
		ACCOOM_PRINTF(1, ("%s: set to SLAVE\n",
		    ac->art_dev.dv_xname));
		/*
		 * ONESEC pulse input,
		 * RDL/TDL/INDY ignored,
		 * RFSYNC Receive Frame Sync input,
		 * RMSYNC Reveice MultiFrame Sync input,
		 * TFYNC Transmit Frame Sync input,
		 * TMSYNC Transmit MultiFrame Sync input.
		 */
		ebus_write(&ac->art_ebus, Bt8370_PIO, PIO_TDL_IO);
		/*
		 * TDL/RDL/INDY/TCKO three-stated.
		 * CLADO enabled, is connected to own ACKI and
		 * RSBCKI, ACKI on master.
		 * RCKO enabled, is connected to TCKI on master.
		 */
		ebus_write(&ac->art_ebus, Bt8370_POE, POE_TDL_OE |
		    POE_RDL_OE | POE_INDY_OE | POE_TCKO_OE);
		/*
		 * We are the SBI bus slave and take clock from TSBCKI.
		 * The TCKI source depends on line or internal clocking.
		 */
		cmux = CMUX_RSBCKI_TSBCKI | CMUX_TSBCKI_TSBCKI |
			CMUX_CLADI_CLADI;
		break;
	case ART_SBI_SINGLE:
		ACCOOM_PRINTF(1, ("%s: set to SINGLE\n",
		    ac->art_dev.dv_xname));
		/*
		 * ONESEC pulse output,
		 * RDL/TDL/INDY ignored,
		 * RFSYNC Receive Frame Sync output,
		 * RMSYNC Reveice MultiFrame Sync output,
		 * TFSYNC Transmit Frame Sync output,
		 * TMSYNC Transmit MultiFrame Sync output.
		 */
		ebus_write(&ac->art_ebus, Bt8370_PIO, PIO_ONESEC_IO |
		    PIO_TDL_IO | PIO_RFSYNC_IO | PIO_RMSYNC_IO |
		    PIO_TFSYNC_IO | PIO_TMSYNC_IO);
		/*
		 * TDL/RDL/INDY/TCKO three-stated, CLADO/RCKO enabled.
		 */
		ebus_write(&ac->art_ebus, Bt8370_POE, POE_TDL_OE |
		    POE_RDL_OE | POE_INDY_OE | POE_RCKO_OE);
		/*
		 * We are the SBI bus master and take clock from our own
		 * CLADO. The TCKI source is always CLADO (jitter attenuated
		 * if receive clock).
		 */
		cmux = CMUX_RSBCKI_CLADO | CMUX_TSBCKI_CLADO |
			CMUX_CLADI_RCKO;
		break;
	}

	/* Transmit clock from where? */
	switch (linemode) {
	case IFM_TDM_MASTER:
		ACCOOM_PRINTF(1, ("%s: clock MASTER\n",
		    ac->art_dev.dv_xname));
		if (mode == ART_SBI_MASTER)
			cmux |= CMUX_TCKI_RSBCKI;
		else
			cmux |= CMUX_TCKI_CLADO;
		jatcr = JAT_CR_JFREE;
		break;
	/* case ART_CLOCK_EXTREF: */
	default:
		ACCOOM_PRINTF(1, ("%s: clock LINE\n",
		    ac->art_dev.dv_xname));
		cmux |= CMUX_TCKI_RCKO;
		jatcr = JAT_CR_JEN | JAT_CR_JDIR_RX | JAT_CR_JSIZE32;
		break;
	}

	ebus_write(&ac->art_ebus, Bt8370_CMUX, cmux);
	ebus_write(&ac->art_ebus, Bt8370_JAT_CR, jatcr);

	/* Set up the SBI (System Bus Interface) and clock source. */
	switch (mode) {
	case ART_SBI_MASTER:
		ebus_write(&ac->art_ebus, Bt8370_CSEL, CSEL_VSEL_4096 |
		    CSEL_OSEL_4096);
		bt8370_set_bus_mode(ac, SBI_MODE_4096_A, channels);
		/* no need to set musycc port mode */
		break;
	case ART_SBI_SLAVE:
		/*
		 * On the slave the CLADO depends on the line type
		 * of the master.
		 */
		bt8370_set_bus_mode(ac, SBI_MODE_4096_B, channels);
		/* no need to set musycc port mode */
		break;
	case ART_SBI_SINGLE:
		if (channels == 25) {
			ACCOOM_PRINTF(1, ("%s: SINGLE T1\n",
			    ac->art_dev.dv_xname));
			ebus_write(&ac->art_ebus, Bt8370_CSEL, CSEL_VSEL_1544 |
			    CSEL_OSEL_1544);
			bt8370_set_bus_mode(ac, SBI_MODE_1544, channels);
			musycc_set_port(ac->art_channel->cc_group,
			    MUSYCC_PORT_MODE_T1);
		} else {
			ACCOOM_PRINTF(1, ("%s: SINGLE E1\n",
			    ac->art_dev.dv_xname));
			ebus_write(&ac->art_ebus, Bt8370_CSEL, CSEL_VSEL_2048 |
			    CSEL_OSEL_2048);
			bt8370_set_bus_mode(ac, SBI_MODE_2048, channels);
			musycc_set_port(ac->art_channel->cc_group,
			    MUSYCC_PORT_MODE_E1);
		}
		break;
	}
	ebus_write(&ac->art_ebus, Bt8370_CLAD_CR, CLAD_CR_LFGAIN);
}

void
bt8370_set_bus_mode(struct art_softc *ac, enum art_sbi_mode mode, int nchannels)
{
	bus_size_t channel;

	/*
	 * Be aware that on the CN847x 'TSYNC_EDGE' has to be set to
	 * 'raising edge' in the port config for this to work correctly.
	 * All others (including RSYNC) are on 'falling edge'.
	 */
	ebus_write(&ac->art_ebus, Bt8370_RSB_CR, RSB_CR_BUS_RSB |
	    RSB_CR_SIG_OFF | RSB_CR_RPCM_NEG | RSB_CR_RSYN_NEG |
	    RSB_CR_RSB_CTR | TSB_CR_TSB_NORMAL);
	ebus_write(&ac->art_ebus, Bt8370_RSYNC_BIT, 0x00);
	ebus_write(&ac->art_ebus, Bt8370_RSYNC_TS, 0x00);
	ebus_write(&ac->art_ebus, Bt8370_TSB_CR, TSB_CR_BUS_TSB |
	    TSB_CR_TPCM_NEG | TSB_CR_TSYN_NEG | TSB_CR_TSB_CTR |
	    TSB_CR_TSB_NORMAL);
	ebus_write(&ac->art_ebus, Bt8370_TSYNC_BIT, 0x00);
	ebus_write(&ac->art_ebus, Bt8370_TSYNC_TS, 0x00);
	ebus_write(&ac->art_ebus, Bt8370_RSIG_CR, 0x00);
	ebus_write(&ac->art_ebus, Bt8370_RSYNC_FRM, 0x00);

	/* Mode dependent */
	switch (mode) {
	case SBI_MODE_1536:
		ebus_write(&ac->art_ebus, Bt8370_SBI_CR, SBI_CR_SBI_OE |
		    SBI_CR_1536);
		break;
	case SBI_MODE_1544:
		ebus_write(&ac->art_ebus, Bt8370_SBI_CR, SBI_CR_SBI_OE |
		    SBI_CR_1544);
		break;
	case SBI_MODE_2048:
		ebus_write(&ac->art_ebus, Bt8370_SBI_CR, SBI_CR_SBI_OE |
		    SBI_CR_2048);
		break;
	case SBI_MODE_4096_A:
		ebus_write(&ac->art_ebus, Bt8370_SBI_CR, SBI_CR_SBI_OE |
		    SBI_CR_4096_A);
		break;
	case SBI_MODE_4096_B:
		ebus_write(&ac->art_ebus, Bt8370_SBI_CR, SBI_CR_SBI_OE |
		    SBI_CR_4096_B);
		break;
	case SBI_MODE_8192_A:
		ebus_write(&ac->art_ebus, Bt8370_SBI_CR, SBI_CR_SBI_OE |
		    SBI_CR_8192_A);
		break;
	case SBI_MODE_8192_B:
		ebus_write(&ac->art_ebus, Bt8370_SBI_CR, SBI_CR_SBI_OE |
		    SBI_CR_8192_B);
		break;
	case SBI_MODE_8192_C:
		ebus_write(&ac->art_ebus, Bt8370_SBI_CR, SBI_CR_SBI_OE |
		    SBI_CR_8192_C);
		break;
	case SBI_MODE_8192_D:
		ebus_write(&ac->art_ebus, Bt8370_SBI_CR, SBI_CR_SBI_OE |
		    SBI_CR_8192_D);
		break;
	}

	/* Initialize and reset all channels */
	for (channel = 0; channel < 32; channel++) {
		ebus_write(&ac->art_ebus, Bt8370_SBCn + channel, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TPCn + channel, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TSIGn + channel, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_TSLIP_LOn + channel, 0x7e);
		ebus_write(&ac->art_ebus, Bt8370_RSLIP_LOn + channel, 0x7e);
		ebus_write(&ac->art_ebus, Bt8370_RPCn + channel, 0x00);
	}

	/* Configure used channels */
	for (channel = Bt8370_SBCn; channel < Bt8370_SBCn +
	    nchannels; channel++) {
		ebus_write(&ac->art_ebus, channel, SBCn_RINDO |
		    SBCn_TINDO | SBCn_ASSIGN);
		/* In T1 mode timeslot 0 must not be used. */
		if (nchannels == 25 && channel == Bt8370_SBCn)
			ebus_write(&ac->art_ebus, channel, 0x00);
	}
	for (channel = Bt8370_TPCn; channel < Bt8370_TPCn +
	    nchannels; channel++) {
		ebus_write(&ac->art_ebus, channel, TPCn_CLEAR);
	}
	for (channel = Bt8370_RPCn; channel < Bt8370_RPCn +
	    nchannels; channel++) {
		ebus_write(&ac->art_ebus, channel, RPCn_CLEAR);
	}
}

void
bt8370_set_line_buildout(struct art_softc *ac, int mode)
{
	/*
	 * LIU Stuff: Send and Reveive side
	 * T1: 0-133, 133-266, 266-399, 399-533, 533-655,
	 *     Long-Haul FCC Part 68.
	 * E1: ITU-T G.703 120 Ohm Twisted Pair.
	 */

	ebus_write(&ac->art_ebus, Bt8370_RLIU_CR, RLIU_CR_FRZ_SHORT |
	    RLIU_CR_AGC2048 | RLIU_CR_LONG_EYE);

	switch (mode) {
	case FRAMER_LIU_T1_133:
		/* Short haul */
		ebus_write(&ac->art_ebus, Bt8370_VGA_MAX, 0x1F);
		/* Force EQ off */
		ebus_write(&ac->art_ebus, Bt8370_PRE_EQ, 0xA6);

		ebus_write(&ac->art_ebus, Bt8370_TLIU_CR, TLIU_CR_100);
		break;
#if 0
	case FRAMER_LIU_T1_266:
	case FRAMER_LIU_T1_399:
	case FRAMER_LIU_T1_533:
	case FRAMER_LIU_T1_655:
	case FRAMER_LIU_T1_LH68:
#endif
	case FRAMER_LIU_E1_120:
		/* Short haul */
		ebus_write(&ac->art_ebus, Bt8370_VGA_MAX, 0x1F);
		/* Force EQ off */
		ebus_write(&ac->art_ebus, Bt8370_PRE_EQ, 0xA6);

		ebus_write(&ac->art_ebus, Bt8370_TLIU_CR, TLIU_CR_120);
		break;
	}

	/*
	 * Run this last.  The RLIU reset causes the values written above
	 * to be activated.
	 */
	ebus_write(&ac->art_ebus, Bt8370_LIU_CR, LIU_CR_MAGIC |
	    LIU_CR_SQUELCH | LIU_CR_RST_LIU);
}

void
bt8370_set_loopback_mode(struct art_softc *ac, enum art_loopback mode)
{
	switch (mode) {
	case ART_RLOOP_PAYLOAD:	/* Remote digital payload loopback */
		ebus_write(&ac->art_ebus, Bt8370_LOOP, LOOP_PLOOP);
		break;
	case ART_RLOOP_LINE:	/* Remote analog line signal loopback */
		ebus_write(&ac->art_ebus, Bt8370_LOOP, LOOP_LLOOP);
		break;
	case ART_LLOOP_PAYLOAD:	/* Local digital payload loopback */
		ebus_write(&ac->art_ebus, Bt8370_LOOP, LOOP_FLOOP);
		break;
	case ART_LLOOP_LINE:	/* Local analog line signal loopback */
		ebus_write(&ac->art_ebus, Bt8370_LOOP, LOOP_ALOOP);
		break;
	case ART_NOLOOP:	/* Disable all loopbacks */
		ebus_write(&ac->art_ebus, Bt8370_LOOP, 0x00);
		break;
	}
}

void
bt8370_set_bop_mode(struct art_softc *ac, int mode)
{
	/* disabled or ESF mode */
	switch (mode) {
	case ART_BOP_ESF:
		ebus_write(&ac->art_ebus, Bt8370_BOP, 0x9A);
		break;
	default:
		ebus_write(&ac->art_ebus, Bt8370_BOP, 0x00);
		break;
	}
}

void
bt8370_set_dl_1_mode(struct art_softc *ac, int mode)
{
	/*
	 * We don't support the builtin HDLC controllers,
	 * however some DL1 registers are used for the BOP
	 * in ESF mode.
	 */
	switch (mode) {
	case ART_DL1_BOP:
		ebus_write(&ac->art_ebus, Bt8370_DL1_TS, 0x40);
		ebus_write(&ac->art_ebus, Bt8370_DL1_CTL, 0x03);
		ebus_write(&ac->art_ebus, Bt8370_RDL1_FFC, 0x0A);
		ebus_write(&ac->art_ebus, Bt8370_PRM1, 0x80);
		ebus_write(&ac->art_ebus, Bt8370_TDL1_FEC, 0x0A);
		break;
	default:
		ebus_write(&ac->art_ebus, Bt8370_RDL1_FFC, 0x0A);
		ebus_write(&ac->art_ebus, Bt8370_TDL1, 0x00);
		break;
	}
}

void
bt8370_set_dl_2_mode(struct art_softc *ac, int mode)
{
	/* We don't support the builtin HDLC controllers. */
	ebus_write(&ac->art_ebus, Bt8370_RDL2_FFC, 0x0A);
	ebus_write(&ac->art_ebus, Bt8370_TDL2, 0x00);
}

void
bt8370_intr_enable(struct art_softc *ac, int intr)
{
	switch (intr) {
	default:
		/* Disable all interrupts */
		ebus_write(&ac->art_ebus, Bt8370_IER7, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_IER6, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_IER5, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_IER4, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_IER3, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_IER2, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_IER1, 0x00);
		ebus_write(&ac->art_ebus, Bt8370_IER0, 0x00);
		break;
	}
	return;
}

void
bt8370_intr(struct art_softc *ac)
{
	u_int8_t irr, alrm;

	/* IRR tells us which interrupt class fired. */
	irr = ebus_read(&ac->art_ebus, Bt8370_IRR);
	/* If it wasn't us don't waste time. */
	if (irr == 0x00)
		return;

	/* Reding the interrupt service registers clears them. */
	alrm = ebus_read(&ac->art_ebus, Bt8370_ISR7);
	alrm = ebus_read(&ac->art_ebus, Bt8370_ISR6);
	alrm = ebus_read(&ac->art_ebus, Bt8370_ISR5);
	alrm = ebus_read(&ac->art_ebus, Bt8370_ISR4);
	alrm = ebus_read(&ac->art_ebus, Bt8370_ISR3);
	alrm = ebus_read(&ac->art_ebus, Bt8370_ISR2);
	alrm = ebus_read(&ac->art_ebus, Bt8370_ISR1);
	alrm = ebus_read(&ac->art_ebus, Bt8370_ISR0);

	/* IRR should be zero now or something went wrong. */
	irr = ebus_read(&ac->art_ebus, Bt8370_IRR);
	if (irr != 0x00)
		ACCOOM_PRINTF(0, ("%s: Interrupts did not clear properly\n",
			ac->art_dev.dv_xname));
	return;
}

int
bt8370_link_status(struct art_softc *ac)
{
	u_int8_t rstat, alm1, alm2, alm3, alm1mask;
	int status = 1;

	/*
	 *  1 everything fine
	 *  0 framing problems but link detected
	 * -1 no link detected
	 */

	alm1mask = ALM1_RYEL | ALM1_RAIS | ALM1_RALOS | ALM1_RLOF;
	/*
	 * XXX don't check RYEL in T1 mode it toggles more or less
	 * regular.
	 */
	if (IFM_SUBTYPE(ac->art_media) == IFM_TDM_T1)
		alm1mask &= ~ALM1_RYEL;

	rstat = ebus_read(&ac->art_ebus, Bt8370_RSTAT);
	alm1 = ebus_read(&ac->art_ebus, Bt8370_ALM1);
	alm2 = ebus_read(&ac->art_ebus, Bt8370_ALM2);
	alm3 = ebus_read(&ac->art_ebus, Bt8370_ALM3);

	if ((rstat & (RSTAT_EXZ | RSTAT_BPV)) ||
	    (alm1 & alm1mask) || (alm3 & (ALM3_SEF)))
		status = 0;

	if ((alm1 & (ALM1_RLOS)) ||
	    (alm2 & (ALM2_TSHORT)))
		status = -1;

	return (status);
}

#ifdef ACCOOM_DEBUG
void
bt8370_print_status(struct art_softc *ac)
{
	u_int8_t	fstat, rstat, vga, alm1, alm2, alm3, sstat, loop;

	/* FSTAT Register. */
	fstat = ebus_read(&ac->art_ebus, Bt8370_FSTAT);
	printf("%s: Current FSTAT:\n", ac->art_dev.dv_xname);
	if (fstat & FSTAT_ACTIVE) {
		printf("\tOffline Framer active ");
		if (fstat & FSTAT_RXTXN)
			printf("in Receive direction\n");
		else
			printf("in Transmit direction\n");
		if (fstat & FSTAT_INVALID)
			printf("\tNo Candidate found\n");
		if (fstat & FSTAT_FOUND)
			printf("\tFrame Alignment found\n");
		if (fstat & FSTAT_TIMEOUT)
			printf("\tFramer Search timeout\n");
	} else
		printf("\tOffline inactive\n");

	/* RSTAT and VGA Register. */
	rstat = ebus_read(&ac->art_ebus, Bt8370_RSTAT);
	printf("%s: Current RSTAT:\n", ac->art_dev.dv_xname);
	if (rstat & RSTAT_CPDERR)
		printf("\tCLAD phase detector lost lock to CLADI reference\n");
	if (rstat & RSTAT_ZCSUB)
		printf("\tHDB3/B8ZS pattern detected\n");
	if (rstat & RSTAT_EXZ)
		printf("\tExcessive zeros detected\n");
	if (rstat & RSTAT_BPV)
		printf("\tBipolar violations\n");
	if (rstat & RSTAT_EYEOPEN)
		printf("\tReceived signal valid and RPLL locked\n");
	else
		printf("\tReceived signal invalid\n");
	if (rstat & RSTAT_PRE_EQ)
		printf("\tPre-Equalizer is ON\n");
	else
		printf("\tPre-Equalizer is OFF\n");
	/* Need to write something to cause internal update. */
	ebus_write(&ac->art_ebus, Bt8370_VGA, 0x00);
	vga = ebus_read(&ac->art_ebus, Bt8370_VGA);
	printf("\t%i dB Gain\n", vga);

	/* Alarm 1 Status. */
	alm1 = ebus_read(&ac->art_ebus, Bt8370_ALM1);
	printf("%s: Current ALM1:\n", ac->art_dev.dv_xname);
	if (alm1 & ALM1_RMYEL)
		printf("\tMultiframe Yellow Alarm [MYEL]\n");
	if (alm1 & ALM1_RYEL)
		printf("\tYellow Alarm [YEL]\n");
	if (alm1 & ALM1_RAIS)
		printf("\tRemote Alarm Indication [RAIS]\n");
	if (alm1 & ALM1_RALOS)
		printf("\tAnalog Loss of Signal or RCKI Loss of Clock [RALOS]\n");
	if (alm1 & ALM1_RLOS)
		printf("\tLoss of Signal [RLOS]\n");
	if (alm1 & ALM1_RLOF)
		printf("\tLoss of Frame Alignment [RLOF]\n");
	if (alm1 & ALM1_SIGFRZ)
		printf("\tSignalling Freeze\n");

	/* Alarm 2 Status. */
	alm2 = ebus_read(&ac->art_ebus, Bt8370_ALM2);
	printf("%s: Current ALM2:\n", ac->art_dev.dv_xname);
	if (alm2 & ALM2_LOOPDN)
		printf("\tLOOPDN code detected\n");
	if (alm2 & ALM2_LOOPUP)
		printf("\tLOOPUP code detected\n");
	if (alm2 & ALM2_TSHORT)
		printf("\tTransmitter short circuit\n");
	if (alm2 & ALM2_TLOC)
		printf("\tTransmit loss of clock (relative to ACKI)\n");
	if (alm2 & ALM2_TLOF)
		printf("\tTransmit loss of frame alignment (ignored)\n");

	/* Alarm 3 Status. */
	alm3 = ebus_read(&ac->art_ebus, Bt8370_ALM3);
	printf("%s: Current ALM3:\n", ac->art_dev.dv_xname);
	if (alm3 & ALM3_RMAIS)
		printf("\tRMAIS TS16 Alarm Indication Signal\n");
	if (alm3 & ALM3_SEF)
		printf("\tSeverely Errored Frame encountered\n");
	if (alm3 & ALM3_SRED)
		printf("\tLoss of CAS Alignment\n");
	if (alm3 & ALM3_MRED)
		printf("\tLoss of MFAS Alignment\n");
	if (alm3 & ALM3_FRED)
		printf("\tLoss of T1/FAS Alignment\n");
	/* LOF omitted */

	/* Slip Buffer Status. */
	sstat = ebus_read(&ac->art_ebus, Bt8370_SSTAT);
	printf("%s: Current SSTAT:\n", ac->art_dev.dv_xname);
	if (sstat & SSTAT_TFSLIP) {
		if (sstat & SSTAT_TUSLIP)
			printf("\tControlled Transmit Slip, ");
		else
			printf("\tUncontrolled Transmit Slip, ");
		if (sstat & SSTAT_TSDIR)
			printf("repeated one frame\n");
		else
			printf("deleted one frame\n");
	} else if (sstat & SSTAT_RFSLIP) {
		if (sstat & SSTAT_RUSLIP)
			printf("\tControlled Receive Slip, ");
		else
			printf("\tUncontrolled Receive Slip, ");
		if (sstat & SSTAT_RSDIR)
			printf("repeated one frame\n");
		else
			printf("deleted one frame\n");
	}

	/* Loopback Status. */
	loop = ebus_read(&ac->art_ebus, Bt8370_LOOP);
	printf("%s: Current LOOP:\n", ac->art_dev.dv_xname);
	if (loop & LOOP_PLOOP)
		printf("\tRemote Payload Loopback\n");
	if (loop & LOOP_LLOOP)
		printf("\tRemote Line Loopback\n");
	if (loop & LOOP_FLOOP)
		printf("\tLocal Payload Loopback\n");
	if (loop & LOOP_ALOOP)
		printf("\tLocal Line Loopback\n");
	if (loop & 0x00)
		printf("\tNo active Loopbacks\n");
}

void
bt8370_print_counters(struct art_softc *ac)
{
	u_int16_t	counters[5];
	u_int16_t	hi, lo;
	int		i;

	for (i = 0; i < 5; i++) {
		lo = ebus_read(&ac->art_ebus, Bt8370_FERR_LSB + i);
		hi = ebus_read(&ac->art_ebus, Bt8370_FERR_LSB + i + 1);

		counters[i] = lo | (hi << 8);
	}

	printf("%s: %hu framing bit errors, %hu CRC errors, ",
	    ac->art_dev.dv_xname, counters[0], counters[1]);
	printf("%hu line code violations\n", counters[2]);
	printf("%s: %hu Far End Errors %hu PRBS bit errors\n",
	    ac->art_dev.dv_xname, counters[3], counters[4]);
}
void
bt8370_dump_registers(struct art_softc *ac)
{
	int	i;

	printf("%s: dummping registers", ac->art_dev.dv_xname);
	for (i = 0; i < 0x200; i++) {
		if (i % 16 == 0)
			printf("\n%03x:", i);
		printf("%s%02x%s", i % 2 ? "" : " ",
		    ebus_read(&ac->art_ebus, i),
		    i % 8 == 7 ? " " : "");
	}
	printf("\n");
}

#endif

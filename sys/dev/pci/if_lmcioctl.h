/*	$NetBSD: if_lmcioctl.h,v 1.2 1999/03/25 04:09:34 explorer Exp $	*/

/*-
 * Copyright (c) 1997-1999 LAN Media Corporation (LMC)
 * All rights reserved.  www.lanmedia.com
 *
 * This code is written by Michael Graff <graff@vix.com> for LMC.
 * The code is derived from permitted modifications to software created
 * by Matt Thomas (matt@3am-software.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All marketing or advertising materials mentioning features or
 *    use of this software must display the following acknowledgement:
 *      This product includes software developed by LAN Media Corporation
 *      and its contributors.
 * 4. Neither the name of LAN Media Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LAN MEDIA CORPORATION AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(linux)
/*
 * IOCTLs that we use for linux.  The structures passed in really should
 * go into an OS inspecific file, since BSD will use these as well.
 *
 * Under linux, the 16 reserved-for-device IOCTLs are numbered 0x89f0
 * through 0x89ff.
 */
#define LMCIOCGINFO	0x89f0	/* get current state */
#define LMCIOCSINFO	0x89f1	/* set state to user provided values */

#else
/*
 * IOCTLs for the sane world.
 */
#define LMCIOCGINFO	_IOW('i', 240, struct ifreq)
#define LMCIOCSINFO	_IOWR('i', 241, struct ifreq)

#endif

typedef struct {
	u_int32_t	n;
	u_int32_t	m;
	u_int32_t	v;
	u_int32_t	x;
	u_int32_t	r;
	u_int32_t	f;
	u_int32_t	exact;
} lmc_av9110_t;

/*
 * Common structure passed to the ioctl code.
 */
struct lmc___ctl {
	u_int32_t	cardtype;
	u_int32_t	clock_source;		/* HSSI, T1 */
	u_int32_t	clock_rate;		/* T1 */
	u_int32_t	crc_length;
	u_int32_t	cable_length;		/* DS3 */
	u_int32_t	scrambler_onoff;	/* DS3 */
	u_int32_t	cable_type;		/* T1 */
	u_int32_t	keepalive_onoff;	/* protocol */
	u_int32_t	ticks;			/* ticks/sec */
	union {
		lmc_av9110_t	t1;
	} cardspec;
};

#define LMC_CTL_CARDTYPE_LMC5200	0	/* HSSI */
#define LMC_CTL_CARDTYPE_LMC5245	1	/* DS3 */
#define LMC_CTL_CARDTYPE_LMC1000	2	/* T1, E1, etc */

#define LMC_CTL_OFF			0	/* generic OFF value */
#define LMC_CTL_ON			1	/* generic ON value */

#define LMC_CTL_CLOCK_SOURCE_EXT	0	/* clock off line */
#define LMC_CTL_CLOCK_SOURCE_INT	1	/* internal clock */

#define LMC_CTL_CRC_LENGTH_16		16
#define LMC_CTL_CRC_LENGTH_32		32

#define LMC_CTL_CABLE_LENGTH_LT_100FT	0	/* DS3 cable < 100 feet */
#define LMC_CTL_CABLE_LENGTH_GT_100FT	1	/* DS3 cable >= 100 feet */

/*
 * These are not in the least IOCTL related, but I want them common.
 */
/*
 * assignments for the GPIO register on the DEC chip (common)
 */
#define LMC_GEP_INIT		0x01 /* 0: */
#define LMC_GEP_RESET		0x02 /* 1: */
#define LMC_GEP_LOAD		0x10 /* 4: */
#define LMC_GEP_DP		0x20 /* 5: */
#define LMC_GEP_SERIAL		0x40 /* 6: serial out */
#define LMC_GEP_SERIALCLK	0x80 /* 7: serial clock */

/*
 * HSSI GPIO assignments
 */
#define LMC_GEP_HSSI_ST		0x04 /* 2: receive timing sense (deprecated) */
#define LMC_GEP_HSSI_CLOCK	0x08 /* 3: clock source */

/*
 * T1 GPIO assignments
 */
#define LMC_GEP_T1_GENERATOR	0x04 /* 2: enable prog freq gen serial i/f */
#define LMC_GEP_T1_TXCLOCK	0x08 /* 3: provide clock on TXCLOCK output */

/*
 * Common MII16 bits
 */
#define LMC_MII16_LED0         0x0080
#define LMC_MII16_LED1         0x0100
#define LMC_MII16_LED2         0x0200
#define LMC_MII16_LED3         0x0400  /* Error, and the red one */
#define LMC_MII16_LED_ALL      0x0780  /* LED bit mask */
#define LMC_MII16_FIFO_RESET   0x0800

/*
 * definitions for HSSI
 */
#define LMC_MII16_HSSI_TA      0x0001
#define LMC_MII16_HSSI_CA      0x0002
#define LMC_MII16_HSSI_LA      0x0004
#define LMC_MII16_HSSI_LB      0x0008
#define LMC_MII16_HSSI_LC      0x0010
#define LMC_MII16_HSSI_TM      0x0020
#define LMC_MII16_HSSI_CRC     0x0040

/*
 * assignments for the MII register 16 (DS3)
 */
#define LMC_MII16_DS3_ZERO	0x0001
#define LMC_MII16_DS3_TRLBK	0x0002
#define LMC_MII16_DS3_LNLBK	0x0004
#define LMC_MII16_DS3_RAIS	0x0008
#define LMC_MII16_DS3_TAIS	0x0010
#define LMC_MII16_DS3_BIST	0x0020
#define LMC_MII16_DS3_DLOS	0x0040
#define LMC_MII16_DS3_CRC	0x1000
#define LMC_MII16_DS3_SCRAM	0x2000

/*
 * And T1
 */
#define LMC_MII16_T1_DTR	0x0001	/* DTR output RW */
#define LMC_MII16_T1_DSR	0x0002	/* DSR input RO */
#define LMC_MII16_T1_RTS	0x0004	/* RTS output RW */
#define LMC_MII16_T1_CTS	0x0008	/* CTS input RO */
#define LMC_MII16_T1_DCD	0x0010	/* DCD input RO */
#define LMC_MII16_T1_RI		0x0020	/* RI input RO */
#define LMC_MII16_T1_CRC	0x0040	/* CRC select */

/*
 * bits 0x0080 through 0x0800 are generic, and described
 * above with LMC_MII16_LED[0123] _LED_ALL, and _FIFO_RESET
 */
#define LMC_MII16_T1_LL		0x1000	/* LL output RW */
#define LMC_MII16_T1_RL		0x2000	/* RL output RW */
#define LMC_MII16_T1_TM		0x4000	/* TM input RO */
#define LMC_MII16_T1_LOOP	0x8000	/* loopback enable RW */

/*
 * Some of the MII16 bits are mirrored in the MII17 register as well,
 * but let's keep thing seperate for now, and get only the cable from
 * the MII17.
 */
#define LMC_MII17_T1_CABLE_MASK	0x0038	/* mask to extract the cable type */
#define LMC_MII17_T1_CABLE_SHIFT 3	/* shift to extract the cable type */

/*
 * framer register 0 and 7 (7 is latched and reset on read)
 */
#define LMC_FRAMER_REG0_DLOS	0x80	/* digital loss of service */
#define LMC_FRAMER_REG0_OOFS	0x40	/* out of frame sync */
#define LMC_FRAMER_REG0_AIS	0x20	/* alarm indication signal */
#define LMC_FRAMER_REG0_CIS	0x10	/* channel idle */
#define LMC_FRAMER_REG0_LOC	0x08	/* loss of clock */

#define LMC_CARDTYPE_UNKNOWN	-1
#define LMC_CARDTYPE_HSSI	 1	/* probed card is a HSSI card */
#define LMC_CARDTYPE_DS3	 2	/* probed card is a DS3 card */
#define LMC_CARDTYPE_T1		 3	/* probed card is a T1 card */

#if defined(LMC_IS_KERNEL) /* defined in if_lmc_types.h */

/*
 * media independent methods to check on media status, link, light LEDs,
 * etc.
 */
struct lmc___media {
	void	(* init)(lmc_softc_t * const);
	void	(* defaults)(lmc_softc_t * const);
	void	(* set_status)(lmc_softc_t * const, lmc_ctl_t *);
	void	(* set_clock_source)(lmc_softc_t * const, int);
	void	(* set_speed)(lmc_softc_t * const, lmc_ctl_t *);
	void	(* set_cable_length)(lmc_softc_t * const, int);
	void	(* set_scrambler)(lmc_softc_t * const, int);
	int	(* get_link_status)(lmc_softc_t * const);
	void	(* set_link_status)(lmc_softc_t * const, int);
	void	(* set_crc_length)(lmc_softc_t * const, int);
};

u_int32_t lmc_mii_readreg(lmc_softc_t * const sc, u_int32_t devaddr,
			  u_int32_t regno);
void lmc_mii_writereg(lmc_softc_t * const sc, u_int32_t devaddr,
		      u_int32_t regno, u_int32_t data);
void lmc_initcsrs(lmc_softc_t * const sc, lmc_csrptr_t csr_base,
		  size_t csr_size);
void lmc_dec_reset(lmc_softc_t * const sc);
void lmc_reset(lmc_softc_t * const sc);
void lmc_led_on(lmc_softc_t * const sc, u_int32_t led);
void lmc_led_off(lmc_softc_t * const sc, u_int32_t led);
void lmc_gpio_mkinput(lmc_softc_t * const sc, u_int32_t bits);
void lmc_gpio_mkoutput(lmc_softc_t * const sc, u_int32_t bits);
lmc_intrfunc_t lmc_intr_normal(void *);
int lmc_read_macaddr(lmc_softc_t * const sc);
void lmc_attach(lmc_softc_t * const sc);
void lmc_initring(lmc_softc_t * const sc, lmc_ringinfo_t * const ri,
		  tulip_desc_t *descs, int ndescs);

#endif /* LMC_IS_KERNEL */

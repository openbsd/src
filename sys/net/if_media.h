/*	$OpenBSD: if_media.h,v 1.24 2007/05/29 22:11:57 henning Exp $	*/
/*	$NetBSD: if_media.h,v 1.22 2000/02/17 21:53:16 sommerfeld Exp $	*/

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1997
 *	Jonathan Stone and Jason R. Thorpe.  All rights reserved.
 *
 * This software is derived from information provided by Matt Thomas.
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
 *	This product includes software developed by Jonathan Stone
 *	and Jason R. Thorpe for the NetBSD Project.
 * 4. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
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

#ifndef _NET_IF_MEDIA_H_
#define _NET_IF_MEDIA_H_

/*
 * Prototypes and definitions for BSD/OS-compatible network interface
 * media selection.
 *
 * Where it is safe to do so, this code strays slightly from the BSD/OS
 * design.  Software which uses the API (device drivers, basically)
 * shouldn't notice any difference.
 *
 * Many thanks to Matt Thomas for providing the information necessary
 * to implement this interface.
 */

#ifdef _KERNEL

#include <sys/queue.h>

/*
 * Driver callbacks for media status and change requests.
 */
typedef	int (*ifm_change_cb_t)(struct ifnet *);
typedef	void (*ifm_stat_cb_t)(struct ifnet *, struct ifmediareq *);

/*
 * In-kernel representation of a single supported media type.
 */
struct ifmedia_entry {
	TAILQ_ENTRY(ifmedia_entry) ifm_list;
	u_int	ifm_media;	/* description of this media attachment */
	u_int	ifm_data;	/* for driver-specific use */
	void	*ifm_aux;	/* for driver-specific use */
};

/*
 * One of these goes into a network interface's softc structure.
 * It is used to keep general media state.
 */
struct ifmedia {
	u_int	ifm_mask;	/* mask of changes we don't care about */
	u_int	ifm_media;	/* current user-set media word */
	struct ifmedia_entry *ifm_cur;	/* currently selected media */
	TAILQ_HEAD(, ifmedia_entry) ifm_list; /* list of all supported media */
	ifm_change_cb_t	ifm_change;	/* media change driver callback */
	ifm_stat_cb_t	ifm_status;	/* media status driver callback */
};

/* Initialize an interface's struct if_media field. */
void	ifmedia_init(struct ifmedia *, int, ifm_change_cb_t,
	     ifm_stat_cb_t);

/* Add one supported medium to a struct ifmedia. */
void	ifmedia_add(struct ifmedia *, int, int, void *);

/* Add an array (of ifmedia_entry) media to a struct ifmedia. */
void	ifmedia_list_add(struct ifmedia *, struct ifmedia_entry *,
	    int);

/* Set default media type on initialization. */
void	ifmedia_set(struct ifmedia *, int);

/* Common ioctl function for getting/setting media, called by driver. */
int	ifmedia_ioctl(struct ifnet *, struct ifreq *, struct ifmedia *,
	    u_long);

/* Locate a media entry */
struct	ifmedia_entry *ifmedia_match(struct ifmedia *, u_int, u_int);

/* Delete all media for a given media instance */
void	ifmedia_delete_instance(struct ifmedia *, u_int);

/* Compute baudrate for a given media. */
int	ifmedia_baudrate(int);
#endif /*_KERNEL */

/*
 * if_media Options word:
 *	Bits	Use
 *	----	-------
 *	0-4	Media subtype		MAX SUBTYPE == 31!
 *	5-7	Media type
 *	8-15	Type specific options
 *	16-19	RFU
 *	20-27	Shared (global) options
 *	28-31	Instance
 */

/*
 * Ethernet
 */
#define IFM_ETHER	0x00000020
#define	IFM_10_T	3		/* 10BaseT - RJ45 */
#define	IFM_10_2	4		/* 10Base2 - Thinnet */
#define	IFM_10_5	5		/* 10Base5 - AUI */
#define	IFM_100_TX	6		/* 100BaseTX - RJ45 */
#define	IFM_100_FX	7		/* 100BaseFX - Fiber */
#define	IFM_100_T4	8		/* 100BaseT4 - 4 pair cat 3 */
#define	IFM_100_VG	9		/* 100VG-AnyLAN */
#define	IFM_100_T2	10		/* 100BaseT2 */
#define	IFM_1000_SX	11		/* 1000BaseSX - multi-mode fiber */
#define	IFM_10_STP	12		/* 10BaseT over shielded TP */
#define	IFM_10_FL	13		/* 10BaseFL - Fiber */
#define	IFM_1000_LX	14		/* 1000baseLX - single-mode fiber */
#define	IFM_1000_CX	15		/* 1000baseCX - 150ohm STP */
#define	IFM_1000_T	16		/* 1000baseT - 4 pair cat 5 */
#define	IFM_1000_TX	IFM_1000_T	/* for backwards compatibility */
#define	IFM_HPNA_1	17		/* HomePNA 1.0 (1Mb/s) */
#define	IFM_10G_LR	18		/* 10GBase-LR - single-mode fiber */
#define	IFM_10G_SR	19		/* 10GBase-SR - multi-mode fiber */
#define	IFM_10G_CX4	20		/* 10GBase-CX4 - copper */

#define	IFM_ETH_MASTER	0x00000100	/* master mode (1000baseT) */
#define	IFM_ETH_RXPAUSE	0x00000200	/* receive PAUSE frames */
#define	IFM_ETH_TXPAUSE	0x00000400	/* transmit PAUSE frames */

/*
 * FDDI
 */
#define	IFM_FDDI	0x00000060
#define	IFM_FDDI_SMF	3		/* Single-mode fiber */
#define	IFM_FDDI_MMF	4		/* Multi-mode fiber */
#define IFM_FDDI_UTP	5		/* CDDI / UTP */
#define IFM_FDDI_DA	0x00000100	/* Dual attach / single attach */

/*
 * IEEE 802.11 Wireless
 */
#define	IFM_IEEE80211	0x00000080
#define	IFM_IEEE80211_FH1	3	/* Frequency Hopping 1Mbps */
#define	IFM_IEEE80211_FH2	4	/* Frequency Hopping 2Mbps */
#define	IFM_IEEE80211_DS2	5	/* Direct Sequence 2Mbps */
#define	IFM_IEEE80211_DS5	6	/* Direct Sequence 5Mbps*/
#define	IFM_IEEE80211_DS11	7	/* Direct Sequence 11Mbps*/
#define	IFM_IEEE80211_DS1	8	/* Direct Sequence  1Mbps*/
#define IFM_IEEE80211_DS22	9	/* Direct Sequence 22Mbps */ 
#define IFM_IEEE80211_OFDM6	10	/* OFDM 6Mbps */
#define IFM_IEEE80211_OFDM9	11	/* OFDM 9Mbps */
#define IFM_IEEE80211_OFDM12	12	/* OFDM 12Mbps */
#define IFM_IEEE80211_OFDM18	13	/* OFDM 18Mbps */
#define IFM_IEEE80211_OFDM24	14	/* OFDM 24Mbps */
#define IFM_IEEE80211_OFDM36	15	/* OFDM 36Mbps */
#define IFM_IEEE80211_OFDM48	16	/* OFDM 48Mbps */
#define IFM_IEEE80211_OFDM54	17	/* OFDM 54Mbps */
#define IFM_IEEE80211_OFDM72	18	/* OFDM 72Mbps */

#define	IFM_IEEE80211_ADHOC	0x100	/* Operate in Adhoc mode */
#define	IFM_IEEE80211_HOSTAP	0x200	/* Operate in Host AP mode */
#define	IFM_IEEE80211_IBSS	0x400	/* Operate in IBSS mode */
#define	IFM_IEEE80211_IBSSMASTER 0x800	/* Operate as an IBSS master */
#define	IFM_IEEE80211_MONITOR	0x1000	/* Operate in Monitor mode */
#define	IFM_IEEE80211_TURBO	0x2000	/* Operate in Turbo mode */

/* operating mode for multi-mode devices */
#define IFM_IEEE80211_11A	0x00010000	/* 5Ghz, OFDM mode */
#define IFM_IEEE80211_11B	0x00020000	/* Direct Sequence mode */
#define IFM_IEEE80211_11G	0x00030000	/* 2Ghz, CCK mode */
#define IFM_IEEE80211_FH	0x00040000	/* 2Ghz, GFSK mode */

/*
 * Digitally multiplexed "Carrier" Serial Interfaces
 */
#define	IFM_TDM		0x000000a0
#define IFM_TDM_T1		3	/* T1 B8ZS+ESF 24 ts */
#define IFM_TDM_T1_AMI		4	/* T1 AMI+SF 24 ts */
#define IFM_TDM_E1		5	/* E1 HDB3+G.703 clearchannel 32 ts */
#define IFM_TDM_E1_G704		6	/* E1 HDB3+G.703+G.704 channelized 31 ts */
#define IFM_TDM_E1_AMI		7	/* E1 AMI+G.703 32 ts */
#define IFM_TDM_E1_AMI_G704	8	/* E1 AMI+G.703+G.704 31 ts */
#define IFM_TDM_T3		9	/* T3 B3ZS+C-bit 672 ts */
#define IFM_TDM_T3_M13		10	/* T3 B3ZS+M13 672 ts */
#define IFM_TDM_E3		11	/* E3 HDB3+G.751 512? ts */
#define IFM_TDM_E3_G751		12	/* E3 G.751 512 ts */
#define IFM_TDM_E3_G832		13	/* E3 G.832 512 ts */
#define IFM_TDM_E1_G704_CRC4	14	/* E1 HDB3+G.703+G.704 31 ts + CRC4 */
/*
 * 6 major ways that networks talk: Drivers enforce independent selection,
 * meaning, a driver will ensure that only one of these is set at a time.
 * Default is cisco hdlc mode with 32 bit CRC.
 */
#define IFM_TDM_HDLC_CRC16	0x0100	/* Use 16-bit CRC for HDLC instead */
#define IFM_TDM_PPP		0x0200	/* SPPP (dumb) */
#define IFM_TDM_FR_ANSI		0x0400	/* Frame Relay + LMI ANSI "Annex D" */
#define IFM_TDM_FR_CISCO	0x0800	/* Frame Relay + LMI Cisco */
#define IFM_TDM_FR_ITU		0x1000	/* Frame Relay + LMI ITU "Q933A" */

/* operating mode */
#define IFM_TDM_MASTER		0x00010000	/* aka clock source internal */

/*
 * Common Access Redundancy Protocol
 */
#define	IFM_CARP		0x000000c0

/*
 * Shared media sub-types
 */
#define	IFM_AUTO	0		/* Autoselect best media */
#define	IFM_MANUAL	1		/* Jumper/dipswitch selects media */
#define	IFM_NONE	2		/* Deselect all media */

/*
 * Shared options
 */
#define IFM_FDX		0x00100000	/* Force full duplex */
#define	IFM_HDX		0x00200000	/* Force half duplex */
#define	IFM_FLOW	0x00400000	/* enable hardware flow control */
#define IFM_FLAG0	0x01000000	/* Driver defined flag */
#define IFM_FLAG1	0x02000000	/* Driver defined flag */
#define IFM_FLAG2	0x04000000	/* Driver defined flag */
#define	IFM_LOOP	0x08000000	/* Put hardware in loopback */

/*
 * Masks
 */
#define	IFM_NMASK	0x000000e0	/* Network type */
#define	IFM_TMASK	0x0000001f	/* Media sub-type */
#define	IFM_IMASK	0xf0000000	/* Instance */
#define	IFM_ISHIFT	28		/* Instance shift */
#define	IFM_OMASK	0x0000ff00	/* Type specific options */
#define	IFM_MMASK	0x00070000	/* Mode */
#define	IFM_MSHIFT	16		/* Mode shift */
#define	IFM_GMASK	0x0ff00000	/* Global options */

/* Ethernet flow control mask */
#define	IFM_ETH_FMASK	(IFM_FLOW|IFM_ETH_RXPAUSE|IFM_ETH_TXPAUSE)

#define	IFM_NMIN	IFM_ETHER	/* lowest Network type */
#define	IFM_NMAX	IFM_NMASK	/* highest Network type */

/*
 * Status bits
 */
#define	IFM_AVALID	0x00000001	/* Active bit valid */
#define	IFM_ACTIVE	0x00000002	/* Interface attached to working net */

/* Mask of "status valid" bits, for ifconfig(8). */
#define	IFM_STATUS_VALID	IFM_AVALID

/* List of "status valid" bits, for ifconfig(8). */
#define	IFM_STATUS_VALID_LIST {						\
	IFM_AVALID,							\
	0								\
}

/*
 * Macros to extract various bits of information from the media word.
 */
#define	IFM_TYPE(x)	((x) & IFM_NMASK)
#define	IFM_SUBTYPE(x)	((x) & IFM_TMASK)
#define	IFM_INST(x)	(((x) & IFM_IMASK) >> IFM_ISHIFT)
#define	IFM_OPTIONS(x)	((x) & (IFM_OMASK|IFM_GMASK))
#define	IFM_MODE(x)	((x) & IFM_MMASK)

#define	IFM_INST_MAX	IFM_INST(IFM_IMASK)
#define	IFM_INST_ANY	((u_int) -1)

/*
 * Macro to create a media word.
 */
#define	IFM_MAKEWORD(type, subtype, options, instance)			\
	((type) | (subtype) | (options) | ((instance) << IFM_ISHIFT))
#define IFM_MAKEMODE(mode)						\
	(((mode) << IFM_MSHIFT) & IFM_MMASK)
/*
 * NetBSD extension not defined in the BSDI API.  This is used in various
 * places to get the canonical description for a given type/subtype.
 *
 * In the subtype and mediaopt descriptions, the valid TYPE bits are OR'd
 * in to indicate which TYPE the subtype/option corresponds to.  If no
 * TYPE is present, it is a shared media/mediaopt.
 *
 * Note that these are parsed case-insensitive.
 *
 * Order is important.  The first matching entry is the canonical name
 * for a media type; subsequent matches are aliases.
 */
struct ifmedia_description {
	int	ifmt_word;		/* word value; may be masked */
	const char *ifmt_string;	/* description */
};

#define	IFM_TYPE_DESCRIPTIONS {						\
	{ IFM_ETHER,			"Ethernet" },			\
	{ IFM_ETHER,			"ether" },			\
	{ IFM_FDDI,			"FDDI" },			\
	{ IFM_IEEE80211,		"IEEE802.11" },			\
	{ IFM_TDM,			"TDM" },			\
	{ IFM_CARP,			"CARP" },			\
	{ 0, NULL },							\
}

#define	IFM_TYPE_MATCH(dt, t)						\
	(IFM_TYPE((dt)) == 0 || IFM_TYPE((dt)) == IFM_TYPE((t)))

#define	IFM_SUBTYPE_DESCRIPTIONS {					\
	{ IFM_AUTO,			"autoselect" },			\
	{ IFM_AUTO,			"auto" },			\
	{ IFM_MANUAL,			"manual" },			\
	{ IFM_NONE,			"none" },			\
									\
	{ IFM_ETHER|IFM_10_T,		"10baseT" },			\
	{ IFM_ETHER|IFM_10_T,		"10baseT/UTP" },		\
	{ IFM_ETHER|IFM_10_T,		"UTP" },			\
	{ IFM_ETHER|IFM_10_T,		"10UTP" },			\
	{ IFM_ETHER|IFM_10_2,		"10base2" },			\
	{ IFM_ETHER|IFM_10_2,		"10base2/BNC" },		\
	{ IFM_ETHER|IFM_10_2,		"BNC" },			\
	{ IFM_ETHER|IFM_10_2,		"10BNC" },			\
	{ IFM_ETHER|IFM_10_5,		"10base5" },			\
	{ IFM_ETHER|IFM_10_5,		"10base5/AUI" },		\
	{ IFM_ETHER|IFM_10_5,		"AUI" },			\
	{ IFM_ETHER|IFM_10_5,		"10AUI" },			\
	{ IFM_ETHER|IFM_100_TX,		"100baseTX" },			\
	{ IFM_ETHER|IFM_100_TX,		"100TX" },			\
	{ IFM_ETHER|IFM_100_FX,		"100baseFX" },			\
	{ IFM_ETHER|IFM_100_FX,		"100FX" },			\
	{ IFM_ETHER|IFM_100_T4,		"100baseT4" },			\
	{ IFM_ETHER|IFM_100_T4,		"100T4" },			\
	{ IFM_ETHER|IFM_100_VG,		"100baseVG" },			\
	{ IFM_ETHER|IFM_100_VG,		"100VG" },			\
	{ IFM_ETHER|IFM_100_T2,		"100baseT2" },			\
	{ IFM_ETHER|IFM_100_T2,		"100T2" },			\
	{ IFM_ETHER|IFM_1000_SX,	"1000baseSX" },			\
	{ IFM_ETHER|IFM_1000_SX,	"1000SX" },			\
	{ IFM_ETHER|IFM_10_STP,		"10baseSTP" },			\
	{ IFM_ETHER|IFM_10_STP,		"STP" },			\
	{ IFM_ETHER|IFM_10_STP,		"10STP" },			\
	{ IFM_ETHER|IFM_10_FL,		"10baseFL" },			\
	{ IFM_ETHER|IFM_10_FL,		"FL" },				\
	{ IFM_ETHER|IFM_10_FL,		"10FL" },			\
	{ IFM_ETHER|IFM_1000_LX,	"1000baseLX" },			\
	{ IFM_ETHER|IFM_1000_LX,	"1000LX" },			\
	{ IFM_ETHER|IFM_1000_CX,	"1000baseCX" },			\
	{ IFM_ETHER|IFM_1000_CX,	"1000CX" },			\
	{ IFM_ETHER|IFM_1000_T,		"1000baseT" },			\
	{ IFM_ETHER|IFM_1000_T,		"1000T" },			\
	{ IFM_ETHER|IFM_1000_T,		"1000baseTX" },			\
	{ IFM_ETHER|IFM_1000_T,		"1000TX" },			\
	{ IFM_ETHER|IFM_HPNA_1,		"HomePNA1" },			\
	{ IFM_ETHER|IFM_HPNA_1,		"HPNA1" },			\
	{ IFM_ETHER|IFM_10G_LR,		"10GbaseLR" },			\
	{ IFM_ETHER|IFM_10G_LR,		"10GLR" },			\
	{ IFM_ETHER|IFM_10G_LR,		"10GBASE-LR" },			\
	{ IFM_ETHER|IFM_10G_SR,		"10GbaseSR" },			\
	{ IFM_ETHER|IFM_10G_SR,		"10GSR" },			\
	{ IFM_ETHER|IFM_10G_SR,		"10GBASE-SR" },			\
	{ IFM_ETHER|IFM_10G_CX4,	"10GbaseCX4" },			\
	{ IFM_ETHER|IFM_10G_CX4,	"10GCX4" },			\
	{ IFM_ETHER|IFM_10G_CX4,	"10GBASE-CX4" },		\
									\
	{ IFM_FDDI|IFM_FDDI_SMF,	"Single-mode" },		\
	{ IFM_FDDI|IFM_FDDI_SMF,	"SMF" },			\
	{ IFM_FDDI|IFM_FDDI_MMF,	"Multi-mode" },			\
	{ IFM_FDDI|IFM_FDDI_MMF,	"MMF" },			\
	{ IFM_FDDI|IFM_FDDI_UTP,	"UTP" },			\
	{ IFM_FDDI|IFM_FDDI_UTP,	"CDDI" },			\
									\
	{ IFM_IEEE80211|IFM_IEEE80211_FH1,	"FH1" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_FH2,	"FH2" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_DS2,	"DS2" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_DS5,	"DS5" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_DS11,	"DS11" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_DS1,	"DS1" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_DS22,	"DS22" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM6,	"OFDM6" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM9,	"OFDM9" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM12,	"OFDM12" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM18,	"OFDM18" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM24,	"OFDM24" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM36,	"OFDM36" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM48,	"OFDM48" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM54,	"OFDM54" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM72,	"OFDM72" },		\
									\
	{ IFM_TDM|IFM_TDM_T1,		"t1" },				\
	{ IFM_TDM|IFM_TDM_T1_AMI,	"t1-ami" },			\
	{ IFM_TDM|IFM_TDM_E1,		"e1" },				\
	{ IFM_TDM|IFM_TDM_E1_G704,	"e1-g.704" },			\
	{ IFM_TDM|IFM_TDM_E1_AMI,	"e1-ami" },			\
	{ IFM_TDM|IFM_TDM_E1_AMI_G704,	"e1-ami-g.704" },		\
	{ IFM_TDM|IFM_TDM_T3,		"t3" },				\
	{ IFM_TDM|IFM_TDM_T3_M13,	"t3-m13" },			\
	{ IFM_TDM|IFM_TDM_E3,		"e3" },				\
	{ IFM_TDM|IFM_TDM_E3_G751,	"e3-g.751" },			\
	{ IFM_TDM|IFM_TDM_E3_G832,	"e3-g.832" },			\
	{ IFM_TDM|IFM_TDM_E1_G704_CRC4,	"e1-g.704-crc4" },		\
									\
	{ 0, NULL },							\
}

#define IFM_MODE_DESCRIPTIONS {						\
	{ IFM_AUTO,				"autoselect" },		\
	{ IFM_AUTO,				"auto" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_11A,	"11a" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_11B,	"11b" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_11G,	"11g" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_FH,	"fh" },			\
	{ IFM_TDM|IFM_TDM_MASTER,		"master" },		\
	{ 0, NULL },							\
}

#define	IFM_OPTION_DESCRIPTIONS {					\
	{ IFM_FDX,			"full-duplex" },		\
	{ IFM_FDX,			"fdx" },			\
	{ IFM_HDX,			"half-duplex" },		\
	{ IFM_HDX,			"hdx" },			\
	{ IFM_FLAG0,			"flag0" },			\
	{ IFM_FLAG1,			"flag1" },			\
	{ IFM_FLAG2,			"flag2" },			\
	{ IFM_LOOP,			"loopback" },			\
	{ IFM_LOOP,			"hw-loopback"},			\
	{ IFM_LOOP,			"loop" },			\
									\
	{ IFM_ETHER|IFM_ETH_MASTER,	"master" },			\
	{ IFM_ETHER|IFM_ETH_RXPAUSE,	"rxpause" },			\
	{ IFM_ETHER|IFM_ETH_TXPAUSE,	"txpause" },			\
									\
	{ IFM_FDDI|IFM_FDDI_DA,		"dual-attach" },		\
	{ IFM_FDDI|IFM_FDDI_DA,		"das" },			\
									\
	{ IFM_IEEE80211|IFM_IEEE80211_ADHOC,	"adhoc" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_HOSTAP,	"hostap" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_IBSS,	"ibss" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_IBSSMASTER, "ibss-master" },	\
	{ IFM_IEEE80211|IFM_IEEE80211_MONITOR,	"monitor" },		\
	{ IFM_IEEE80211|IFM_IEEE80211_TURBO,	"turbo" },		\
									\
	{ IFM_TDM|IFM_TDM_HDLC_CRC16,	"hdlc-crc16" },			\
	{ IFM_TDM|IFM_TDM_PPP,		"ppp" },			\
	{ IFM_TDM|IFM_TDM_FR_ANSI,	"framerelay-ansi" },		\
	{ IFM_TDM|IFM_TDM_FR_CISCO,	"framerelay-cisco" },		\
	{ IFM_TDM|IFM_TDM_FR_ANSI,	"framerelay-itu" },		\
									\
	{ 0, NULL },							\
}

/*
 * Baudrate descriptions for the various media types.
 */
struct ifmedia_baudrate {
	int	ifmb_word;		/* media word */
	int	ifmb_baudrate;		/* corresponding baudrate */
};

#define	IFM_BAUDRATE_DESCRIPTIONS {					\
	{ IFM_ETHER|IFM_10_T,		IF_Mbps(10) },			\
	{ IFM_ETHER|IFM_10_2,		IF_Mbps(10) },			\
	{ IFM_ETHER|IFM_10_5,		IF_Mbps(10) },			\
	{ IFM_ETHER|IFM_100_TX,		IF_Mbps(100) },			\
	{ IFM_ETHER|IFM_100_FX,		IF_Mbps(100) },			\
	{ IFM_ETHER|IFM_100_T4,		IF_Mbps(100) },			\
	{ IFM_ETHER|IFM_100_VG,		IF_Mbps(100) },			\
	{ IFM_ETHER|IFM_100_T2,		IF_Mbps(100) },			\
	{ IFM_ETHER|IFM_1000_SX,	IF_Mbps(1000) },		\
	{ IFM_ETHER|IFM_10_STP,		IF_Mbps(10) },			\
	{ IFM_ETHER|IFM_10_FL,		IF_Mbps(10) },			\
	{ IFM_ETHER|IFM_1000_LX,	IF_Mbps(1000) },		\
	{ IFM_ETHER|IFM_1000_CX,	IF_Mbps(1000) },		\
	{ IFM_ETHER|IFM_1000_T,		IF_Mbps(1000) },		\
	{ IFM_ETHER|IFM_HPNA_1,		IF_Mbps(1) },			\
	{ IFM_ETHER|IFM_10G_LR,		IF_Mbps(1000) },		\
	{ IFM_ETHER|IFM_10G_SR,		IF_Mbps(1000) },		\
	{ IFM_ETHER|IFM_10G_CX4,	IF_Mbps(1000) },		\
									\
	{ IFM_FDDI|IFM_FDDI_SMF,	IF_Mbps(100) },			\
	{ IFM_FDDI|IFM_FDDI_MMF,	IF_Mbps(100) },			\
	{ IFM_FDDI|IFM_FDDI_UTP,	IF_Mbps(100) },			\
									\
	{ IFM_IEEE80211|IFM_IEEE80211_FH1, IF_Mbps(1) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_FH2, IF_Mbps(2) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_DS1, IF_Mbps(1) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_DS2, IF_Mbps(2) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_DS5, IF_Mbps(5) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_DS11, IF_Mbps(11) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_DS22, IF_Mbps(22) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM6, IF_Mbps(6) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM9, IF_Mbps(9) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM12, IF_Mbps(12) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM18, IF_Mbps(18) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM24, IF_Mbps(24) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM36, IF_Mbps(36) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM48, IF_Mbps(48) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM54, IF_Mbps(54) },		\
	{ IFM_IEEE80211|IFM_IEEE80211_OFDM72, IF_Mbps(72) },		\
									\
	{ IFM_TDM|IFM_TDM_T1,		IF_Kbps(1536) },		\
	{ IFM_TDM|IFM_TDM_T1_AMI,	IF_Kbps(1536) },		\
	{ IFM_TDM|IFM_TDM_E1,		IF_Kbps(2048) },		\
	{ IFM_TDM|IFM_TDM_E1_G704,	IF_Kbps(2048) },		\
	{ IFM_TDM|IFM_TDM_E1_AMI,	IF_Kbps(2048) },		\
	{ IFM_TDM|IFM_TDM_E1_AMI_G704,	IF_Kbps(2048) },		\
	{ IFM_TDM|IFM_TDM_T3,		IF_Kbps(44736) },		\
	{ IFM_TDM|IFM_TDM_T3_M13,	IF_Kbps(44736) },		\
	{ IFM_TDM|IFM_TDM_E3,		IF_Kbps(34368) },		\
	{ IFM_TDM|IFM_TDM_E3_G751,	IF_Kbps(34368) },		\
	{ IFM_TDM|IFM_TDM_E3_G832,	IF_Kbps(34368) },		\
	{ IFM_TDM|IFM_TDM_E1_G704_CRC4,	IF_Kbps(2048) },		\
									\
	{ 0, 0 },							\
}

/*
 * Status bit descriptions for the various media types.
 */
struct ifmedia_status_description {
	int	ifms_type;
	int	ifms_valid;
	int	ifms_bit;
	const char *ifms_string[2];
};

#define	IFM_STATUS_DESC(ifms, bit)					\
	(ifms)->ifms_string[((ifms)->ifms_bit & (bit)) ? 1 : 0]

#define	IFM_STATUS_DESCRIPTIONS {					\
	{ IFM_ETHER,		IFM_AVALID,	IFM_ACTIVE,		\
	    { "no carrier", "active" } },				\
	{ IFM_FDDI,		IFM_AVALID,	IFM_ACTIVE,		\
	    { "no ring", "inserted" } },				\
	{ IFM_IEEE80211,	IFM_AVALID,	IFM_ACTIVE,		\
	    { "no network", "active" } },				\
	{ IFM_TDM,		IFM_AVALID,	IFM_ACTIVE,		\
	    { "no carrier", "active" } },				\
	{ IFM_CARP,		IFM_AVALID,	IFM_ACTIVE,		\
	    { "backup", "master" } },					\
	{ 0,			0,		0,			\
	    { NULL, NULL } }						\
}
#endif	/* _NET_IF_MEDIA_H_ */

/*	$NetBSD: if_aereg.h,v 1.12 1996/01/12 04:53:05 briggs Exp $	*/

/*
 * National Semiconductor DS8390 NIC register definitions.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

/*
 * The following is a structure that we should be pulling from
 * dev/ic/dp8390reg.h, but since we have the card in x86 mode and
 * the std. include file assumes that big-endian processors will
 * have the card in m68k mode, we're hosed for the moment.
 * Fix this.  --  XXX
 */
struct ae_ring {
	u_char  rsr;		/* receiver status */
	u_char  next_packet;	/* pointer to next packet */
	u_short count;		/* bytes in packet (length + 4) */
};
/*
 * Vendor types
 */
#define AE_VENDOR_UNKNOWN	0xFF	/* Unknown network card */
#define AE_VENDOR_APPLE		0x00	/* Apple Ethernet card */
#define AE_VENDOR_INTERLAN	0x01	/* Interlan A310 card (GatorCard) */
#define AE_VENDOR_DAYNA		0x02	/* DaynaPORT E/30s (and others?) */
#define AE_VENDOR_ASANTE	0x03	/* Asante MacCon II/E */
#define AE_VENDOR_FARALLON	0x04	/* Farallon EtherMac II-TP */
#define AE_VENDOR_FOCUS		0x05	/* FOCUS Enhancements EtherLAN */

/*
 * Compile-time config flags
 */
/*
 * This sets the default for enabling/disablng the tranceiver.
 */
#define AE_FLAGS_DISABLE_TRANCEIVER	0x0001

/*
 * This disables the use of double transmit buffers.
 */
#define AE_FLAGS_NO_DOUBLE_BUFFERING	0x0008

/* */
#define	GC_RESET_OFFSET		0x000c0000	/* writes here reset NIC */
#define	GC_ROM_OFFSET		0x000c0000	/* address prom */
#define GC_DATA_OFFSET		0x000d0000	/* Offset to NIC memory */
#define GC_NIC_OFFSET		0x000e0000	/* Offset to NIC registers */

#define DP_ROM_OFFSET		0x000f0000
#define DP_DATA_OFFSET		0x000d0000	/* Offset to SONIC memory */
#define DP_NIC_OFFSET		0x000e0000	/* Offset to SONIC registers */

#define AE_ROM_OFFSET		0x000f0000
#define AE_DATA_OFFSET		0x000d0000	/* Offset to NIC memory */
#define AE_NIC_OFFSET		0x000e0000	/* Offset to NIC registers */

#define FE_ROM_OFFSET		0x000d0006	/* Determined empirically */

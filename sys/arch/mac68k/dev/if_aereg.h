/*	$OpenBSD: if_aereg.h,v 1.8 2007/01/22 13:17:45 martin Exp $	*/
/*	$NetBSD: if_aereg.h,v 1.17 1998/08/12 07:19:09 scottr Exp $	*/

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
 * Memory offsets from slot base PA
 */
#define	GC_RESET_OFFSET		0x000c0000	/* writes here reset NIC */
#define	GC_ROM_OFFSET		0x000c0000	/* address prom */
#define GC_DATA_OFFSET		0x000d0000	/* Offset to NIC memory */
#define GC_REG_OFFSET		0x000e0000	/* Offset to NIC registers */

#define DP_ROM_OFFSET		0x000f0000
#define DP_DATA_OFFSET		0x000d0000	/* Offset to SONIC memory */
#define DP_REG_OFFSET		0x000e0000	/* Offset to SONIC registers */

#define AE_ROM_OFFSET		0x000f0000
#define AE_DATA_OFFSET		0x000d0000	/* Offset to NIC memory */
#define AE_REG_OFFSET		0x000e0000	/* Offset to NIC registers */

#define FE_ROM_OFFSET		0x000d0006	/* Determined empirically */

#define KE_ROM_OFFSET		0x000f0007
#define KE_DATA_OFFSET		0x00000000	/* Offset to NIC memory */
#define KE_REG_OFFSET		0x00080003	/* Offset to NIC registers */

#define CT_ROM_OFFSET		0x00030000	/* ROM offset */
#define CT_DATA_OFFSET		0x00000000	/* RAM offset */
#define CT_REG_OFFSET		0x00010000	/* REG offset */

#define	AE_REG_SIZE		0x40		/* Size of register space */

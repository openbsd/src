/*	$NetBSD: am9516.h,v 1.1 1995/10/29 21:19:06 gwr Exp $	*/

/*
 * This file is derived from the file dev/devSCSI3.c from
 * the Berkeley SPRITE distribution, which says:
 *
 * Copyright 1988 Regents of the University of California
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

/*
 * AMD 9516 UDC (Universal DMA Controller) Registers.
 * This is used only in the OBIO version (3/50,3/60).
 */

/* addresses of the udc registers accessed directly by driver */
#define UDC_ADR_MODE		0x38	/* master mode register */
#define UDC_ADR_COMMAND		0x2e	/* command register (write only) */
#define UDC_ADR_STATUS		0x2e	/* status register (read only) */
#define UDC_ADR_CAR_HIGH	0x26	/* chain addr reg, high word */
#define UDC_ADR_CAR_LOW		0x22	/* chain addr reg, low word */
#define UDC_ADR_CARA_HIGH	0x1a	/* cur addr reg A, high word */
#define UDC_ADR_CARA_LOW	0x0a	/* cur addr reg A, low word */
#define UDC_ADR_CARB_HIGH	0x12	/* cur addr reg B, high word */
#define UDC_ADR_CARB_LOW	0x02	/* cur addr reg B, low word */
#define UDC_ADR_CMR_HIGH	0x56	/* channel mode reg, high word */
#define UDC_ADR_CMR_LOW		0x52	/* channel mode reg, low word */
#define UDC_ADR_COUNT		0x32	/* number of words to transfer */

/* 
 * For a dma transfer, the appropriate udc registers are loaded from a 
 * table in memory pointed to by the chain address register.
 */
struct udc_table {
	u_short			rsel;	/* tells udc which regs to load */
	u_short			addrh;	/* high word of main mem dma address */
	u_short			addrl;	/* low word of main mem dma address */
	u_short			count;	/* num words to transfer */
	u_short			cmrh;	/* high word of channel mode reg */
	u_short			cmrl;	/* low word of channel mode reg */
};

/* indicates which udc registers are to be set based on info in above table */
#define UDC_RSEL_RECV		0x0182
#define UDC_RSEL_SEND		0x0282

/* setting of chain mode reg: selects how the dma op is to be executed */
#define UDC_CMR_HIGH		0x0040	/* high word of channel mode reg */
#define UDC_CMR_LSEND		0x00c2	/* low word of cmr when send */
#define UDC_CMR_LRECV		0x00d2	/* low word of cmr when receiving */

/* setting for the master mode register */
#define UDC_MODE		0xd	/* enables udc chip */

/* setting for the low byte in the high word of an address */
#define UDC_ADDR_INFO		0x40	/* inc addr after each word is dma'd */

/* udc commands */
#define UDC_CMD_STRT_CHN	0xa0	/* start chaining */
#define UDC_CMD_CIE		0x32	/* channel 1 interrupt enable */
#define UDC_CMD_RESET		0x00	/* reset udc, same as hdw reset */

/* bits in the udc status register */
#define UDC_SR_CIE		0x8000	/* channel interrupt enable */
#define UDC_SR_IP		0x2000	/* interrupt pending */
#define UDC_SR_CA		0x1000	/* channel abort */
#define UDC_SR_NAC		0x0800	/* no auto reload or chaining*/
#define UDC_SR_WFB		0x0400	/* waiting for bus */
#define UDC_SR_SIP		0x0200	/* second interrupt pending */
#define UDC_SR_HM		0x0040	/* hardware mask */
#define UDC_SR_HRQ		0x0020	/* hardware request */
#define UDC_SR_MCH		0x0010	/* match on upper comparator byte */
#define UDC_SR_MCL		0x0008	/* match on lower comparator byte */
#define UDC_SR_MC		0x0004	/* match condition ended dma */
#define UDC_SR_EOP		0x0002	/* eop condition ended dma */
#define UDC_SR_TC		0x0001	/* termination of count ended dma */


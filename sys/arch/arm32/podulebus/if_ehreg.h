/* $NetBSD: if_ehreg.h,v 1.2 1996/03/08 16:24:51 mark Exp $ */

/*
 * Copyright (c) 1995 Melvin Tang-Richardson.
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
 *	This product includes software developed by RiscBSD.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RISCBSD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL RISCBSD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * if_ehreg.h
 *
 * Ether H register definitions.
 */

#define EH_RESET	(0x18<<2)

#define EH_DATA_PORT	(0x10<<2)

/* Special registers */

#define EH_CONFIGA	(0x0a<<2)

/* Page 0 read registers */

#define EH_COMMAND	(0x00<<2)
#define EH_CLAD0	(0x01<<2)
#define EH_CLDA1	(0x02<<2)
#define EH_BNRY		(0x03<<2)
#define EH_TSR		(0x04<<2)
#define EH_NCR		(0x05<<2)
#define EH_FIFO		(0x06<<2)
#define EH_ISR		(0x07<<2)
#define EH_CRDA0	(0x08<<2)
#define EH_CRDA1	(0x09<<2)
#define EH_RSR		(0x0c<<2)

/* Page 0 write registers */

#define EH_PSTART	(0x01<<2)
#define EH_PSTOP	(0x02<<2)
#define EH_TPSR		(0x04<<2)
#define EH_TBCR0	(0x05<<2)
#define EH_TBCR1	(0x06<<2)
#define EH_RSAR0	(0x08<<2)
#define EH_RSAR1	(0x09<<2)
#define EH_RBCR0	(0x0a<<2)
#define EH_RBCR1	(0x0b<<2)
#define EH_RCR		(0x0c<<2)
#define EH_TCR		(0x0d<<2)
#define EH_DCR		(0x0e<<2)
#define EH_IMR		(0x0f<<2)

/* Page 1 write registers */

#define EH_CURR		(0x07<<2)

/* Command bits */

#define COM_STP		(0x01)
#define COM_STA		(0x02)
#define COM_TXP		(0x04)
#define COM_MASK_PAGE	(0x3f)
#define COM_READ	(0x08)
#define COM_WRITE	(0x10)
#define COM_SEND	(0x18)
#define COM_ABORT	(0x20)
#define COM_MASK_DMA	(0xc7)
#define COM_DMA_MASK	(0x38)

/* DCR bits */

#define DCR_WTS		(0x01)
#define DCR_BOS		(0x02)
#define DCR_LAS		(0x04)
#define DCR_LS		(0x08)
#define DCR_ARM		(0x10)

/* TSR bits */

#define TSR_PTX		(0x01)
#define TSR_COL		(0x04)
#define TSR_ABT		(0x08)
#define TSR_CRS		(0x10)
#define TSR_FU		(0x20)
#define TSR_CDH		(0x40)
#define TSR_OWC		(0x80)
#define TSR_DONE	((TSR_PTX)|(TSR_ABT))

/* TCR bits */

#define TCR_CRC		(0x01)
#define TCR_NORMALLOOP	(0x00)
#define TCR_NICMOD	(0x02)
#define TCR_ENDECMOD	(0x04)
#define TCR_EXTLOOP	(0x06)
#define TCR_ATD		(0x08)
#define TCR_OFST	(0x10)

/* ISR bits */

#define ISR_PRX		(0x01)
#define ISR_PTX		(0x02)
#define ISR_RXE		(0x04)
#define ISR_TXE		(0x08)
#define ISR_OVW		(0x10)
#define ISR_CNT		(0x20)
#define ISR_RDC		(0x40)
#define ISR_RST		(0x80)

/* RSR bits */

#define RSR_PRX		(0x01)
#define RSR_CRC		(0x02)
#define RSR_FAE		(0x04)
#define RSR_FO		(0x08)
#define RSR_MPA		(0x10)
#define RSR_PHY		(0x20)
#define RSR_DIS		(0x40)
#define RSR_DFR		(0x80)

struct eh_rxhdr {
	char rx_status;
	char rx_nxtpkt;
	char rx_rbc0;
	char rx_rbc1;
};


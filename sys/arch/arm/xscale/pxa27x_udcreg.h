/*	$OpenBSD: pxa27x_udcreg.h,v 1.1 2005/02/17 22:10:35 dlg Exp $ */

/*
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
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
 * Register Descriptions for the USB Device Controller 
 *
 * Reference:
 *  Intel(r) PXA27x Processor Family
 *   Developer's Manual
 *  (2800002.pdf)
 */

#ifndef _ARM_XSCALE_PXA27X_UDCREG_H_
#define _ARM_XSCALE_PXA27X_UDCREG_H_

#define USBDC_UDCCR	0x0000  /* UDC Control Register */
#define  USBDC_UDCCR_UDE	(1<<0)	/* UDC Enable */
#define  USBDC_UDCCR_UDA	(1<<1)	/* UDC Active */
#define  USBDC_UDCCR_UDR	(1<<2)	/* UDC Resume */
#define  USBDC_UDCCR_EMCE	(1<<3)	/* Endpoint Mem Config Error */
#define  USBDC_UDCCR_SMAC	(1<<4)	/* Switch EndPt Mem to Active Config */
#define  USBDC_UDCCR_AAISN	(7<<5)	/* Active UDC Alt Iface Setting */
#define  USBDC_UDCCR_AIN	(7<<8)	/* Active UDC Iface */
#define  USBDC_UDCCR_ACN	(7<<11)	/* Active UDC Config */
#define  USBDC_UDCCR_DWRE	(1<<16)	/* Device Remote Wake-Up Feature */
#define  USBDC_UDCCR_BHNP	(1<<28)	/* B-Device Host Neg Proto Enable */
#define  USBDC_UDCCR_AHNP	(1<<29)	/* A-Device Host NEg Proto Support */
#define  USBDC_UDCCR_AALTHNP	(1<<30) /* A-Dev Alt Host Neg Proto Port Sup */
#define  USBDC_UDCCR_OEN	(1<<31)	/* On-The-Go Enable */
#define USBDC_UDCICR0	0x0004	/* UDC Interrupt Control Register 0 */
#define  USBDC_UDCICR0_IE(n)	(3<<(n)) /* Interrupt Enables */
#define USBDC_UDCICR1	0x0008	/* UDC Interrupt Control Register 1 */
#define  USBDC_UDCICR1_IE(n)	(3<<(n)) /* Interrupt Enables */
#define  USBDC_UDCICR1_IERS	(1<<27)	/* Interrupt Enable Reset */
#define  USBDC_UDCICR1_IESU	(1<<28)	/* Interrupt Enable Suspend */
#define  USBDC_UDCICR1_IERU	(1<<29)	/* Interrupt Enable Resume */
#define  USBDC_UDCICR1_IESOF	(1<<30)	/* Interrupt Enable Start of Frame */
#define  USBDC_UDCICR1_IECC	(1<<31)	/* Interrupt Enable Config Change */
#define USBDC_UDCISR0	0x000c	/* UDC Interrupt Status Register 0 */
#define  USBDC_UDCISR0_IR(n)	(3<<(n)) /* Interrupt Requests */
#define USBDC_UDCISR1	0x0010	/* UDC Interrupt Status Register 1 */
#define  USBDC_UDCISR1_IR(n)	(3<<(n)) /* Interrupt Requests */
#define  USBDC_UDCISR1_IRRS	(1<<27)	/* Interrupt Enable Reset */
#define  USBDC_UDCISR1_IRSU	(1<<28)	/* Interrupt Enable Suspend */
#define  USBDC_UDCISR1_IRRU	(1<<29)	/* Interrupt Enable Resume */
#define  USBDC_UDCISR1_IRSOF	(1<<30)	/* Interrupt Enable Start of Frame */
#define  USBDC_UDCISR1_IRCC	(1<<31)	/* Interrupt Enable Config Change */
#define USBDC_UDCFNR	0x0014	/* UDC Frame Number Register */
#define  USBDC_UDCFNR_FN	(1023<<0) /* Frame Number */
#define USBDC_UDCOTGICR	0x0018	/* UDC OTG Interrupt Control Register */
#define  USBDC_UDCOTGICR_IEIDF	(1<<0)	/* OTG ID Change Fall Intr En */
#define  USBDC_UDCOTGICR_IEIDR	(1<<1)	/* OTG ID Change Ris Intr En */
#define  USBDC_UDCOTGICR_IESDF	(1<<2)	/* OTG A-Dev SRP Detect Fall Intr En */
#define  USBDC_UDCOTGICR_IESDR	(1<<3)	/* OTG A-Dev SRP Detect Ris Intr En */
#define  USBDC_UDCOTGICR_IESVF	(1<<4)	/* OTG Session Valid Fall Intr En */
#define  USBDC_UDCOTGICR_IESVR	(1<<5)	/* OTG Session Valid Ris Intr En */
#define  USBDC_UDCOTGICR_IEVV44F (1<<6)	/* OTG Vbus Valid 4.4V Fall Intr En */
#define  USBDC_UDCOTGICR_IEVV44R (1<<7)	/* OTG Vbus Valid 4.4V Ris Intr En */
#define  USBDC_UDCOTGICR_IEVV40F (1<<8)	/* OTG Vbus Valid 4.0V Fall Intr En */
#define  USBDC_UDCOTGICR_IEVV40R (1<<9)	/* OTG Vbus Valid 4.0V Ris Intr En */
#define  USBDC_UDCOTGICR_IEXF	(1<<16)	/* Extern Transceiver Intr Fall En */
#define  USBDC_UDCOTGICR_IEXR	(1<<17)	/* Extern Transceiver Intr Ris En */
#define  USBDC_UDCOTGICR_IESF	(1<<24)	/* OTG SET_FEATURE Command Recvd */
#define USBDC_UDCOTGISR	0x001c	/* UDC OTG Interrupt Status Register */
#define  USBDC_UDCOTGISR_IRIDF	(1<<0)	/* OTG ID Change Fall Intr Req */
#define  USBDC_UDCOTGISR_IRIDR	(1<<1)	/* OTG ID Change Ris Intr Req */
#define  USBDC_UDCOTGISR_IRSDF	(1<<2)	/* OTG A-Dev SRP Detect Fall Intr Req */
#define  USBDC_UDCOTGISR_IRSDR	(1<<3)	/* OTG A-Dev SRP Detect Ris Intr Req */
#define  USBDC_UDCOTGISR_IRSVF	(1<<4)	/* OTG Session Valid Fall Intr Req */
#define  USBDC_UDCOTGISR_IRSVR	(1<<5)	/* OTG Session Valid Ris Intr Req */
#define  USBDC_UDCOTGISR_IRVV44F (1<<6)	/* OTG Vbus Valid 4.4V Fall Intr Req */
#define  USBDC_UDCOTGISR_IRVV44R (1<<7)	/* OTG Vbus Valid 4.4V Ris Intr Req */
#define  USBDC_UDCOTGISR_IRVV40F (1<<8)	/* OTG Vbus Valid 4.0V Fall Intr Req */
#define  USBDC_UDCOTGISR_IRVV40R (1<<9)	/* OTG Vbus Valid 4.0V Ris Intr Req */
#define  USBDC_UDCOTGISR_IRXF	(1<<16)	/* Extern Transceiver Intr Fall Req */
#define  USBDC_UDCOTGISR_IRXR	(1<<17)	/* Extern Transceiver Intr Ris Req */
#define  USBDC_UDCOTGISR_IRSF	(1<<24)	/* OTG SET_FEATURE Command Recvd */
#define USBDC_UP2OCR	0x0020	/* USB Port 2 Output Control Register */
#define  USBDC_UP2OCR_CPVEN	(1<<0)	/* Charge Pump Vbus Enable */
#define  USBDC_UP2OCR_CPVPE	(1<<1)	/* Charge Pump Vbus Pulse Enable */
#define  USBDC_UP2OCR_DPPDE	(1<<2)	/* Host Transc D+ Pull Down En */
#define  USBDC_UP2OCR_DMPDE	(1<<3)	/* Host Transc D- Pull Down En */
#define  USBDC_UP2OCR_DPPUE	(1<<4)	/* Host Transc D+ Pull Up En */
#define  USBDC_UP2OCR_DMPUE	(1<<5)	/* Host Transc D- Pull Up En */
#define  USBDC_UP2OCR_DPPUBE	(1<<6)	/* Host Transc D+ Pull Up Bypass En */
#define  USBDC_UP2OCR_DMPUBE	(1<<7)	/* Host Transc D- Pull Up Bypass En */
#define  USBDC_UP2OCR_EXSP	(1<<8)	/* External Transc Speed Control */
#define  USBDC_UP2OCR_EXSUS	(1<<9)	/* External Transc Suspend Control */
#define  USBDC_UP2OCR_IDON	(1<<10)	/* OTG ID Read Enable */
#define  USBDC_UP2OCR_HXS	(1<<16)	/* Host Transc Output Select */
#define  USBDC_UP2OCR_HXOE	(1<<17)	/* Host Transc Output Enable */
#define  USBDC_UP2OCR_SEOS	(7<<24)	/* Single-Ended Output Select */
#define USBDC_UP3OCR	0x0024	/* USB Port 3 Output Control Register */
#define  USBDC_UP3OCR_CFG	(3<<0)	/* Host Port Configuration */
/* 0x0028 to 0x00fc is reserved */
#define USBDC_UDCCSR0	0x0100	/* UDC Endpoint 0 Control/Status Registers */
#define  USBDC_UDCCSR0_OPC	(1<<0)	/* OUT Packet Complete */
#define  USBDC_UDCCSR0_IPR	(1<<1)	/* IN Packet Ready */
#define  USBDC_UDCCSR0_FTF	(1<<2)	/* Flush Transmit FIFO */
#define  USBDC_UDCCSR0_DME	(1<<3)	/* DMA Enable */
#define  USBDC_UDCCSR0_SST	(1<<4)	/* Sent Stall */
#define  USBDC_UDCCSR0_FST	(1<<5)	/* Force Stall */
#define  USBDC_UDCCSR0_RNE	(1<<6)	/* Receive FIFO Not Empty */
#define  USBDC_UDCCSR0_SA	(1<<7)	/* Setup Active */
#define  USBDC_UDCCSR0_AREN	(1<<8)	/* ACK Response Enable */
#define  USBDC_UDCCSR0_ACM	(1<<9)	/* ACK Control Mode */
#define USBDC_UDCCSR(n)	(0x0100+4*(n)) /* UDC Control/Status Registers */
#define  USBDC_UDCCSR_FS	(1<<0)	/* FIFO Needs Service */
#define  USBDC_UDCCSR_PC	(1<<1)	/* Packet Complete */
#define  USBDC_UDCCSR_TRN	(1<<2)	/* Tx/Rx NAK */
#define  USBDC_UDCCSR_DME	(1<<3)	/* DMA Enable */
#define  USBDC_UDCCSR_SST	(1<<4)	/* Sent STALL */
#define  USBDC_UDCCSR_FST	(1<<5)	/* Force STALL */
#define  USBDC_UDCCSR_BNE	(1<<6)	/* OUT: Buffer Not Empty */
#define  USBDC_UDCCSR_BNF	(1<<6)	/* IN: Buffer Not Full */
#define  USBDC_UDCCSR_SP	(1<<7)	/* Short Packet Control/Status */
#define  USBDC_UDCCSR_FEF	(1<<8)	/* Flush Endpoint FIFO */
#define  USBDC_UDCCSR_DPE	(1<<9)	/* Data Packet Empty (async EP only) */
/* 0x0160 to 0x01fc is reserved */
#define USBDC_UDCBCR(n)	(0x0200+4*(n)) /* UDC Byte Count Registers */
#define  USBDC_UDCBCR_BC	(1023<<0) /* Byte Count */
/* 0x0260 to 0x02fc is reserved */
#define USBDC_UDCDR(n)	(0x0300+4*(n))	/* UDC Data Registers */
/* 0x0360 to 0x03fc is reserved */
/* 0x0400 is reserved */
#define USBDC_UDCECR(n)	(0x0400+4*(n)) /* UDC Configuration Registers */
#define  USBDC_UDCECR_EE	(1<<0)	/* Endpoint Enable */
#define  USBDC_UDCECR_DE	(1<<1)	/* Double-Buffering Enable */
#define  USBDC_UDCECR_MPE	(1023<<2) /* Maximum Packet Size */
#define  USBDC_UDCECR_ED	(1<<12)	/* USB Endpoint Direction */
#define  USBDC_UDCECR_ET	(3<<13)	/* USB Enpoint Type */
#define  USBDC_UDCECR_EN	(15<<15) /* Endpoint Number */
#define  USBDC_UDCECR_AISN	(7<<19)	/* Alternate Interface Number */
#define  USBDC_UDCECR_IN	(7<<22)	/* Interface Number */
#define  USBDC_UDCECR_CN	(3<<25)	/* Configuration Number */

#endif /* _ARM_XSCALE_PXA27X_UDCREG_H_ */

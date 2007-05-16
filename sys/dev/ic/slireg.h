/*	$OpenBSD: slireg.h,v 1.3 2007/05/16 09:31:01 dlg Exp $ */

/*
 * Copyright (c) 2007 David Gwynne <dlg@openbsd.org>
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

#define SLI_PCI_BAR_SLIM		0x10
#define SLI_PCI_BAR_REGISTER		0x18
#define SLI_PCI_BAR_BIU			0x20
#define SLI_PCI_BAR_REGISTER_IO		0x24

#define SLI_REG_HA		0x0 /* Host Attention */
#define  SLI_REG_HA_ERATT		(1<<31)
#define  SLI_REG_HA_MBATT		(1<<30)
#define  SLI_REG_HA_LATT		(1<<29)
#define  SLI_REG_HA_R3ATT		(1<<15)
#define  SLI_REG_HA_R3CE_RSP		(1<<13)
#define  SLI_REG_HA_R3RE_REQ		(1<<12)
#define  SLI_REG_HA_R2ATT		(1<<11)
#define  SLI_REG_HA_R2CE_RSP		(1<<9)
#define  SLI_REG_HA_R2RE_REQ		(1<<8)
#define  SLI_REG_HA_R1ATT		(1<<7)
#define  SLI_REG_HA_R1CE_RSP		(1<<5)
#define  SLI_REG_HA_R1RE_REQ		(1<<4)
#define  SLI_REG_HA_R0ATT		(1<<3)
#define  SLI_REG_HA_R0CE_RSP		(1<<1)
#define  SLI_REG_HA_R0RE_REQ		(1<<0)
#define SLI_FMT_HA		"\020" "\040ERATT" "\037MBATT" "\036LATT" \
				    "\020R3ATT" "\016R3CE_RSP" "\015R3RE_REQ" \
				    "\014R3ATT" "\012R3CE_RSP" "\011R3RE_REQ" \
				    "\010R3ATT" "\006R3CE_RSP" "\005R3RE_REQ" \
				    "\040R3ATT" "\002R3CE_RSP" "\001R3RE_REQ"
#define SLI_REG_CA		0x4 /* Chip Attention */
#define  SLI_REG_CA_MBATT		(1<<30)
#define  SLI_REG_CA_R3ATT		(1<<15)
#define  SLI_REG_CA_R3RE_RSP		(1<<13)
#define  SLI_REG_CA_R3CE_REQ		(1<<12)
#define  SLI_REG_CA_R2ATT		(1<<11)
#define  SLI_REG_CA_R2RE_RSP		(1<<9)
#define  SLI_REG_CA_R2CE_REQ		(1<<8)
#define  SLI_REG_CA_R1ATT		(1<<7)
#define  SLI_REG_CA_R1RE_RSP		(1<<5)
#define  SLI_REG_CA_R1CE_REQ		(1<<4)
#define  SLI_REG_CA_R0ATT		(1<<3)
#define  SLI_REG_CA_R0RE_RSP		(1<<1)
#define  SLI_REG_CA_R0CE_REQ		(1<<0)
#define SLI_FMT_CA		"\020" "\037MBATT" \
				    "\020R3ATT" "\016R3RE_RSP" "\015R3CE_REQ" \
				    "\014R3ATT" "\012R3RE_RSP" "\011R3CE_REQ" \
				    "\010R3ATT" "\006R3RE_RSP" "\005R3CE_REQ" \
				    "\040R3ATT" "\002R3RE_RSP" "\001R3CE_REQ"
#define SLI_REG_HS		0x8 /* Host Status */
#define  SLI_REG_HS_FFER_MASK		(0xff000000)
#define  SLI_REG_HS_FF1			(1<<31)
#define  SLI_REG_HS_FF2			(1<<30)
#define  SLI_REG_HS_FF3			(1<<29)
#define  SLI_REG_HS_FF4			(1<<28)
#define  SLI_REG_HS_FF5			(1<<27)
#define  SLI_REG_HS_FF6			(1<<26)
#define  SLI_REG_HS_FF7			(1<<25)
#define  SLI_REG_HS_FF8			(1<<24)
#define  SLI_REG_HS_FFRDY		(1<<23)
#define  SLI_REG_HS_MBRDY		(1<<22)
#define SLI_FMT_HS		"\020" "\040FF1" "\037FF2" "\036FF3" \
				    "\035FF4" "\034FF5" "\033FF6" "\032FF7" \
				    "\031FF8" "\030FFRDY" "\027MBRDY"
#define SLI_REG_HC		0xc /* Host Control */
#define  SLI_REG_HC_ERINT		(1<<31)
#define  SLI_REG_HC_LAINT		(1<<29)
#define  SLI_REG_HC_INITFF		(1<<27)
#define  SLI_REG_HC_INITMB		(1<<26)
#define  SLI_REG_HC_INITHBI		(1<<25)
#define  SLI_REG_HC_R3INT		(1<<4)
#define  SLI_REG_HC_R2INT		(1<<3)
#define  SLI_REG_HC_R1INT		(1<<2)
#define  SLI_REG_HC_R0INT		(1<<1)
#define  SLI_REG_HC_MBINT		(1<<0)
#define SLI_FMT_HC		"\020" "\040ERINT" "\036LAINT" "\035INITFF" \
				    "\034INITMB" "\033INITHBI" "\005R3INT" \
				    "\004R2INT" "\003R1INT" "\002R0INT" \
				    "\001MBINT"

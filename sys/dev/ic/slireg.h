/*	$OpenBSD: slireg.h,v 1.4 2007/05/19 10:24:18 dlg Exp $ */

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

/*
 * PCI BARs
 */
#define SLI_PCI_BAR_SLIM		0x10
#define SLI_PCI_BAR_REGISTER		0x18
#define SLI_PCI_BAR_BIU			0x20
#define SLI_PCI_BAR_REGISTER_IO		0x24

/*
 * Registers in the REGISTER BAR
 */
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

/*
 * Mailbox commands
 */
#define SLI_CMD_SHUTDOWN	0x00
#define SLI_CMD_LOAD_SM		0x01
#define SLI_CMD_READ_NV		0x02
#define SLI_CMD_WRITE_NV	0x03
#define SLI_CMD_RUN_BIU_DIAG	0x04
#define SLI_CMD_INIT_LINK	0x05
#define SLI_CMD_DOWN_LINK	0x06
#define SLI_CMD_CONFIG_LINK	0x07
#define SLI_CMD_CONFIG_RING	0x09
#define SLI_CMD_RESET_RING	0x0a
#define SLI_CMD_READ_CONFIG	0x0b
#define SLI_CMD_READ_RCONFIG	0x0c
#define SLI_CMD_READ_SPARM	0x0d
#define SLI_CMD_READ_STATUS	0x0e
#define SLI_CMD_READ_RPI	0x0f
#define SLI_CMD_READ_XRI	0x10
#define SLI_CMD_READ_REV	0x11
#define SLI_CMD_READ_LNK_STAT	0x12
#define SLI_CMD_REG_LOGIN	0x13
#define SLI_CMD_UNREG_LOGIN	0x14
#define SLI_CMD_READ_LA		0x15
#define SLI_CMD_CLEAR_LA	0x16
#define SLI_CMD_DUMP_MEMORY	0x17
#define SLI_CMD_DUMP_CONTEXT	0x18
#define SLI_CMD_RUN_DIAGS	0x19
#define SLI_CMD_RESTART		0x1a
#define SLI_CMD_UPDATE_CFG	0x1b
#define SLI_CMD_DOWN_LOAD	0x1c
#define SLI_CMD_DEL_LD_ENTRY	0x1d
#define SLI_CMD_RUN_PROGRAM	0x1e
#define SLI_CMD_SET_MASK	0x20
#define SLI_CMD_SEL_SLIM	0x21
#define SLI_CMD_UNREG_D_ID	0x23
#define SLI_CMD_KILL_BOARD	0x24
#define SLI_CMD_CONFIG_FARP	0x25
#define SLI_CMD_LOAD_AREA	0x81
#define SLI_CMD_RUN_BIU_DIAG64	0x84
#define SLI_CMD_CONFIG_PORT	0x88
#define SLI_CMD_READ_SPARM64	0x8d
#define SLI_CMD_READ_RPI64	0x8f
#define SLI_CMD_REG_LOGIN64	0x93
#define SLI_CMD_READ_LA64	0x95
#define SLI_CMD_FLAGS_WR_ULA	0x98
#define SLI_CMD_FLSET_DEBUG	0x99
#define SLI_CMD_LOAD_EXP_ROM	0x9c
#define SLI_CMD_MAX_CMDS	0x9d

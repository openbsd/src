/*	$OpenBSD: grtworeg.h,v 1.3 2014/03/27 20:15:00 miod Exp $	*/
/* $NetBSD: grtworeg.h,v 1.2 2005/12/11 12:18:53 christos Exp $	 */

/*
 * Copyright (c) 2004 Christopher SEKIYA
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * <<Id: LICENSE_GC,v 1.1 2001/10/01 23:24:05 cgd Exp>>
 */

/*
 * Memory map:
 *
 * 0x1f000000 - 0x1f01ffff	Shared data RAM
 * 0x1f020000 - 0x1f03ffff	(unused)
 * 0x1f040000 - 0x1f05ffff	FIFO
 * 0x1f060000 - 0x1f068000	HQ2 ucode
 * 0x1f068000 - 0x1f069fff	GE7 (eight of them)
 * 0x1f06a000 - 0x1f06b004	HQ2
 * 0x1f06c000			Board revision register
 * 0x1f06c020			clock
 * 0x1f06c040			VC1
 * 0x1f06c060			BT479 Triple-DAC (read)
 * 0x1f06c080			BT479 Triple-DAC (write)
 * 0x1f06c0a0			BT457 DAC (red)
 * 0x1f06c0c0			BT457 DAC (green)
 * 0x1f06c0e0			BT457 DAC (blue)
 * 0x1f06c100			XMAP5 (five of them)
 * 0x1f06c1a0			XMAP5 ("xmap all")
 * 0x1f06c1c0			Kaleidoscope (AB1)
 * 0x1f06c1e0			Kaleidoscope (CC1)
 * 0x1f06c200			RE3 (27-bit registers)
 * 0x1f06c280			RE3 (24-bit registers)
 * 0x1f06c600			RE3 (32-bit registers)
 */

#define GR2_FIFO			0x40000
#define GR2_FIFO_INIT			(GR2_FIFO + 0x644)
#define GR2_FIFO_COLOR               	(GR2_FIFO + 0x648)
#define GR2_FIFO_FINISH              	(GR2_FIFO + 0x64c)
#define GR2_FIFO_PNT2I               	(GR2_FIFO + 0x650)
#define GR2_FIFO_RECTI2D             	(GR2_FIFO + 0x654)
#define GR2_FIFO_CMOV2I              	(GR2_FIFO + 0x658)
#define GR2_FIFO_LINE2I              	(GR2_FIFO + 0x65c)
#define GR2_FIFO_DRAWCHAR            	(GR2_FIFO + 0x660)
#define GR2_FIFO_RECTCOPY            	(GR2_FIFO + 0x664)
#define GR2_FIFO_DATA			(GR2_FIFO + 0x77c)

/* HQ2 */

#define	HQ2_BASE			0x6a000
#define	HQ2_ATTRJUMP			(HQ2_BASE + 0x00)
#define HQ2_VERSION			(HQ2_BASE + 0x40)
#define HQ2_VERSION_MASK		0xff000000
#define HQ2_VERSION_SHIFT		23

#define HQ2_NUMGE			(HQ2_BASE + 0x44)
#define HQ2_FIN1			(HQ2_BASE + 0x48)
#define HQ2_FIN2			(HQ2_BASE + 0x4c)
#define HQ2_DMASYNC			(HQ2_BASE + 0x50)
#define HQ2_FIFO_FULL_TIMEOUT		(HQ2_BASE + 0x54)
#define HQ2_FIFO_EMPTY_TIMEOUT		(HQ2_BASE + 0x58)
#define HQ2_FIFO_FULL			(HQ2_BASE + 0x5c)
#define HQ2_FIFO_EMPTY			(HQ2_BASE + 0x60)
#define HQ2_GE7_LOAD_UCODE		(HQ2_BASE + 0x64)
#define HQ2_GEDMA			(HQ2_BASE + 0x68)
#define HQ2_HQ_GEPC			(HQ2_BASE + 0x6c)
#define HQ2_GEPC			(HQ2_BASE + 0x70)
#define HQ2_INTR			(HQ2_BASE + 0x74)
#define HQ2_UNSTALL			(HQ2_BASE + 0x78)
#define HQ2_MYSTERY			(HQ2_BASE + 0x7c)
#define	HQ2_MYSTERY_VALUE		0xdeadbeef
#define HQ2_REFRESH			(HQ2_BASE + 0x80)
#define HQ2_FIN3			(HQ2_BASE + 0x1000)

/* GE7 */

#define GE7_REVISION			0x680fc
#define GE7_REVISION_MASK		0xe0
#define	GE7_REVISION_SHIFT		5

/* VC1 */

#define VC1_BASE			0x6c040
#define VC1_COMMAND			(VC1_BASE + 0x00)
#define VC1_XMAPMODE			(VC1_BASE + 0x04)
#define VC1_SRAM			(VC1_BASE + 0x08)
#define VC1_TESTREG			(VC1_BASE + 0x0c)
#define VC1_ADDRLO			(VC1_BASE + 0x10)
#define VC1_ADDRHI			(VC1_BASE + 0x14)
#define VC1_SYSCTL			(VC1_BASE + 0x18)

/* VC1 System Control Register */
#define VC1_SYSCTL_INTERRUPT		0x01	/* active low */
#define VC1_SYSCTL_VTG			0x02	/* active low */
#define VC1_SYSCTL_VC1			0x04
#define VC1_SYSCTL_DID			0x08
#define VC1_SYSCTL_CURSOR		0x10
#define VC1_SYSCTL_CURSOR_DISPLAY	0x20
#define VC1_SYSCTL_GENSYNC		0x40
#define VC1_SYSCTL_VIDEO		0x80

/* VC1 SRAM memory map */
#define VC1_SRAM_VIDTIM_LST_BASE	0x0000
#define VC1_SRAM_VIDTIM_CURSLST_BASE	0x0400
#define VC1_SRAM_VIDTIM_FRMT_BASE	0x0800
#define VC1_SRAM_VIDTIM_CURSFRMT_BASE	0x0900
#define VC1_SRAM_INTERLACED		0x09f0
#define VC1_SRAM_SCREENWIDTH		0x09f2
#define VC1_SRAM_NEXTDID_ADDR		0x09f4
#define VC1_SRAM_CURSOR0_BASE		0x0a00	/* 32x32 */
#define VC1_SRAM_DID_FRMT_BASE		0x0b00
#define VC1_SRAM_DID_MAX_FMTSIZE	0x0900
#define VC1_SRAM_DID_LST_END		0x8000

/* VC1 registers */
#define VC1_VIDEO_EP			0x00
#define VC1_VIDEO_LC			0x02
#define VC1_VIDEO_SC			0x04
#define VC1_VIDEO_TSA			0x06
#define VC1_VIDEO_TSB			0x07
#define VC1_VIDEO_TSC			0x08
#define VC1_VIDEO_LP			0x09
#define VC1_VIDEO_LS_EP			0x0b
#define VC1_VIDEO_LR			0x0d
#define VC1_VIDEO_FC			0x10
#define VC1_VIDEO_ENABLE		0x14

/* Cursor Generator */
#define VC1_CURSOR_EP			0x20
#define VC1_CURSOR_XL			0x22
#define VC1_CURSOR_YL			0x24
#define VC1_CURSOR_MODE			0x26
#define VC1_CURSOR_BX			0x27
#define VC1_CURSOR_LY			0x28
#define VC1_CURSOR_YC			0x2a
#define VC1_CURSOR_CC			0x2e
#define VC1_CURSOR_RC			0x30

/* Board revision register */

#define	GR2_REVISION			0x6c000
#define GR2_REVISION_RD0		0x6c000
#define GR2_REVISION_RD0_VERSION_MASK	0x0f
#define GR2_REVISION4_RD0_MONITOR_MASK	0xf0
#define GR2_REVISION4_RD0_MONITOR_SHIFT	4

#define GR2_REVISION_RD1		0x6c004
#define GR2_REVISION_RD1_BACKEND_REV	0x03
#define GR2_REVISION_RD1_ZBUFFER	0x0c

#define GR2_REVISION4_RD1_BACKEND	0x03
#define GR2_REVISION4_RD1_24BPP		0x10
#define GR2_REVISION4_RD1_ZBUFFER	0x20

#define GR2_REVISION_RD2		0x6c008
#define GR2_REVISION_RD2_BACKEND_REV	0x0c
#define GR2_REVISION_RD2_BACKEND_SHIFT	2

/* one slot = 8bpp, two slots = 16bpp, three slots = 24bpp, br < 4 only */
#define GR2_REVISION_RD3		0x6c00c
#define GR2_REVISION_RD3_VMA		0x03	/* both bits set == empty
						 * slot */
#define GR2_REVISION_RD3_VMB		0x0c
#define GR2_REVISION_RD3_VMC		0x30

/* Bt479 */

#define	BT479_R			0x6c060
#define	BT479_W			0x6c080

#define	BT479_WRADDR			0x00
#define	BT479_CMAPDATA			0x04
#define	BT479_MASK			0x08	/* pixel read mask */
#define	BT479_RDADDR			0x0c
#define	BT479_OVWRADDR			0x10
#define	BT479_OVDATA			0x14
#define	BT479_CTL			0x18
#define	BT479_OVRDADDR			0x1c

#define	GR2_CMAP12		0x0000
#define	GR2_CMAP8		0x1000
#define	GR2_CMAP4		0x1400

/* Bt457 */

#define	BT457_R			0x6c0a0
#define	BT457_G			0x6c0c0
#define	BT457_B			0x6c0e0

#define	BT457_ADDR		0x00
#define	BT457_CMAPDATA		0x04
#define	BT457_CTRL		0x08
#define	BT457_OVDATA		0x0c

/* XMAP5 */

#define	XMAP5_BASE0		0x6c100
#define	XMAP5_BASE1		0x6c120
#define	XMAP5_BASE2		0x6c140
#define	XMAP5_BASE3		0x6c160
#define	XMAP5_BASE4		0x6c180
#define	XMAP5_BASEALL		0x6c1a0

#define XMAP5_MISC		0x00
#define XMAP5_MODE		0x04
#define XMAP5_CLUT		0x08
#define XMAP5_CRC		0x0c
#define XMAP5_ADDRLO		0x10
#define XMAP5_ADDRHI		0x14
#define XMAP5_BYTECOUNT		0x18
#define XMAP5_FIFOSTATUS	0x1c

/*
 * FIFO operation constraints.
 */

#define	GR2_DRAWCHAR_HEIGHT	18

/*	$OpenBSD: gbereg.h,v 1.3 2007/12/31 12:46:14 jsing Exp $ */

/*
 * Copyright (c) 2007, Joel Sing <jsing@openbsd.org>
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
 * GBE Framebuffer for SGI O2.
 */

#define GBE_BASE		0x16000000
#define GBE_REG_SIZE		0x100000
#define GBE_TLB_SIZE		128

#define GBE_TILE_SHIFT	16
#define GBE_TILE_SIZE		(1 << GBE_TILE_SHIFT)

/* 
 * Register locations.
 */

#define GBE_CTRL_STAT		0x00000000	/* General control/status */
#define GBE_DOTCLOCK		0x00000004	/* Dotclock */
#define   GBE_DOTCLOCK_RUN	0x00100000	/* Enable dotclock */
#define GBE_VT_XY		0x00010000	/* Current dot coordinates */
#define   GBE_VT_XY_X_SHIFT	0
#define   GBE_VT_XY_X_MASK	0x00000fff
#define   GBE_VT_XY_Y_SHIFT	12
#define   GBE_VT_XY_Y_MASK	0x00fff000
#define   GBE_VT_XY_FREEZE	0x80000000	/* Freeze pixel counter */
#define GBE_VT_MAXXY		0x00010004	/* */
#define GBE_VT_VSYNC		0x00010008	/* Vertical sync on/off */
#define GBE_VT_HSYNC		0x0001000c	/* Horizontal sync on/off */
#define GBE_VT_VBLANK		0x00010010	/* Vertical blanking */
#define GBE_VT_HBLANK		0x00010014	/* Horizontal blanking */
#define GBE_VT_FLAGS		0x00010018	/* Video timing flags */
#define   GBE_VT_SYNC_LOW	0x00000010	/* Sync on green */
#define GBE_VT_HPIX		0x00010034	/* Horizontal pixel on/off */
#define GBE_VT_VPIX		0x00010038	/* Vertical pixel on/off */
#define   GBE_VT_VPIX_OFF_MASK	0x00000fff
#define   GBE_VT_VPIX_OFF_SHIFT	0
#define   GBE_VT_VPIX_ON_MASK	0x00fff000
#define   GBE_VT_VPIX_ON_SHIFT	12
#define GBE_VT_HCMAP		0x0001003c	/* Horizontal cmap write */
#define   GBE_VT_HCMAP_ON_SHIFT	12
#define GBE_VT_VCMAP		0x00010040	/* Vertical cmap write */
#define   GBE_VT_VCMAP_ON_SHIFT	12
#define GBE_VT_DIDSTARTXY	0x00010044	/* DID reset at x/y */
#define GBE_VT_CRSSTARTXY	0x00010048	/* CRS reset at x/y */
#define GBE_VT_VCSTARTXY	0x0001004c	/* VC reset at x/y */
#define GBE_OVERLAY_TILE	0x00020000	/* Overlay plane - tile width */
#define GBE_OVERLAY_HW_CTRL	0x00020004	/* Overlay plane - h/w control */
#define GBE_OVERLAY_CTRL	0x00020008	/* Overlay plane - control */
#define   GBE_OVERLAY_CTRL_DMA_ENABLE	0x00000001
#define GBE_FB_SIZE_TILE	0x00030000	/* Framebuffer - tile size */
#define   GBE_FB_SIZE_TILE_WIDTH_SHIFT		5
#define   GBE_FB_SIZE_TILE_DEPTH_SHIFT		13
#define   GBE_FB_SIZE_TILE_FIFO_RESET_SHIFT	15
#define GBE_FB_SIZE_PIXEL	0x00030004	/* Framebuffer - pixel size */
#define   GBE_FB_SIZE_PIXEL_HEIGHT_SHIFT	16
#define GBE_FB_HW_CTRL		0x00030008	/* Framebuffer - hardware control */
#define GBE_FB_CTRL		0x0003000c	/* Framebuffer - control */
#define   GBE_FB_CTRL_TILE_PTR_SHIFT	9
#define   GBE_FB_CTRL_DMA_ENABLE	0x00000001
#define GBE_DID_HW_CTRL		0x00040000	/* DID hardware control */
#define GBE_DID_CTRL		0x00040004	/* DID control */
#define GBE_MODE		0x00048000	/* Colour mode */
#define   GBE_WID_MODE_SHIFT	2
#define GBE_CMAP		0x050000	/* Colourmap */
#define   GBE_CMAP_ENTRIES	6144
#define GBE_CMAP_FIFO		0x058000	/* Colourmap FIFO status */
#define   GBE_CMAP_FIFO_ENTRIES	63
#define GBE_GMAP		0x060000	/* Gammamap */
#define   GBE_GMAP_ENTRIES	256
#define GBE_CURSOR_POS		0x00070000	/* Cursor position */
#define GBE_CURSOR_CTRL		0x00070004	/* Cursor control */

/* 
 * Constants.
 */

#define GBE_FB_DEPTH_8		0
#define GBE_FB_DEPTH_16		1
#define GBE_FB_DEPTH_32		2

#define GBE_CMODE_I8		0	/* 8 bit indexed */
#define GBE_CMODE_I12		1	/* 12 bit indexed */
#define GBE_CMODE_RG3B2		2	/* 3:3:2 direct */
#define GBE_CMODE_RGB4		3	/* 4:4:4 direct */
#define GBE_CMODE_ARGB5		4	/* 1:5:5:5 direct */
#define GBE_CMODE_RGB8		5	/* 8:8:8 direct */
#define GBE_CMODE_RGBA5		6	/* 5:5:5:5 direct */
#define GBE_CMODE_RGB10		7	/* 10:10:10 direct */

#define GBE_BMODE_BOTH		3

/*
 * Console functions.
 */
int gbe_cnprobe(bus_space_tag_t, bus_addr_t addr);
int gbe_cnattach(bus_space_tag_t, bus_addr_t addr);
